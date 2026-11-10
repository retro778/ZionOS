// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "sseparser.h"
#include "types.h"

#include <QJsonObject>
#include <QNetworkRequest>
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QUrl>

class QNetworkAccessManager;
class QNetworkReply;

namespace zion::ai {

/// One in-flight chat completion. Emits textDelta() as the reply streams in,
/// then exactly one of finished() or failed(). Deleting the object or calling
/// cancel() aborts the HTTP request.
///
/// A ChatStream performs exactly one HTTP request. It never retries: a
/// repeated request could be billed twice.
class ChatStream : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;
    virtual void cancel() = 0;

signals:
    void textDelta(const QString &text);
    void finished(const zion::ai::ChatResult &result);
    void failed(const zion::ai::ProviderError &error);
};

class ModelListReply : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;
signals:
    void finished(const QList<zion::ai::ModelInfo> &models);
    void failed(const zion::ai::ProviderError &error);
};

struct ProviderOptions
{
    int idleTimeoutMs = 120000;   ///< abort when no bytes arrive for this long
    bool anthropicFallback = true;
    bool openAiStore = false;
};

/// Common interface of the AI back-ends. Implementations are thin adapters
/// that translate ChatRequest to the provider's HTTP API and its streaming
/// events back to ChatStream signals.
class ChatProvider : public QObject
{
    Q_OBJECT
public:
    explicit ChatProvider(QNetworkAccessManager *nam, QObject *parent = nullptr);

    virtual QString id() const = 0;            ///< stable identifier ("anthropic")
    virtual QString displayName() const = 0;   ///< "Anthropic (Claude)"
    virtual QUrl baseUrl() const = 0;
    /// Host the user's data is sent to; shown in the transmission notice.
    QString endpointHost() const { return baseUrl().host(); }
    /// Models offered when the live model list has not been fetched.
    virtual QList<ModelInfo> builtinModels() const = 0;
    virtual QString defaultModel() const = 0;

    virtual ChatStream *startChat(const ChatRequest &request, const QString &apiKey,
                                  QObject *parent = nullptr) = 0;
    virtual ModelListReply *listModels(const QString &apiKey, QObject *parent = nullptr) = 0;

    /// The JSON body that startChat() would send. Used by the "review what
    /// will be sent" dialog and by tests. Contains no credentials.
    virtual QJsonObject buildRequestBody(const ChatRequest &request) const = 0;

    void setOptions(const ProviderOptions &o) { m_options = o; }
    const ProviderOptions &options() const { return m_options; }

protected:
    QNetworkAccessManager *nam() const { return m_nam; }
    ProviderOptions m_options;

private:
    QNetworkAccessManager *m_nam;
};

/// Shared machinery for HTTP + SSE providers.
class HttpChatStream : public ChatStream
{
    Q_OBJECT
public:
    HttpChatStream(QNetworkAccessManager *nam, const QNetworkRequest &request,
                   const QByteArray &body, int idleTimeoutMs, QObject *parent);
    ~HttpChatStream() override;
    void cancel() override;

    /// Maps an HTTP error response to a ProviderError (shared with model listing).
    static ProviderError errorFromHttp(int status, const QByteArray &body,
                                       const QNetworkReply *reply);
    static ProviderError errorFromNetwork(const QNetworkReply *reply);
    /// TLS policy for every provider request: system CA store, peer
    /// verification on, redirects off (credentials must not follow a redirect).
    static void applyTransportPolicy(QNetworkRequest &request);

protected:
    /// Called for every SSE event with its parsed JSON payload.
    virtual void handleEvent(const QByteArray &event, const QJsonObject &payload) = 0;
    void complete(const ChatResult &result);
    void fail(const ProviderError &error);
    bool isDone() const { return m_done; }
    QString requestId() const { return m_requestId; }

private:
    void onReadyRead();
    void onFinished();
    void onTimeout();

    QPointer<QNetworkReply> m_reply;
    SseParser m_parser;
    QTimer m_idle;
    QByteArray m_errorBody;
    QString m_requestId;
    bool m_done = false;
    bool m_gotEvents = false;
};

} // namespace zion::ai
