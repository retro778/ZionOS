// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QString>
#include <QStringList>

namespace zion {

/// Removes credentials from text before it is logged or shown in diagnostics.
/// Works on patterns (API key shapes, Authorization headers) and on literal
/// secrets registered at runtime, so a key is redacted even if its format
/// changes.
class Redactor
{
public:
    static QString redact(const QString &text);
    /// Registers a literal secret for the lifetime of the process.
    static void registerSecret(const QString &secret);
    static void clearSecrets();
    static QString placeholder();
};

} // namespace zion
