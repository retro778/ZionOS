// SPDX-License-Identifier: GPL-3.0-or-later
#include "markdown.h"

#include "core/i18n.h"

#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>

namespace zion::ai {

bool CodeBlock::isShellCommand() const
{
    static const QStringList shells = {
        QStringLiteral("sh"), QStringLiteral("bash"), QStringLiteral("shell"),
        QStringLiteral("zsh"), QStringLiteral("console"), QStringLiteral("terminal"),
        QStringLiteral("shell-session"), QStringLiteral("fish"),
    };
    return shells.contains(language);
}

namespace Markdown {

void render(QTextDocument *doc, const QString &markdown)
{
    // MarkdownNoHTML: inline and block HTML in the input is treated as text.
    doc->setMarkdown(markdown, QTextDocument::MarkdownFeatures(QTextDocument::MarkdownDialectGitHub
                                                             | QTextDocument::MarkdownNoHTML));
    // Paragraph direction follows each paragraph's own first strong character
    // (Qt's default for blocks without an explicit direction), so Hebrew and
    // English paragraphs can alternate. Code is always left-to-right, even
    // when it contains Hebrew comments or strings.
    for (QTextBlock b = doc->begin(); b.isValid(); b = b.next()) {
        QTextBlockFormat f = b.blockFormat();
        const bool code = f.hasProperty(QTextFormat::BlockCodeFence)
                          || f.hasProperty(QTextFormat::BlockCodeLanguage)
                          || b.charFormat().fontFixedPitch();
        if (code) {
            f.setLayoutDirection(Qt::LeftToRight);
            f.setAlignment(Qt::AlignLeft | Qt::AlignAbsolute);
            QTextCursor c(b);
            c.setBlockFormat(f);
        }
    }
}

QList<CodeBlock> codeBlocks(const QString &markdown)
{
    QList<CodeBlock> out;
    const QStringList lines = markdown.split(QLatin1Char('\n'));
    bool inside = false;
    QString fence;
    CodeBlock current;
    for (const QString &raw : lines) {
        const QString t = raw.trimmed();
        if (!inside) {
            if (t.startsWith(QLatin1String("```")) || t.startsWith(QLatin1String("~~~"))) {
                const QChar ch = t.at(0);
                qsizetype n = 0;
                while (n < t.size() && t.at(n) == ch)
                    ++n;
                fence = QString(n, ch);
                current = {};
                current.language = t.mid(n).trimmed().section(QLatin1Char(' '), 0, 0).toLower();
                inside = true;
            }
        } else if (t.startsWith(fence) && t.mid(fence.size()).trimmed().isEmpty()) {
            if (current.code.endsWith(QLatin1Char('\n')))
                current.code.chop(1);
            out.append(current);
            inside = false;
        } else {
            current.code += raw + QLatin1Char('\n');
        }
    }
    return out; // an unterminated block (still streaming) is not reported
}

bool isSafeLink(const QUrl &url)
{
    if (!url.isValid() || url.host().isEmpty())
        return false;
    const QString s = url.scheme().toLower();
    return s == QLatin1String("https") || s == QLatin1String("http");
}

QString commandForClipboard(const QString &code)
{
    QString out;
    const QString stripped = I18n::stripBidiControls(code);
    out.reserve(stripped.size());
    for (const QChar c : stripped) {
        const ushort u = c.unicode();
        const bool invisible = u == 0x200B || u == 0x200C || u == 0x200D || u == 0x2060
                               || u == 0xFEFF || u == 0x00AD
                               || (u < 0x20 && u != '\n' && u != '\t') || u == 0x7F;
        if (!invisible)
            out.append(c);
    }
    while (!out.isEmpty() && out.back().isSpace())
        out.chop(1);
    return out;
}

} // namespace Markdown
} // namespace zion::ai
