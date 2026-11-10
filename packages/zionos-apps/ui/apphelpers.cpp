// SPDX-License-Identifier: GPL-3.0-or-later
#include "apphelpers.h"

#include "core/i18n.h"

#include <QApplication>
#include <QIcon>
#include <QProcess>
#include <QStandardPaths>

#include <sys/prctl.h>
#include <sys/resource.h>

namespace zion {

void initApplication(QApplication &app, const QString &name, const QString &desktopFile)
{
    QCoreApplication::setOrganizationName(QStringLiteral("ZionOS"));
    QCoreApplication::setApplicationName(name);
    QCoreApplication::setApplicationVersion(QStringLiteral(ZIONOS_VERSION));
    QGuiApplication::setDesktopFileName(desktopFile);
    QIcon icon = QIcon::fromTheme(QStringLiteral("zionos-logo"));
    if (icon.isNull())
        icon = QIcon(QStringLiteral("/usr/share/zionos/artwork/zionos-logo.svg"));
    QApplication::setWindowIcon(icon);
    I18n::install(&app);
}

void protectProcessMemory()
{
    prctl(PR_SET_DUMPABLE, 0, 0, 0, 0);
    const struct rlimit none = {0, 0};
    setrlimit(RLIMIT_CORE, &none);
}

bool programAvailable(const QString &program)
{
    return !QStandardPaths::findExecutable(program).isEmpty();
}

bool launch(const QString &program, const QStringList &arguments)
{
    if (!programAvailable(program))
        return false;
    return QProcess::startDetached(program, arguments);
}

void restartApplication()
{
    QStringList args = QCoreApplication::arguments();
    const QString program = args.takeFirst();
    QProcess::startDetached(program, args);
    QCoreApplication::quit();
}

} // namespace zion
