// SPDX-License-Identifier: GPL-3.0-or-later
#include "openaiprovider.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>

namespace zion::ai {

namespace {

class OpenAiStream : public HttpChatStream
{
public:
    using HttpChatStream::HttpChatStream;

protected:
    void handleEvent(const QByteArray &, const QJsonObject &p) override
    {
        const QString type = p.value(QLatin1String("type")).toString();
        if (type == QLatin1String("response.output_text.delta")) {
            const QString t = p.value(QLatin1String("delta")).toString();
            if (!t.isEmpty())
                emit textDelta(t);
        } else if (type == QLatin1String("response.refusal.delta")) {
            m_refused = true;
            const QString t = p.value(QLatin1String("delta")).toString();
            if (!t.isEmpty())
                emit textDelta(t);
        } else if (type == QLatin1String("response.completed")
                   || type == QLatin1String("response.incomplete")) {
            const QJsonObject r = p.value(QLatin1String("response")).toObject();
            ChatResult res;
            res.servedModel = r.value(QLatin1String("model")).toString();
            res.requestId = r.value(QLatin1String("id")).toString();
            const QJsonObject u = r.value(QLatin1String("usage")).toObject();
            if (u.contains(QLatin1String("input_tokens")))
                res.usage.inputTokens = u.value(QLatin1String("input_tokens")).toInteger();
            if (u.contains(QLatin1String("output_tokens")))
                res.usage.outputTokens = u.value(QLatin1String("output_tokens")).toInteger();
            if (u.contains(QLatin1String("total_tokens")))
                res.usage.totalTokens = u.value(QLatin1String("total_tokens")).toInteger();
            if (type == QLatin1String("response.incomplete")) {
                res.rawStopReason = r.value(QLatin1String("incomplete_details")).toObject()
                                        .value(QLatin1String("reason")).toString();
                res.stopReason = res.rawStopReason == QLatin1String("max_output_tokens")
                                     ? StopReason::MaxTokens
                                     : (res.rawStopReason == QLatin1String("content_filter")
                                            ? StopReason::Refusal : StopReason::Other);
            } else {
                res.rawStopReason = QStringLiteral("completed");
                res.stopReason = m_refused ? StopReason::Refusal : StopReason::Complete;
            }
            complete(res);
        } else if (type == QLatin1String("response.failed")) {
            const QJsonObject err = p.value(QLatin1String("response")).toObject()
                                        .value(QLatin1String("error")).toObject();
            fail(fromCode(err.value(QLatin1String("code")).toString(),
                          err.value(QLatin1String("message")).toString()));
        } else if (type == QLatin1String("error")) {
            fail(fromCode(p.value(QLatin1String("code")).toString(),
                          p.value(QLatin1String("message")).toString()));
        }
        // All other events (response.created, output_item.*, reasoning.*,
        // content_part.*, output_text.done, ...) carry nothing we display.
    }

private:
    static ProviderError fromCode(const QString &code, const QString &message)
    {
        ProviderError e;
        e.providerType = code;
        e.message = message;
        if (code == QLatin1String("rate_limit_exceeded"))
            e.kind = ProviderError::RateLimited;
        else if (code == QLatin1String("insufficient_quota"))
            e.kind = ProviderError::Permission;
        else if (code == QLatin1String("invalid_api_key"))
            e.kind = ProviderError::Authentication;
        else if (code == QLatin1String("invalid_prompt") || code.startsWith(QLatin1String("invalid_")))
            e.kind = ProviderError::InvalidRequest;
        else
            e.kind = ProviderError::Server;
        return e;
    }

    bool m_refused = false;
};

} // namespace

OpenAiProvider::OpenAiProvider(QNetworkAccessManager *nam, QObject *parent, const QUrl &baseUrl)
    : ChatProvider(nam, parent), m_base(baseUrl)
{
}

QList<ModelInfo> OpenAiProvider::builtinModels() const
{
    // Flagship model IDs from OpenAI's models page (checked 2026-11-10).
    // The live list from GET /v1/models replaces this when a key is present.
    return {
        {QStringLiteral("gpt-6-astra"), QStringLiteral("GPT-6 Astra")},
        {QStringLiteral("gpt-6.1-sol"), QStringLiteral("GPT-6.1 Sol")},
        {QStringLiteral("gpt-6.1-terra"), QStringLiteral("GPT-6.1 Terra")},
        {QStringLiteral("gpt-6-luna"), QStringLiteral("GPT-6 Luna")},
    };
}

bool OpenAiProvider::isChatModel(const QString &id)
{
    static const QStringList excluded = {
        QStringLiteral("image"), QStringLiteral("realtime"), QStringLiteral("audio"),
        QStringLiteral("transcribe"), QStringLiteral("tts"), QStringLiteral("whisper"),
        QStringLiteral("embedding"), QStringLiteral("moderation"), QStringLiteral("dall-e"),
        QStringLiteral("sora"), QStringLiteral("live"), QStringLiteral("search"),
        QStringLiteral("instruct"), QStringLiteral("codex"), QStringLiteral("computer-use"),
    };
    if (!(id.startsWith(QLatin1String("gpt-")) || id.startsWith(QLatin1String("o"))
          || id.startsWith(QLatin1String("chatgpt-"))))
        return false;
    if (id.startsWith(QLatin1String("o")) && !(id.size() > 1 && id.at(1).isDigit()))
        return false;
    for (const QString &x : excluded)
        if (id.contains(x))
            return false;
    return true;
}

QNetworkRequest OpenAiProvider::makeRequest(const QString &path, const QString &apiKey) const
{
    QUrl url = m_base;
    url.setPath(m_base.path() + path);
    QNetworkRequest req(url);
    HttpChatStream::applyTransportPolicy(req);
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    req.setRawHeader("Authorization", "Bearer " + apiKey.toUtf8());
    return req;
}

QJsonObject OpenAiProvider::buildRequestBody(const ChatRequest &request) const
{
    QJsonArray input;
    for (const ChatMessage &m : request.messages) {
        input.append(QJsonObject{
            {QStringLiteral("type"), QStringLiteral("message")},
            {QStringLiteral("role"), m.role == Role::User ? QStringLiteral("user") : QStringLiteral("assistant")},
            {QStringLiteral("content"), m.text},
        });
    }
    QJsonObject body{
        {QStringLiteral("model"), request.model.isEmpty() ? defaultModel() : request.model},
        {QStringLiteral("input"), input},
        {QStringLiteral("stream"), true},
        {QStringLiteral("max_output_tokens"), qMax(16, request.maxOutputTokens)},
        // `store` defaults to true on the server (responses kept >= 30 days).
        // ZionOS sends an explicit value and defaults it to false.
        {QStringLiteral("store"), m_options.openAiStore},
    };
    if (!request.systemPrompt.isEmpty())
        body.insert(QStringLiteral("instructions"), request.systemPrompt);
    return body;
}

ChatStream *OpenAiProvider::startChat(const ChatRequest &request, const QString &apiKey,
                                      QObject *parent)
{
    QNetworkRequest req = makeRequest(QStringLiteral("/responses"), apiKey);
    req.setRawHeader("accept", "text/event-stream");
    return new OpenAiStream(nam(), req,
                            QJsonDocument(buildRequestBody(request)).toJson(QJsonDocument::Compact),
                            m_options.idleTimeoutMs, parent);
}

ModelListReply *OpenAiProvider::listModels(const QString &apiKey, QObject *parent)
{
    auto *out = new ModelListReply(parent);
    QNetworkRequest req = makeRequest(QStringLiteral("/models"), apiKey);
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
                const QString id = v.toObject().value(QLatin1String("id")).toString();
                if (isChatModel(id))
                    models.append({id, id});
            }
            std::sort(models.begin(), models.end(),
                      [](const ModelInfo &a, const ModelInfo &b) { return a.id > b.id; });
            emit out->finished(models);
        }
        out->deleteLater();
    });
    return out;
}

} // namespace zion::ai
