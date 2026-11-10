// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QList>
#include <QMetaType>
#include <QString>

namespace zion::ai {

enum class Role { User, Assistant };

struct ChatMessage
{
    Role role = Role::User;
    QString text;
};

struct ChatRequest
{
    QString model;
    QString systemPrompt;
    QList<ChatMessage> messages;
    int maxOutputTokens = 16000;
};

/// Token usage exactly as reported by the provider. Fields the provider did
/// not return stay at -1; the UI must not estimate or price them.
struct Usage
{
    qint64 inputTokens = -1;
    qint64 outputTokens = -1;
    qint64 totalTokens = -1;
    bool isKnown() const { return inputTokens >= 0 || outputTokens >= 0 || totalTokens >= 0; }
};

/// Why the model stopped, normalised across providers.
enum class StopReason {
    Complete,    ///< natural end of the reply
    MaxTokens,   ///< output limit reached, reply is truncated
    Refusal,     ///< provider declined to answer
    Other
};

struct ChatResult
{
    Usage usage;
    StopReason stopReason = StopReason::Complete;
    QString rawStopReason;   ///< provider's own value, for diagnostics
    QString servedModel;     ///< model that produced the reply, if reported
    QString requestId;
};

struct ProviderError
{
    enum Kind {
        Network,        ///< DNS, connection, TLS failure
        Timeout,        ///< no data within the configured time
        Authentication, ///< 401: missing/invalid key
        Permission,     ///< 403 or billing problem
        RateLimited,    ///< 429
        Overloaded,     ///< provider temporarily over capacity (529 / 503)
        InvalidRequest, ///< 400/404/413/422
        Server,         ///< other 5xx
        Protocol,       ///< response could not be understood
        Cancelled       ///< user pressed Stop
    };
    Kind kind = Network;
    int httpStatus = 0;
    QString providerType;    ///< provider's error type/code string
    QString message;         ///< provider's message, credentials redacted
    int retryAfterSeconds = -1; ///< from Retry-After, if present
    QString requestId;

    /// Whether trying again later can help. The application never retries
    /// automatically; this only shapes the message shown to the user.
    bool isTransient() const
    {
        return kind == Network || kind == Timeout || kind == RateLimited || kind == Overloaded
               || kind == Server;
    }
};

struct ModelInfo
{
    QString id;
    QString displayName;
};

QString kindName(ProviderError::Kind kind);

} // namespace zion::ai

Q_DECLARE_METATYPE(zion::ai::ProviderError)
Q_DECLARE_METATYPE(zion::ai::ChatResult)
Q_DECLARE_METATYPE(QList<zion::ai::ModelInfo>)
