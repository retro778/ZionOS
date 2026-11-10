// SPDX-License-Identifier: GPL-3.0-or-later
#include "sysinfo.h"

#include <QFile>
#include <QSysInfo>
#include <QTextStream>

namespace zion {

QMap<QString, QString> SystemInfo::parseOsRelease(const QString &text)
{
    QMap<QString, QString> out;
    const auto lines = text.split(QLatin1Char('\n'));
    for (const QString &raw : lines) {
        const QString line = raw.trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
            continue;
        const qsizetype eq = line.indexOf(QLatin1Char('='));
        if (eq <= 0)
            continue;
        QString v = line.mid(eq + 1).trimmed();
        if (v.size() >= 2 && ((v.startsWith(QLatin1Char('"')) && v.endsWith(QLatin1Char('"')))
                              || (v.startsWith(QLatin1Char('\'')) && v.endsWith(QLatin1Char('\'')))))
            v = v.mid(1, v.size() - 2);
        v.replace(QLatin1String("\\\""), QLatin1String("\""));
        v.replace(QLatin1String("\\$"), QLatin1String("$"));
        v.replace(QLatin1String("\\\\"), QLatin1String("\\"));
        out.insert(line.left(eq).trimmed(), v);
    }
    return out;
}

static QString readAll(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    return QString::fromUtf8(f.readAll());
}

SystemInfo::SystemInfo(const QString &osReleasePath)
    : m_values(parseOsRelease(readAll(osReleasePath)))
{
}

QString SystemInfo::value(const QString &key, const QString &fallback) const
{
    return m_values.value(key, fallback);
}

QString SystemInfo::prettyName() const
{
    return value(QStringLiteral("PRETTY_NAME"), QStringLiteral("ZionOS"));
}

QString SystemInfo::version() const
{
    return value(QStringLiteral("VERSION"), value(QStringLiteral("VERSION_ID")));
}

QString SystemInfo::baseDistribution() const
{
    // The original Debian file is kept by the zionos-base diversion.
    const auto debian = parseOsRelease(readAll(QStringLiteral("/usr/lib/os-release.debian")));
    if (debian.contains(QStringLiteral("PRETTY_NAME")))
        return debian.value(QStringLiteral("PRETTY_NAME"));
    const QString suite = value(QStringLiteral("DEBIAN_BASE_SUITE"));
    return suite.isEmpty() ? QStringLiteral("Debian GNU/Linux")
                           : QStringLiteral("Debian GNU/Linux (%1)").arg(suite);
}

QString SystemInfo::kernel() const
{
    return QSysInfo::kernelType() + QLatin1Char(' ') + QSysInfo::kernelVersion();
}

QString SystemInfo::architecture() const
{
    return QSysInfo::currentCpuArchitecture();
}

bool SystemInfo::isLiveSession() const
{
    return readAll(QStringLiteral("/proc/cmdline")).contains(QLatin1String("boot=live"));
}

} // namespace zion
