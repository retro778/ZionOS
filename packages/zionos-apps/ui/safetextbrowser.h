// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QTextBrowser>

namespace zion {

/// Displays untrusted Markdown (model output).
///  * Raw HTML is never interpreted (see ai::Markdown::render).
///  * No resource is ever loaded: images, stylesheets and remote content
///    referenced by the text are ignored, so displaying a reply cannot
///    trigger network or file access.
///  * Links are not followed. Clicking an http(s) link asks for
///    confirmation showing the full address; every other scheme is refused.
///  * The widget grows to fit its content (no inner scroll bar).
class SafeTextBrowser : public QTextBrowser
{
    Q_OBJECT
public:
    explicit SafeTextBrowser(QWidget *parent = nullptr);
    void setMarkdownText(const QString &markdown);
    QString markdownText() const { return m_markdown; }
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    QVariant loadResource(int type, const QUrl &name) override;
    void resizeEvent(QResizeEvent *e) override;

private:
    void onAnchorClicked(const QUrl &url);
    void fitHeight();
    QString m_markdown;
};

} // namespace zion
