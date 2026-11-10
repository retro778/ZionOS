// SPDX-License-Identifier: GPL-3.0-or-later
#include "appsettings.h"

namespace zion {

AppSettings::AppSettings()
    : m_s(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("ZionOS"), QStringLiteral("zionos"))
{
}

QString AppSettings::language() const { return m_s.value(QStringLiteral("general/language")).toString(); }
void AppSettings::setLanguage(const QString &lang) { m_s.setValue(QStringLiteral("general/language"), lang); }

bool AppSettings::showWelcomeAtLogin() const { return m_s.value(QStringLiteral("welcome/showAtLogin"), true).toBool(); }
void AppSettings::setShowWelcomeAtLogin(bool on) { m_s.setValue(QStringLiteral("welcome/showAtLogin"), on); }

QString AppSettings::provider() const { return m_s.value(QStringLiteral("assistant/provider"), QStringLiteral("anthropic")).toString(); }
void AppSettings::setProvider(const QString &id) { m_s.setValue(QStringLiteral("assistant/provider"), id); }

QString AppSettings::model(const QString &providerId) const
{
    return m_s.value(QStringLiteral("assistant/model_") + providerId).toString();
}
void AppSettings::setModel(const QString &providerId, const QString &model)
{
    m_s.setValue(QStringLiteral("assistant/model_") + providerId, model);
}

bool AppSettings::historyEnabled() const { return m_s.value(QStringLiteral("assistant/historyEnabled"), true).toBool(); }
void AppSettings::setHistoryEnabled(bool on) { m_s.setValue(QStringLiteral("assistant/historyEnabled"), on); }

int AppSettings::maxOutputTokens() const
{
    return qBound(256, m_s.value(QStringLiteral("assistant/maxOutputTokens"), 16000).toInt(), 128000);
}
void AppSettings::setMaxOutputTokens(int n) { m_s.setValue(QStringLiteral("assistant/maxOutputTokens"), n); }

int AppSettings::requestTimeoutSeconds() const
{
    return qBound(10, m_s.value(QStringLiteral("assistant/timeoutSeconds"), 120).toInt(), 1800);
}
void AppSettings::setRequestTimeoutSeconds(int s) { m_s.setValue(QStringLiteral("assistant/timeoutSeconds"), s); }

bool AppSettings::anthropicFallbackEnabled() const { return m_s.value(QStringLiteral("assistant/anthropicFallback"), true).toBool(); }
void AppSettings::setAnthropicFallbackEnabled(bool on) { m_s.setValue(QStringLiteral("assistant/anthropicFallback"), on); }

bool AppSettings::openAiStoreResponses() const { return m_s.value(QStringLiteral("assistant/openaiStore"), false).toBool(); }
void AppSettings::setOpenAiStoreResponses(bool on) { m_s.setValue(QStringLiteral("assistant/openaiStore"), on); }

void AppSettings::sync() { m_s.sync(); }
QString AppSettings::fileName() const { return m_s.fileName(); }

} // namespace zion
