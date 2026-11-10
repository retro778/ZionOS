// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "core/ai/credentialstore.h"
#include "core/ai/history.h"
#include "core/ai/provider.h"

#include <QHash>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QNetworkAccessManager;
class QPushButton;
class QSpinBox;

namespace zion {

/// Assistant settings: provider, models, API keys, history and privacy.
/// Embedded in ZionOS Settings and shown as a dialog by the assistant.
///
/// @p allowSessionKeys enables the "keep for this session only" choice when
/// the keyring is unavailable. It only makes sense inside the assistant
/// process, because a session key lives in that process's memory.
class AiSettingsWidget : public QWidget
{
    Q_OBJECT
public:
    AiSettingsWidget(ai::CredentialStore *store, QList<ai::ChatProvider *> providers,
                     bool allowSessionKeys, QWidget *parent = nullptr);

signals:
    void settingsChanged();
    void historyCleared();

private:
    struct ProviderUi
    {
        ai::ChatProvider *provider = nullptr;
        QLabel *status = nullptr;
        QLineEdit *key = nullptr;
        QPushButton *save = nullptr;
        QPushButton *remove = nullptr;
        QComboBox *model = nullptr;
        QPushButton *refresh = nullptr;
    };
    QWidget *buildProviderBox(ai::ChatProvider *p);
    void refreshStatus(const QString &providerId);
    void saveKey(const QString &providerId);
    void removeKey(const QString &providerId);
    void refreshModels(const QString &providerId);
    void fillModels(ProviderUi &ui, const QList<ai::ModelInfo> &models);

    ai::CredentialStore *m_store;
    QList<ai::ChatProvider *> m_providers;
    bool m_allowSession;
    QHash<QString, ProviderUi> m_ui;
    QComboBox *m_defaultProvider = nullptr;
};

} // namespace zion
