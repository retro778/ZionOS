// SPDX-License-Identifier: GPL-3.0-or-later
#include "sseparser.h"

namespace zion::ai {

void SseParser::reset()
{
    m_buffer.clear();
    m_current = {};
    m_hasFields = false;
    m_overflow = false;
    m_first = true;
}

QList<SseEvent> SseParser::feed(const QByteArray &chunk)
{
    QList<SseEvent> out;
    m_buffer.append(chunk);
    if (m_first && m_buffer.size() >= 3) {
        if (m_buffer.startsWith("\xEF\xBB\xBF"))
            m_buffer.remove(0, 3); // UTF-8 BOM
        m_first = false;
    }
    qsizetype start = 0;
    for (;;) {
        // A line ends with CRLF, LF or CR. A trailing lone CR may be the first
        // half of a CRLF split across chunks, so wait for the next byte.
        qsizetype i = start;
        const qsizetype n = m_buffer.size();
        while (i < n && m_buffer[i] != '\n' && m_buffer[i] != '\r')
            ++i;
        if (i >= n)
            break;
        qsizetype next = i + 1;
        if (m_buffer[i] == '\r') {
            if (next >= n)
                break;
            if (m_buffer[next] == '\n')
                ++next;
        }
        processLine(m_buffer.mid(start, i - start), out);
        start = next;
    }
    m_buffer.remove(0, start);
    if (m_buffer.size() + m_current.data.size() > MaxEventBytes) {
        m_overflow = true;
        m_buffer.clear();
        m_current = {};
        m_hasFields = false;
    }
    return out;
}

void SseParser::processLine(const QByteArray &line, QList<SseEvent> &out)
{
    if (line.isEmpty()) { // dispatch
        if (m_hasFields) {
            if (m_current.data.endsWith('\n'))
                m_current.data.chop(1);
            out.append(m_current);
        }
        m_current = {};
        m_hasFields = false;
        return;
    }
    if (line.startsWith(':'))
        return; // comment / keep-alive
    const qsizetype colon = line.indexOf(':');
    const QByteArray field = colon < 0 ? line : line.left(colon);
    QByteArray value = colon < 0 ? QByteArray() : line.mid(colon + 1);
    if (value.startsWith(' '))
        value.remove(0, 1);
    if (field == "event") {
        m_current.event = value;
        m_hasFields = true;
    } else if (field == "data") {
        m_current.data.append(value);
        m_current.data.append('\n');
        m_hasFields = true;
    }
    // "id" and "retry" are not used: the client never reconnects on its own,
    // because resending a request could repeat a billable operation.
}

} // namespace zion::ai
