// SPDX-License-Identifier: GPL-3.0-or-later
#include "safetextbrowser.h"

#include "core/ai/markdown.h"
#include "core/i18n.h"

#include <QAbstractTextDocumentLayout>
#include <QDesktopServices>
#include <QMessageBox>
#include <QPushButton>
#include <QResizeEvent>

namespace zion {

SafeTextBrowser::SafeTextBrowser(QWidget *parent) : QTextBrowser(parent)
{
    setOpenLinks(false);
    setOpenExternalLinks(false);
    setFrameShape(QFrame::NoFrame);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    viewport()->setAutoFillBackground(false);
    setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard
                            | Qt::LinksAccessibleByMouse | Qt::LinksAccessibleByKeyboard);
    connect(this, &QTextBrowser::anchorClicked, this, &SafeTextBrowser::onAnchorClicked);
    connect(document()->documentLayout(), &QAbstractTextDocumentLayout::documentSizeChanged, this,
            [this] { fitHeight(); });
}

QVariant SafeTextBrowser::loadResource(int, const QUrl &)
{
    return {}; // never load anything referenced by untrusted text
}

void SafeTextBrowser::setMarkdownText(const QString &markdown)
{
    m_markdown = markdown;
    ai::Markdown::render(document(), markdown);
    fitHeight();
}

void SafeTextBrowser::fitHeight()
{
    document()->setTextWidth(viewport()->width());
    const int h = int(document()->size().height()) + 2 * frameWidth() + 4;
    if (h != height())
        setFixedHeight(h);
}

QSize SafeTextBrowser::sizeHint() const
{
    return {400, height()};
}

QSize SafeTextBrowser::minimumSizeHint() const
{
    return {120, 20};
}

void SafeTextBrowser::resizeEvent(QResizeEvent *e)
{
    QTextBrowser::resizeEvent(e);
    if (e->size().width() != e->oldSize().width())
        fitHeight();
}

void SafeTextBrowser::onAnchorClicked(const QUrl &url)
{
    if (!ai::Markdown::isSafeLink(url)) {
        QMessageBox::warning(this, tr("Link blocked"),
                             tr("This link was not opened because it does not point to a web page:\n%1")
                                 .arg(I18n::ltrIsolate(url.toDisplayString())));
        return;
    }
    QMessageBox box(QMessageBox::Question, tr("Open link?"),
                    tr("The assistant's reply contains a link. Open it in your web browser?"),
                    QMessageBox::NoButton, this);
    box.setInformativeText(I18n::ltrIsolate(url.toString(QUrl::FullyEncoded)));
    QPushButton *open = box.addButton(tr("Open"), QMessageBox::AcceptRole);
    box.addButton(QMessageBox::Cancel);
    box.setDefaultButton(QMessageBox::Cancel);
    box.exec();
    if (box.clickedButton() == open)
        QDesktopServices::openUrl(url);
}

} // namespace zion
