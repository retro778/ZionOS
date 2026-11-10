// SPDX-License-Identifier: GPL-3.0-or-later
#include "chatcontroller.h"

namespace zion::ai {

ChatController::ChatController(CredentialStore *credentials, HistoryStore *history, QObject *parent)
    : QObject(parent), m_credentials(credentials), m_history(history)
{
    newConversation();
}

void ChatController::addProvider(ChatProvider *provider)
{
    provider->setParent(this);
    m_providers.append(provider);
    if (m_providerId.isEmpty())
        m_providerId = provider->id();
}

ChatProvider *ChatController::provider(const QString &id) const
{
    for (ChatProvider *p : m_providers)
        if (p->id() == id)
            return p;
    return nullptr;
}

ChatProvider *ChatController::currentProvider() const
{
    return provider(m_providerId);
}

void ChatController::setProviderId(const QString &id)
{
    if (provider(id))
        m_providerId = id;
}

QString ChatController::model() const
{
    const QString m = m_models.value(m_providerId);
    if (!m.isEmpty())
        return m;
    const ChatProvider *p = currentProvider();
    return p ? p->defaultModel() : QString();
}

void ChatController::setModel(const QString &model)
{
    m_models.insert(m_providerId, model.trimmed());
}

ChatRequest ChatController::previewRequest(const QString &text, const QList<Attachment> &attachments) const
{
    ChatRequest r;
    r.model = model();
    r.systemPrompt = m_systemPrompt;
    r.maxOutputTokens = m_maxOutputTokens;
    r.messages = m_conv.messages;
    r.messages.append({Role::User, Attachments::composeUserMessage(text, attachments)});
    return r;
}

void ChatController::newConversation()
{
    stop();
    m_conv = {};
    m_conv.id = HistoryStore::newId();
    m_conv.created = m_conv.updated = QDateTime::currentDateTimeUtc();
    emit conversationReset();
}

bool ChatController::openConversation(const QString &id)
{
    Conversation c;
    if (!m_history || !m_history->load(id, &c))
        return false;
    stop();
    m_conv = c;
    if (provider(c.providerId)) {
        m_providerId = c.providerId;
        if (!c.model.isEmpty())
            m_models.insert(c.providerId, c.model);
    }
    emit conversationReset();
    return true;
}

void ChatController::deleteConversation(const QString &id)
{
    if (m_history)
        m_history->remove(id);
    if (id == m_conv.id)
        newConversation();
    emit historyChanged();
}

int ChatController::deleteAllConversations()
{
    const int n = m_history ? m_history->removeAll() : 0;
    newConversation();
    emit historyChanged();
    return n;
}

void ChatController::send(const QString &text, const QList<Attachment> &attachments)
{
    if (busy() || (text.trimmed().isEmpty() && attachments.isEmpty()) || !currentProvider())
        return;
    const QString composed = Attachments::composeUserMessage(text, attachments);
    m_conv.messages.append({Role::User, composed});
    if (m_conv.title.isEmpty())
        m_conv.title = HistoryStore::titleFor(text.isEmpty() ? attachments.first().label : text);
    m_conv.providerId = m_providerId;
    m_conv.model = model();
    emit userMessageAdded(composed);

    m_waitingForKey = true;
    emit busyChanged(true);
    const quint64 gen = ++m_generation;
    const QString pid = m_providerId;
    m_credentials->read(pid, [this, gen, pid](const CredentialStore::Result &r, const QString &key) {
        if (gen != m_generation || !m_waitingForKey)
            return; // stopped or superseded while the keyring was being asked
        m_waitingForKey = false;
        if (!r.ok || key.isEmpty()) {
            // Undo the optimistic append: nothing was sent.
            if (!m_conv.messages.isEmpty() && m_conv.messages.last().role == Role::User)
                m_conv.messages.removeLast();
            emit busyChanged(false);
            emit keyNeeded(pid, r.unavailable, r.error);
            return;
        }
        start(key);
    });
}

void ChatController::start(const QString &apiKey)
{
    ChatRequest req;
    req.model = model();
    req.systemPrompt = m_systemPrompt;
    req.maxOutputTokens = m_maxOutputTokens;
    req.messages = m_conv.messages;

    m_pending.clear();
    m_stream = currentProvider()->startChat(req, apiKey, this);
    emit assistantStarted();
    connect(m_stream, &ChatStream::textDelta, this, [this](const QString &d) {
        m_pending += d;
        emit assistantDelta(d);
    });
    connect(m_stream, &ChatStream::finished, this, [this](const ChatResult &result) {
        const QString text = m_pending;
        if (!text.isEmpty())
            m_conv.messages.append({Role::Assistant, text});
        finishTurn();
        emit assistantFinished(text, result);
    });
    connect(m_stream, &ChatStream::failed, this, [this](const ProviderError &error) {
        const QString text = m_pending;
        // Keep what the user already saw so the conversation stays coherent;
        // an empty reply leaves the user turn last, ready to be sent again
        // deliberately.
        if (!text.isEmpty())
            m_conv.messages.append({Role::Assistant, text});
        else if (!m_conv.messages.isEmpty() && m_conv.messages.last().role == Role::User)
            m_conv.messages.removeLast();
        finishTurn();
        emit assistantFailed(text, error);
    });
}

void ChatController::finishTurn()
{
    if (m_stream) {
        m_stream->disconnect(this);
        m_stream->deleteLater();
        m_stream = nullptr;
    }
    m_pending.clear();
    m_conv.updated = QDateTime::currentDateTimeUtc();
    persist();
    emit busyChanged(false);
}

void ChatController::persist()
{
    if (!m_historyEnabled || !m_history || m_conv.messages.isEmpty())
        return;
    if (m_history->save(m_conv))
        emit historyChanged();
}

void ChatController::stop()
{
    if (m_waitingForKey) {
        m_waitingForKey = false;
        ++m_generation;
        if (!m_conv.messages.isEmpty() && m_conv.messages.last().role == Role::User)
            m_conv.messages.removeLast();
        emit busyChanged(false);
        ProviderError e;
        e.kind = ProviderError::Cancelled;
        emit assistantFailed({}, e);
        return;
    }
    if (m_stream)
        m_stream->cancel(); // emits failed(Cancelled) -> handled above
}

} // namespace zion::ai
