// SPDX-License-Identifier: GPL-3.0-or-later
#include "errortext.h"

#include "core/i18n.h"

namespace zion {

using ai::ProviderError;

QString ErrorText::describe(const ProviderError &e, const QString &provider)
{
    QString text;
    switch (e.kind) {
    case ProviderError::Cancelled:
        return tr("Stopped.");
    case ProviderError::Network:
        text = tr("Could not reach %1. Check your Internet connection and try again.").arg(provider);
        break;
    case ProviderError::Timeout:
        text = tr("%1 did not respond in time, so the request was stopped.").arg(provider);
        break;
    case ProviderError::Authentication:
        text = tr("%1 rejected the API key. Check the key, or replace it in the assistant settings.").arg(provider);
        break;
    case ProviderError::Permission:
        text = tr("%1 refused the request for this account. This usually means missing permissions, a billing problem or no remaining credit.").arg(provider);
        break;
    case ProviderError::RateLimited:
        text = tr("%1 is limiting requests from your account right now.").arg(provider);
        if (e.retryAfterSeconds >= 0)
            text += QLatin1Char(' ') + tr("The provider asks you to wait %n second(s) before trying again.", nullptr, e.retryAfterSeconds);
        break;
    case ProviderError::Overloaded:
        text = tr("%1 is temporarily overloaded. Try again in a little while.").arg(provider);
        break;
    case ProviderError::InvalidRequest:
        text = tr("%1 did not accept the request.").arg(provider);
        break;
    case ProviderError::Server:
        text = tr("%1 reported an internal error. Try again later.").arg(provider);
        break;
    case ProviderError::Protocol:
        text = tr("The reply from %1 was incomplete or could not be understood.").arg(provider);
        break;
    }
    // Requests are never repeated automatically, so nothing is billed twice.
    text += QLatin1Char(' ') + tr("The request was not sent again automatically.");
    if (!e.message.isEmpty() && e.kind != ProviderError::Network)
        text += QStringLiteral("\n\n") + tr("Message from the provider: %1").arg(I18n::ltrIsolate(e.message));
    return text;
}

QString ErrorText::details(const ProviderError &e)
{
    QStringList parts;
    if (e.httpStatus > 0)
        parts << QStringLiteral("HTTP %1").arg(e.httpStatus);
    if (!e.providerType.isEmpty())
        parts << e.providerType;
    if (!e.requestId.isEmpty())
        parts << QStringLiteral("request %1").arg(e.requestId);
    return parts.join(QStringLiteral(" · "));
}

} // namespace zion
