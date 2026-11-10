// SPDX-License-Identifier: GPL-3.0-or-later
#include "aboutwidget.h"

#include "core/i18n.h"
#include "core/sysinfo.h"

#include <QClipboard>
#include <QFormLayout>
#include <QGuiApplication>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace zion {

AboutWidget::AboutWidget(QWidget *parent) : QWidget(parent)
{
    const SystemInfo info;
    auto *outer = new QVBoxLayout(this);
    outer->setSpacing(12);

    auto *logo = new QLabel;
    logo->setPixmap(QIcon(QStringLiteral("/usr/share/zionos/artwork/zionos-logo.svg")).pixmap(96, 96));
    logo->setAlignment(Qt::AlignHCenter);
    outer->addWidget(logo);

    auto *title = new QLabel(QStringLiteral("<h2>%1</h2>").arg(info.prettyName().toHtmlEscaped()));
    title->setAlignment(Qt::AlignHCenter);
    title->setObjectName(QStringLiteral("aboutTitle"));
    outer->addWidget(title);

    const QString desktop = qEnvironmentVariable("XDG_CURRENT_DESKTOP", tr("unknown"));
    const QString session = qEnvironmentVariable("XDG_SESSION_TYPE");
    const QList<QPair<QString, QString>> rows = {
        {tr("Version"), info.version()},
        {tr("Based on"), info.baseDistribution()},
        {tr("Kernel"), info.kernel()},
        {tr("Architecture"), info.architecture()},
        {tr("Desktop"), session.isEmpty() ? desktop : QStringLiteral("%1 (%2)").arg(desktop, session)},
        {tr("Session"), info.isLiveSession() ? tr("Live session (not installed)") : tr("Installed system")},
    };
    auto *form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignTrailing);
    QStringList lines;
    for (const auto &r : rows) {
        // Version strings are technical, left-to-right data even in Hebrew.
        auto *value = new QLabel(I18n::ltrIsolate(r.second));
        value->setTextInteractionFlags(Qt::TextSelectableByMouse);
        value->setTextFormat(Qt::PlainText);
        form->addRow(r.first + QLatin1Char(':'), value);
        lines << r.first + QStringLiteral(": ") + r.second;
    }
    m_details = lines.join(QLatin1Char('\n'));
    outer->addLayout(form);

    auto *support = new QLabel(tr(
        "ZionOS is a community distribution built on Debian. Security updates for the "
        "base system are provided by the Debian project through the normal update "
        "tools. Documentation is installed in /usr/share/doc/zionos-base."));
    support->setWordWrap(true);
    outer->addWidget(support);

    auto *legal = new QLabel(tr(
        "ZionOS is not affiliated with or endorsed by the Debian Project. "
        "Debian is a registered trademark of Software in the Public Interest, Inc."));
    legal->setWordWrap(true);
    QFont small = legal->font();
    small.setPointSizeF(small.pointSizeF() * 0.9);
    legal->setFont(small);
    outer->addWidget(legal);

    auto *copy = new QPushButton(tr("Copy details"));
    copy->setObjectName(QStringLiteral("copyDetails"));
    connect(copy, &QPushButton::clicked, this, [this] {
        QGuiApplication::clipboard()->setText(m_details);
    });
    outer->addWidget(copy, 0, Qt::AlignHCenter);
    outer->addStretch();
}

} // namespace zion
