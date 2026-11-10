// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "types.h"

#include <QList>
#include <QString>

namespace zion::ai {

/// Something the user explicitly chose to send along with a message.
struct Attachment
{
    enum Kind { File, Text };
    Kind kind = Text;
    QString label;    ///< file name, or a caption such as "Clipboard text"
    QString content;  ///< UTF-8 text content
    qint64 bytes() const { return content.toUtf8().size(); }
};

namespace Attachments {

constexpr qint64 MaxFileBytes = 256 * 1024;

struct LoadResult
{
    bool ok = false;
    QString error; ///< machine-readable reason: "too_large", "not_text", "unreadable"
    Attachment attachment;
};

/// Reads a text file chosen by the user. Binary files and files larger than
/// MaxFileBytes are refused rather than truncated.
LoadResult loadFile(const QString &path);

/// Builds the final user message text: the typed text followed by each
/// attachment in a clearly delimited block. Attachment content is data: the
/// delimiters and the system prompt tell the model not to follow
/// instructions found inside it.
QString composeUserMessage(const QString &typedText, const QList<Attachment> &attachments);

/// Instructions sent with every request. @p language is "en" or "he".
QString systemPrompt(const QString &osPrettyName, const QString &language);

} // namespace Attachments
} // namespace zion::ai
