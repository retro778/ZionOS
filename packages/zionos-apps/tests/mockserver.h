// SPDX-License-Identifier: GPL-3.0-or-later
// Minimal scripted HTTP/1.1 server for provider tests. Header-only.
#pragma once
#include <QList>
#include <QMap>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QUrl>

struct MockRequest
{
    QByteArray method, path;
    QMap<QByteArray, QByteArray> headers; // lower-cased names
    QByteArray body;
};

struct MockResponse
{
    int status = 200;
    QList<QPair<QByteArray, QByteArray>> headers = {{"content-type", "text/event-stream"}};
    QList<QByteArray> chunks;  // sent one by one
    int chunkDelayMs = 0;      // delay between chunks
    bool hang = false;         // after the chunks, keep the connection open forever
};

class MockServer : public QTcpServer
{
    Q_OBJECT
public:
    explicit MockServer(QObject *parent = nullptr) : QTcpServer(parent)
    {
        listen(QHostAddress::LocalHost, 0);
        connect(this, &QTcpServer::newConnection, this, [this] {
            while (QTcpSocket *s = nextPendingConnection()) {
                connect(s, &QTcpSocket::readyRead, this, [this, s] { onData(s); });
                connect(s, &QTcpSocket::disconnected, s, &QObject::deleteLater);
            }
        });
    }
    QUrl url(const QString &path = {}) const
    {
        return QUrl(QStringLiteral("http://127.0.0.1:%1%2").arg(serverPort()).arg(path));
    }
    MockResponse response;
    QList<MockRequest> requests;

    static QByteArray sse(const QByteArray &event, const QByteArray &json)
    {
        return (event.isEmpty() ? QByteArray() : "event: " + event + "\n") + "data: " + json + "\n\n";
    }

private:
    void onData(QTcpSocket *s)
    {
        QByteArray &buf = m_buffers[s];
        buf += s->readAll();
        if (!buf.isEmpty() && buf.at(0) == 0x16) { // TLS ClientHello on a plain-text port
            s->write("HTTP/1.1 400 Bad Request\r\n\r\n");
            s->disconnectFromHost();
            return;
        }
        const qsizetype end = buf.indexOf("\r\n\r\n");
        if (end < 0)
            return;
        MockRequest req;
        const QList<QByteArray> lines = buf.left(end).split('\n');
        const QList<QByteArray> first = lines.first().trimmed().split(' ');
        req.method = first.value(0);
        req.path = first.value(1);
        for (qsizetype i = 1; i < lines.size(); ++i) {
            const qsizetype c = lines[i].indexOf(':');
            if (c > 0)
                req.headers.insert(lines[i].left(c).trimmed().toLower(), lines[i].mid(c + 1).trimmed());
        }
        const qsizetype len = req.headers.value("content-length").toLongLong();
        if (buf.size() < end + 4 + len)
            return;
        req.body = buf.mid(end + 4, len);
        m_buffers.remove(s);
        requests.append(req);

        QByteArray head = "HTTP/1.1 " + QByteArray::number(response.status) + " X\r\n";
        for (const auto &h : response.headers)
            head += h.first + ": " + h.second + "\r\n";
        head += "connection: close\r\n\r\n";
        s->write(head);
        s->flush();
        sendChunk(s, 0);
    }
    void sendChunk(QTcpSocket *s, qsizetype i)
    {
        if (s->state() != QAbstractSocket::ConnectedState)
            return;
        if (i >= response.chunks.size()) {
            if (!response.hang)
                s->disconnectFromHost();
            return;
        }
        s->write(response.chunks.at(i));
        s->flush();
        QTimer::singleShot(response.chunkDelayMs, s, [this, s, i] { sendChunk(s, i + 1); });
    }
    QMap<QTcpSocket *, QByteArray> m_buffers;
};
