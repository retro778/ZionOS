// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QLocale>
#include <QString>

class QCoreApplication;

namespace zion {

/// Language handling shared by all ZionOS applications.
namespace I18n {

/// Languages with a complete ZionOS translation.
QStringList supportedLanguages(); // {"en", "he"}

/// The language ZionOS applications should use: the explicit user choice if
/// one was made, otherwise the system locale, otherwise English.
QString effectiveLanguage();

/// Installs translators for @p language (ZionOS strings plus Qt's own) and
/// sets the application layout direction. Returns true if the ZionOS
/// catalogue was loaded (always true for English, the source language).
bool install(QCoreApplication *app, const QString &language = effectiveLanguage(),
             const QString &translationsDir = {});

bool isRightToLeft(const QString &language);

/// Base direction of a paragraph according to its first strong character
/// (Unicode Bidi rule P2/P3). Neutral-only text yields @p fallback.
Qt::LayoutDirection baseDirection(const QString &text,
                                  Qt::LayoutDirection fallback = Qt::LeftToRight);

/// Wraps text that must always read left-to-right (commands, paths, URLs,
/// model names) in Unicode isolates (LRI ... PDI) so it stays intact when
/// embedded in a right-to-left sentence.
QString ltrIsolate(const QString &text);

/// Removes bidi control characters. Used on anything copied to the clipboard
/// as a command so invisible characters never reach a terminal.
QString stripBidiControls(const QString &text);

} // namespace I18n
} // namespace zion
