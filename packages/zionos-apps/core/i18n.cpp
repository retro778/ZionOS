// SPDX-License-Identifier: GPL-3.0-or-later
#include "i18n.h"

#include "appsettings.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QLibraryInfo>
#include <QTranslator>

namespace zion::I18n {

QStringList supportedLanguages()
{
    return {QStringLiteral("en"), QStringLiteral("he")};
}

static QString normalise(const QString &name)
{
    const QString lang = name.section(QLatin1Char('_'), 0, 0).section(QLatin1Char('-'), 0, 0).toLower();
    if (lang == QLatin1String("iw")) // legacy ISO code for Hebrew
        return QStringLiteral("he");
    return lang;
}

QString effectiveLanguage()
{
    const QString chosen = normalise(AppSettings().language());
    if (supportedLanguages().contains(chosen))
        return chosen;
    const QStringList ui = QLocale::system().uiLanguages();
    for (const QString &l : ui) {
        const QString n = normalise(l);
        if (supportedLanguages().contains(n))
            return n;
    }
    return QStringLiteral("en");
}

bool isRightToLeft(const QString &language)
{
    return QLocale(normalise(language)).textDirection() == Qt::RightToLeft;
}

bool install(QCoreApplication *app, const QString &language, const QString &translationsDir)
{
    const QString lang = normalise(language);
    bool ok = true;
    if (lang != QLatin1String("en")) {
        QString dir = translationsDir;
        if (dir.isEmpty())
            dir = qEnvironmentVariable("ZIONOS_TRANSLATIONS_DIR", QStringLiteral(ZIONOS_TRANSLATIONS_DIR));
        auto *zion = new QTranslator(app);
        ok = zion->load(QStringLiteral("zionos_") + lang, dir);
        if (ok)
            app->installTranslator(zion);
        else
            delete zion;
        // Qt's own strings (dialog buttons, context menus).
        auto *qt = new QTranslator(app);
        if (qt->load(QStringLiteral("qtbase_") + lang,
                     QLibraryInfo::path(QLibraryInfo::TranslationsPath)))
            app->installTranslator(qt);
        else
            delete qt;
    }
    QLocale::setDefault(QLocale(lang == QLatin1String("he") ? QStringLiteral("he_IL")
                                                           : QLocale::system().name()));
    if (auto *gui = qobject_cast<QGuiApplication *>(app))
        gui->setLayoutDirection(isRightToLeft(lang) ? Qt::RightToLeft : Qt::LeftToRight);
    return ok;
}

Qt::LayoutDirection baseDirection(const QString &text, Qt::LayoutDirection fallback)
{
    const auto ucs = text.toUcs4();
    for (const char32_t c : ucs) {
        switch (QChar::direction(c)) {
        case QChar::DirL:
            return Qt::LeftToRight;
        case QChar::DirR:
        case QChar::DirAL:
            return Qt::RightToLeft;
        default:
            break;
        }
    }
    return fallback;
}

QString ltrIsolate(const QString &text)
{
    return QChar(0x2066) + text + QChar(0x2069);
}

QString stripBidiControls(const QString &text)
{
    QString out;
    out.reserve(text.size());
    for (const QChar c : text) {
        const ushort u = c.unicode();
        const bool control = (u >= 0x202A && u <= 0x202E) || (u >= 0x2066 && u <= 0x2069)
                             || u == 0x200E || u == 0x200F || u == 0x061C;
        if (!control)
            out.append(c);
    }
    return out;
}

} // namespace zion::I18n
