// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/ai/sseparser.h"

#include <QtTest>

using zion::ai::SseParser;

class TstSse : public QObject
{
    Q_OBJECT
private slots:
    void simpleEvent()
    {
        SseParser p;
        const auto ev = p.feed("event: a\ndata: {\"x\":1}\n\n");
        QCOMPARE(ev.size(), 1);
        QCOMPARE(ev[0].event, QByteArray("a"));
        QCOMPARE(ev[0].data, QByteArray("{\"x\":1}"));
    }
    void byteByByte()
    {
        const QByteArray stream = "event: one\r\ndata: 1\r\n\r\n: keep-alive\r\n\r\ndata: 2a\ndata: 2b\n\nevent: three\ndata:3\n\n";
        SseParser p;
        QList<zion::ai::SseEvent> all;
        for (char c : stream)
            all += p.feed(QByteArray(1, c));
        QCOMPARE(all.size(), 3);
        QCOMPARE(all[0].event, QByteArray("one"));
        QCOMPARE(all[0].data, QByteArray("1"));
        QCOMPARE(all[1].event, QByteArray());
        QCOMPARE(all[1].data, QByteArray("2a\n2b"));
        QCOMPARE(all[2].data, QByteArray("3"));
        QCOMPARE(p.pendingBytes(), 0);
    }
    void incompleteEventIsHeld()
    {
        SseParser p;
        QVERIFY(p.feed("data: partial\n").isEmpty());
        QVERIFY(p.feed("data: more").isEmpty());
        const auto ev = p.feed("\n\n");
        QCOMPARE(ev.size(), 1);
        QCOMPARE(ev[0].data, QByteArray("partial\nmore"));
    }
    void utf8SplitAcrossChunks()
    {
        const QByteArray payload = QStringLiteral("data: שלום עולם\n\n").toUtf8();
        SseParser p;
        QList<zion::ai::SseEvent> all;
        for (qsizetype i = 0; i < payload.size(); i += 3)
            all += p.feed(payload.mid(i, 3));
        QCOMPARE(all.size(), 1);
        QCOMPARE(QString::fromUtf8(all[0].data), QStringLiteral("שלום עולם"));
    }
    void bomAndUnknownFields()
    {
        SseParser p;
        const auto ev = p.feed("\xEF\xBB\xBFid: 7\nretry: 100\nfoo: bar\ndata: x\n\n");
        QCOMPARE(ev.size(), 1);
        QCOMPARE(ev[0].data, QByteArray("x"));
    }
    void overflowIsBounded()
    {
        SseParser p;
        const QByteArray big(1024 * 1024, 'a');
        for (int i = 0; i < 10 && !p.overflowed(); ++i)
            p.feed(big);
        QVERIFY(p.overflowed());
        QVERIFY(p.pendingBytes() < SseParser::MaxEventBytes);
    }
};

QTEST_GUILESS_MAIN(TstSse)
#include "tst_sse.moc"
