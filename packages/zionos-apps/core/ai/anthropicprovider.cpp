// SPDX-License-Identifier: GPL-3.0-or-later
#include "anthropicprovider.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>

namespace zion::ai {

namespace {

class AnthropicStream : public HttpChatStream
{
public:
    using HttpChatStream::HttpChatStream;

protected:
    void handleEvent(const QByteArray &, const QJsonObject &p) override
    {
        const QString type = p.value(QLatin1String("type")).toString();
        if (type == QLatin1String("content_block_delta")) {
            const QJsonObject d = p.value(QLatin1String("delta")).toObject();
            // Only text is shown. thinking_delta / signature_delta / tool
            // input deltas are ignored.
            if (d.value(QLatin1String("type")).toString() == QLatin1String("text_delta")) {
                const QString t = d.value(QLatin1String("text")).toString();
                if (!t.isEmpty())
                    emit textDelta(t);
            }
        } else if (type == QLatin1String("message_start")) {
            const QJsonObject m = p.value(QLatin1String("message")).toObject();
            m_result.servedModel = m.value(QLatin1String("model")).toString();
            readUsage(m.value(QLatin1String("usage")).toObject());
        } else if (type == QLatin1String("message_delta")) {
            const QJsonObject d = p.value(QLatin1String("delta")).toObject();
            const QString stop = d.value(QLatin1String("stop_reason")).toString();
            if (!stop.isEmpty())
                m_result.rawStopReason = stop;
            readUsage(p.value(QLatin1String("usage")).toObject());
        } else if (type == QLatin1String("message_stop")) {
            const QString &s = m_result.rawStopReason;
            if (s == QLatin1String("end_turn") || s == QLatin1String("stop_sequence") || s.isEmpty())
                m_result.stopReason = StopReason::Complete;
            else if (s == QLatin1String("max_tokens"))
                m_result.stopReason = StopReason::MaxTokens;
            else if (s == QLatin1String("refusal"))
                m_result.stopReason = StopReason::Refusal;
            else
                m_result.stopReason = StopReason::Other;
            if (m_result.usage.inputTokens >= 0 && m_result.usage.outputTokens >= 0)
                m_result.usage.totalTokens = m_result.usage.inputTokens + m_result.usage.outputTokens;
            complete(m_result);
        } else if (type == QLatin1String("error")) {
            // Errors can arrive mid-stream after an HTTP 200.
            const QJsonObject err = p.value(QLatin1String("error")).toObject();
            ProviderError e;
            e.providerType = err.value(QLatin1String("type")).toString();
            e.message = err.value(QLatin1String("message")).toString();
            if (e.providerType == QLatin1String("overloaded_error"))
                e.kind = ProviderError::Overloaded;
            else if (e.providerType == QLatin1String("rate_limit_error"))
                e.kind = ProviderError::RateLimited;
            else if (e.providerType == QLatin1String("authentication_error"))
                e.kind = ProviderError::Authentication;
            else if (e.providerType == QLatin1String("permission_error")
                     || e.providerType == QLatin1String("billing_error"))
                e.kind = ProviderError::Permission;
            else if (e.providerType == QLatin1String("invalid_request_error"))
                e.kind = ProviderError::InvalidRequest;
            else
                e.kind = ProviderError::Server;
            fail(e);
        }
        // ping, content_block_start, content_block_stop: nothing to do.
    }

private:
    void readUsage(const QJsonObject &u)
    {
        // Anthropic reports uncached input separately from cache reads and
        // writes; the total prompt size is their sum.
        if (u.contains(QLatin1String("input_tokens"))) {
            qint64 in = u.value(QLatin1String("input_tokens")).toInteger();
            in += u.value(QLatin1String("cache_creation_input_tokens")).toInteger(0);
            in += u.value(QLatin1String("cache_read_input_tokens")).toInteger(0);
            m_result.usage.inputTokens = in;
        }
        if (u.contains(QLatin1String("output_tokens")))
            m_result.usage.outputTokens = u.value(QLatin1String("output_tokens")).toInteger();
    }

    ChatResult m_result;
};

} // namespace

AnthropicProvider::AnthropicProvider(QNetworkAccessManager *nam, QObject *parent, const QUrl &baseUrl)
    : ChatProvider(nam, parent), m_base(baseUrl)
{
}

QList<ModelInfo> AnthropicProvider::builtinModels() const
{
    // Current model IDs from Anthropic's model overview (checked 2026-11-10).
    // The live list from GET /v1/models replaces this when a key is present.
    return {
        {QStringLiteral("claude-opus-5-5"), QStringLiteral("Claude Opus 5.5")},
        {QStringLiteral("claude-sonnet-5-5"), QStringLiteral("Claude Sonnet 5.5")},
        {QStringLiteral("claude-haiku-5-5"), QStringLiteral("Claude Haiku 5.5")},
        {QStringLiteral("claude-fable-5-2"), QStringLiteral("Claude Fable 5.2")},
    };
}

bool AnthropicProvider::supportsFallback(const QString &model)
{
    static const QStringList ids = {
        QStringLiteral("claude-fable-5-2"), QStringLiteral("claude-fable-5-1"),
        QStringLiteral("claude-opus-5-5"), QStringLiteral("claude-opus-5"),
        QStringLiteral("claude-sonnet-5-5"),
    };
    return ids.contains(model);
}

QNetworkRequest AnthropicProvider::makeRequest(const QString &path, const QString &apiKey) const
{
    QUrl url = m_base;
    url.setPath(path);
    QNetworkRequest req(url);
    HttpChatStream::applyTransportPolicy(req);
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    req.setRawHeader("x-api-key", apiKey.toUtf8());
    req.setRawHeader("anthropic-version", ApiVersion);
    return req;
}

QJsonObject AnthropicProvider::buildRequestBody(const ChatRequest &request) const
{
    QJsonArray messages;
    for (const ChatMessage &m : request.messages) {
        messages.append(QJsonObject{
            {QStringLiteral("role"), m.role == Role::User ? QStringLiteral("user") : QStringLiteral("assistant")},
            {QStringLiteral("content"), m.text},
        });
    }
    QJsonObject body{
        {QStringLiteral("model"), request.model.isEmpty() ? defaultModel() : request.model},
        {QStringLiteral("max_tokens"), request.maxOutputTokens},
        {QStringLiteral("stream"), true},
        {QStringLiteral("messages"), messages},
    };
    if (!request.systemPrompt.isEmpty())
        body.insert(QStringLiteral("system"), request.systemPrompt);
    // No sampling parameters: API version 2026-10-21 rejects them.
    if (m_options.anthropicFallback && supportsFallback(body.value(QLatin1String("model")).toString()))
        body.insert(QStringLiteral("fallbacks"), QStringLiteral("default"));
    return body;
}

ChatStream *AnthropicProvider::startChat(const ChatRequest &request, const QString &apiKey,
                                         QObject *parent)
{
    QNetworkRequest req = makeRequest(QStringLiteral("/v1/messages"), apiKey);
    req.setRawHeader("accept", "text/event-stream");
    const QJsonObject body = buildRequestBody(request);
    if (body.contains(QLatin1String("fallbacks")))
        req.setRawHeader("anthropic-beta", FallbackBeta);
    return new AnthropicStream(nam(), req, QJsonDocument(body).toJson(QJsonDocument::Compact),
                               m_options.idleTimeoutMs, parent);
}

ModelListReply *AnthropicProvider::listModels(const QString &apiKey, QObject *parent)
{
    auto *out = new ModelListReply(parent);
    QNetworkRequest req = makeRequest(QStringLiteral("/v1/models"), apiKey);
    QUrl url = req.url();
    url.setQuery(QStringLiteral("limit=100"));
    req.setUrl(url);
    req.setTransferTimeout(30000);
    QNetworkReply *reply = nam()->get(req);
    reply->setParent(out);
    connect(reply, &QNetworkReply::finished, out, [reply, out] {
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray data = reply->readAll();
        if (status == 0) {
            emit out->failed(HttpChatStream::errorFromNetwork(reply));
        } else if (status != 200) {
            emit out->failed(HttpChatStream::errorFromHttp(status, data, reply));
        } else {
            QList<ModelInfo> models;
            const QJsonArray arr = QJsonDocument::fromJson(data).object().value(QLatin1String("data")).toArray();
            for (const QJsonValue &v : arr) {
                const QJsonObject o = v.toObject();
                const QString id = o.value(QLatin1String("id")).toString();
                if (!id.isEmpty())
                    models.append({id, o.value(QLatin1String("display_name")).toString(id)});
            }
            emit out->finished(models);
        }
        out->deleteLater();
    });
    return out;
}

} // namespace zion::ai
