// SPDX-License-Identifier: GPL-3.0-or-later
// Provider adapters against a scripted local HTTP server (no network, no keys).
#include "core/ai/anthropicprovider.h"
#include "core/ai/openaiprovider.h"
#include "core/redact.h"
#include "mockserver.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QSignalSpy>
#include <QtTest>

using namespace zion::ai;

static const QString kFakeKey = QStringLiteral("sk-ant-test-0123456789abcdefFAKE");
static const QString kFakeOpenAiKey = QStringLiteral("sk-proj-test-0123456789abcdefFAKE");

struct Outcome
{
    QString text;
    bool finished = false, failed = false;
    ChatResult result;
    ProviderError error;
    int terminalSignals = 0;
};

class TstProviders : public QObject
{
    Q_OBJECT

    QNetworkAccessManager nam;

    static ChatRequest sampleRequest()
    {
        ChatRequest r;
        r.model = QStringLiteral("claude-opus-5-5");
        r.systemPrompt = QStringLiteral("be brief");
        r.maxOutputTokens = 1000;
        r.messages = {{Role::User, QStringLiteral("שלום, מה זה sudo?")},
                      {Role::Assistant, QStringLiteral("פקודה")},
                      {Role::User, QStringLiteral("thanks")}};
        return r;
    }

    Outcome run(ChatProvider &p, const ChatRequest &req, const QString &key, int waitMs = 5000,
                std::function<void(ChatStream *)> during = {})
    {
        Outcome o;
        ChatStream *s = p.startChat(req, key, this);
        connect(s, &ChatStream::textDelta, this, [&o](const QString &t) { o.text += t; });
        connect(s, &ChatStream::finished, this, [&o](const ChatResult &r) { o.finished = true; o.result = r; ++o.terminalSignals; });
        connect(s, &ChatStream::failed, this, [&o](const ProviderError &e) { o.failed = true; o.error = e; ++o.terminalSignals; });
        if (during)
            during(s);
        [&] { QTRY_VERIFY_WITH_TIMEOUT(o.finished || o.failed, waitMs); }();
        QTest::qWait(150); // catch any stray second terminal signal
        delete s;
        return o;
    }

    static QList<QByteArray> anthropicHappy()
    {
        return {
            MockServer::sse("message_start", R"j({"type":"message_start","message":{"id":"msg_1","type":"message","role":"assistant","model":"claude-opus-5-5","content":[],"usage":{"input_tokens":25,"output_tokens":1}}})j"),
            MockServer::sse("content_block_start", R"j({"type":"content_block_start","index":0,"content_block":{"type":"thinking","thinking":""}})j"),
            MockServer::sse("content_block_delta", R"j({"type":"content_block_delta","index":0,"delta":{"type":"thinking_delta","thinking":"SECRET REASONING"}})j"),
            MockServer::sse("content_block_stop", R"j({"type":"content_block_stop","index":0})j"),
            MockServer::sse("ping", R"j({"type":"ping"})j"),
            MockServer::sse("content_block_start", R"j({"type":"content_block_start","index":1,"content_block":{"type":"text","text":""}})j"),
            MockServer::sse("content_block_delta", R"j({"type":"content_block_delta","index":1,"delta":{"type":"text_delta","text":"שלום"}})j"),
            MockServer::sse("content_block_delta", R"j({"type":"content_block_delta","index":1,"delta":{"type":"text_delta","text":" world"}})j"),
            MockServer::sse("content_block_stop", R"j({"type":"content_block_stop","index":1})j"),
            MockServer::sse("message_delta", R"j({"type":"message_delta","delta":{"stop_reason":"end_turn"},"usage":{"output_tokens":12}})j"),
            MockServer::sse("message_stop", R"j({"type":"message_stop"})j"),
        };
    }

private slots:
    void initTestCase()
    {
        qRegisterMetaType<ProviderError>();
        qRegisterMetaType<ChatResult>();
    }

    // ---------------- Anthropic ----------------
    void anthropicStreamsTextAndUsage()
    {
        MockServer srv;
        srv.response.chunks = anthropicHappy();
        srv.response.chunkDelayMs = 2;
        AnthropicProvider p(&nam, nullptr, srv.url());
        const Outcome o = run(p, sampleRequest(), kFakeKey);
        QVERIFY(o.finished);
        QCOMPARE(o.terminalSignals, 1);
        QCOMPARE(o.text, QStringLiteral("שלום world"));
        QVERIFY(!o.text.contains(QStringLiteral("SECRET")));
        QCOMPARE(o.result.usage.inputTokens, 25);
        QCOMPARE(o.result.usage.outputTokens, 12);
        QCOMPARE(o.result.usage.totalTokens, 37);
        QCOMPARE(o.result.stopReason, StopReason::Complete);
        QCOMPARE(o.result.servedModel, QStringLiteral("claude-opus-5-5"));
    }
    void anthropicRequestShape()
    {
        MockServer srv;
        srv.response.chunks = anthropicHappy();
        AnthropicProvider p(&nam, nullptr, srv.url());
        run(p, sampleRequest(), kFakeKey);
        QCOMPARE(srv.requests.size(), 1);
        const MockRequest &r = srv.requests.first();
        QCOMPARE(r.method, QByteArray("POST"));
        QCOMPARE(r.path, QByteArray("/v1/messages"));
        QCOMPARE(r.headers.value("x-api-key"), kFakeKey.toUtf8());
        QCOMPARE(r.headers.value("anthropic-version"), QByteArray("2026-10-21"));
        QCOMPARE(r.headers.value("anthropic-beta"), QByteArray("server-side-fallback-2026-07-01"));
        QVERIFY(r.headers.value("content-type").startsWith("application/json"));
        QVERIFY(!r.headers.contains("authorization"));
        QVERIFY(!r.headers.contains("cookie"));
        const QJsonObject b = QJsonDocument::fromJson(r.body).object();
        QCOMPARE(b.value("model").toString(), QStringLiteral("claude-opus-5-5"));
        QCOMPARE(b.value("max_tokens").toInt(), 1000);
        QCOMPARE(b.value("stream").toBool(), true);
        QCOMPARE(b.value("system").toString(), QStringLiteral("be brief"));
        QCOMPARE(b.value("fallbacks").toString(), QStringLiteral("default"));
        // Rejected by API version 2026-10-21:
        for (const char *k : {"temperature", "top_p", "top_k"})
            QVERIFY2(!b.contains(QLatin1String(k)), k);
        const QJsonArray m = b.value("messages").toArray();
        QCOMPARE(m.size(), 3);
        QCOMPARE(m[0].toObject().value("role").toString(), QStringLiteral("user"));
        QCOMPARE(m[0].toObject().value("content").toString(), QStringLiteral("שלום, מה זה sudo?"));
        QCOMPARE(m[1].toObject().value("role").toString(), QStringLiteral("assistant"));
        // The body shown to the user for review is exactly what is sent.
        QCOMPARE(b, p.buildRequestBody(sampleRequest()));
        QVERIFY(!r.body.contains(kFakeKey.toUtf8()));
    }
    void anthropicFallbackOnlyWhereSupportedAndEnabled()
    {
        QNetworkAccessManager n;
        AnthropicProvider p(&n);
        ChatRequest r = sampleRequest();
        r.model = QStringLiteral("claude-haiku-5-5");
        QVERIFY(!p.buildRequestBody(r).contains("fallbacks"));
        r.model = QStringLiteral("claude-opus-5-5");
        ProviderOptions o;
        o.anthropicFallback = false;
        p.setOptions(o);
        QVERIFY(!p.buildRequestBody(r).contains("fallbacks"));
        MockServer srv;
        srv.response.chunks = anthropicHappy();
        AnthropicProvider p2(&nam, nullptr, srv.url());
        p2.setOptions(o);
        run(p2, r, kFakeKey);
        QVERIFY(!srv.requests.first().headers.contains("anthropic-beta"));
    }
    void anthropicRefusalAndMaxTokens()
    {
        for (const auto &[raw, expect] : {std::pair{"refusal", StopReason::Refusal},
                                          std::pair{"max_tokens", StopReason::MaxTokens}}) {
            MockServer srv;
            srv.response.chunks = {
                MockServer::sse("message_start", R"j({"type":"message_start","message":{"model":"m","usage":{"input_tokens":3}}})j"),
                MockServer::sse("message_delta", QJsonDocument(QJsonObject{
                    {"type", "message_delta"}, {"delta", QJsonObject{{"stop_reason", raw}}},
                    {"usage", QJsonObject{{"output_tokens", 0}}}}).toJson(QJsonDocument::Compact)),
                MockServer::sse("message_stop", R"j({"type":"message_stop"})j"),
            };
            AnthropicProvider p(&nam, nullptr, srv.url());
            const Outcome o = run(p, sampleRequest(), kFakeKey);
            QVERIFY(o.finished);
            QCOMPARE(o.result.stopReason, expect);
        }
    }
    void anthropicHttpErrors_data()
    {
        QTest::addColumn<int>("status");
        QTest::addColumn<QByteArray>("type");
        QTest::addColumn<int>("kind");
        QTest::newRow("401") << 401 << QByteArray("authentication_error") << int(ProviderError::Authentication);
        QTest::newRow("402") << 402 << QByteArray("billing_error") << int(ProviderError::Permission);
        QTest::newRow("403") << 403 << QByteArray("permission_error") << int(ProviderError::Permission);
        QTest::newRow("400") << 400 << QByteArray("invalid_request_error") << int(ProviderError::InvalidRequest);
        QTest::newRow("404") << 404 << QByteArray("not_found_error") << int(ProviderError::InvalidRequest);
        QTest::newRow("413") << 413 << QByteArray("request_too_large") << int(ProviderError::InvalidRequest);
        QTest::newRow("429") << 429 << QByteArray("rate_limit_error") << int(ProviderError::RateLimited);
        QTest::newRow("500") << 500 << QByteArray("api_error") << int(ProviderError::Server);
        QTest::newRow("529") << 529 << QByteArray("overloaded_error") << int(ProviderError::Overloaded);
    }
    void anthropicHttpErrors()
    {
        QFETCH(int, status);
        QFETCH(QByteArray, type);
        QFETCH(int, kind);
        MockServer srv;
        srv.response.status = status;
        srv.response.headers = {{"content-type", "application/json"}, {"retry-after", "17"}, {"request-id", "req_hdr"}};
        srv.response.chunks = {QJsonDocument(QJsonObject{
            {"type", "error"},
            {"error", QJsonObject{{"type", QString::fromLatin1(type)}, {"message", "problem with key " + kFakeKey}}},
            {"request_id", "req_body"}}).toJson(QJsonDocument::Compact)};
        AnthropicProvider p(&nam, nullptr, srv.url());
        const Outcome o = run(p, sampleRequest(), kFakeKey);
        QVERIFY(o.failed);
        QCOMPARE(o.terminalSignals, 1);
        QCOMPARE(int(o.error.kind), kind);
        QCOMPARE(o.error.httpStatus, status);
        QCOMPARE(o.error.providerType, QString::fromLatin1(type));
        QCOMPARE(o.error.retryAfterSeconds, 17);
        QCOMPARE(o.error.requestId, QStringLiteral("req_body"));
        QVERIFY(o.text.isEmpty());
        // The key must never surface in an error message.
        QVERIFY2(!o.error.message.contains(kFakeKey), qPrintable(o.error.message));
        QVERIFY(o.error.message.contains(zion::Redactor::placeholder()));
        // No automatic retry, even for retryable statuses.
        QCOMPARE(srv.requests.size(), 1);
    }
    void anthropicMidStreamError()
    {
        MockServer srv;
        srv.response.chunks = {
            MockServer::sse("content_block_delta", R"j({"type":"content_block_delta","index":0,"delta":{"type":"text_delta","text":"partial"}})j"),
            MockServer::sse("error", R"j({"type":"error","error":{"type":"overloaded_error","message":"Overloaded"}})j"),
        };
        AnthropicProvider p(&nam, nullptr, srv.url());
        const Outcome o = run(p, sampleRequest(), kFakeKey);
        QVERIFY(o.failed);
        QCOMPARE(o.error.kind, ProviderError::Overloaded);
        QCOMPARE(o.text, QStringLiteral("partial"));
        QCOMPARE(srv.requests.size(), 1);
    }
    void truncatedStreamIsAnError()
    {
        MockServer srv;
        srv.response.chunks = {
            MockServer::sse("content_block_delta", R"j({"type":"content_block_delta","index":0,"delta":{"type":"text_delta","text":"half"}})j"),
        };
        AnthropicProvider p(&nam, nullptr, srv.url());
        const Outcome o = run(p, sampleRequest(), kFakeKey);
        QVERIFY(o.failed);
        QCOMPARE(o.error.kind, ProviderError::Protocol);
        QCOMPARE(srv.requests.size(), 1);
    }
    void cancelStopsStream()
    {
        MockServer srv;
        srv.response.chunks = {
            MockServer::sse("content_block_delta", R"j({"type":"content_block_delta","index":0,"delta":{"type":"text_delta","text":"a"}})j"),
        };
        srv.response.hang = true;
        AnthropicProvider p(&nam, nullptr, srv.url());
        const Outcome o = run(p, sampleRequest(), kFakeKey, 5000, [this](ChatStream *s) {
            QTimer::singleShot(300, s, [s] { s->cancel(); });
        });
        QVERIFY(o.failed);
        QCOMPARE(o.error.kind, ProviderError::Cancelled);
        QCOMPARE(o.terminalSignals, 1);
        QCOMPARE(o.text, QStringLiteral("a"));
        QCOMPARE(srv.requests.size(), 1);
    }
    void idleTimeout()
    {
        MockServer srv;
        srv.response.hang = true; // headers only, then silence
        AnthropicProvider p(&nam, nullptr, srv.url());
        ProviderOptions o;
        o.idleTimeoutMs = 400;
        p.setOptions(o);
        QElapsedTimer t;
        t.start();
        const Outcome out = run(p, sampleRequest(), kFakeKey);
        QVERIFY(out.failed);
        QCOMPARE(out.error.kind, ProviderError::Timeout);
        QVERIFY(t.elapsed() < 3000);
        QCOMPARE(srv.requests.size(), 1);
    }
    void connectionRefusedIsNetworkError()
    {
        quint16 port;
        {
            QTcpServer tmp;
            tmp.listen(QHostAddress::LocalHost, 0);
            port = tmp.serverPort();
        }
        AnthropicProvider p(&nam, nullptr, QUrl(QStringLiteral("http://127.0.0.1:%1").arg(port)));
        const Outcome o = run(p, sampleRequest(), kFakeKey);
        QVERIFY(o.failed);
        QCOMPARE(o.error.kind, ProviderError::Network);
        QVERIFY(o.error.isTransient());
    }
    void redirectIsNotFollowed()
    {
        MockServer other;
        other.response.chunks = anthropicHappy();
        MockServer srv;
        srv.response.status = 307;
        srv.response.headers = {{"location", other.url(QStringLiteral("/v1/messages")).toString().toUtf8()}};
        AnthropicProvider p(&nam, nullptr, srv.url());
        const Outcome o = run(p, sampleRequest(), kFakeKey);
        QVERIFY(o.failed);
        QCOMPARE(o.error.kind, ProviderError::Protocol);
        // The credential was not forwarded to the redirect target.
        QCOMPARE(other.requests.size(), 0);
    }
    void tlsCertificateErrorsAreFatal()
    {
        // A plain-HTTP server answering on an https:// URL fails the TLS
        // handshake; the request must fail rather than downgrade.
        MockServer srv;
        srv.response.chunks = anthropicHappy();
        QUrl u = srv.url();
        u.setScheme(QStringLiteral("https"));
        AnthropicProvider p(&nam, nullptr, u);
        const Outcome o = run(p, sampleRequest(), kFakeKey, 10000);
        QVERIFY(o.failed);
        QCOMPARE(o.error.kind, ProviderError::Network);
        QVERIFY(o.text.isEmpty());
    }
    void anthropicModelList()
    {
        MockServer srv;
        srv.response.headers = {{"content-type", "application/json"}};
        srv.response.chunks = {R"j({"data":[{"type":"model","id":"claude-opus-5-5","display_name":"Claude Opus 5.5"},{"type":"model","id":"claude-haiku-5-5","display_name":"Claude Haiku 5.5"}],"has_more":false})j"};
        AnthropicProvider p(&nam, nullptr, srv.url());
        ModelListReply *r = p.listModels(kFakeKey, this);
        QSignalSpy spy(r, &ModelListReply::finished);
        QVERIFY(spy.wait(5000));
        const auto models = spy.first().first().value<QList<ModelInfo>>();
        QCOMPARE(models.size(), 2);
        QCOMPARE(models[0].displayName, QStringLiteral("Claude Opus 5.5"));
        QCOMPARE(srv.requests.first().method, QByteArray("GET"));
        QVERIFY(srv.requests.first().path.startsWith("/v1/models"));
        QCOMPARE(srv.requests.first().headers.value("x-api-key"), kFakeKey.toUtf8());
    }

    // ---------------- OpenAI ----------------
    void openAiStreamsTextAndUsage()
    {
        MockServer srv;
        srv.response.chunks = {
            MockServer::sse("response.created", R"j({"type":"response.created","sequence_number":0,"response":{"id":"resp_1","status":"in_progress"}})j"),
            MockServer::sse("response.reasoning_summary_text.delta", R"j({"type":"response.reasoning_summary_text.delta","delta":"HIDDEN","sequence_number":1})j"),
            MockServer::sse("response.output_text.delta", R"j({"type":"response.output_text.delta","item_id":"msg_1","output_index":0,"content_index":0,"delta":"Hello ","sequence_number":2,"logprobs":[]})j"),
            MockServer::sse("response.output_text.delta", R"j({"type":"response.output_text.delta","item_id":"msg_1","output_index":0,"content_index":0,"delta":"עולם","sequence_number":3,"logprobs":[]})j"),
            MockServer::sse("response.output_text.done", R"j({"type":"response.output_text.done","text":"Hello עולם","sequence_number":4})j"),
            MockServer::sse("response.completed", R"j({"type":"response.completed","sequence_number":5,"response":{"id":"resp_1","status":"completed","model":"gpt-6-astra","usage":{"input_tokens":36,"input_tokens_details":{"cached_tokens":0,"cache_write_tokens":0},"output_tokens":87,"output_tokens_details":{"reasoning_tokens":0},"total_tokens":123}}})j"),
        };
        srv.response.chunkDelayMs = 2;
        OpenAiProvider p(&nam, nullptr, srv.url(QStringLiteral("/v1")));
        ChatRequest req = sampleRequest();
        req.model = QStringLiteral("gpt-6-astra");
        const Outcome o = run(p, req, kFakeOpenAiKey);
        QVERIFY(o.finished);
        QCOMPARE(o.text, QStringLiteral("Hello עולם"));
        QCOMPARE(o.result.usage.inputTokens, 36);
        QCOMPARE(o.result.usage.outputTokens, 87);
        QCOMPARE(o.result.usage.totalTokens, 123);
        QCOMPARE(o.result.servedModel, QStringLiteral("gpt-6-astra"));

        const MockRequest &r = srv.requests.first();
        QCOMPARE(r.path, QByteArray("/v1/responses"));
        QCOMPARE(r.headers.value("authorization"), QByteArray("Bearer ") + kFakeOpenAiKey.toUtf8());
        QVERIFY(!r.headers.contains("x-api-key"));
        const QJsonObject b = QJsonDocument::fromJson(r.body).object();
        QCOMPARE(b.value("model").toString(), QStringLiteral("gpt-6-astra"));
        QCOMPARE(b.value("stream").toBool(), true);
        QVERIFY(b.contains("store"));
        QCOMPARE(b.value("store").toBool(), false); // server default would be true
        QCOMPARE(b.value("instructions").toString(), QStringLiteral("be brief"));
        QCOMPARE(b.value("max_output_tokens").toInt(), 1000);
        const QJsonArray in = b.value("input").toArray();
        QCOMPARE(in.size(), 3);
        QCOMPARE(in[1].toObject().value("role").toString(), QStringLiteral("assistant"));
        QCOMPARE(in[2].toObject().value("content").toString(), QStringLiteral("thanks"));
        QVERIFY(!r.body.contains(kFakeOpenAiKey.toUtf8()));
    }
    void openAiIncompleteAndFailed()
    {
        {
            MockServer srv;
            srv.response.chunks = {MockServer::sse("response.incomplete", R"j({"type":"response.incomplete","response":{"status":"incomplete","incomplete_details":{"reason":"max_output_tokens"},"usage":null},"sequence_number":1})j")};
            OpenAiProvider p(&nam, nullptr, srv.url(QStringLiteral("/v1")));
            const Outcome o = run(p, sampleRequest(), kFakeOpenAiKey);
            QVERIFY(o.finished);
            QCOMPARE(o.result.stopReason, StopReason::MaxTokens);
            QVERIFY(!o.result.usage.isKnown()); // provider sent none: nothing invented
        }
        {
            MockServer srv;
            srv.response.chunks = {MockServer::sse("response.failed", R"j({"type":"response.failed","response":{"status":"failed","error":{"code":"server_error","message":"The model failed to generate a response."}},"sequence_number":1})j")};
            OpenAiProvider p(&nam, nullptr, srv.url(QStringLiteral("/v1")));
            const Outcome o = run(p, sampleRequest(), kFakeOpenAiKey);
            QVERIFY(o.failed);
            QCOMPARE(o.error.kind, ProviderError::Server);
            QCOMPARE(srv.requests.size(), 1);
        }
        {
            MockServer srv;
            srv.response.chunks = {MockServer::sse("error", R"j({"type":"error","code":"rate_limit_exceeded","message":"slow down","param":null,"sequence_number":1})j")};
            OpenAiProvider p(&nam, nullptr, srv.url(QStringLiteral("/v1")));
            const Outcome o = run(p, sampleRequest(), kFakeOpenAiKey);
            QVERIFY(o.failed);
            QCOMPARE(o.error.kind, ProviderError::RateLimited);
        }
    }
    void openAiHttpErrors()
    {
        struct Case { int status; const char *type; const char *code; ProviderError::Kind kind; };
        for (const Case &c : {Case{401, "invalid_request_error", "invalid_api_key", ProviderError::Authentication},
                              Case{429, "requests", "rate_limit_exceeded", ProviderError::RateLimited},
                              Case{429, "insufficient_quota", "insufficient_quota", ProviderError::Permission},
                              Case{503, "server_error", "", ProviderError::Overloaded},
                              Case{500, "server_error", "", ProviderError::Server}}) {
            MockServer srv;
            srv.response.status = c.status;
            srv.response.headers = {{"content-type", "application/json"}, {"x-request-id", "req_oai"}};
            srv.response.chunks = {QJsonDocument(QJsonObject{{"error", QJsonObject{
                {"message", "Incorrect API key provided: " + kFakeOpenAiKey}, {"type", c.type},
                {"param", QJsonValue::Null}, {"code", c.code}}}}).toJson(QJsonDocument::Compact)};
            OpenAiProvider p(&nam, nullptr, srv.url(QStringLiteral("/v1")));
            const Outcome o = run(p, sampleRequest(), kFakeOpenAiKey);
            QVERIFY(o.failed);
            QCOMPARE(o.error.kind, c.kind);
            QCOMPARE(o.error.requestId, QStringLiteral("req_oai"));
            QVERIFY(!o.error.message.contains(kFakeOpenAiKey));
            QCOMPARE(srv.requests.size(), 1);
        }
    }
    void openAiModelListFiltersNonChatModels()
    {
        MockServer srv;
        srv.response.headers = {{"content-type", "application/json"}};
        srv.response.chunks = {R"j({"object":"list","data":[{"id":"gpt-6-astra","object":"model"},{"id":"gpt-image-2.6-flare","object":"model"},{"id":"gpt-realtime-2","object":"model"},{"id":"text-embedding-3-large","object":"model"},{"id":"gpt-6-luna","object":"model"},{"id":"gpt-transcribe","object":"model"},{"id":"omni-moderation-latest","object":"model"}]})j"};
        OpenAiProvider p(&nam, nullptr, srv.url(QStringLiteral("/v1")));
        ModelListReply *r = p.listModels(kFakeOpenAiKey, this);
        QSignalSpy spy(r, &ModelListReply::finished);
        QVERIFY(spy.wait(5000));
        QStringList ids;
        for (const ModelInfo &m : spy.first().first().value<QList<ModelInfo>>())
            ids << m.id;
        ids.sort();
        QCOMPARE(ids, (QStringList{QStringLiteral("gpt-6-astra"), QStringLiteral("gpt-6-luna")}));
        QCOMPARE(srv.requests.first().path, QByteArray("/v1/models"));
    }
    void modelListAuthError()
    {
        MockServer srv;
        srv.response.status = 401;
        srv.response.headers = {{"content-type", "application/json"}};
        srv.response.chunks = {R"j({"error":{"message":"bad key","type":"invalid_request_error","code":"invalid_api_key","param":null}})j"};
        OpenAiProvider p(&nam, nullptr, srv.url(QStringLiteral("/v1")));
        ModelListReply *r = p.listModels(kFakeOpenAiKey, this);
        QSignalSpy spy(r, &ModelListReply::failed);
        QVERIFY(spy.wait(5000));
        QCOMPARE(spy.first().first().value<ProviderError>().kind, ProviderError::Authentication);
    }
    void builtinDefaults()
    {
        QNetworkAccessManager n;
        AnthropicProvider a(&n);
        OpenAiProvider o(&n);
        QCOMPARE(a.endpointHost(), QStringLiteral("api.anthropic.com"));
        QCOMPARE(o.endpointHost(), QStringLiteral("api.openai.com"));
        QCOMPARE(a.baseUrl().scheme(), QStringLiteral("https"));
        QCOMPARE(o.baseUrl().scheme(), QStringLiteral("https"));
        auto has = [](const QList<ModelInfo> &l, const QString &id) {
            return std::any_of(l.begin(), l.end(), [&](const ModelInfo &m) { return m.id == id; });
        };
        QVERIFY(has(a.builtinModels(), a.defaultModel()));
        QVERIFY(has(o.builtinModels(), o.defaultModel()));
    }
};

QTEST_GUILESS_MAIN(TstProviders)
#include "tst_providers.moc"
