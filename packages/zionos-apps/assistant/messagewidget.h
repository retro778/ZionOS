// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QFrame>

class QLabel;
class QVBoxLayout;

namespace zion {

class SafeTextBrowser;

/// One chat bubble. Assistant text is rendered as sanitised Markdown; once
/// the reply is complete, each code block gets a "copy" card. Shell
/// commands are clearly marked as proposals: they are never executed.
class MessageWidget : public QFrame
{
    Q_OBJECT
public:
    enum Kind { User, Assistant, Notice, Error };
    MessageWidget(Kind kind, const QString &heading, QWidget *parent = nullptr);

    void setMarkdown(const QString &markdown);   ///< assistant text (may still be streaming)
    void setPlainText(const QString &text);      ///< user / notice text, shown literally
    void finalize();                             ///< reply complete: add command cards
    void setFootnote(const QString &text);
    QString text() const { return m_text; }
    Kind kind() const { return m_kind; }
    /// Adds a widget (e.g. a button) below the text.
    void addAction(QWidget *w);

    /// Replaces attachment blocks by a one-line marker for display.
    static QString collapseAttachments(const QString &composedUserMessage);

private:
    Kind m_kind;
    QString m_text;
    SafeTextBrowser *m_view;
    QLabel *m_footnote;
    QVBoxLayout *m_extras;
    bool m_finalized = false;
};

} // namespace zion
