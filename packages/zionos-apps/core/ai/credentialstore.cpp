// SPDX-License-Identifier: GPL-3.0-or-later
#include "credentialstore.h"

#include "core/redact.h"

#include <qt6keychain/keychain.h>

namespace zion::ai {

CredentialStore::CredentialStore(QObject *parent) : QObject(parent) {}

CredentialStore::~CredentialStore()
{
    // Best effort: do not leave key material in freed memory.
    for (auto it = m_session.begin(); it != m_session.end(); ++it)
        it.value().fill(QChar(0));
}

static CredentialStore::Result toResult(QKeychain::Job *job)
{
    CredentialStore::Result r;
    switch (job->error()) {
    case QKeychain::NoError:
        r.ok = true;
        break;
    case QKeychain::EntryNotFound:
        r.notFound = true;
        break;
    case QKeychain::NoBackendAvailable:
    case QKeychain::AccessDenied:
    case QKeychain::AccessDeniedByUser:
    case QKeychain::CouldNotDeleteEntry:
    case QKeychain::NotImplemented:
    case QKeychain::OtherError:
        r.unavailable = true;
        r.error = Redactor::redact(job->errorString());
        break;
    }
    return r;
}

template <typename JobT>
static JobT *makeJob(const QString &providerId, QObject *parent)
{
    auto *job = new JobT(CredentialStore::serviceName(), parent);
    job->setAutoDelete(true);
    // Never let QtKeychain fall back to an unencrypted QSettings file.
    job->setInsecureFallback(false);
    job->setKey(CredentialStore::entryName(providerId));
    return job;
}

void CredentialStore::read(const QString &providerId, Callback cb)
{
    if (m_session.contains(providerId)) {
        Result r;
        r.ok = true;
        cb(r, m_session.value(providerId));
        return;
    }
    auto *job = makeJob<QKeychain::ReadPasswordJob>(providerId, this);
    connect(job, &QKeychain::Job::finished, this, [job, cb](QKeychain::Job *) {
        const Result r = toResult(job);
        const QString secret = r.ok ? job->textData() : QString();
        if (r.ok)
            Redactor::registerSecret(secret);
        cb(r, secret);
    });
    job->start();
}

void CredentialStore::writePersistent(const QString &providerId, const QString &secret, Callback cb)
{
    Redactor::registerSecret(secret);
    auto *job = makeJob<QKeychain::WritePasswordJob>(providerId, this);
    job->setTextData(secret);
    connect(job, &QKeychain::Job::finished, this, [this, job, providerId, cb](QKeychain::Job *) {
        const Result r = toResult(job);
        if (r.ok) {
            m_session.remove(providerId);
            emit changed(providerId);
        }
        cb(r, {});
    });
    job->start();
}

void CredentialStore::writeSession(const QString &providerId, const QString &secret)
{
    Redactor::registerSecret(secret);
    m_session.insert(providerId, secret);
    emit changed(providerId);
}

void CredentialStore::remove(const QString &providerId, Callback cb)
{
    if (m_session.contains(providerId)) {
        m_session[providerId].fill(QChar(0));
        m_session.remove(providerId);
    }
    auto *job = makeJob<QKeychain::DeletePasswordJob>(providerId, this);
    connect(job, &QKeychain::Job::finished, this, [this, job, providerId, cb](QKeychain::Job *) {
        Result r = toResult(job);
        if (r.notFound) { // nothing stored is a successful removal
            r.ok = true;
        }
        emit changed(providerId);
        cb(r, {});
    });
    job->start();
}

KeyLocation CredentialStore::sessionLocation(const QString &providerId) const
{
    return m_session.contains(providerId) ? KeyLocation::Session : KeyLocation::None;
}

// ---- MemoryCredentialStore -------------------------------------------------

void MemoryCredentialStore::read(const QString &providerId, Callback cb)
{
    Result r;
    if (m_session.contains(providerId)) {
        r.ok = true;
        cb(r, m_session.value(providerId));
    } else if (simulateUnavailable) {
        r.unavailable = true;
        r.error = QStringLiteral("keyring unavailable");
        cb(r, {});
    } else if (persistent.contains(providerId)) {
        r.ok = true;
        cb(r, persistent.value(providerId));
    } else {
        r.notFound = true;
        cb(r, {});
    }
}

void MemoryCredentialStore::writePersistent(const QString &providerId, const QString &secret, Callback cb)
{
    Result r;
    if (simulateUnavailable) {
        r.unavailable = true;
        r.error = QStringLiteral("keyring unavailable");
    } else {
        persistent.insert(providerId, secret);
        m_session.remove(providerId);
        r.ok = true;
        emit changed(providerId);
    }
    cb(r, {});
}

void MemoryCredentialStore::remove(const QString &providerId, Callback cb)
{
    m_session.remove(providerId);
    persistent.remove(providerId);
    Result r;
    r.ok = true;
    emit changed(providerId);
    cb(r, {});
}

} // namespace zion::ai
