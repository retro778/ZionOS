// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QByteArray>
#include <QList>

namespace zion::ai {

struct SseEvent
{
    QByteArray event; ///< "event:" field ("" if absent)
    QByteArray data;  ///< "data:" lines joined with '\n'
};

/// Incremental parser for the text/event-stream format (WHATWG HTML,
/// "Server-sent events"). Feed it network chunks of any size; complete
/// events are returned as soon as their terminating blank line arrives.
class SseParser
{
public:
    QList<SseEvent> feed(const QByteArray &chunk);
    /// Bytes received that do not yet form a complete event.
    qsizetype pendingBytes() const { return m_buffer.size(); }
    void reset();

    /// Upper bound for a single buffered event; protects against a server
    /// that never sends a line terminator.
    static constexpr qsizetype MaxEventBytes = 8 * 1024 * 1024;
    bool overflowed() const { return m_overflow; }

private:
    void processLine(const QByteArray &line, QList<SseEvent> &out);

    QByteArray m_buffer;
    SseEvent m_current;
    bool m_hasFields = false;
    bool m_overflow = false;
    bool m_first = true;
};

} // namespace zion::ai
