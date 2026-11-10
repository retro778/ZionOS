// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktopconfig.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>

namespace zion {

DesktopConfig::DesktopConfig(const QString &configDir)
    : m_dir(configDir.isEmpty()
                ? QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
                : configDir)
{
}

QList<DesktopConfig::SwitchShortcut> DesktopConfig::switchShortcuts()
{
    return {
        {QStringLiteral("grp:alt_shift_toggle"), QStringLiteral("alt_shift")},
        {QStringLiteral("grp:win_space_toggle"), QStringLiteral("win_space")},
        {QStringLiteral("grp:ctrl_shift_toggle"), QStringLiteral("ctrl_shift")},
        {QStringLiteral("grp:caps_toggle"), QStringLiteral("caps")},
        {QStringLiteral("grp:alt_space_toggle"), QStringLiteral("alt_space")},
    };
}

// KConfig files are INI-like but not QSettings-compatible in all details
// (group names such as [$Version], list separators), so they are edited
// line by line, preserving everything that is not touched.
QString DesktopConfig::readIniValue(const QString &file, const QString &group, const QString &key)
{
    QFile f(file);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    const QStringList lines = QString::fromUtf8(f.readAll()).split(QLatin1Char('\n'));
    bool in = false;
    for (const QString &raw : lines) {
        const QString line = raw.trimmed();
        if (line.startsWith(QLatin1Char('[')) && line.endsWith(QLatin1Char(']'))) {
            in = line.mid(1, line.size() - 2) == group;
        } else if (in) {
            const qsizetype eq = line.indexOf(QLatin1Char('='));
            if (eq > 0 && line.left(eq).trimmed() == key)
                return line.mid(eq + 1).trimmed();
        }
    }
    return {};
}

bool DesktopConfig::writeIniValues(const QString &file, const QString &group,
                                   const QList<QPair<QString, QString>> &values)
{
    QStringList lines;
    {
        QFile f(file);
        if (f.open(QIODevice::ReadOnly | QIODevice::Text))
            lines = QString::fromUtf8(f.readAll()).split(QLatin1Char('\n'));
    }
    while (!lines.isEmpty() && lines.last().trimmed().isEmpty())
        lines.removeLast();

    qsizetype start = -1, end = lines.size();
    for (qsizetype i = 0; i < lines.size(); ++i) {
        const QString t = lines.at(i).trimmed();
        if (t.startsWith(QLatin1Char('[')) && t.endsWith(QLatin1Char(']'))) {
            if (start >= 0) {
                end = i;
                break;
            }
            if (t.mid(1, t.size() - 2) == group)
                start = i;
        }
    }
    if (start < 0) {
        if (!lines.isEmpty())
            lines.append(QString());
        lines.append(QLatin1Char('[') + group + QLatin1Char(']'));
        start = lines.size() - 1;
        end = lines.size();
    }
    for (const auto &kv : values) {
        bool replaced = false;
        for (qsizetype i = start + 1; i < end; ++i) {
            const qsizetype eq = lines.at(i).indexOf(QLatin1Char('='));
            if (eq > 0 && lines.at(i).left(eq).trimmed() == kv.first) {
                lines[i] = kv.first + QLatin1Char('=') + kv.second;
                replaced = true;
                break;
            }
        }
        if (!replaced) {
            qsizetype at = end;
            while (at > start + 1 && lines.at(at - 1).trimmed().isEmpty())
                --at;
            lines.insert(at, kv.first + QLatin1Char('=') + kv.second);
            ++end;
        }
    }
    QDir().mkpath(QFileInfo(file).absolutePath());
    QSaveFile out(file);
    if (!out.open(QIODevice::WriteOnly))
        return false;
    out.write(lines.join(QLatin1Char('\n')).toUtf8());
    out.write("\n");
    return out.commit();
}

QStringList DesktopConfig::keyboardLayouts() const
{
    const QString v = readIniValue(m_dir + QStringLiteral("/kxkbrc"), QStringLiteral("Layout"),
                                   QStringLiteral("LayoutList"));
    return v.split(QLatin1Char(','), Qt::SkipEmptyParts);
}

QString DesktopConfig::switchOption() const
{
    const QString v = readIniValue(m_dir + QStringLiteral("/kxkbrc"), QStringLiteral("Layout"),
                                   QStringLiteral("Options"));
    const QStringList opts = v.split(QLatin1Char(','), Qt::SkipEmptyParts);
    for (const QString &o : opts)
        if (o.startsWith(QLatin1String("grp:")))
            return o;
    return {};
}

bool DesktopConfig::setKeyboard(const QStringList &layouts, const QString &xkbSwitchOption) const
{
    const QString file = m_dir + QStringLiteral("/kxkbrc");
    // Keep non-switching options (compose key etc.) the user may have set.
    QStringList opts = readIniValue(file, QStringLiteral("Layout"), QStringLiteral("Options"))
                           .split(QLatin1Char(','), Qt::SkipEmptyParts);
    opts.erase(std::remove_if(opts.begin(), opts.end(),
                              [](const QString &o) { return o.startsWith(QLatin1String("grp:")); }),
               opts.end());
    if (!xkbSwitchOption.isEmpty())
        opts.append(xkbSwitchOption);
    QStringList variants;
    for (qsizetype i = 0; i < layouts.size(); ++i)
        variants.append(QString());
    return writeIniValues(file, QStringLiteral("Layout"),
                          {{QStringLiteral("LayoutList"), layouts.join(QLatin1Char(','))},
                           {QStringLiteral("VariantList"), variants.join(QLatin1Char(','))},
                           {QStringLiteral("Options"), opts.join(QLatin1Char(','))},
                           {QStringLiteral("ResetOldOptions"), QStringLiteral("true")},
                           {QStringLiteral("Use"), QStringLiteral("true")}});
}

QString DesktopConfig::plasmaLanguage() const
{
    const QString v = readIniValue(m_dir + QStringLiteral("/plasma-localerc"),
                                   QStringLiteral("Translations"), QStringLiteral("LANGUAGE"));
    const QString first = v.section(QLatin1Char(':'), 0, 0);
    if (first.startsWith(QLatin1String("he")))
        return QStringLiteral("he");
    if (first.startsWith(QLatin1String("en")))
        return QStringLiteral("en");
    return {};
}

bool DesktopConfig::setPlasmaLanguage(const QString &language) const
{
    const QString file = m_dir + QStringLiteral("/plasma-localerc");
    const bool he = language == QLatin1String("he");
    const bool a = writeIniValues(file, QStringLiteral("Translations"),
                                  {{QStringLiteral("LANGUAGE"), he ? QStringLiteral("he:en_US") : QStringLiteral("en_US")}});
    const bool b = writeIniValues(file, QStringLiteral("Formats"),
                                  {{QStringLiteral("LANG"), he ? QStringLiteral("he_IL.UTF-8") : QStringLiteral("en_US.UTF-8")}});
    return a && b;
}

void DesktopConfig::notifyKeyboardChanged()
{
    QDBusConnection::sessionBus().send(
        QDBusMessage::createSignal(QStringLiteral("/Layouts"), QStringLiteral("org.kde.keyboard"),
                                   QStringLiteral("reloadConfig")));
}

} // namespace zion
