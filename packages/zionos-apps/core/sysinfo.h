// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QMap>
#include <QString>

namespace zion {

/// Read-only facts about the running system, taken from os-release and uname.
class SystemInfo
{
public:
    /// Parses an os-release style file. Defaults to /etc/os-release.
    explicit SystemInfo(const QString &osReleasePath = QStringLiteral("/etc/os-release"));

    QString value(const QString &key, const QString &fallback = {}) const;
    QString prettyName() const;     ///< e.g. "ZionOS 0.1 (Aleph)"
    QString version() const;        ///< e.g. "0.1.0 (Aleph)"
    QString baseDistribution() const; ///< e.g. "Debian 13 (trixie)"
    QString kernel() const;         ///< e.g. "Linux 6.12.48+deb13-amd64"
    QString architecture() const;
    bool isLiveSession() const;

    static QMap<QString, QString> parseOsRelease(const QString &text);

private:
    QMap<QString, QString> m_values;
};

} // namespace zion
