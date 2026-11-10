// SPDX-License-Identifier: GPL-3.0-or-later
#include "messagewidget.h"

#include "core/ai/markdown.h"
#include "core/i18n.h"
#include "ui/safetextbrowser.h"

#include <QClipboard>
#include <QCoreApplication>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QRegularExpression>
#include <QTimer>
#include <QVBoxLayout>

namespace zion {

MessageWidget::MessageWidget(Kind kind, const QString &heading, QWidget *parent)
    : QFrame(parent), m_kind(kind)
{
    setFrameShape(QFrame::StyledPanel);
    setObjectName(kind == User ? QStringLiteral("userMessage")
                  : kind == Assistant ? QStringLiteral("assistantMessage")
                  : kind == Error ? QStringLiteral("errorMessage") : QStringLiteral("noticeMessage"));
    const char *bg = kind == User ? "palette(alternate-base)" : kind == Error ? "rgba(200, 60, 60, 40)"
                     : kind == Notice ? "rgba(120, 120, 120, 30)" : "palette(base)";
    setStyleSheet(QStringLiteral("zion--MessageWidget { background: %1; border-radius: 8px; }").arg(QLatin1String(bg)));
    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(12, 8, 12, 8);
    lay->setSpacing(4);
    if (!heading.isEmpty()) {
        auto *head = new QLabel(heading);
        QFont f = head->font();
        f.setBold(true);
        head->setFont(f);
        lay->addWidget(head);
    }
    m_view = new SafeTextBrowser;
    lay->addWidget(m_view);
    m_extras = new QVBoxLayout;
    m_extras->setSpacing(6);
    lay->addLayout(m_extras);
    m_footnote = new QLabel;
    m_footnote->setWordWrap(true);
    m_footnote->setVisible(false);
    QFont small = m_footnote->font();
    small.setPointSizeF(small.pointSizeF() * 0.85);
    m_footnote->setFont(small);
    m_footnote->setTextInteractionFlags(Qt::TextSelectableByMouse);
    lay->addWidget(m_footnote);
}

void MessageWidget::setMarkdown(const QString &markdown)
{
    m_text = markdown;
    m_view->setMarkdownText(markdown);
}

void MessageWidget::setPlainText(const QString &text)
{
    m_text = text;
    m_view->setPlainText(text);
}

void MessageWidget::setFootnote(const QString &text)
{
    m_footnote->setText(text);
    m_footnote->setVisible(!text.isEmpty());
}

void MessageWidget::addAction(QWidget *w)
{
    m_extras->addWidget(w, 0, Qt::AlignLeading);
}

QString MessageWidget::collapseAttachments(const QString &composed)
{
    static const QRegularExpression re(
        QStringLiteral("\\n*<attached-content name=\"([^\"]*)\">\\n(.*?)\\n</attached-content>"),
        QRegularExpression::DotMatchesEverythingOption);
    QString out = composed;
    auto it = re.globalMatch(composed);
    QStringList markers;
    while (it.hasNext()) {
        const auto m = it.next();
        const int count = int(m.captured(2).size());
        const QString size = tr("%n character(s)", nullptr, count);
        markers << QString::fromUtf8("\xF0\x9F\x93\x8E %1 (%2)").arg(m.captured(1), size);
    }
    out.remove(re);
    if (!markers.isEmpty())
        out += (out.isEmpty() ? QString() : QStringLiteral("\n\n")) + markers.join(QLatin1Char('\n'));
    return out;
}

void MessageWidget::finalize()
{
    if (m_finalized || m_kind != Assistant)
        return;
    m_finalized = true;
    const auto blocks = ai::Markdown::codeBlocks(m_text);
    for (const ai::CodeBlock &b : blocks) {
        if (b.code.trimmed().isEmpty())
            continue;
        auto *card = new QFrame;
        card->setObjectName(b.isShellCommand() ? QStringLiteral("commandCard") : QStringLiteral("codeCard"));
        card->setFrameShape(QFrame::StyledPanel);
        auto *cl = new QVBoxLayout(card);
        cl->setContentsMargins(8, 6, 8, 6);
        if (b.isShellCommand()) {
            auto *warn = new QLabel(tr("Proposed command — it has not been run. Read the explanation above and "
                                       "make sure you understand it before pasting it into a terminal."));
            warn->setWordWrap(true);
            cl->addWidget(warn);
        }
        auto *row = new QHBoxLayout;
        // Code is shown as plain text, always left to right.
        auto *code = new QLabel(b.code);
        code->setTextFormat(Qt::PlainText);
        code->setLayoutDirection(Qt::LeftToRight);
        code->setAlignment(Qt::AlignLeft | Qt::AlignAbsolute | Qt::AlignVCenter);
        code->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
        code->setTextInteractionFlags(Qt::TextSelectableByMouse);
        code->setWordWrap(true);
        row->addWidget(code, 1);
        auto *copy = new QPushButton(b.isShellCommand() ? tr("Copy command") : tr("Copy code"));
        copy->setObjectName(QStringLiteral("copyButton"));
        const QString clean = ai::Markdown::commandForClipboard(b.code);
        connect(copy, &QPushButton::clicked, copy, [copy, clean] {
            QGuiApplication::clipboard()->setText(clean);
            const QString old = copy->text();
            copy->setText(MessageWidget::tr("Copied"));
            QTimer::singleShot(1500, copy, [copy, old] { copy->setText(old); });
        });
        row->addWidget(copy, 0, Qt::AlignTop);
        cl->addLayout(row);
        m_extras->addWidget(card);
    }
}

} // namespace zion
