// SPDX-License-Identifier: GPL-3.0-or-later
#include "redact.h"

#include <QMutex>
#include <QRegularExpression>

namespace zion {

namespace {
QMutex g_mutex;
QStringList &secrets()
{
    static QStringList s;
    return s;
}
} // namespace

QString Redactor::placeholder()
{
    return QStringLiteral("[REDACTED]");
}

void Redactor::registerSecret(const QString &secret)
{
    if (secret.size() < 8)
        return; // too short to redact safely without mangling ordinary text
    QMutexLocker lock(&g_mutex);
    if (!secrets().contains(secret))
        secrets().append(secret);
}

void Redactor::clearSecrets()
{
    QMutexLocker lock(&g_mutex);
    secrets().clear();
}

QString Redactor::redact(const QString &text)
{
    QString out = text;
    {
        QMutexLocker lock(&g_mutex);
        for (const QString &s : std::as_const(secrets()))
            out.replace(s, placeholder());
    }
    static const QRegularExpression headerRe(
        QStringLiteral("((?:authorization|x-api-key|api[-_]?key|openai-api-key)\"?\\s*[:=]\\s*\"?(?:bearer\\s+)?)[^\\s\"',;]+"),
        QRegularExpression::CaseInsensitiveOption);
    out.replace(headerRe, QStringLiteral("\\1") + placeholder());
    static const QRegularExpression bearerRe(QStringLiteral("(bearer\\s+)[A-Za-z0-9._~+/=-]{8,}"),
                                             QRegularExpression::CaseInsensitiveOption);
    out.replace(bearerRe, QStringLiteral("\\1") + placeholder());
    // Key shapes used by the supported providers (sk-..., sk-ant-..., sk-proj-...).
    static const QRegularExpression keyRe(QStringLiteral("\\bsk-[A-Za-z0-9_-]{12,}"));
    out.replace(keyRe, placeholder());
    return out;
}

} // namespace zion
