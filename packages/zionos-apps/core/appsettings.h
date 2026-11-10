// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QSettings>
#include <QString>

namespace zion {

/// Per-user, non-secret preferences shared by the ZionOS applications
/// (~/.config/ZionOS/zionos.conf). API keys are never stored here.
class AppSettings
{
public:
    AppSettings();

    QString language() const;               ///< "" = follow system
    void setLanguage(const QString &lang);

    bool showWelcomeAtLogin() const;
    void setShowWelcomeAtLogin(bool on);

    // --- assistant ---
    QString provider() const;               ///< "anthropic" | "openai"
    void setProvider(const QString &id);
    QString model(const QString &providerId) const;      ///< "" = provider default
    void setModel(const QString &providerId, const QString &model);
    bool historyEnabled() const;
    void setHistoryEnabled(bool on);
    int maxOutputTokens() const;
    void setMaxOutputTokens(int n);
    int requestTimeoutSeconds() const;      ///< idle timeout while waiting for data
    void setRequestTimeoutSeconds(int s);
    bool anthropicFallbackEnabled() const;  ///< server-side fallback on refusal
    void setAnthropicFallbackEnabled(bool on);
    bool openAiStoreResponses() const;      ///< OpenAI `store` flag (default false)
    void setOpenAiStoreResponses(bool on);

    void sync();
    QString fileName() const;

private:
    QSettings m_s;
};

} // namespace zion
