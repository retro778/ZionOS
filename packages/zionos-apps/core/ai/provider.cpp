// SPDX-License-Identifier: GPL-3.0-or-later
#include "provider.h"

#include "core/redact.h"

#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSslConfiguration>

namespace zion::ai {

ChatProvider::ChatProvider(QNetworkAccessManager *nam, QObject *parent)
    : QObject(parent), m_nam(nam)
{
}

void HttpChatStream::applyTransportPolicy(QNetworkRequest &request)
{
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
    request.setAttribute(QNetworkRequest::CacheSaveControlAttribute, false);
    request.setAttribute(QNetworkRequest::CookieSaveControlAttribute, QNetworkRequest::Manual);
    request.setAttribute(QNetworkRequest::CookieLoadControlAttribute, QNetworkRequest::Manual);
    if (request.url().scheme() == QLatin1String("https")) {
        QSslConfiguration ssl = QSslConfiguration::defaultConfiguration();
        ssl.setPeerVerifyMode(QSslSocket::VerifyPeer);
        ssl.setProtocol(QSsl::TlsV1_2OrLater);
        request.setSslConfiguration(ssl);
    }
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("zionos-assistant/" ZIONOS_VERSION));
}

HttpChatStream::HttpChatStream(QNetworkAccessManager *nam, const QNetworkRequest &request,
                               const QByteArray &body, int idleTimeoutMs, QObject *parent)
    : ChatStream(parent)
{
    m_idle.setSingleShot(true);
    m_idle.setInterval(idleTimeoutMs);
    connect(&m_idle, &QTimer::timeout, this, &HttpChatStream::onTimeout);

    m_reply = nam->post(request, body);
    // Certificate problems are fatal. ignoreSslErrors() is never called, so
    // Qt aborts the handshake; this hook only makes that explicit.
    connect(m_reply, &QNetworkReply::sslErrors, this, [this](const QList<QSslError> &) {
        if (m_reply)
            m_reply->abort();
    });
    connect(m_reply, &QNetworkReply::readyRead, this, &HttpChatStream::onReadyRead);
    connect(m_reply, &QNetworkReply::finished, this, &HttpChatStream::onFinished);
    m_idle.start();
}

HttpChatStream::~HttpChatStream()
{
    if (m_reply) {
        m_reply->disconnect(this);
        m_reply->abort();
        m_reply->deleteLater();
    }
}

void HttpChatStream::cancel()
{
    if (m_done)
        return;
    ProviderError e;
    e.kind = ProviderError::Cancelled;
    fail(e);
}

void HttpChatStream::complete(const ChatResult &result)
{
    if (m_done)
        return;
    m_done = true;
    m_idle.stop();
    ChatResult r = result;
    if (r.requestId.isEmpty())
        r.requestId = m_requestId;
    emit finished(r);
    if (m_reply)
        m_reply->abort();
}

void HttpChatStream::fail(const ProviderError &error)
{
    if (m_done)
        return;
    m_done = true;
    m_idle.stop();
    ProviderError e = error;
    e.message = Redactor::redact(e.message);
    if (e.requestId.isEmpty())
        e.requestId = m_requestId;
    emit failed(e);
    if (m_reply)
        m_reply->abort();
}

void HttpChatStream::onTimeout()
{
    ProviderError e;
    e.kind = ProviderError::Timeout;
    fail(e);
}

static QString headerValue(const QNetworkReply *reply, const char *name)
{
    return reply ? QString::fromLatin1(reply->rawHeader(name)) : QString();
}

void HttpChatStream::onReadyRead()
{
    if (m_done || !m_reply)
        return;
    m_idle.start(); // data arrived: restart the idle timer
    const int status = m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (m_requestId.isEmpty()) {
        m_requestId = headerValue(m_reply, "request-id");
        if (m_requestId.isEmpty())
            m_requestId = headerValue(m_reply, "x-request-id");
    }
    const QByteArray chunk = m_reply->readAll();
    if (status < 200 || status >= 300) {
        if (m_errorBody.size() < 256 * 1024)
            m_errorBody.append(chunk);
        return;
    }
    const auto events = m_parser.feed(chunk);
    if (m_parser.overflowed()) {
        ProviderError e;
        e.kind = ProviderError::Protocol;
        e.httpStatus = status;
        e.message = QStringLiteral("stream event exceeded the size limit");
        fail(e);
        return;
    }
    for (const SseEvent &ev : events) {
        if (m_done)
            return;
        if (ev.data == "[DONE]")
            continue;
        QJsonParseError pe;
        const QJsonDocument doc = QJsonDocument::fromJson(ev.data, &pe);
        if (pe.error != QJsonParseError::NoError || !doc.isObject())
            continue; // tolerate unknown non-JSON events
        m_gotEvents = true;
        handleEvent(ev.event, doc.object());
    }
}

void HttpChatStream::onFinished()
{
    if (m_done || !m_reply)
        return;
    onReadyRead();
    if (m_done)
        return;
    const int status = m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status >= 300 || (status > 0 && status < 200)) {
        fail(errorFromHttp(status, m_errorBody, m_reply));
        return;
    }
    if (m_reply->error() != QNetworkReply::NoError) {
        fail(errorFromNetwork(m_reply));
        return;
    }
    // HTTP 200 and the connection closed without a terminal event.
    ProviderError e;
    e.kind = ProviderError::Protocol;
    e.httpStatus = status;
    e.message = m_gotEvents ? QStringLiteral("stream ended before the reply was complete")
                            : QStringLiteral("no streaming events in response");
    fail(e);
}

ProviderError HttpChatStream::errorFromNetwork(const QNetworkReply *reply)
{
    ProviderError e;
    switch (reply->error()) {
    case QNetworkReply::TimeoutError:
    case QNetworkReply::OperationCanceledError:
        e.kind = ProviderError::Timeout;
        break;
    default:
        e.kind = ProviderError::Network;
        break;
    }
    e.message = reply->errorString();
    return e;
}

ProviderError HttpChatStream::errorFromHttp(int status, const QByteArray &body,
                                            const QNetworkReply *reply)
{
    ProviderError e;
    e.httpStatus = status;
    // Both providers answer errors with {"error": {"type", "message", ...}}.
    const QJsonObject root = QJsonDocument::fromJson(body).object();
    const QJsonObject err = root.value(QLatin1String("error")).toObject();
    e.providerType = err.value(QLatin1String("type")).toString();
    if (e.providerType.isEmpty())
        e.providerType = err.value(QLatin1String("code")).toString();
    e.message = err.value(QLatin1String("message")).toString();
    if (e.message.isEmpty())
        e.message = QString::fromUtf8(body.left(500)).simplified();
    e.requestId = root.value(QLatin1String("request_id")).toString();
    if (e.requestId.isEmpty())
        e.requestId = headerValue(reply, "request-id");
    if (e.requestId.isEmpty())
        e.requestId = headerValue(reply, "x-request-id");

    bool ok = false;
    const int retry = headerValue(reply, "retry-after").toInt(&ok);
    if (ok && retry >= 0)
        e.retryAfterSeconds = retry;

    const QString code = err.value(QLatin1String("code")).toString();
    if (status == 401) {
        e.kind = ProviderError::Authentication;
    } else if (status == 402 || status == 403) {
        e.kind = ProviderError::Permission;
    } else if (status == 429) {
        // OpenAI also answers 429 when the account has no credit left;
        // waiting does not help in that case.
        e.kind = (code == QLatin1String("insufficient_quota")
                  || e.providerType == QLatin1String("insufficient_quota"))
                     ? ProviderError::Permission
                     : ProviderError::RateLimited;
    } else if (status == 529 || status == 503
               || e.providerType == QLatin1String("overloaded_error")) {
        e.kind = ProviderError::Overloaded;
    } else if (status >= 500) {
        e.kind = ProviderError::Server;
    } else if (status >= 300 && status < 400) {
        e.kind = ProviderError::Protocol;
        e.message = QStringLiteral("unexpected redirect (not followed)");
    } else {
        e.kind = ProviderError::InvalidRequest;
    }
    e.message = Redactor::redact(e.message);
    return e;
}

} // namespace zion::ai
