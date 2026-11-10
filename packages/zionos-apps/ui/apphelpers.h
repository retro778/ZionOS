// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QString>
#include <QStringList>

class QApplication;
class QWidget;

namespace zion {

/// Common start-up for the ZionOS applications: identity, icon, language.
void initApplication(QApplication &app, const QString &name, const QString &desktopFile);

/// For processes that hold API keys in memory: disables core dumps and
/// same-user ptrace attachment (PR_SET_DUMPABLE=0), so a crash report or a
/// debugger attached by another process of the user cannot capture a key.
void protectProcessMemory();

/// Starts a desktop program detached. Returns false if it could not be started.
bool launch(const QString &program, const QStringList &arguments = {});
/// Whether @p program exists in PATH.
bool programAvailable(const QString &program);

/// Restarts the current application (used after a language change).
void restartApplication();

} // namespace zion
