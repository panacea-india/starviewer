/*************************************************************************************
  Copyright (C) 2014 Laboratori de Gràfics i Imatge, Universitat de Girona &
  Institut de Diagnòstic per la Imatge.
  Girona 2014. All rights reserved.
  http://starviewer.udg.edu

  This file is part of the Starviewer (Medical Imaging Software) open source project.
  It is subject to the license terms in the LICENSE file found in the top-level
  directory of this distribution and at http://starviewer.udg.edu/license. No part of
  the Starviewer (Medical Imaging Software) open source project, including this file,
  may be copied, modified, propagated, or distributed except according to the
  terms contained in the LICENSE file.
 *************************************************************************************/

#include "pacscredentialstore.h"

#include "logging.h"
#include "pacsdevice.h"
#include "pacsoauthtokenprovider.h"

#include <QHash>
#include <QMutex>
#include <QMutexLocker>
#include <QScopedPointer>

#ifdef Q_OS_WIN
#include <windows.h>
#include <wincred.h>
#endif

namespace udg {

namespace {

#ifdef Q_OS_WIN

// Keeps logins in Windows Credential Manager, where Windows protects them with the login of the current user. They stay on this machine.
class WindowsCredentialBackend : public PacsCredentialBackend
{
public:
    bool save(const QString &key, const PacsLogin &login) override
    {
        std::wstring target = getTarget(key);
        std::wstring username = login.username.toStdWString();
        QByteArray password = login.password.toUtf8();

        CREDENTIALW credential;
        ZeroMemory(&credential, sizeof(credential));
        credential.Type = CRED_TYPE_GENERIC;
        credential.TargetName = const_cast<wchar_t*>(target.c_str());
        credential.UserName = const_cast<wchar_t*>(username.c_str());
        credential.CredentialBlobSize = static_cast<DWORD>(password.size());
        credential.CredentialBlob = reinterpret_cast<LPBYTE>(password.data());
        credential.Persist = CRED_PERSIST_LOCAL_MACHINE;

        bool saved = CredWriteW(&credential, 0);

        if (!saved)
        {
            WARN_LOG(QString("Windows Credential Manager refused the login of PACS %1 with error %2.").arg(key).arg(GetLastError()));
        }

        SecureZeroMemory(password.data(), password.size());

        return saved;
    }

    bool load(const QString &key, PacsLogin &login) override
    {
        PCREDENTIALW credential = nullptr;

        if (!CredReadW(getTarget(key).c_str(), CRED_TYPE_GENERIC, 0, &credential))
        {
            if (GetLastError() != ERROR_NOT_FOUND)
            {
                WARN_LOG(QString("Could not read the login of PACS %1 from Windows Credential Manager, error %2.").arg(key).arg(GetLastError()));
            }

            return false;
        }

        login.username = QString::fromWCharArray(credential->UserName);
        login.password = QString::fromUtf8(reinterpret_cast<const char*>(credential->CredentialBlob), static_cast<int>(credential->CredentialBlobSize));
        CredFree(credential);

        return true;
    }

    void remove(const QString &key) override
    {
        if (!CredDeleteW(getTarget(key).c_str(), CRED_TYPE_GENERIC, 0) && GetLastError() != ERROR_NOT_FOUND)
        {
            WARN_LOG(QString("Could not remove the login of PACS %1 from Windows Credential Manager, error %2.").arg(key).arg(GetLastError()));
        }
    }

private:
    static std::wstring getTarget(const QString &key)
    {
        return QString("Starviewer/PACS/%1").arg(key).toStdWString();
    }
};

#endif // Q_OS_WIN

PacsCredentialBackend *testingBackend = nullptr;

// Logins that can't be kept by the backend. Only valid during this run of the application. Accesses must be protected with the mutex.
QHash<QString, PacsLogin>& getLoginsOfThisRun()
{
    static QHash<QString, PacsLogin> logins;
    return logins;
}

QMutex& getLoginsOfThisRunMutex()
{
    static QMutex mutex;
    return mutex;
}

PacsCredentialBackend* getBackend()
{
    if (testingBackend)
    {
        return testingBackend;
    }

    static QScopedPointer<PacsCredentialBackend> platformBackend(PacsCredentialStore::createPlatformBackend());
    return platformBackend.data();
}

}

bool PacsLogin::isEmpty() const
{
    return username.isEmpty();
}

PacsCredentialBackend::~PacsCredentialBackend()
{
}

bool PacsCredentialStore::save(const QUrl &baseUri, const PacsLogin &login)
{
    QString key = getKey(baseUri);
    PacsCredentialBackend *backend = getBackend();
    bool saved = backend && backend->save(key, login);

    QMutexLocker locker(&getLoginsOfThisRunMutex());

    if (saved)
    {
        getLoginsOfThisRun().remove(key);
    }
    else
    {
        WARN_LOG(QString("The login of PACS %1 will be kept only until the application closes.").arg(key));
        getLoginsOfThisRun().insert(key, login);
    }

    return saved;
}

PacsLogin PacsCredentialStore::load(const QUrl &baseUri)
{
    PacsLogin login;

    if (baseUri.isEmpty())
    {
        return login;
    }

    QString key = getKey(baseUri);

    {
        QMutexLocker locker(&getLoginsOfThisRunMutex());

        if (getLoginsOfThisRun().contains(key))
        {
            return getLoginsOfThisRun().value(key);
        }
    }

    PacsCredentialBackend *backend = getBackend();

    if (backend)
    {
        backend->load(key, login);
    }

    return login;
}

void PacsCredentialStore::remove(const QUrl &baseUri)
{
    if (baseUri.isEmpty())
    {
        return;
    }

    QString key = getKey(baseUri);
    PacsCredentialBackend *backend = getBackend();

    if (backend)
    {
        backend->remove(key);
    }

    QMutexLocker locker(&getLoginsOfThisRunMutex());
    getLoginsOfThisRun().remove(key);
}

bool PacsCredentialStore::isKeptOnlyForThisRun(const QUrl &baseUri)
{
    QMutexLocker locker(&getLoginsOfThisRunMutex());
    return getLoginsOfThisRun().contains(getKey(baseUri));
}

PacsLogin PacsCredentialStore::getLoginFor(const PacsDevice &pacs)
{
    if (pacs.isLoginSpecified())
    {
        return PacsLogin{pacs.getUsername(), pacs.getPassword()};
    }

    return load(pacs.getBaseUri());
}

QByteArray PacsCredentialStore::getAuthorizationHeader(const PacsDevice &pacs, const QUrl &url, QString *errorText)
{
    const QUrl &baseUri = pacs.getBaseUri();

    if (url.scheme() != baseUri.scheme() || url.host() != baseUri.host() || url.port() != baseUri.port())
    {
        return QByteArray();
    }

    if (pacs.isOAuthEnabled())
    {
        return PacsOAuthTokenProvider::getAuthorizationHeader(pacs, errorText);
    }

    PacsLogin login = getLoginFor(pacs);

    if (login.isEmpty())
    {
        return QByteArray();
    }

    return "Basic " + QString("%1:%2").arg(login.username, login.password).toUtf8().toBase64();
}

QString PacsCredentialStore::getKey(const QUrl &baseUri)
{
    return baseUri.adjusted(QUrl::RemoveUserInfo | QUrl::StripTrailingSlash).toString(QUrl::FullyEncoded);
}

PacsCredentialBackend* PacsCredentialStore::createPlatformBackend()
{
#ifdef Q_OS_WIN
    return new WindowsCredentialBackend();
#else
    return nullptr;
#endif
}

void PacsCredentialStore::setStaticTestingBackend(PacsCredentialBackend *backend)
{
    testingBackend = backend;

    QMutexLocker locker(&getLoginsOfThisRunMutex());
    getLoginsOfThisRun().clear();
}

} // namespace udg
