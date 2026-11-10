// SPDX-License-Identifier: GPL-3.0-or-later
#include "settingswindow.h"

#include "core/ai/anthropicprovider.h"
#include "core/ai/credentialstore.h"
#include "core/ai/openaiprovider.h"
#include "core/appsettings.h"
#include "core/desktopconfig.h"
#include "core/i18n.h"
#include "ui/aboutwidget.h"
#include "ui/aisettingswidget.h"
#include "ui/apphelpers.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QIcon>
#include <QLabel>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QProcess>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QTabWidget>
#include <QVBoxLayout>

namespace zion {

static QWidget *scrollable(QWidget *inner)
{
    auto *area = new QScrollArea;
    area->setWidgetResizable(true);
    area->setFrameShape(QFrame::NoFrame);
    area->setWidget(inner);
    return area;
}

SettingsWindow::SettingsWindow(QWidget *parent) : QMainWindow(parent)
{
    setWindowTitle(tr("ZionOS Settings"));
    resize(820, 640);
    m_nam = new QNetworkAccessManager(this);
    m_store = new ai::CredentialStore(this);

    m_tabs = new QTabWidget;
    m_tabs->setDocumentMode(true);
    auto add = [this](const QString &id, QWidget *page, const QString &icon, const QString &title) {
        m_pageIds << id;
        m_tabs->addTab(scrollable(page), QIcon::fromTheme(icon), title);
    };
    add(QStringLiteral("language"), languagePage(), QStringLiteral("preferences-desktop-locale"), tr("Language"));
    add(QStringLiteral("keyboard"), keyboardPage(), QStringLiteral("input-keyboard"), tr("Keyboard"));
    add(QStringLiteral("system"), systemPage(), QStringLiteral("preferences-system"), tr("System"));
    add(QStringLiteral("software"), softwarePage(), QStringLiteral("system-software-install"), tr("Software"));
    add(QStringLiteral("assistant"), assistantPage(), QStringLiteral("zionos-logo"), tr("Assistant"));
    add(QStringLiteral("about"), new AboutWidget, QStringLiteral("help-about"), tr("About"));
    setCentralWidget(m_tabs);
}

void SettingsWindow::showPage(const QString &id)
{
    const qsizetype i = m_pageIds.indexOf(id);
    if (i >= 0)
        m_tabs->setCurrentIndex(int(i));
}

// ---------------------------------------------------------------- language

QWidget *SettingsWindow::languagePage()
{
    auto *page = new QWidget;
    auto *lay = new QVBoxLayout(page);
    auto *intro = new QLabel(tr("Choose the language of the desktop and of ZionOS applications."));
    intro->setWordWrap(true);
    lay->addWidget(intro);

    const QString current = I18n::effectiveLanguage();
    auto *en = new QRadioButton(QStringLiteral("English"));
    en->setObjectName(QStringLiteral("langEnglish"));
    auto *he = new QRadioButton(QStringLiteral("עברית (Hebrew)"));
    he->setObjectName(QStringLiteral("langHebrew"));
    (current == QLatin1String("he") ? he : en)->setChecked(true);
    lay->addWidget(en);
    lay->addWidget(he);

    auto *apply = new QPushButton(tr("Apply language"));
    apply->setObjectName(QStringLiteral("applyLanguage"));
    lay->addWidget(apply, 0, Qt::AlignLeading);
    connect(apply, &QPushButton::clicked, this, [this, he] {
        const QString lang = he->isChecked() ? QStringLiteral("he") : QStringLiteral("en");
        AppSettings s;
        s.setLanguage(lang);
        s.sync();
        if (!DesktopConfig().setPlasmaLanguage(lang)) {
            QMessageBox::warning(this, windowTitle(), tr("The desktop language setting could not be saved."));
            return;
        }
        QMessageBox::information(this, windowTitle(),
                                 tr("ZionOS applications use the new language the next time they start. "
                                    "The rest of the desktop follows after you log out and back in."));
        restartApplication();
    });

    auto *note = new QLabel(tr(
        "Hebrew is displayed right to left. Some programs that come from other projects "
        "are only partly translated to Hebrew; untranslated parts appear in English."));
    note->setWordWrap(true);
    lay->addWidget(note);

    auto *more = new QPushButton(QIcon::fromTheme(QStringLiteral("preferences-desktop-locale")),
                                 tr("Region and formats (System Settings)…"));
    connect(more, &QPushButton::clicked, this, [] { launch(QStringLiteral("systemsettings"), {QStringLiteral("kcm_regionandlang")}); });
    lay->addWidget(more, 0, Qt::AlignLeading);
    lay->addStretch();
    return page;
}

// ---------------------------------------------------------------- keyboard

QString SettingsWindow::shortcutLabel(const QString &key)
{
    if (key == QLatin1String("alt_shift")) return tr("Alt + Shift");
    if (key == QLatin1String("win_space")) return tr("Meta (Windows key) + Space");
    if (key == QLatin1String("ctrl_shift")) return tr("Ctrl + Shift");
    if (key == QLatin1String("caps")) return tr("Caps Lock");
    if (key == QLatin1String("alt_space")) return tr("Alt + Space");
    return key;
}

QWidget *SettingsWindow::keyboardPage()
{
    auto *page = new QWidget;
    auto *lay = new QVBoxLayout(page);
    const DesktopConfig cfg;
    QStringList layouts = cfg.keyboardLayouts();
    if (layouts.isEmpty()) // nothing in the user's file: system default applies
        layouts = DesktopConfig(QStringLiteral("/etc/xdg")).keyboardLayouts();
    QString sw = cfg.switchOption();
    if (sw.isEmpty())
        sw = DesktopConfig(QStringLiteral("/etc/xdg")).switchOption();

    auto *box = new QGroupBox(tr("Keyboard layouts"));
    auto *bl = new QVBoxLayout(box);
    m_layoutUs = new QCheckBox(tr("English (US)"));
    m_layoutUs->setObjectName(QStringLiteral("layoutUs"));
    m_layoutIl = new QCheckBox(tr("Hebrew"));
    m_layoutIl->setObjectName(QStringLiteral("layoutIl"));
    m_layoutUs->setChecked(layouts.isEmpty() || layouts.contains(QStringLiteral("us")));
    m_layoutIl->setChecked(layouts.contains(QStringLiteral("il")));
    bl->addWidget(m_layoutUs);
    bl->addWidget(m_layoutIl);
    lay->addWidget(box);

    auto *form = new QFormLayout;
    m_switch = new QComboBox;
    m_switch->setObjectName(QStringLiteral("switchShortcut"));
    for (const auto &s : DesktopConfig::switchShortcuts())
        m_switch->addItem(shortcutLabel(s.labelKey), s.xkbOption);
    m_switch->addItem(tr("No shortcut"), QString());
    m_switch->setCurrentIndex(qMax(0, m_switch->findData(sw)));
    form->addRow(tr("Switch layout with:"), m_switch);
    lay->addLayout(form);

    auto *apply = new QPushButton(tr("Apply"));
    apply->setObjectName(QStringLiteral("applyKeyboard"));
    connect(apply, &QPushButton::clicked, this, &SettingsWindow::applyKeyboard);
    lay->addWidget(apply, 0, Qt::AlignLeading);
    m_keyboardStatus = new QLabel;
    m_keyboardStatus->setObjectName(QStringLiteral("keyboardStatus"));
    m_keyboardStatus->setWordWrap(true);
    lay->addWidget(m_keyboardStatus);

    auto *more = new QPushButton(QIcon::fromTheme(QStringLiteral("input-keyboard")),
                                 tr("More layouts and options (System Settings)…"));
    connect(more, &QPushButton::clicked, this, [] { launch(QStringLiteral("systemsettings"), {QStringLiteral("kcm_keyboard")}); });
    lay->addWidget(more, 0, Qt::AlignLeading);
    lay->addStretch();
    return page;
}

void SettingsWindow::applyKeyboard()
{
    QStringList layouts;
    if (m_layoutUs->isChecked())
        layouts << QStringLiteral("us");
    if (m_layoutIl->isChecked())
        layouts << QStringLiteral("il");
    if (layouts.isEmpty()) {
        m_keyboardStatus->setText(tr("Select at least one layout."));
        return;
    }
    // Keep any other layouts the user added through System Settings.
    const QStringList existing = DesktopConfig().keyboardLayouts();
    for (const QString &l : existing)
        if (l != QLatin1String("us") && l != QLatin1String("il"))
            layouts << l;
    if (DesktopConfig().setKeyboard(layouts, m_switch->currentData().toString())) {
        DesktopConfig::notifyKeyboardChanged();
        m_keyboardStatus->setText(tr("Keyboard settings saved."));
    } else {
        m_keyboardStatus->setText(tr("The keyboard settings could not be saved."));
    }
}

// ------------------------------------------------------------------ system

QWidget *SettingsWindow::systemPage()
{
    auto *page = new QWidget;
    auto *lay = new QVBoxLayout(page);
    auto *intro = new QLabel(tr("These settings are provided by KDE Plasma. Each button opens the matching page of System Settings."));
    intro->setWordWrap(true);
    lay->addWidget(intro);
    auto *grid = new QGridLayout;
    struct Item { QString icon, title, module; };
    const QList<Item> items = {
        {QStringLiteral("preferences-system-network"), tr("Network and Wi-Fi"), QStringLiteral("kcm_networkmanagement")},
        {QStringLiteral("audio-volume-high"), tr("Sound"), QStringLiteral("kcm_pulseaudio")},
        {QStringLiteral("preferences-desktop-display"), tr("Displays"), QStringLiteral("kcm_kscreen")},
        {QStringLiteral("preferences-system-time"), tr("Date and time"), QStringLiteral("kcm_clock")},
        {QStringLiteral("preferences-desktop-user"), tr("Users"), QStringLiteral("kcm_users")},
        {QStringLiteral("preferences-desktop-theme-global"), tr("Appearance"), QStringLiteral("kcm_lookandfeel")},
        {QStringLiteral("preferences-system-power-management"), tr("Power"), QStringLiteral("kcm_powerdevilprofilesconfig")},
        {QStringLiteral("preferences-system-bluetooth"), tr("Bluetooth"), QStringLiteral("kcm_bluetooth")},
        {QStringLiteral("printer"), tr("Printers"), QStringLiteral("kcm_printer_manager")},
        {QStringLiteral("preferences-desktop-accessibility"), tr("Accessibility"), QStringLiteral("kcm_access")},
    };
    int n = 0;
    for (const Item &it : items) {
        auto *b = new QPushButton(QIcon::fromTheme(it.icon), it.title);
        b->setMinimumHeight(44);
        b->setIconSize(QSize(28, 28));
        b->setStyleSheet(QStringLiteral("text-align: start; padding: 6px 12px;"));
        const QString module = it.module;
        connect(b, &QPushButton::clicked, this, [this, module] {
            if (!launch(QStringLiteral("systemsettings"), {module}))
                QMessageBox::warning(this, windowTitle(), tr("System Settings could not be started."));
        });
        grid->addWidget(b, n / 2, n % 2);
        ++n;
    }
    lay->addLayout(grid);
    auto *all = new QPushButton(QIcon::fromTheme(QStringLiteral("preferences-system")), tr("Open System Settings"));
    connect(all, &QPushButton::clicked, this, [] { launch(QStringLiteral("systemsettings")); });
    lay->addWidget(all, 0, Qt::AlignLeading);
    lay->addStretch();
    return page;
}

// ---------------------------------------------------------------- software

QWidget *SettingsWindow::softwarePage()
{
    auto *page = new QWidget;
    auto *lay = new QVBoxLayout(page);
    auto *intro = new QLabel(tr(
        "ZionOS installs software from the Debian archive with APT. Security updates come "
        "directly from Debian. Use Discover to browse, install and update software."));
    intro->setWordWrap(true);
    lay->addWidget(intro);
    auto *discover = new QPushButton(QIcon::fromTheme(QStringLiteral("plasmadiscover")), tr("Open Discover"));
    connect(discover, &QPushButton::clicked, this, [] { launch(QStringLiteral("plasma-discover")); });
    lay->addWidget(discover, 0, Qt::AlignLeading);
    auto *updates = new QPushButton(QIcon::fromTheme(QStringLiteral("system-software-update")), tr("Check for updates"));
    connect(updates, &QPushButton::clicked, this, [] { launch(QStringLiteral("plasma-discover"), {QStringLiteral("--mode"), QStringLiteral("update")}); });
    lay->addWidget(updates, 0, Qt::AlignLeading);

    auto *box = new QGroupBox(tr("Flatpak applications (optional)"));
    auto *bl = new QVBoxLayout(box);
    auto *info = new QLabel(tr(
        "Flatpak can install additional applications that are not packaged by Debian. "
        "Flathub is a third-party repository: its applications are not reviewed by Debian "
        "or ZionOS. It is not enabled unless you choose to add it. It is added for your "
        "user account only."));
    info->setWordWrap(true);
    bl->addWidget(info);
    auto *status = new QLabel;
    status->setObjectName(QStringLiteral("flathubStatus"));
    status->setWordWrap(true);
    bl->addWidget(status);
    auto *enable = new QPushButton(tr("Add the Flathub repository"));
    enable->setObjectName(QStringLiteral("enableFlathub"));
    auto *disable = new QPushButton(tr("Remove the Flathub repository"));
    bl->addWidget(enable, 0, Qt::AlignLeading);
    bl->addWidget(disable, 0, Qt::AlignLeading);
    lay->addWidget(box);
    lay->addStretch();

    auto refresh = [status, enable, disable] {
        if (!programAvailable(QStringLiteral("flatpak"))) {
            status->setText(SettingsWindow::tr("Flatpak is not installed."));
            enable->setEnabled(false);
            disable->setEnabled(false);
            return;
        }
        QProcess p;
        p.start(QStringLiteral("flatpak"), {QStringLiteral("remotes"), QStringLiteral("--user"), QStringLiteral("--columns=name")});
        p.waitForFinished(5000);
        const bool has = QString::fromUtf8(p.readAllStandardOutput()).split(QLatin1Char('\n')).contains(QStringLiteral("flathub"));
        status->setText(has ? SettingsWindow::tr("Flathub is enabled for your account.")
                            : SettingsWindow::tr("Flathub is not enabled."));
        enable->setVisible(!has);
        disable->setVisible(has);
    };
    auto runFlatpak = [this, refresh](const QStringList &args) {
        auto *p = new QProcess(this);
        connect(p, &QProcess::finished, this, [this, p, refresh](int code) {
            if (code != 0)
                QMessageBox::warning(this, windowTitle(),
                                     tr("The Flatpak command failed:\n%1").arg(QString::fromUtf8(p->readAllStandardError()).left(500)));
            p->deleteLater();
            refresh();
        });
        p->start(QStringLiteral("flatpak"), args);
    };
    connect(enable, &QPushButton::clicked, this, [runFlatpak] {
        runFlatpak({QStringLiteral("remote-add"), QStringLiteral("--user"), QStringLiteral("--if-not-exists"),
                    QStringLiteral("flathub"), QStringLiteral("https://dl.flathub.org/repo/flathub.flatpakrepo")});
    });
    connect(disable, &QPushButton::clicked, this, [runFlatpak] {
        runFlatpak({QStringLiteral("remote-delete"), QStringLiteral("--user"), QStringLiteral("flathub")});
    });
    refresh();
    return page;
}

// --------------------------------------------------------------- assistant

QWidget *SettingsWindow::assistantPage()
{
    // Providers are instantiated only to describe themselves and to list
    // models; the Settings application never sends chat messages.
    QList<ai::ChatProvider *> providers = {new ai::AnthropicProvider(m_nam, this),
                                           new ai::OpenAiProvider(m_nam, this)};
    auto *w = new AiSettingsWidget(m_store, providers, /*allowSessionKeys=*/false);
    return w;
}

} // namespace zion
