// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "core/ai/chatcontroller.h"

#include <QMainWindow>
#include <QPointer>
#include <QTimer>

class QComboBox;
class QHBoxLayout;
class QLabel;
class QListWidget;
class QNetworkAccessManager;
class QPlainTextEdit;
class QPushButton;
class QScrollArea;
class QVBoxLayout;

namespace zion {

class MessageWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

protected:
    bool eventFilter(QObject *o, QEvent *e) override;

private:
    void buildUi();
    void applySettings();
    void rebuildConversation();
    void refreshHistoryList();
    void refreshModelCombo();
    void updateNotice();
    void updateAttachmentBar();
    void updateBusy(bool busy);
    MessageWidget *addMessage(int kind, const QString &heading);
    void scrollToBottom();

    void sendClicked();
    bool reviewDialog(const QString &text, bool confirm);
    void attachFile();
    void attachClipboard();
    void openSettings();
    void onKeyNeeded(const QString &providerId, bool unavailable, const QString &detail);
    void onFinished(const QString &text, const ai::ChatResult &result);
    void onFailed(const QString &partial, const ai::ProviderError &error);

    QNetworkAccessManager *m_nam;
    ai::CredentialStore *m_store;
    ai::HistoryStore m_history;
    ai::ChatController *m_chat;

    QComboBox *m_provider = nullptr;
    QComboBox *m_model = nullptr;
    QListWidget *m_list = nullptr;
    QWidget *m_historyPanel = nullptr;
    QLabel *m_historyOff = nullptr;
    QScrollArea *m_scroll = nullptr;
    QVBoxLayout *m_messages = nullptr;
    QLabel *m_notice = nullptr;
    QLabel *m_usage = nullptr;
    QWidget *m_attachBar = nullptr;
    QHBoxLayout *m_attachLay = nullptr;
    QPlainTextEdit *m_input = nullptr;
    QPushButton *m_send = nullptr;
    QPushButton *m_stop = nullptr;
    QPushButton *m_review = nullptr;
    QPushButton *m_attachFile = nullptr;
    QPushButton *m_attachClip = nullptr;

    QList<ai::Attachment> m_attachments;
    QPointer<MessageWidget> m_streaming;
    QString m_streamText;
    QTimer m_renderTimer;
    QString m_lastUnsentText;
};

} // namespace zion
