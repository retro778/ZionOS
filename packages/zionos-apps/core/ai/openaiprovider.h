// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "provider.h"

namespace zion::ai {

/// Adapter for the OpenAI Responses API (POST /v1/responses, SSE streaming).
/// Wire format verified against OpenAI's published OpenAPI specification
/// (openai/openai-openapi, spec version 2.3.0) on 2026-11-10;
/// see docs/AI-PROVIDERS.md.
class OpenAiProvider : public ChatProvider
{
    Q_OBJECT
public:
    explicit OpenAiProvider(QNetworkAccessManager *nam, QObject *parent = nullptr,
                            const QUrl &baseUrl = QUrl(QStringLiteral("https://api.openai.com/v1")));

    QString id() const override { return QStringLiteral("openai"); }
    QString displayName() const override { return QStringLiteral("OpenAI"); }
    QUrl baseUrl() const override { return m_base; }
    QList<ModelInfo> builtinModels() const override;
    QString defaultModel() const override { return QStringLiteral("gpt-6-astra"); }

    ChatStream *startChat(const ChatRequest &request, const QString &apiKey,
                          QObject *parent = nullptr) override;
    ModelListReply *listModels(const QString &apiKey, QObject *parent = nullptr) override;
    QJsonObject buildRequestBody(const ChatRequest &request) const override;

    /// Whether a model id from GET /v1/models is a text chat model worth
    /// offering in the assistant (filters out image/audio/embedding models).
    static bool isChatModel(const QString &id);

private:
    QNetworkRequest makeRequest(const QString &path, const QString &apiKey) const;
    QUrl m_base;
};

} // namespace zion::ai
