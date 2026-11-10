// SPDX-License-Identifier: GPL-3.0-or-later
#include "mainwindow.h"
#include "ui/apphelpers.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QMessageBox>

#include <unistd.h>

int main(int argc, char **argv)
{
    zion::protectProcessMemory(); // API keys live in this process
    QApplication app(argc, argv);
    zion::initApplication(app, QStringLiteral("zionos-assistant"), QStringLiteral("zionos-assistant"));
    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addVersionOption();
    parser.process(app);
    // The assistant handles credentials and untrusted model output; it must
    // run with the user's own privileges only.
    if (geteuid() == 0) {
        QMessageBox::critical(nullptr, QStringLiteral("ZionOS Assistant"),
                              QCoreApplication::translate("zion::MainWindow",
                                  "The assistant does not run as the administrator (root). Start it as a normal user."));
        return 1;
    }
    qRegisterMetaType<zion::ai::ProviderError>();
    qRegisterMetaType<zion::ai::ChatResult>();
    zion::MainWindow w;
    w.show();
    return app.exec();
}
