// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QHash>
#include <QObject>
#include <QString>
#include <functional>

namespace zion::ai {

/// Where an API key currently lives.
enum class KeyLocation {
    None,     ///< no key configured
    Keyring,  ///< desktop Secret Service / KWallet (persistent, encrypted)
    Session   ///< memory of this process only; gone when the app closes
};

/// Stores provider API keys.
///
/// Persistent storage goes exclusively through the desktop keyring
/// (freedesktop Secret Service or KWallet, via QtKeychain). There is no
/// plaintext fallback: when the keyring is unavailable or locked the write
/// fails with an explanation and the caller may offer session-only storage,
/// which keeps the key in process memory.
class CredentialStore : public QObject
{
    Q_OBJECT
public:
    struct Result
    {
        bool ok = false;
        bool notFound = false;      ///< read: no entry (not an error)
        bool unavailable = false;   ///< keyring missing, locked or access denied
        QString error;              ///< backend message, safe to show
    };
    using Callback = std::function<void(const Result &, const QString &secret)>;

    explicit CredentialStore(QObject *parent = nullptr);
    ~CredentialStore() override;

    /// Looks up the key: session memory first, then the keyring.
    virtual void read(const QString &providerId, Callback cb);
    /// Saves to the keyring. On failure nothing is stored anywhere.
    virtual void writePersistent(const QString &providerId, const QString &secret, Callback cb);
    /// Keeps the key in memory for this process only.
    void writeSession(const QString &providerId, const QString &secret);
    /// Removes the key from session memory and from the keyring.
    virtual void remove(const QString &providerId, Callback cb);

    KeyLocation sessionLocation(const QString &providerId) const;

    static QString serviceName() { return QStringLiteral("zionos-assistant"); }
    static QString entryName(const QString &providerId) { return providerId + QStringLiteral("-api-key"); }

signals:
    void changed(const QString &providerId);

protected:
    QHash<QString, QString> m_session;
};

/// In-memory implementation for tests and for systems without any keyring.
class MemoryCredentialStore : public CredentialStore
{
    Q_OBJECT
public:
    using CredentialStore::CredentialStore;
    void read(const QString &providerId, Callback cb) override;
    void writePersistent(const QString &providerId, const QString &secret, Callback cb) override;
    void remove(const QString &providerId, Callback cb) override;
    bool simulateUnavailable = false;
    QHash<QString, QString> persistent;
};

} // namespace zion::ai
