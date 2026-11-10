// SPDX-License-Identifier: GPL-3.0-or-later
// Conversation logic of the assistant against a scripted local server.
#include "core/ai/anthropicprovider.h"
#include "core/ai/chatcontroller.h"
#include "core/ai/openaiprovider.h"
#include "mockserver.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

using namespace zion::ai;

static const QString kKey = QStringLiteral("sk-ant-test-AAAABBBBCCCCDDDD0000");

static QList<QByteArray> reply(const QByteArray &text)
{
    const QByteArray delta = QJsonDocument(QJsonObject{
        {"type", "content_block_delta"}, {"index", 0},
        {"delta", QJsonObject{{"type", "text_delta"}, {"text", QString::fromUtf8(text)}}}}).toJson(QJsonDocument::Compact);
    return {
        MockServer::sse("message_start", "{\"type\":\"message_start\",\"message\":{\"model\":\"claude-opus-5-5\",\"usage\":{\"input_tokens\":7}}}"),
        MockServer::sse("content_block_delta", delta),
        MockServer::sse("message_delta", "{\"type\":\"message_delta\",\"delta\":{\"stop_reason\":\"end_turn\"},\"usage\":{\"output_tokens\":3}}"),
        MockServer::sse("message_stop", "{\"type\":\"message_stop\"}"),
    };
}

class TstChat : public QObject
{
    Q_OBJECT
    QNetworkAccessManager nam;

    struct Fixture
    {
        QTemporaryDir tmp;
        MockServer server;
        MemoryCredentialStore store;
        HistoryStore history{tmp.path() + QStringLiteral("/conv")};
        ChatController chat{&store, &history};
        explicit Fixture(QNetworkAccessManager *nam)
        {
            chat.addProvider(new AnthropicProvider(nam, nullptr, server.url()));
            chat.addProvider(new OpenAiProvider(nam, nullptr, server.url(QStringLiteral("/v1"))));
            chat.setSystemPrompt(QStringLiteral("sys"));
        }
    };

private slots:
    void initTestCase()
    {
        qRegisterMetaType<ProviderError>();
        qRegisterMetaType<ChatResult>();
    }

    void nothingIsSentWithoutAKey()
    {
        Fixture f(&nam);
        QSignalSpy need(&f.chat, &ChatController::keyNeeded);
        QSignalSpy started(&f.chat, &ChatController::assistantStarted);
        f.chat.send(QStringLiteral("hello"));
        QTRY_COMPARE(need.count(), 1);
        QCOMPARE(need.first().at(0).toString(), QStringLiteral("anthropic"));
        QCOMPARE(need.first().at(1).toBool(), false);
        QTest::qWait(200);
        QCOMPARE(f.server.requests.size(), 0);
        QCOMPARE(started.count(), 0);
        QVERIFY(!f.chat.busy());
        QVERIFY(f.chat.conversation().messages.isEmpty());
        QVERIFY(f.history.list().isEmpty());
    }
    void lockedKeyringIsReportedAndNothingIsSent()
    {
        Fixture f(&nam);
        f.store.persistent.insert(QStringLiteral("anthropic"), kKey);
        f.store.simulateUnavailable = true;
        QSignalSpy need(&f.chat, &ChatController::keyNeeded);
        f.chat.send(QStringLiteral("hello"));
        QTRY_COMPARE(need.count(), 1);
        QCOMPARE(need.first().at(1).toBool(), true);
        QCOMPARE(f.server.requests.size(), 0);
        // Explicit opt-in to a session key makes it work without the keyring.
        f.store.writeSession(QStringLiteral("anthropic"), kKey);
        f.server.response.chunks = reply("ok");
        QSignalSpy done(&f.chat, &ChatController::assistantFinished);
        f.chat.send(QStringLiteral("hello"));
        QTRY_COMPARE(done.count(), 1);
        QCOMPARE(f.server.requests.size(), 1);
        QVERIFY(f.store.persistent.value(QStringLiteral("anthropic")) == kKey); // untouched
    }
    void sendsExactlyWhatWasPreviewedAndSavesHistory()
    {
        Fixture f(&nam);
        f.store.persistent.insert(QStringLiteral("anthropic"), kKey);
        f.server.response.chunks = reply("תשובה");
        Attachment a;
        a.label = QStringLiteral("error.log");
        a.content = QStringLiteral("E: Unable to locate package foo");
        const QString text = QStringLiteral("מה הבעיה כאן?");
        const ChatRequest preview = f.chat.previewRequest(text, {a});
        const QJsonObject expected = f.chat.currentProvider()->buildRequestBody(preview);

        QSignalSpy done(&f.chat, &ChatController::assistantFinished);
        QSignalSpy busy(&f.chat, &ChatController::busyChanged);
        f.chat.send(text, {a});
        QVERIFY(f.chat.busy());
        QTRY_COMPARE(done.count(), 1);
        QVERIFY(!f.chat.busy());
        QCOMPARE(busy.count(), 2);
        QCOMPARE(done.first().at(0).toString(), QStringLiteral("תשובה"));
        QCOMPARE(done.first().at(1).value<ChatResult>().usage.outputTokens, 3);

        QCOMPARE(f.server.requests.size(), 1);
        const QJsonObject sent = QJsonDocument::fromJson(f.server.requests.first().body).object();
        QCOMPARE(sent, expected);
        const QString user = sent.value("messages").toArray().first().toObject().value("content").toString();
        QVERIFY(user.startsWith(text));
        QVERIFY(user.contains(QStringLiteral("<attached-content name=\"error.log\">")));
        QVERIFY(user.contains(a.content));
        QCOMPARE(sent.value("system").toString(), QStringLiteral("sys"));

        QCOMPARE(f.chat.conversation().messages.size(), 2);
        const auto saved = f.history.list();
        QCOMPARE(saved.size(), 1);
        QCOMPARE(saved.first().title, text);
        QCOMPARE(saved.first().messages.size(), 2);
        QCOMPARE(saved.first().providerId, QStringLiteral("anthropic"));
        // The key is never written to the history.
        QFile file(f.history.directory() + QLatin1Char('/') + saved.first().id + QStringLiteral(".json"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        QVERIFY(!file.readAll().contains(kKey.toUtf8()));

        // Second turn carries the first exchange; the attachment is not re-attached.
        f.server.response.chunks = reply("second");
        f.chat.send(QStringLiteral("thanks"));
        QTRY_COMPARE(done.count(), 2);
        const QJsonArray m2 = QJsonDocument::fromJson(f.server.requests.last().body).object().value("messages").toArray();
        QCOMPARE(m2.size(), 3);
        QCOMPARE(m2[1].toObject().value("role").toString(), QStringLiteral("assistant"));
        QCOMPARE(m2[2].toObject().value("content").toString(), QStringLiteral("thanks"));
        QCOMPARE(f.server.requests.size(), 2);
        QCOMPARE(f.history.list().size(), 1);
    }
    void disabledHistoryWritesNothing()
    {
        Fixture f(&nam);
        f.store.persistent.insert(QStringLiteral("anthropic"), kKey);
        f.chat.setHistoryEnabled(false);
        f.server.response.chunks = reply("ok");
        QSignalSpy done(&f.chat, &ChatController::assistantFinished);
        f.chat.send(QStringLiteral("private question"));
        QTRY_COMPARE(done.count(), 1);
        QVERIFY(!QDir(f.history.directory()).exists() || f.history.list().isEmpty());
        QCOMPARE(f.chat.conversation().messages.size(), 2); // still usable in memory
    }
    void failureIsNotRetriedAndLeavesAConsistentConversation()
    {
        Fixture f(&nam);
        f.store.persistent.insert(QStringLiteral("anthropic"), kKey);
        f.server.response.status = 529;
        f.server.response.headers = {{"content-type", "application/json"}};
        f.server.response.chunks = {"{\"type\":\"error\",\"error\":{\"type\":\"overloaded_error\",\"message\":\"Overloaded\"}}"};
        QSignalSpy failed(&f.chat, &ChatController::assistantFailed);
        f.chat.send(QStringLiteral("q1"));
        QTRY_COMPARE(failed.count(), 1);
        QCOMPARE(failed.first().at(1).value<ProviderError>().kind, ProviderError::Overloaded);
        QTest::qWait(700);
        QCOMPARE(f.server.requests.size(), 1); // no silent retry
        QVERIFY(f.chat.conversation().messages.isEmpty());
        QVERIFY(!f.chat.busy());
        QVERIFY(f.history.list().isEmpty());

        // A later, deliberate send works and has well-formed alternation.
        f.server.response = MockResponse();
        f.server.response.chunks = reply("fine");
        QSignalSpy done(&f.chat, &ChatController::assistantFinished);
        f.chat.send(QStringLiteral("q1 again"));
        QTRY_COMPARE(done.count(), 1);
        const QJsonArray m = QJsonDocument::fromJson(f.server.requests.last().body).object().value("messages").toArray();
        QCOMPARE(m.size(), 1);
        QCOMPARE(f.server.requests.size(), 2);
    }
    void stopKeepsPartialReply()
    {
        Fixture f(&nam);
        f.store.persistent.insert(QStringLiteral("anthropic"), kKey);
        f.server.response.chunks = {reply("partial ").at(1)};
        f.server.response.hang = true;
        QSignalSpy delta(&f.chat, &ChatController::assistantDelta);
        QSignalSpy failed(&f.chat, &ChatController::assistantFailed);
        f.chat.send(QStringLiteral("long question"));
        QTRY_COMPARE(delta.count(), 1);
        f.chat.stop();
        QTRY_COMPARE(failed.count(), 1);
        QCOMPARE(failed.first().at(1).value<ProviderError>().kind, ProviderError::Cancelled);
        QCOMPARE(failed.first().at(0).toString(), QStringLiteral("partial "));
        QVERIFY(!f.chat.busy());
        QCOMPARE(f.chat.conversation().messages.size(), 2);
        QCOMPARE(f.server.requests.size(), 1);
    }
    void sendWhileBusyIsIgnored()
    {
        Fixture f(&nam);
        f.store.persistent.insert(QStringLiteral("anthropic"), kKey);
        f.server.response.chunks = reply("one");
        f.server.response.chunkDelayMs = 50;
        QSignalSpy done(&f.chat, &ChatController::assistantFinished);
        f.chat.send(QStringLiteral("a"));
        f.chat.send(QStringLiteral("b"));
        f.chat.send(QString());
        QTRY_COMPARE(done.count(), 1);
        QTest::qWait(300);
        QCOMPARE(f.server.requests.size(), 1);
    }
    void providerSwitchUsesTheOtherEndpointAndKey()
    {
        Fixture f(&nam);
        f.store.persistent.insert(QStringLiteral("openai"), QStringLiteral("sk-proj-test-ZZZZYYYYXXXX0000"));
        f.chat.setProviderId(QStringLiteral("openai"));
        QCOMPARE(f.chat.model(), QStringLiteral("gpt-6-astra"));
        f.chat.setModel(QStringLiteral("gpt-6-luna"));
        f.server.response.chunks = {
            MockServer::sse("response.output_text.delta", "{\"type\":\"response.output_text.delta\",\"delta\":\"hi\"}"),
            MockServer::sse("response.completed", "{\"type\":\"response.completed\",\"response\":{\"model\":\"gpt-6-luna\",\"usage\":{\"input_tokens\":1,\"output_tokens\":1,\"total_tokens\":2}}}"),
        };
        QSignalSpy done(&f.chat, &ChatController::assistantFinished);
        f.chat.send(QStringLiteral("hello"));
        QTRY_COMPARE(done.count(), 1);
        const MockRequest &r = f.server.requests.first();
        QCOMPARE(r.path, QByteArray("/v1/responses"));
        QCOMPARE(r.headers.value("authorization"), QByteArray("Bearer sk-proj-test-ZZZZYYYYXXXX0000"));
        QCOMPARE(QJsonDocument::fromJson(r.body).object().value("model").toString(), QStringLiteral("gpt-6-luna"));
        QCOMPARE(QJsonDocument::fromJson(r.body).object().value("store").toBool(true), false);
    }
    void deleteControls()
    {
        Fixture f(&nam);
        f.store.persistent.insert(QStringLiteral("anthropic"), kKey);
        QSignalSpy done(&f.chat, &ChatController::assistantFinished);
        for (int i = 0; i < 3; ++i) {
            f.server.response.chunks = reply("r");
            f.chat.newConversation();
            f.chat.send(QStringLiteral("question %1").arg(i));
            QTRY_COMPARE(done.count(), i + 1);
        }
        QCOMPARE(f.history.list().size(), 3);
        const QString current = f.chat.conversation().id;
        f.chat.deleteConversation(current);
        QCOMPARE(f.history.list().size(), 2);
        QVERIFY(f.chat.conversation().id != current);
        QVERIFY(f.chat.conversation().messages.isEmpty());
        const QString other = f.history.list().first().id;
        QVERIFY(f.chat.openConversation(other));
        QCOMPARE(f.chat.conversation().messages.size(), 2);
        QCOMPARE(f.chat.deleteAllConversations(), 2);
        QVERIFY(f.history.list().isEmpty());
        QVERIFY(f.chat.conversation().messages.isEmpty());
        QVERIFY(!f.chat.openConversation(other));
    }
};

QTEST_GUILESS_MAIN(TstChat)
#include "tst_chat.moc"
