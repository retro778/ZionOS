// SPDX-License-Identifier: GPL-3.0-or-later
#include "aisettingswidget.h"

#include "apphelpers.h"
#include "core/appsettings.h"
#include "core/i18n.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPointer>
#include <QSignalBlocker>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace zion {

using namespace ai;

AiSettingsWidget::AiSettingsWidget(CredentialStore *store, QList<ChatProvider *> providers,
                                   bool allowSessionKeys, QWidget *parent)
    : QWidget(parent), m_store(store), m_providers(std::move(providers)), m_allowSession(allowSessionKeys)
{
    AppSettings settings;
    auto *outer = new QVBoxLayout(this);

    auto *intro = new QLabel(tr(
        "The assistant is optional. It works only after you add your own API key for a "
        "provider below. When you send a message, the conversation text and anything you "
        "explicitly attach are sent to the selected provider over an encrypted connection. "
        "Nothing is sent in the background, and usage is billed by the provider to your account."));
    intro->setWordWrap(true);
    outer->addWidget(intro);

    auto *defaults = new QFormLayout;
    m_defaultProvider = new QComboBox;
    m_defaultProvider->setObjectName(QStringLiteral("defaultProvider"));
    for (ChatProvider *p : m_providers)
        m_defaultProvider->addItem(p->displayName(), p->id());
    m_defaultProvider->setCurrentIndex(qMax(0, m_defaultProvider->findData(settings.provider())));
    connect(m_defaultProvider, &QComboBox::activated, this, [this] {
        AppSettings().setProvider(m_defaultProvider->currentData().toString());
        emit settingsChanged();
    });
    defaults->addRow(tr("Default provider:"), m_defaultProvider);
    outer->addLayout(defaults);

    for (ChatProvider *p : m_providers)
        outer->addWidget(buildProviderBox(p));

    // --- history ---
    auto *histBox = new QGroupBox(tr("Conversation history"));
    auto *histLay = new QVBoxLayout(histBox);
    auto *histEnabled = new QCheckBox(tr("Save conversations on this computer"));
    histEnabled->setObjectName(QStringLiteral("historyEnabled"));
    histEnabled->setChecked(settings.historyEnabled());
    connect(histEnabled, &QCheckBox::toggled, this, [this](bool on) {
        AppSettings().setHistoryEnabled(on);
        emit settingsChanged();
    });
    histLay->addWidget(histEnabled);
    auto *histInfo = new QLabel(tr("History is stored only in your home folder: %1")
                                    .arg(I18n::ltrIsolate(HistoryStore::defaultDirectory())));
    histInfo->setWordWrap(true);
    histInfo->setTextInteractionFlags(Qt::TextSelectableByMouse);
    histLay->addWidget(histInfo);
    auto *clear = new QPushButton(tr("Delete all saved conversations…"));
    clear->setObjectName(QStringLiteral("deleteAllHistory"));
    connect(clear, &QPushButton::clicked, this, [this] {
        if (QMessageBox::question(this, tr("Delete history"),
                                  tr("Delete all saved conversations from this computer? This cannot be undone."))
            != QMessageBox::Yes)
            return;
        const int n = HistoryStore().removeAll();
        emit historyCleared();
        QMessageBox::information(this, tr("Delete history"), tr("%n conversation(s) deleted.", nullptr, n));
    });
    histLay->addWidget(clear, 0, Qt::AlignLeading);
    outer->addWidget(histBox);

    // --- privacy / advanced ---
    auto *advBox = new QGroupBox(tr("Privacy and limits"));
    auto *adv = new QFormLayout(advBox);
    auto *store30 = new QCheckBox(tr("Allow OpenAI to store responses on its servers (about 30 days)"));
    store30->setObjectName(QStringLiteral("openaiStore"));
    store30->setChecked(settings.openAiStoreResponses());
    connect(store30, &QCheckBox::toggled, this, [this](bool on) {
        AppSettings().setOpenAiStoreResponses(on);
        emit settingsChanged();
    });
    adv->addRow(store30);
    auto *fallback = new QCheckBox(tr("If Anthropic declines a request, let it answer with its fallback model"));
    fallback->setObjectName(QStringLiteral("anthropicFallback"));
    fallback->setToolTip(tr("Uses Anthropic's server-side fallback. The reply is then billed at the fallback model's rates."));
    fallback->setChecked(settings.anthropicFallbackEnabled());
    connect(fallback, &QCheckBox::toggled, this, [this](bool on) {
        AppSettings().setAnthropicFallbackEnabled(on);
        emit settingsChanged();
    });
    adv->addRow(fallback);
    auto *maxTokens = new QSpinBox;
    maxTokens->setRange(256, 128000);
    maxTokens->setSingleStep(1000);
    maxTokens->setValue(settings.maxOutputTokens());
    connect(maxTokens, &QSpinBox::valueChanged, this, [this](int v) {
        AppSettings().setMaxOutputTokens(v);
        emit settingsChanged();
    });
    adv->addRow(tr("Maximum reply length (tokens):"), maxTokens);
    auto *timeout = new QSpinBox;
    timeout->setRange(10, 1800);
    timeout->setSuffix(QLatin1Char(' ') + tr("s"));
    timeout->setValue(settings.requestTimeoutSeconds());
    connect(timeout, &QSpinBox::valueChanged, this, [this](int v) {
        AppSettings().setRequestTimeoutSeconds(v);
        emit settingsChanged();
    });
    adv->addRow(tr("Give up when no data arrives for:"), timeout);
    outer->addWidget(advBox);
    outer->addStretch();

    connect(m_store, &CredentialStore::changed, this, &AiSettingsWidget::refreshStatus);
    for (ChatProvider *p : m_providers)
        refreshStatus(p->id());
}

QWidget *AiSettingsWidget::buildProviderBox(ChatProvider *p)
{
    ProviderUi ui;
    ui.provider = p;
    auto *box = new QGroupBox(p->displayName());
    auto *lay = new QFormLayout(box);

    ui.status = new QLabel;
    ui.status->setObjectName(p->id() + QStringLiteral("Status"));
    ui.status->setWordWrap(true);
    lay->addRow(tr("API key:"), ui.status);

    auto *row = new QHBoxLayout;
    ui.key = new QLineEdit;
    ui.key->setObjectName(p->id() + QStringLiteral("Key"));
    ui.key->setEchoMode(QLineEdit::Password);
    ui.key->setLayoutDirection(Qt::LeftToRight); // keys are always LTR
    ui.key->setPlaceholderText(tr("Paste a new key here"));
    ui.key->setInputMethodHints(Qt::ImhSensitiveData | Qt::ImhNoPredictiveText | Qt::ImhNoAutoUppercase);
    ui.save = new QPushButton(tr("Save key"));
    ui.save->setObjectName(p->id() + QStringLiteral("Save"));
    ui.remove = new QPushButton(tr("Remove key"));
    ui.remove->setObjectName(p->id() + QStringLiteral("Remove"));
    row->addWidget(ui.key, 1);
    row->addWidget(ui.save);
    row->addWidget(ui.remove);
    lay->addRow(QString(), row);

    auto *mrow = new QHBoxLayout;
    ui.model = new QComboBox;
    ui.model->setObjectName(p->id() + QStringLiteral("Model"));
    ui.model->setEditable(true);
    ui.model->setLayoutDirection(Qt::LeftToRight);
    ui.model->setInsertPolicy(QComboBox::NoInsert);
    ui.refresh = new QPushButton(tr("Refresh list"));
    ui.refresh->setToolTip(tr("Ask the provider which models your key can use"));
    mrow->addWidget(ui.model, 1);
    mrow->addWidget(ui.refresh);
    lay->addRow(tr("Model:"), mrow);

    auto *host = new QLabel(tr("Data is sent to: %1").arg(I18n::ltrIsolate(p->endpointHost())));
    lay->addRow(QString(), host);

    const QString id = p->id();
    m_ui.insert(id, ui);
    fillModels(m_ui[id], p->builtinModels());

    connect(ui.save, &QPushButton::clicked, this, [this, id] { saveKey(id); });
    connect(ui.key, &QLineEdit::returnPressed, this, [this, id] { saveKey(id); });
    connect(ui.remove, &QPushButton::clicked, this, [this, id] { removeKey(id); });
    connect(ui.refresh, &QPushButton::clicked, this, [this, id] { refreshModels(id); });
    auto commitModel = [this, id] {
        const ProviderUi &u = m_ui[id];
        const int idx = u.model->findText(u.model->currentText());
        const QString model = idx >= 0 && u.model->itemData(idx).isValid()
                                  ? u.model->itemData(idx).toString() : u.model->currentText().trimmed();
        AppSettings().setModel(id, model);
        emit settingsChanged();
    };
    connect(ui.model, &QComboBox::activated, this, commitModel);
    connect(ui.model->lineEdit(), &QLineEdit::editingFinished, this, commitModel);
    return box;
}

void AiSettingsWidget::fillModels(ProviderUi &ui, const QList<ModelInfo> &models)
{
    const QString id = ui.provider->id();
    QString wanted = AppSettings().model(id);
    if (wanted.isEmpty())
        wanted = ui.provider->defaultModel();
    QSignalBlocker block(ui.model);
    ui.model->clear();
    for (const ModelInfo &m : models)
        ui.model->addItem(m.displayName == m.id ? m.id : QStringLiteral("%1 (%2)").arg(m.displayName, m.id), m.id);
    int idx = ui.model->findData(wanted);
    if (idx < 0) {
        ui.model->addItem(wanted, wanted);
        idx = ui.model->count() - 1;
    }
    ui.model->setCurrentIndex(idx);
}

void AiSettingsWidget::refreshStatus(const QString &providerId)
{
    if (!m_ui.contains(providerId))
        return;
    QLabel *status = m_ui[providerId].status;
    QPushButton *remove = m_ui[providerId].remove;
    if (m_store->sessionLocation(providerId) == KeyLocation::Session) {
        status->setText(tr("A key is held in memory for this session only. It will be forgotten when the assistant closes."));
        remove->setEnabled(true);
        return;
    }
    status->setText(tr("Checking the keyring…"));
    m_store->read(providerId, [status = QPointer<QLabel>(status), remove = QPointer<QPushButton>(remove)](
                                  const CredentialStore::Result &r, const QString &) {
        if (!status)
            return;
        if (r.ok) {
            status->setText(AiSettingsWidget::tr("A key is saved in the system keyring (encrypted)."));
        } else if (r.unavailable) {
            status->setText(AiSettingsWidget::tr("The system keyring is not available or is locked: %1").arg(r.error));
        } else {
            status->setText(AiSettingsWidget::tr("No key saved."));
        }
        if (remove)
            remove->setEnabled(r.ok);
    });
}

void AiSettingsWidget::saveKey(const QString &providerId)
{
    QLineEdit *edit = m_ui[providerId].key;
    const QString key = edit->text().trimmed();
    if (key.isEmpty())
        return;
    edit->clear(); // do not keep the key in the widget
    m_store->writePersistent(providerId, key, [this, providerId, key](const CredentialStore::Result &r, const QString &) {
        if (r.ok) {
            refreshStatus(providerId);
            emit settingsChanged();
            return;
        }
        // No plaintext fallback: explain, and offer memory-only storage.
        QMessageBox box(QMessageBox::Warning, tr("Keyring unavailable"),
                        tr("The key could not be saved because the system keyring (KDE Wallet / Secret Service) "
                           "is not available or is locked."),
                        QMessageBox::NoButton, this);
        QString info = tr("ZionOS never stores API keys in plain text. To save the key permanently, open "
                          "KDE Wallet Manager, enable or unlock the wallet, and try again.");
        if (!r.error.isEmpty())
            info += QStringLiteral("\n\n") + r.error;
        box.setInformativeText(info);
        QPushButton *session = nullptr;
        if (m_allowSession)
            session = box.addButton(tr("Use for this session only"), QMessageBox::AcceptRole);
        QPushButton *wallet = programAvailable(QStringLiteral("kwalletmanager5"))
                                  ? box.addButton(tr("Open Wallet Manager"), QMessageBox::ActionRole) : nullptr;
        box.addButton(QMessageBox::Cancel);
        box.exec();
        if (session && box.clickedButton() == session) {
            m_store->writeSession(providerId, key);
            emit settingsChanged();
        } else if (wallet && box.clickedButton() == wallet) {
            launch(QStringLiteral("kwalletmanager5"));
        }
        refreshStatus(providerId);
    });
}

void AiSettingsWidget::removeKey(const QString &providerId)
{
    if (QMessageBox::question(this, tr("Remove key"),
                              tr("Remove the saved API key for %1 from this computer?")
                                  .arg(m_ui[providerId].provider->displayName()))
        != QMessageBox::Yes)
        return;
    m_store->remove(providerId, [this, providerId](const CredentialStore::Result &r, const QString &) {
        if (!r.ok)
            QMessageBox::warning(this, tr("Remove key"), tr("The key could not be removed from the keyring: %1").arg(r.error));
        refreshStatus(providerId);
        emit settingsChanged();
    });
}

void AiSettingsWidget::refreshModels(const QString &providerId)
{
    ProviderUi &ui = m_ui[providerId];
    ui.refresh->setEnabled(false);
    m_store->read(providerId, [this, providerId](const CredentialStore::Result &r, const QString &key) {
        ProviderUi &u = m_ui[providerId];
        if (!r.ok || key.isEmpty()) {
            u.refresh->setEnabled(true);
            QMessageBox::information(this, tr("Refresh list"), tr("Save an API key first."));
            return;
        }
        ModelListReply *reply = u.provider->listModels(key, this);
        connect(reply, &ModelListReply::finished, this, [this, providerId](const QList<ModelInfo> &models) {
            ProviderUi &x = m_ui[providerId];
            x.refresh->setEnabled(true);
            if (!models.isEmpty())
                fillModels(x, models);
        });
        connect(reply, &ModelListReply::failed, this, [this, providerId](const ProviderError &e) {
            m_ui[providerId].refresh->setEnabled(true);
            QMessageBox::warning(this, tr("Refresh list"),
                                 tr("The model list could not be loaded (%1).").arg(e.message.isEmpty() ? kindName(e.kind) : e.message));
        });
    });
}

} // namespace zion
