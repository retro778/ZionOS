// SPDX-License-Identifier: GPL-3.0-or-later
// Optional test against the real provider APIs. Skipped unless
// ZIONOS_LIVE_API_TESTS=1 and the corresponding key variable is set.
// Each enabled provider is sent ONE short request (billable).
#include "core/ai/anthropicprovider.h"
#include "core/ai/openaiprovider.h"

#include <QNetworkAccessManager>
#include <QSignalSpy>
#include <QtTest>

using namespace zion::ai;

class TstLiveApi : public QObject
{
    Q_OBJECT
    void runOne(ChatProvider &p, const QString &key, const QString &model)
    {
        ChatRequest req;
        req.model = model;
        req.maxOutputTokens = 256;
        req.messages = {{Role::User, QStringLiteral("Reply with the single word: pong")}};
        QString text;
        ChatStream *s = p.startChat(req, key, this);
        connect(s, &ChatStream::textDelta, this, [&](const QString &t) { text += t; });
        QSignalSpy done(s, &ChatStream::finished);
        QSignalSpy failed(s, &ChatStream::failed);
        QTRY_VERIFY_WITH_TIMEOUT(done.count() + failed.count() == 1, 120000);
        if (failed.count()) {
            const auto e = failed.first().first().value<ProviderError>();
            QFAIL(qPrintable(QStringLiteral("%1 HTTP %2: %3").arg(kindName(e.kind)).arg(e.httpStatus).arg(e.message)));
        }
        QVERIFY(text.contains(QStringLiteral("pong"), Qt::CaseInsensitive));
        const auto r = done.first().first().value<ChatResult>();
        QVERIFY(r.usage.isKnown());
    }
private slots:
    void initTestCase()
    {
        qRegisterMetaType<ProviderError>();
        qRegisterMetaType<ChatResult>();
        if (qEnvironmentVariable("ZIONOS_LIVE_API_TESTS") != QLatin1String("1"))
            QSKIP("live API tests are disabled (set ZIONOS_LIVE_API_TESTS=1 to enable)");
    }
    void anthropic()
    {
        const QString key = qEnvironmentVariable("ANTHROPIC_API_KEY");
        if (key.isEmpty())
            QSKIP("ANTHROPIC_API_KEY not set");
        QNetworkAccessManager nam;
        AnthropicProvider p(&nam);
        runOne(p, key, qEnvironmentVariable("ZIONOS_ANTHROPIC_MODEL", p.defaultModel()));
    }
    void openai()
    {
        const QString key = qEnvironmentVariable("OPENAI_API_KEY");
        if (key.isEmpty())
            QSKIP("OPENAI_API_KEY not set");
        QNetworkAccessManager nam;
        OpenAiProvider p(&nam);
        runOne(p, key, qEnvironmentVariable("ZIONOS_OPENAI_MODEL", p.defaultModel()));
    }
};

QTEST_GUILESS_MAIN(TstLiveApi)
#include "tst_live_api.moc"
