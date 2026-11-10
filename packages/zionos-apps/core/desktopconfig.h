// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QString>
#include <QStringList>

namespace zion {

/// Reads and writes the small set of KDE Plasma per-user settings that the
/// ZionOS tools manage: display language and keyboard layouts. The files are
/// ordinary KConfig INI files; Plasma's own System Settings edits the same keys.
class DesktopConfig
{
public:
    /// @p configDir defaults to ~/.config (overridable for tests).
    explicit DesktopConfig(const QString &configDir = {});

    struct SwitchShortcut
    {
        QString xkbOption;  ///< e.g. "grp:alt_shift_toggle"
        QString labelKey;   ///< stable id used to look up the translated label
    };
    static QList<SwitchShortcut> switchShortcuts();

    // Keyboard (kxkbrc).
    QStringList keyboardLayouts() const;           ///< e.g. {"us","il"}
    QString switchOption() const;                  ///< "" if none
    bool setKeyboard(const QStringList &layouts, const QString &xkbSwitchOption) const;

    // Display language (plasma-localerc). @p language is "en" or "he".
    QString plasmaLanguage() const;
    bool setPlasmaLanguage(const QString &language) const;

    /// Asks the running Plasma session to re-read the keyboard configuration.
    static void notifyKeyboardChanged();

    static QString readIniValue(const QString &file, const QString &group, const QString &key);
    static bool writeIniValues(const QString &file, const QString &group,
                               const QList<QPair<QString, QString>> &values);

private:
    QString m_dir;
};

} // namespace zion
