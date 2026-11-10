// SPDX-License-Identifier: GPL-3.0-or-later
#include "welcomewindow.h"

#include "core/appsettings.h"
#include "core/desktopconfig.h"
#include "core/i18n.h"
#include "core/sysinfo.h"
#include "ui/apphelpers.h"

#include <QCheckBox>
#include <QDir>
#include <QFile>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QStandardPaths>
#include <QVBoxLayout>

namespace zion {

static QPushButton *taskButton(const QString &icon, const QString &title, const QString &subtitle)
{
    auto *b = new QPushButton(QIcon::fromTheme(icon), title + QLatin1Char('\n') + subtitle);
    b->setIconSize(QSize(40, 40));
    b->setMinimumHeight(72);
    b->setStyleSheet(QStringLiteral("QPushButton { text-align: start; padding: 8px 14px; }"));
    b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    return b;
}

WelcomeWindow::WelcomeWindow(QWidget *parent) : QWidget(parent)
{
    const SystemInfo info;
    setWindowTitle(tr("Welcome to ZionOS"));
    resize(760, 600);
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(28, 24, 28, 20);
    outer->setSpacing(14);

    auto *header = new QHBoxLayout;
    auto *logo = new QLabel;
    logo->setPixmap(QIcon(QStringLiteral("/usr/share/zionos/artwork/zionos-logo.svg")).pixmap(72, 72));
    header->addWidget(logo);
    auto *titles = new QVBoxLayout;
    auto *title = new QLabel(QStringLiteral("<h1>%1</h1>").arg(tr("Welcome to ZionOS").toHtmlEscaped()));
    title->setObjectName(QStringLiteral("welcomeTitle"));
    titles->addWidget(title);
    auto *sub = new QLabel(tr("%1 — a Debian-based desktop in English and Hebrew.").arg(info.prettyName()));
    sub->setWordWrap(true);
    titles->addWidget(sub);
    header->addLayout(titles, 1);
    outer->addLayout(header);

    // Language choice: both names are shown in their own language so the
    // user can always find theirs, whatever the current language is.
    auto *langRow = new QHBoxLayout;
    langRow->addWidget(new QLabel(tr("Language:")));
    const QString current = I18n::effectiveLanguage();
    auto *en = new QPushButton(QStringLiteral("English"));
    en->setObjectName(QStringLiteral("langEnglish"));
    auto *he = new QPushButton(QStringLiteral("עברית"));
    he->setObjectName(QStringLiteral("langHebrew"));
    for (QPushButton *b : {en, he}) {
        b->setCheckable(true);
        b->setAutoExclusive(true);
        langRow->addWidget(b);
    }
    (current == QLatin1String("he") ? he : en)->setChecked(true);
    connect(en, &QPushButton::clicked, this, [this] { chooseLanguage(QStringLiteral("en")); });
    connect(he, &QPushButton::clicked, this, [this] { chooseLanguage(QStringLiteral("he")); });
    langRow->addStretch();
    outer->addLayout(langRow);

    auto *grid = new QGridLayout;
    grid->setSpacing(10);
    int n = 0;
    auto add = [&](QPushButton *b, const QString &program, const QStringList &args = {}) {
        b->setEnabled(programAvailable(program));
        if (!b->isEnabled())
            b->setToolTip(tr("Not installed"));
        connect(b, &QPushButton::clicked, this, [this, program, args] { run(program, args); });
        grid->addWidget(b, n / 2, n % 2);
        ++n;
    };
    if (info.isLiveSession()) {
        auto *install = taskButton(QStringLiteral("system-software-install"), tr("Install ZionOS"),
                                   tr("Install to this computer's disk. You are running the live system now."));
        install->setObjectName(QStringLiteral("installButton"));
        add(install, QStringLiteral("zionos-install"));
    }
    add(taskButton(QStringLiteral("plasmadiscover"), tr("Install software"),
                   tr("Browse and install applications and updates.")),
        QStringLiteral("plasma-discover"));
    add(taskButton(QStringLiteral("preferences-system"), tr("ZionOS Settings"),
                   tr("Language, keyboard layouts, assistant and system information.")),
        QStringLiteral("zionos-settings"));
    add(taskButton(QStringLiteral("preferences-desktop"), tr("System Settings"),
                   tr("Network, sound, displays, date and time, and everything else.")),
        QStringLiteral("systemsettings"));
    add(taskButton(QStringLiteral("zionos-logo"), tr("ZionOS Assistant"),
                   tr("Optional AI helper. Needs your own OpenAI or Anthropic API key.")),
        QStringLiteral("zionos-assistant"));
    add(taskButton(QStringLiteral("system-file-manager"), tr("Files"), tr("Open your home folder.")),
        QStringLiteral("dolphin"));
    add(taskButton(QStringLiteral("help-about"), tr("About ZionOS"),
                   tr("Version, base system and support information.")),
        QStringLiteral("zionos-settings"), {QStringLiteral("--page"), QStringLiteral("about")});
    outer->addLayout(grid);
    outer->addStretch();

    if (info.isLiveSession()) {
        auto *live = new QLabel(tr("Live session: user “%1”, password “%2”. Changes are lost when you shut down.")
                                    .arg(I18n::ltrIsolate(QStringLiteral("zion")), I18n::ltrIsolate(QStringLiteral("live"))));
        live->setWordWrap(true);
        outer->addWidget(live);
    }

    auto *footer = new QHBoxLayout;
    auto *show = new QCheckBox(tr("Show this window when I log in"));
    show->setObjectName(QStringLiteral("showAtLogin"));
    show->setChecked(AppSettings().showWelcomeAtLogin());
    connect(show, &QCheckBox::toggled, this, [](bool on) { AppSettings().setShowWelcomeAtLogin(on); });
    footer->addWidget(show);
    footer->addStretch();
    auto *close = new QPushButton(tr("Close"));
    connect(close, &QPushButton::clicked, this, &QWidget::close);
    footer->addWidget(close);
    outer->addLayout(footer);
}

void WelcomeWindow::run(const QString &program, const QStringList &args)
{
    if (!launch(program, args))
        QMessageBox::warning(this, windowTitle(),
                             tr("%1 could not be started.").arg(I18n::ltrIsolate(program)));
}

void WelcomeWindow::chooseLanguage(const QString &language)
{
    if (language == I18n::effectiveLanguage())
        return;
    AppSettings s;
    s.setLanguage(language);
    s.sync();
    // Also set the desktop language and make sure both keyboard layouts
    // are available. Plasma applies the language at the next login.
    DesktopConfig cfg;
    cfg.setPlasmaLanguage(language);
    QStringList layouts = cfg.keyboardLayouts();
    if (!layouts.contains(QStringLiteral("us")))
        layouts.prepend(QStringLiteral("us"));
    if (!layouts.contains(QStringLiteral("il")))
        layouts.append(QStringLiteral("il"));
    QString sw = cfg.switchOption();
    if (sw.isEmpty())
        sw = QStringLiteral("grp:alt_shift_toggle");
    cfg.setKeyboard(layouts, sw);
    DesktopConfig::notifyKeyboardChanged();
    // Bilingual on purpose: shown at the moment of switching languages.
    QMessageBox::information(
        this, QStringLiteral("ZionOS"),
        QStringLiteral("ZionOS applications switch language now. The rest of the desktop follows "
                       "after you log out and back in.\n\n"
                       "יישומי ZionOS עוברים לשפה החדשה כעת. שאר שולחן העבודה יתעדכן לאחר יציאה "
                       "וכניסה מחדש."));
    restartApplication();
}

} // namespace zion
