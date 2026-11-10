// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QList>
#include <QString>
#include <QUrl>

class QTextDocument;

namespace zion::ai {

struct CodeBlock
{
    QString language; ///< info string of the fence, lower-cased ("" if none)
    QString code;
    bool isShellCommand() const;
};

/// Rendering of untrusted model output.
namespace Markdown {

/// Loads Markdown into @p doc with raw HTML disabled: tags in the text are
/// shown literally, never interpreted. The document must be displayed by a
/// widget that refuses to load external resources (SafeTextBrowser).
void render(QTextDocument *doc, const QString &markdown);

/// Fenced code blocks, in order. Used to offer "copy" for proposed commands.
QList<CodeBlock> codeBlocks(const QString &markdown);

/// True only for http/https links, the only kind the UI will offer to open
/// (after confirmation). file:, javascript:, data: and others are refused.
bool isSafeLink(const QUrl &url);

/// Text to place on the clipboard for a proposed command: bidi controls and
/// other invisible characters removed, trailing whitespace trimmed.
QString commandForClipboard(const QString &code);

} // namespace Markdown
} // namespace zion::ai
