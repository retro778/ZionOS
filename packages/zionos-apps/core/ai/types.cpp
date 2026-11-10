// SPDX-License-Identifier: GPL-3.0-or-later
#include "types.h"

namespace zion::ai {

QString kindName(ProviderError::Kind kind)
{
    switch (kind) {
    case ProviderError::Network: return QStringLiteral("network");
    case ProviderError::Timeout: return QStringLiteral("timeout");
    case ProviderError::Authentication: return QStringLiteral("authentication");
    case ProviderError::Permission: return QStringLiteral("permission");
    case ProviderError::RateLimited: return QStringLiteral("rate_limited");
    case ProviderError::Overloaded: return QStringLiteral("overloaded");
    case ProviderError::InvalidRequest: return QStringLiteral("invalid_request");
    case ProviderError::Server: return QStringLiteral("server");
    case ProviderError::Protocol: return QStringLiteral("protocol");
    case ProviderError::Cancelled: return QStringLiteral("cancelled");
    }
    return QStringLiteral("unknown");
}

} // namespace zion::ai
