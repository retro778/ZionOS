// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "types.h"

#include <QDateTime>
#include <QString>

namespace zion::ai {

struct Conversation
{
    QString id;            ///< UUID, also the file name
    QString title;
    QString providerId;
    QString model;
    QDateTime created;
    QDateTime updated;
    QList<ChatMessage> messages;
};

/// Conversation history kept only on this computer, one JSON file per
/// conversation under ~/.local/share/zionos-assistant/conversations (mode
/// 0600, directory 0700). Nothing here is ever uploaded; history can be
/// disabled, and conversations deleted one by one or all at once.
class HistoryStore
{
public:
    explicit HistoryStore(const QString &directory = defaultDirectory());

    static QString defaultDirectory();
    QString directory() const { return m_dir; }

    /// Newest first. Only metadata and messages; unreadable files are skipped.
    QList<Conversation> list() const;
    bool load(const QString &id, Conversation *out) const;
    bool save(const Conversation &conversation) const;
    bool remove(const QString &id) const;
    /// Deletes every stored conversation. Returns the number removed.
    int removeAll() const;

    static QString newId();
    static QString titleFor(const QString &firstUserMessage);

private:
    QString pathFor(const QString &id) const;
    QString m_dir;
};

} // namespace zion::ai
