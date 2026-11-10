// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QWidget>

namespace zion {

/// First-run window: language choice and shortcuts to the main tasks.
class WelcomeWindow : public QWidget
{
    Q_OBJECT
public:
    explicit WelcomeWindow(QWidget *parent = nullptr);

private:
    void chooseLanguage(const QString &language);
    void run(const QString &program, const QStringList &args = {});
};

} // namespace zion
