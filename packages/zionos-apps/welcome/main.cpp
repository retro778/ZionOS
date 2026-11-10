// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/appsettings.h"
#include "ui/apphelpers.h"
#include "welcomewindow.h"

#include <QApplication>
#include <QCommandLineParser>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    zion::initApplication(app, QStringLiteral("zionos-welcome"), QStringLiteral("zionos-welcome"));
    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption autostart(QStringLiteral("autostart"),
                                       QStringLiteral("Started at login: exit quietly if disabled."));
    parser.addOption(autostart);
    parser.process(app);
    if (parser.isSet(autostart) && !zion::AppSettings().showWelcomeAtLogin())
        return 0;
    zion::WelcomeWindow w;
    w.show();
    return app.exec();
}
