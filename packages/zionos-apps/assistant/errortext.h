// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "core/ai/types.h"

#include <QCoreApplication>

namespace zion {

/// User-facing explanations of provider errors.
class ErrorText
{
    Q_DECLARE_TR_FUNCTIONS(zion::ErrorText)
public:
    static QString describe(const ai::ProviderError &error, const QString &providerName);
    /// Technical details line (HTTP status, provider error type, request id).
    static QString details(const ai::ProviderError &error);
};

} // namespace zion
