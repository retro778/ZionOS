// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "provider.h"

namespace zion::ai {

/// Adapter for the Anthropic Messages API (POST /v1/messages, SSE streaming).
/// Wire format verified against the official API reference on 2026-11-10;
/// see docs/AI-PROVIDERS.md.
class AnthropicProvider : public ChatProvider
{
    Q_OBJECT
public:
    static constexpr const char *ApiVersion = "2026-10-21";
    static constexpr const char *FallbackBeta = "server-side-fallback-2026-07-01";

    explicit AnthropicProvider(QNetworkAccessManager *nam, QObject *parent = nullptr,
                               const QUrl &baseUrl = QUrl(QStringLiteral("https://api.anthropic.com")));

    QString id() const override { return QStringLiteral("anthropic"); }
    QString displayName() const override { return QStringLiteral("Anthropic (Claude)"); }
    QUrl baseUrl() const override { return m_base; }
    QList<ModelInfo> builtinModels() const override;
    QString defaultModel() const override { return QStringLiteral("claude-opus-5-5"); }

    ChatStream *startChat(const ChatRequest &request, const QString &apiKey,
                          QObject *parent = nullptr) override;
    ModelListReply *listModels(const QString &apiKey, QObject *parent = nullptr) override;
    QJsonObject buildRequestBody(const ChatRequest &request) const override;

    /// Models for which the server-side refusal fallback is requested.
    static bool supportsFallback(const QString &model);

private:
    QNetworkRequest makeRequest(const QString &path, const QString &apiKey) const;
    QUrl m_base;
};

} // namespace zion::ai
