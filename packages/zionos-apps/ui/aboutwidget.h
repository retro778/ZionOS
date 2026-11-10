// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QWidget>

namespace zion {

/// "About ZionOS": version, base distribution, kernel and support information.
class AboutWidget : public QWidget
{
    Q_OBJECT
public:
    explicit AboutWidget(QWidget *parent = nullptr);
    /// Plain-text summary, as copied by the "Copy details" button.
    QString detailsText() const { return m_details; }

private:
    QString m_details;
};

} // namespace zion
