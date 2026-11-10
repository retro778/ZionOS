// SPDX-License-Identifier: GPL-3.0-or-later
#include "settingswindow.h"
#include "ui/apphelpers.h"

#include <QApplication>
#include <QCommandLineParser>

int main(int argc, char **argv)
{
    zion::protectProcessMemory(); // an API key passes through this process when saved
    QApplication app(argc, argv);
    zion::initApplication(app, QStringLiteral("zionos-settings"), QStringLiteral("zionos-settings"));
    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption page(QStringLiteral("page"),
                                  QStringLiteral("Page to open: language, keyboard, system, software, assistant, about."),
                                  QStringLiteral("page"));
    parser.addOption(page);
    parser.process(app);
    zion::SettingsWindow w;
    if (parser.isSet(page))
        w.showPage(parser.value(page));
    w.show();
    return app.exec();
}
