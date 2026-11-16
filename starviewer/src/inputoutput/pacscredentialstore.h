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

#ifndef UDG_PACSCREDENTIALSTORE_H
#define UDG_PACSCREDENTIALSTORE_H

#include <QByteArray>
#include <QString>
#include <QUrl>

namespace udg {

class PacsDevice;

/**
 * @brief The PacsLogin struct holds the user name and password of a DICOMweb PACS.
 */
struct PacsLogin
{
    QString username;
    QString password;

    /// Returns true if there is no user name, which means that no login is used.
    bool isEmpty() const;
};

/**
 * @brief The PacsCredentialBackend class is the interface of a place where logins are kept across runs of the application.
 */
class PacsCredentialBackend
{
public:
    virtual ~PacsCredentialBackend();

    /// Keeps the given login under the given key, replacing any previous one. Returns true if successful and false otherwise.
    virtual bool save(const QString &key, const PacsLogin &login) = 0;
    /// Fills the given login with the one under the given key. Returns true if there is one and false otherwise.
    virtual bool load(const QString &key, PacsLogin &login) = 0;
    /// Removes the login under the given key, if any.
    virtual void remove(const QString &key) = 0;
};

/**
 * @brief The PacsCredentialStore class keeps the logins of DICOMweb PACS outside the settings and the database.
 *
 * Logins are kept by base URI, in the secure store of the operating system. Only Windows Credential Manager is implemented. When there is no such store, or
 * it refuses a login, the login is kept in memory until the application closes and is never written in plain text anywhere else.
 *
 * All methods can be called from any thread.
 */
class PacsCredentialStore
{
public:
    /// Keeps the login for the given base URI. Returns true if it will outlast this run of the application and false if it will not.
    static bool save(const QUrl &baseUri, const PacsLogin &login);
    /// Returns the login for the given base URI, which is empty if there is none.
    static PacsLogin load(const QUrl &baseUri);
    /// Removes the login for the given base URI, if any.
    static void remove(const QUrl &baseUri);
    /// Returns true if the login for the given base URI will be lost when the application closes.
    static bool isKeptOnlyForThisRun(const QUrl &baseUri);

    /// Returns the login to use with the given PACS: its own if it has one specified and the stored one for its base URI otherwise.
    static PacsLogin getLoginFor(const PacsDevice &pacs);

    /// Returns the value of the Authorization header for a request to the given URL of the given PACS: a bearer token if the PACS uses OAuth and Basic with
    /// its login otherwise. It's empty if the PACS has no login or if the URL is not on the host, port and scheme of the base URI of the PACS, so that a login
    /// never reaches another server. If no OAuth token could be got and the given error text is not null, it's filled with the reason.
    static QByteArray getAuthorizationHeader(const PacsDevice &pacs, const QUrl &url, QString *errorText = nullptr);

    /// Returns the key under which the login for the given base URI is kept. It doesn't depend on a trailing slash and never contains a login.
    static QString getKey(const QUrl &baseUri);

    /// Creates the backend for the secure store of the operating system. Returns null if there is none on this platform.
    static PacsCredentialBackend* createPlatformBackend();
    /// Makes this class use the given backend instead of the secure store of the operating system, and forgets the logins kept in memory. For use in tests. Set
    /// it to null to go back to normal behaviour.
    static void setStaticTestingBackend(PacsCredentialBackend *backend);
};

} // namespace udg

#endif // UDG_PACSCREDENTIALSTORE_H
