// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "attachments.h"
#include "credentialstore.h"
#include "history.h"
#include "provider.h"

#include <QHash>
#include <QObject>
#include <QPointer>

namespace zion::ai {

/// Conversation logic of the assistant, independent of any widget.
///
/// Guarantees enforced here (and covered by tests):
///  * nothing is sent without an explicit send() call;
///  * exactly the text returned by previewRequest() is what send() transmits;
///  * one send() produces at most one HTTP request (no automatic retries);
///  * history is written only when enabled, and only to the local store.
class ChatController : public QObject
{
    Q_OBJECT
public:
    ChatController(CredentialStore *credentials, HistoryStore *history, QObject *parent = nullptr);

    void addProvider(ChatProvider *provider); ///< takes ownership
    QList<ChatProvider *> providers() const { return m_providers; }
    ChatProvider *provider(const QString &id) const;
    ChatProvider *currentProvider() const;

    QString providerId() const { return m_providerId; }
    void setProviderId(const QString &id);
    QString model() const;                    ///< effective model for the current provider
    void setModel(const QString &model);

    void setSystemPrompt(const QString &p) { m_systemPrompt = p; }
    void setHistoryEnabled(bool on) { m_historyEnabled = on; }
    bool historyEnabled() const { return m_historyEnabled; }
    void setMaxOutputTokens(int n) { m_maxOutputTokens = n; }

    const Conversation &conversation() const { return m_conv; }
    bool busy() const { return m_stream != nullptr || m_waitingForKey; }
    QString pendingAssistantText() const { return m_pending; }

    /// The exact request that send() would transmit for this input.
    ChatRequest previewRequest(const QString &text, const QList<Attachment> &attachments) const;

    void newConversation();
    bool openConversation(const QString &id);
    void deleteConversation(const QString &id);
    int deleteAllConversations();

public slots:
    void send(const QString &text, const QList<zion::ai::Attachment> &attachments = {});
    void stop();

signals:
    void conversationReset();                       ///< whole conversation replaced
    void userMessageAdded(const QString &text);
    /// The keyring is being asked for the key (may involve an unlock prompt).
    void waitingForKey(const QString &providerId);
    void assistantStarted();
    void assistantDelta(const QString &delta);
    /// The turn ended normally. @p text is the complete reply.
    void assistantFinished(const QString &text, const zion::ai::ChatResult &result);
    /// The turn failed or was stopped. @p partialText is what had arrived.
    void assistantFailed(const QString &partialText, const zion::ai::ProviderError &error);
    /// No usable API key: @p keyringUnavailable distinguishes "not configured"
    /// from "keyring locked or missing".
    void keyNeeded(const QString &providerId, bool keyringUnavailable, const QString &detail);
    void busyChanged(bool busy);
    void historyChanged();

private:
    void start(const QString &apiKey);
    void finishTurn();
    void persist();

    CredentialStore *m_credentials;
    HistoryStore *m_history;
    QList<ChatProvider *> m_providers;
    QString m_providerId;
    QHash<QString, QString> m_models;
    QString m_systemPrompt;
    Conversation m_conv;
    QPointer<ChatStream> m_stream;
    QString m_pending;
    bool m_waitingForKey = false;
    bool m_historyEnabled = true;
    int m_maxOutputTokens = 16000;
    quint64 m_generation = 0;
};

} // namespace zion::ai
