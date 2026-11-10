// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/appsettings.h"
#include "core/i18n.h"

#include <QGuiApplication>
#include <QTextLayout>
#include <QXmlStreamReader>
#include <QtTest>

using namespace zion;

class TstI18n : public QObject
{
    Q_OBJECT
private slots:
    void baseDirection()
    {
        QCOMPARE(I18n::baseDirection(QStringLiteral("hello שלום")), Qt::LeftToRight);
        QCOMPARE(I18n::baseDirection(QStringLiteral("שלום hello")), Qt::RightToLeft);
        QCOMPARE(I18n::baseDirection(QStringLiteral("123 … שלום")), Qt::RightToLeft); // digits are weak
        QCOMPARE(I18n::baseDirection(QStringLiteral("«مرحبا»")), Qt::RightToLeft);
        QCOMPARE(I18n::baseDirection(QStringLiteral("123 !?"), Qt::RightToLeft), Qt::RightToLeft);
        QCOMPARE(I18n::baseDirection(QString()), Qt::LeftToRight);
    }
    void isolatesKeepLtrRunsIntact()
    {
        const QString cmd = QStringLiteral("sudo apt install vlc");
        const QString iso = I18n::ltrIsolate(cmd);
        QCOMPARE(iso.front(), QChar(0x2066));
        QCOMPARE(iso.back(), QChar(0x2069));
        QCOMPARE(I18n::stripBidiControls(iso), cmd);
        // In an RTL paragraph, the isolated command must be laid out as one
        // visually contiguous left-to-right run.
        const QString sentence = QStringLiteral("הריצו את ") + iso + QStringLiteral(" בטרמינל");
        QTextLayout layout(sentence);
        QTextOption opt;
        opt.setTextDirection(Qt::RightToLeft);
        layout.setTextOption(opt);
        layout.beginLayout();
        QTextLine line = layout.createLine();
        line.setLineWidth(100000);
        layout.endLayout();
        const int start = int(sentence.indexOf(cmd));
        qreal prev = -1;
        QStringList dbg;
        // The cursor position exactly at the run boundary is ambiguous (it
        // belongs to both runs), so the first index is skipped.
        for (int i = start + 1; i < start + int(cmd.size()); ++i) {
            const qreal x = line.cursorToX(i);
            dbg << QString::number(x);
            QVERIFY2(x > prev, qPrintable(QStringLiteral("command characters are not in left-to-right visual order: ") + dbg.join(QLatin1Char(' '))));
            prev = x;
        }
        // Hebrew before the command is to its right in an RTL line.
        QVERIFY(line.cursorToX(0) > line.cursorToX(start + 1));
    }
    void hebrewCatalogueIsComplete()
    {
        QFile f(QStringLiteral(ZION_TEST_TS_DIR "/zionos_he.ts"));
        QVERIFY2(f.open(QIODevice::ReadOnly), "zionos_he.ts missing");
        QXmlStreamReader xml(&f);
        int messages = 0;
        QStringList unfinished;
        QString source;
        while (!xml.atEnd()) {
            xml.readNext();
            if (!xml.isStartElement())
                continue;
            if (xml.name() == QLatin1String("source")) {
                source = xml.readElementText();
            } else if (xml.name() == QLatin1String("translation")) {
                ++messages;
                const QString type = xml.attributes().value(QLatin1String("type")).toString();
                const QString text = xml.readElementText(QXmlStreamReader::IncludeChildElements);
                if (type == QLatin1String("unfinished") || (type.isEmpty() && text.trimmed().isEmpty()))
                    unfinished << source;
            }
        }
        QVERIFY(!xml.hasError());
        QVERIFY2(messages > 50, "suspiciously few translatable strings");
        QVERIFY2(unfinished.isEmpty(), qPrintable(QStringLiteral("untranslated: ") + unfinished.join(QStringLiteral(" | "))));
    }
    void hebrewLoadsAndFlipsLayout()
    {
        QVERIFY(I18n::install(qApp, QStringLiteral("he"), QStringLiteral(ZION_TEST_QM_DIR)));
        QCOMPARE(qGuiApp->layoutDirection(), Qt::RightToLeft);
        const QString tr = QCoreApplication::translate("zion::WelcomeWindow", "Welcome to ZionOS");
        QVERIFY2(tr != QStringLiteral("Welcome to ZionOS"), "Hebrew translation not applied");
        QCOMPARE(I18n::baseDirection(tr), Qt::RightToLeft);
        QVERIFY(tr.contains(QStringLiteral("ZionOS")));
    }
    void languageSelection()
    {
        AppSettings s;
        s.setLanguage(QStringLiteral("he_IL"));
        s.sync();
        QCOMPARE(I18n::effectiveLanguage(), QStringLiteral("he"));
        s.setLanguage(QStringLiteral("iw"));
        s.sync();
        QCOMPARE(I18n::effectiveLanguage(), QStringLiteral("he"));
        s.setLanguage(QStringLiteral("en"));
        s.sync();
        QCOMPARE(I18n::effectiveLanguage(), QStringLiteral("en"));
        QVERIFY(I18n::isRightToLeft(QStringLiteral("he")));
        QVERIFY(!I18n::isRightToLeft(QStringLiteral("en")));
        QCOMPARE(I18n::supportedLanguages(), (QStringList{QStringLiteral("en"), QStringLiteral("he")}));
    }
};

QTEST_MAIN(TstI18n)
#include "tst_i18n.moc"
