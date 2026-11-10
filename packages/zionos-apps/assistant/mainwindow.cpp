// SPDX-License-Identifier: GPL-3.0-or-later
#include "mainwindow.h"

#include "core/ai/anthropicprovider.h"
#include "core/ai/openaiprovider.h"
#include "core/appsettings.h"
#include "core/i18n.h"
#include "core/sysinfo.h"
#include "errortext.h"
#include "messagewidget.h"
#include "ui/aisettingswidget.h"

#include <QClipboard>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QShortcut>
#include <QSplitter>
#include <QToolBar>
#include <QVBoxLayout>

namespace zion {

using namespace ai;

/// Test hook: lets integration tests point a provider at a mock server.
/// Only loopback addresses are accepted, so the variable can never be used
/// to redirect a user's API key to another machine.
static QUrl loopbackOverride(const char *envName, const QUrl &fallback)
{
    const QUrl u(qEnvironmentVariable(envName));
    if (u.isValid() && (u.host() == QLatin1String("127.0.0.1") || u.host() == QLatin1String("localhost")
                        || u.host() == QLatin1String("::1")))
        return u;
    return fallback;
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), m_nam(new QNetworkAccessManager(this)), m_store(new CredentialStore(this)),
      m_chat(new ChatController(m_store, &m_history, this))
{
    // Redirects are refused per request; make it the manager default as well.
    m_nam->setRedirectPolicy(QNetworkRequest::ManualRedirectPolicy);
    m_chat->addProvider(new AnthropicProvider(
        m_nam, nullptr, loopbackOverride("ZIONOS_TEST_ANTHROPIC_URL", QUrl(QStringLiteral("https://api.anthropic.com")))));
    m_chat->addProvider(new OpenAiProvider(
        m_nam, nullptr, loopbackOverride("ZIONOS_TEST_OPENAI_URL", QUrl(QStringLiteral("https://api.openai.com/v1")))));

    buildUi();
    applySettings();

    connect(m_chat, &ChatController::conversationReset, this, &MainWindow::rebuildConversation);
    connect(m_chat, &ChatController::historyChanged, this, &MainWindow::refreshHistoryList);
    connect(m_chat, &ChatController::busyChanged, this, &MainWindow::updateBusy);
    connect(m_chat, &ChatController::keyNeeded, this, &MainWindow::onKeyNeeded);
    connect(m_chat, &ChatController::userMessageAdded, this, [this](const QString &composed) {
        addMessage(MessageWidget::User, tr("You"))->setPlainText(MessageWidget::collapseAttachments(composed));
        scrollToBottom();
    });
    connect(m_chat, &ChatController::assistantStarted, this, [this] {
        m_streamText.clear();
        m_streaming = addMessage(MessageWidget::Assistant,
                                 tr("Assistant · %1").arg(I18n::ltrIsolate(m_chat->model())));
        m_streaming->setPlainText(tr("Waiting for the reply…"));
        scrollToBottom();
    });
    connect(m_chat, &ChatController::assistantDelta, this, [this](const QString &d) {
        m_streamText += d;
        if (!m_renderTimer.isActive())
            m_renderTimer.start();
    });
    connect(m_chat, &ChatController::assistantFinished, this, &MainWindow::onFinished);
    connect(m_chat, &ChatController::assistantFailed, this, &MainWindow::onFailed);

    // Re-rendering Markdown for every token would be wasteful; coalesce.
    m_renderTimer.setSingleShot(true);
    m_renderTimer.setInterval(60);
    connect(&m_renderTimer, &QTimer::timeout, this, [this] {
        if (m_streaming) {
            m_streaming->setMarkdown(m_streamText);
            scrollToBottom();
        }
    });

    rebuildConversation();
    refreshHistoryList();
    m_input->setFocus();
}

void MainWindow::buildUi()
{
    setWindowTitle(tr("ZionOS Assistant"));
    resize(980, 700);

    auto *bar = addToolBar(tr("Assistant"));
    bar->setMovable(false);
    auto *newChat = new QPushButton(QIcon::fromTheme(QStringLiteral("document-new")), tr("New chat"));
    newChat->setObjectName(QStringLiteral("newChat"));
    connect(newChat, &QPushButton::clicked, this, [this] { m_chat->newConversation(); });
    bar->addWidget(newChat);
    bar->addSeparator();
    bar->addWidget(new QLabel(tr("Provider:") + QLatin1Char(' ')));
    m_provider = new QComboBox;
    m_provider->setObjectName(QStringLiteral("providerCombo"));
    for (ChatProvider *p : m_chat->providers())
        m_provider->addItem(p->displayName(), p->id());
    bar->addWidget(m_provider);
    bar->addWidget(new QLabel(QLatin1Char(' ') + tr("Model:") + QLatin1Char(' ')));
    m_model = new QComboBox;
    m_model->setObjectName(QStringLiteral("modelCombo"));
    m_model->setEditable(true);
    m_model->setInsertPolicy(QComboBox::NoInsert);
    m_model->setLayoutDirection(Qt::LeftToRight);
    m_model->setMinimumWidth(220);
    bar->addWidget(m_model);
    auto *spacer = new QWidget;
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    bar->addWidget(spacer);
    auto *settings = new QPushButton(QIcon::fromTheme(QStringLiteral("configure")), tr("Settings and API keys"));
    settings->setObjectName(QStringLiteral("openSettings"));
    connect(settings, &QPushButton::clicked, this, &MainWindow::openSettings);
    bar->addWidget(settings);
    // Keyboard access: Ctrl+, settings; Ctrl+N new chat; Escape stops a reply.
    auto *prefs = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Comma), this);
    connect(prefs, &QShortcut::activated, this, &MainWindow::openSettings);
    auto *fresh = new QShortcut(QKeySequence::New, this);
    connect(fresh, &QShortcut::activated, this, [this] { m_chat->newConversation(); });
    auto *esc = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    connect(esc, &QShortcut::activated, m_chat, &ChatController::stop);

    connect(m_provider, &QComboBox::activated, this, [this] {
        m_chat->setProviderId(m_provider->currentData().toString());
        AppSettings().setProvider(m_chat->providerId());
        refreshModelCombo();
        updateNotice();
    });
    auto commitModel = [this] {
        const int idx = m_model->findText(m_model->currentText());
        const QString model = idx >= 0 ? m_model->itemData(idx).toString() : m_model->currentText().trimmed();
        if (model.isEmpty())
            return;
        m_chat->setModel(model);
        AppSettings().setModel(m_chat->providerId(), model);
    };
    connect(m_model, &QComboBox::activated, this, commitModel);
    connect(m_model->lineEdit(), &QLineEdit::editingFinished, this, commitModel);

    auto *split = new QSplitter;
    // --- history panel ---
    m_historyPanel = new QWidget;
    auto *hl = new QVBoxLayout(m_historyPanel);
    hl->setContentsMargins(6, 6, 6, 6);
    hl->addWidget(new QLabel(tr("Saved conversations")));
    m_list = new QListWidget;
    m_list->setObjectName(QStringLiteral("historyList"));
    hl->addWidget(m_list, 1);
    m_historyOff = new QLabel(tr("History is turned off. Conversations are not saved."));
    m_historyOff->setWordWrap(true);
    hl->addWidget(m_historyOff);
    auto *del = new QPushButton(QIcon::fromTheme(QStringLiteral("edit-delete")), tr("Delete selected"));
    del->setObjectName(QStringLiteral("deleteConversation"));
    hl->addWidget(del);
    connect(m_list, &QListWidget::itemActivated, this, [this](QListWidgetItem *it) {
        m_chat->openConversation(it->data(Qt::UserRole).toString());
    });
    connect(m_list, &QListWidget::itemClicked, this, [this](QListWidgetItem *it) {
        if (it->data(Qt::UserRole).toString() != m_chat->conversation().id)
            m_chat->openConversation(it->data(Qt::UserRole).toString());
    });
    connect(del, &QPushButton::clicked, this, [this] {
        if (auto *it = m_list->currentItem())
            m_chat->deleteConversation(it->data(Qt::UserRole).toString());
    });
    split->addWidget(m_historyPanel);

    // --- chat panel ---
    auto *chat = new QWidget;
    auto *cl = new QVBoxLayout(chat);
    cl->setContentsMargins(6, 6, 6, 6);
    m_scroll = new QScrollArea;
    m_scroll->setWidgetResizable(true);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto *inner = new QWidget;
    m_messages = new QVBoxLayout(inner);
    m_messages->setSpacing(10);
    m_messages->addStretch();
    m_scroll->setWidget(inner);
    cl->addWidget(m_scroll, 1);

    m_usage = new QLabel;
    m_usage->setObjectName(QStringLiteral("usageLabel"));
    m_usage->setVisible(false);
    cl->addWidget(m_usage);

    m_notice = new QLabel;
    m_notice->setObjectName(QStringLiteral("transmissionNotice"));
    m_notice->setWordWrap(true);
    cl->addWidget(m_notice);

    m_attachBar = new QWidget;
    m_attachLay = new QHBoxLayout(m_attachBar);
    m_attachLay->setContentsMargins(0, 0, 0, 0);
    m_attachBar->setVisible(false);
    cl->addWidget(m_attachBar);

    m_input = new QPlainTextEdit;
    m_input->setObjectName(QStringLiteral("input"));
    m_input->setPlaceholderText(tr("Ask a question… (Ctrl+Enter to send)"));
    m_input->setMaximumHeight(120);
    m_input->installEventFilter(this);
    cl->addWidget(m_input);

    auto *buttons = new QHBoxLayout;
    m_attachFile = new QPushButton(QIcon::fromTheme(QStringLiteral("mail-attachment")), tr("Attach file…"));
    m_attachFile->setObjectName(QStringLiteral("attachFile"));
    m_attachClip = new QPushButton(QIcon::fromTheme(QStringLiteral("edit-paste")), tr("Attach clipboard text"));
    m_attachClip->setObjectName(QStringLiteral("attachClipboard"));
    m_review = new QPushButton(QIcon::fromTheme(QStringLiteral("document-preview")), tr("Review what will be sent…"));
    m_review->setObjectName(QStringLiteral("review"));
    m_stop = new QPushButton(QIcon::fromTheme(QStringLiteral("process-stop")), tr("Stop"));
    m_stop->setObjectName(QStringLiteral("stop"));
    m_stop->setVisible(false);
    m_send = new QPushButton(QIcon::fromTheme(QStringLiteral("document-send")), tr("Send"));
    m_send->setObjectName(QStringLiteral("send"));
    m_send->setDefault(true);
    buttons->addWidget(m_attachFile);
    buttons->addWidget(m_attachClip);
    buttons->addWidget(m_review);
    buttons->addStretch();
    buttons->addWidget(m_stop);
    buttons->addWidget(m_send);
    cl->addLayout(buttons);
    connect(m_attachFile, &QPushButton::clicked, this, &MainWindow::attachFile);
    connect(m_attachClip, &QPushButton::clicked, this, &MainWindow::attachClipboard);
    connect(m_review, &QPushButton::clicked, this, [this] { reviewDialog(m_input->toPlainText(), false); });
    connect(m_send, &QPushButton::clicked, this, &MainWindow::sendClicked);
    connect(m_stop, &QPushButton::clicked, m_chat, &ChatController::stop);

    split->addWidget(chat);
    split->setStretchFactor(0, 0);
    split->setStretchFactor(1, 1);
    split->setSizes({230, 750});
    setCentralWidget(split);
}

bool MainWindow::eventFilter(QObject *o, QEvent *e)
{
    if (o == m_input && e->type() == QEvent::KeyPress) {
        auto *k = static_cast<QKeyEvent *>(e);
        if ((k->key() == Qt::Key_Return || k->key() == Qt::Key_Enter) && (k->modifiers() & Qt::ControlModifier)) {
            sendClicked();
            return true;
        }
    }
    return QMainWindow::eventFilter(o, e);
}

void MainWindow::applySettings()
{
    AppSettings s;
    ProviderOptions o;
    o.idleTimeoutMs = s.requestTimeoutSeconds() * 1000;
    o.anthropicFallback = s.anthropicFallbackEnabled();
    o.openAiStore = s.openAiStoreResponses();
    for (ChatProvider *p : m_chat->providers())
        p->setOptions(o);
    m_chat->setHistoryEnabled(s.historyEnabled());
    m_chat->setMaxOutputTokens(s.maxOutputTokens());
    m_chat->setSystemPrompt(Attachments::systemPrompt(SystemInfo().prettyName(), I18n::effectiveLanguage()));
    if (!m_chat->busy()) {
        const QString current = m_chat->providerId();
        m_chat->setProviderId(s.provider());
        for (ChatProvider *p : m_chat->providers()) {
            m_chat->setProviderId(p->id());
            const QString m = s.model(p->id());
            m_chat->setModel(m.isEmpty() ? p->defaultModel() : m);
        }
        m_chat->setProviderId(m_chat->provider(s.provider()) ? s.provider() : current);
    }
    m_provider->setCurrentIndex(qMax(0, m_provider->findData(m_chat->providerId())));
    refreshModelCombo();
    updateNotice();
    m_list->setVisible(s.historyEnabled());
    m_historyOff->setVisible(!s.historyEnabled());
    refreshHistoryList();
}

void MainWindow::refreshModelCombo()
{
    QSignalBlocker block(m_model);
    m_model->clear();
    const ChatProvider *p = m_chat->currentProvider();
    if (!p)
        return;
    for (const ModelInfo &m : p->builtinModels())
        m_model->addItem(m.displayName == m.id ? m.id : QStringLiteral("%1 (%2)").arg(m.displayName, m.id), m.id);
    int idx = m_model->findData(m_chat->model());
    if (idx < 0) {
        m_model->addItem(m_chat->model(), m_chat->model());
        idx = m_model->count() - 1;
    }
    m_model->setCurrentIndex(idx);
}

void MainWindow::updateNotice()
{
    const ChatProvider *p = m_chat->currentProvider();
    if (!p)
        return;
    m_notice->setText(tr("When you press Send, this conversation and anything you attached are sent to %1 (%2). "
                         "Nothing else on this computer is shared, and nothing is sent in the background.")
                          .arg(p->displayName(), I18n::ltrIsolate(p->endpointHost())));
}

MessageWidget *MainWindow::addMessage(int kind, const QString &heading)
{
    auto *w = new MessageWidget(MessageWidget::Kind(kind), heading);
    m_messages->insertWidget(m_messages->count() - 1, w);
    return w;
}

void MainWindow::scrollToBottom()
{
    QTimer::singleShot(0, this, [this] {
        m_scroll->verticalScrollBar()->setValue(m_scroll->verticalScrollBar()->maximum());
    });
}

void MainWindow::rebuildConversation()
{
    while (m_messages->count() > 1) {
        QLayoutItem *it = m_messages->takeAt(0);
        delete it->widget();
        delete it;
    }
    m_streaming = nullptr;
    m_usage->setVisible(false);
    const Conversation &c = m_chat->conversation();
    if (c.messages.isEmpty()) {
        auto *hello = addMessage(MessageWidget::Notice, tr("ZionOS Assistant"));
        hello->setPlainText(tr(
            "Ask about ZionOS, paste an error message to have it explained, draft or translate "
            "text, or ask for a terminal command.\n\n"
            "The assistant only suggests commands — it never runs them. Suggested commands can "
            "be wrong: read them before you use them."));
    }
    for (const ChatMessage &m : c.messages) {
        if (m.role == Role::User) {
            addMessage(MessageWidget::User, tr("You"))->setPlainText(MessageWidget::collapseAttachments(m.text));
        } else {
            auto *w = addMessage(MessageWidget::Assistant, tr("Assistant"));
            w->setMarkdown(m.text);
            w->finalize();
        }
    }
    m_provider->setCurrentIndex(qMax(0, m_provider->findData(m_chat->providerId())));
    refreshModelCombo();
    updateNotice();
    scrollToBottom();
}

void MainWindow::refreshHistoryList()
{
    QSignalBlocker block(m_list);
    m_list->clear();
    if (!AppSettings().historyEnabled())
        return;
    const auto all = m_history.list();
    for (const Conversation &c : all) {
        auto *it = new QListWidgetItem(c.title.isEmpty() ? tr("(untitled)") : c.title, m_list);
        it->setData(Qt::UserRole, c.id);
        it->setToolTip(QLocale().toString(c.updated.toLocalTime(), QLocale::ShortFormat));
        if (c.id == m_chat->conversation().id)
            m_list->setCurrentItem(it);
    }
}

void MainWindow::updateBusy(bool busy)
{
    m_send->setVisible(!busy);
    m_stop->setVisible(busy);
    m_input->setReadOnly(busy);
    for (QWidget *w : {static_cast<QWidget *>(m_provider), static_cast<QWidget *>(m_model),
                       static_cast<QWidget *>(m_attachFile), static_cast<QWidget *>(m_attachClip)})
        w->setEnabled(!busy);
}

void MainWindow::updateAttachmentBar()
{
    while (QLayoutItem *it = m_attachLay->takeAt(0)) {
        delete it->widget();
        delete it;
    }
    for (qsizetype i = 0; i < m_attachments.size(); ++i) {
        const Attachment &a = m_attachments.at(i);
        auto *chip = new QPushButton(QIcon::fromTheme(QStringLiteral("edit-delete")),
                                     tr("%1 (%2) — remove").arg(a.label, QLocale().formattedDataSize(a.bytes())));
        chip->setObjectName(QStringLiteral("attachmentChip"));
        connect(chip, &QPushButton::clicked, this, [this, i] {
            if (i < m_attachments.size())
                m_attachments.removeAt(i);
            updateAttachmentBar();
        });
        m_attachLay->addWidget(chip);
    }
    m_attachLay->addStretch();
    m_attachBar->setVisible(!m_attachments.isEmpty());
}

void MainWindow::attachFile()
{
    const QString path = QFileDialog::getOpenFileName(this, tr("Attach a text file"));
    if (path.isEmpty())
        return;
    const auto r = Attachments::loadFile(path);
    if (!r.ok) {
        QString why = tr("The file could not be read.");
        if (r.error == QLatin1String("too_large"))
            why = tr("The file is larger than %1. Attach a smaller file or paste the relevant part.")
                      .arg(QLocale().formattedDataSize(Attachments::MaxFileBytes));
        else if (r.error == QLatin1String("not_text"))
            why = tr("Only plain text files (UTF-8) can be attached.");
        QMessageBox::warning(this, tr("Attach file"), why);
        return;
    }
    m_attachments.append(r.attachment);
    updateAttachmentBar();
}

void MainWindow::attachClipboard()
{
    QClipboard *cb = QGuiApplication::clipboard();
    QString text = cb->text(QClipboard::Clipboard);
    QString label = tr("Clipboard text");
    if (text.isEmpty() && cb->supportsSelection()) {
        text = cb->text(QClipboard::Selection);
        label = tr("Selected text");
    }
    if (text.isEmpty()) {
        QMessageBox::information(this, tr("Attach clipboard text"), tr("The clipboard does not contain any text."));
        return;
    }
    if (text.toUtf8().size() > Attachments::MaxFileBytes) {
        QMessageBox::warning(this, tr("Attach clipboard text"), tr("The clipboard text is too large to attach."));
        return;
    }
    Attachment a;
    a.kind = Attachment::Text;
    a.label = label;
    a.content = text;
    m_attachments.append(a);
    updateAttachmentBar();
}

bool MainWindow::reviewDialog(const QString &text, bool confirm)
{
    const ChatProvider *p = m_chat->currentProvider();
    const ChatRequest req = m_chat->previewRequest(text, m_attachments);
    QDialog dlg(this);
    dlg.setWindowTitle(tr("Review what will be sent"));
    dlg.resize(720, 560);
    auto *lay = new QVBoxLayout(&dlg);
    auto *head = new QLabel(tr("The following will be sent to %1 (%2) using the model %3. "
                               "Your API key is sent as a request header and is not shown here.")
                                .arg(p->displayName(), I18n::ltrIsolate(p->endpointHost()), I18n::ltrIsolate(req.model)));
    head->setWordWrap(true);
    lay->addWidget(head);
    auto *view = new QPlainTextEdit;
    view->setObjectName(QStringLiteral("reviewText"));
    view->setReadOnly(true);
    QString body;
    body += QStringLiteral("── ") + tr("Instructions to the assistant") + QStringLiteral(" ──\n") + req.systemPrompt + QStringLiteral("\n\n");
    for (const ChatMessage &m : req.messages) {
        body += QStringLiteral("── ") + (m.role == Role::User ? tr("You") : tr("Assistant")) + QStringLiteral(" ──\n");
        body += m.text + QStringLiteral("\n\n");
    }
    view->setPlainText(body);
    lay->addWidget(view, 1);
    const qint64 bytes = QJsonDocument(p->buildRequestBody(req)).toJson(QJsonDocument::Compact).size();
    lay->addWidget(new QLabel(tr("Total request size: %1").arg(QLocale().formattedDataSize(bytes))));
    auto *box = new QDialogButtonBox;
    QPushButton *send = nullptr;
    if (confirm) {
        send = box->addButton(tr("Send"), QDialogButtonBox::AcceptRole);
        send->setObjectName(QStringLiteral("reviewSend"));
        box->addButton(QDialogButtonBox::Cancel);
    } else {
        box->addButton(QDialogButtonBox::Close);
    }
    connect(box, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    lay->addWidget(box);
    return dlg.exec() == QDialog::Accepted;
}

void MainWindow::sendClicked()
{
    if (m_chat->busy())
        return;
    const QString text = m_input->toPlainText().trimmed();
    if (text.isEmpty() && m_attachments.isEmpty())
        return;
    // Attachments always go through an explicit review of the exact content.
    if (!m_attachments.isEmpty() && !reviewDialog(text, true))
        return;
    m_lastUnsentText = text;
    const QList<Attachment> attachments = m_attachments;
    m_input->clear();
    m_attachments.clear();
    updateAttachmentBar();
    m_usage->setVisible(false);
    m_chat->send(text, attachments);
}

void MainWindow::onFinished(const QString &text, const ChatResult &result)
{
    m_renderTimer.stop();
    m_lastUnsentText.clear();
    if (m_streaming) {
        m_streaming->setMarkdown(text.isEmpty() ? tr("(The reply was empty.)") : text);
        m_streaming->finalize();
        QStringList notes;
        if (result.stopReason == StopReason::MaxTokens)
            notes << tr("The reply was cut off because it reached the maximum length.");
        else if (result.stopReason == StopReason::Refusal)
            notes << tr("The provider declined to answer this request.");
        if (!result.servedModel.isEmpty() && result.servedModel != m_chat->model())
            notes << tr("Answered by model %1.").arg(I18n::ltrIsolate(result.servedModel));
        m_streaming->setFootnote(notes.join(QLatin1Char(' ')));
    }
    m_streaming = nullptr;
    if (result.usage.isKnown()) {
        // Shown exactly as reported; no cost estimate is derived from it.
        auto n = [](qint64 v) { return v >= 0 ? QLocale().toString(v) : QStringLiteral("–"); };
        m_usage->setText(tr("Tokens reported by the provider — input: %1, output: %2, total: %3")
                             .arg(n(result.usage.inputTokens), n(result.usage.outputTokens), n(result.usage.totalTokens)));
        m_usage->setVisible(true);
    }
    scrollToBottom();
}

void MainWindow::onFailed(const QString &partial, const ProviderError &error)
{
    m_renderTimer.stop();
    if (m_streaming) {
        if (partial.isEmpty()) {
            m_streaming->deleteLater();
        } else {
            m_streaming->setMarkdown(partial);
            m_streaming->finalize();
            m_streaming->setFootnote(tr("This reply is incomplete."));
        }
    }
    m_streaming = nullptr;
    const ChatProvider *p = m_chat->currentProvider();
    auto *w = addMessage(error.kind == ProviderError::Cancelled ? MessageWidget::Notice : MessageWidget::Error, QString());
    w->setPlainText(ErrorText::describe(error, p ? p->displayName() : QString()));
    w->setFootnote(ErrorText::details(error));
    if (partial.isEmpty() && !m_lastUnsentText.isEmpty() && m_input->toPlainText().isEmpty()) {
        // Nothing was answered: give the text back so it is not lost. The
        // user decides whether to send it again.
        m_input->setPlainText(m_lastUnsentText);
        if (auto *last = qobject_cast<MessageWidget *>(m_messages->itemAt(m_messages->count() - 3)
                                                           ? m_messages->itemAt(m_messages->count() - 3)->widget() : nullptr);
            last && last->kind() == MessageWidget::User)
            last->deleteLater();
    }
    m_lastUnsentText.clear();
    scrollToBottom();
}

void MainWindow::onKeyNeeded(const QString &providerId, bool unavailable, const QString &detail)
{
    // The user bubble was added optimistically; nothing was sent.
    if (m_messages->count() >= 2) {
        if (auto *last = qobject_cast<MessageWidget *>(m_messages->itemAt(m_messages->count() - 2)->widget());
            last && last->kind() == MessageWidget::User)
            last->deleteLater();
    }
    if (m_input->toPlainText().isEmpty())
        m_input->setPlainText(m_lastUnsentText);
    m_lastUnsentText.clear();
    const ChatProvider *p = m_chat->provider(providerId);
    auto *w = addMessage(MessageWidget::Notice, tr("API key needed"));
    QString text = unavailable
        ? tr("The system keyring could not be opened, so the API key for %1 is not available. "
             "Unlock the keyring (KDE Wallet), or enter the key for this session only. Nothing was sent.")
        : tr("No API key is saved for %1. Add your own key to use the assistant. Nothing was sent.");
    text = text.arg(p ? p->displayName() : providerId);
    if (unavailable && !detail.isEmpty())
        text += QStringLiteral("\n") + detail;
    w->setPlainText(text);
    auto *open = new QPushButton(tr("Open assistant settings"));
    open->setObjectName(QStringLiteral("openSettingsInline"));
    connect(open, &QPushButton::clicked, this, &MainWindow::openSettings);
    w->addAction(open);
    scrollToBottom();
}

void MainWindow::openSettings()
{
    QDialog dlg(this);
    dlg.setWindowTitle(tr("Assistant settings"));
    dlg.resize(700, 640);
    auto *lay = new QVBoxLayout(&dlg);
    auto *area = new QScrollArea;
    area->setWidgetResizable(true);
    area->setFrameShape(QFrame::NoFrame);
    auto *w = new AiSettingsWidget(m_store, m_chat->providers(), /*allowSessionKeys=*/true);
    area->setWidget(w);
    lay->addWidget(area, 1);
    auto *box = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(box, &QDialogButtonBox::rejected, &dlg, &QDialog::accept);
    lay->addWidget(box);
    connect(w, &AiSettingsWidget::historyCleared, this, [this] { m_chat->deleteAllConversations(); });
    dlg.exec();
    applySettings();
    m_input->setFocus();
}

} // namespace zion
