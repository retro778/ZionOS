// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QMainWindow>

class QCheckBox;
class QComboBox;
class QLabel;
class QNetworkAccessManager;
class QTabWidget;

namespace zion {

namespace ai {
class CredentialStore;
}

/// ZionOS Settings: the ZionOS-specific options (language, keyboard layouts,
/// assistant, about) plus shortcuts into KDE System Settings for everything
/// that Plasma already manages well.
class SettingsWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit SettingsWindow(QWidget *parent = nullptr);
    /// Selects a page by id: language, keyboard, system, software, assistant, about.
    void showPage(const QString &id);

private:
    QWidget *languagePage();
    QWidget *keyboardPage();
    QWidget *systemPage();
    QWidget *softwarePage();
    QWidget *assistantPage();
    void applyKeyboard();
    static QString shortcutLabel(const QString &key);

    QTabWidget *m_tabs = nullptr;
    QStringList m_pageIds;
    QCheckBox *m_layoutUs = nullptr;
    QCheckBox *m_layoutIl = nullptr;
    QComboBox *m_switch = nullptr;
    QLabel *m_keyboardStatus = nullptr;
    QNetworkAccessManager *m_nam = nullptr;
    ai::CredentialStore *m_store = nullptr;
};

} // namespace zion
