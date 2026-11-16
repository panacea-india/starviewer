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

#include "pacsoauthtokenprovider.h"

#include "logging.h"
#include "pacscredentialstore.h"
#include "pacsdevice.h"

#include <QDateTime>
#include <QEventLoop>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutex>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSslCertificate>
#include <QSslConfiguration>
#include <QTimer>
#include <QUrlQuery>

namespace udg {

namespace {

struct CachedToken
{
    QByteArray accessToken;
    QDateTime validUntil;
};

// How long the token endpoint has to answer
const int TokenRequestTimeoutMs = 15000;
// Lifetime assumed when the token endpoint doesn't say, which RFC 6749 allows
const int DefaultLifetimeSeconds = 300;
// A token is no longer used this long before it expires, or half of its lifetime if that is shorter
const int ExpiryMarginMs = 30000;

QMutex& getCacheMutex()
{
    static QMutex mutex;
    return mutex;
}

QHash<QString, CachedToken>& getCache()
{
    static QHash<QString, CachedToken> cache;
    return cache;
}

// Only one thread at a time asks the token endpoint, so that threads that need a token at the same time share the one that is obtained.
QMutex& getFetchMutex()
{
    static QMutex mutex;
    return mutex;
}

QString getCacheKey(const PacsDevice &pacs)
{
    return pacs.getOAuthTokenUri().toString(QUrl::FullyEncoded) + "|" + pacs.getOAuthClientId();
}

QByteArray getCachedHeader(const QString &key)
{
    QMutexLocker locker(&getCacheMutex());
    auto it = getCache().constFind(key);

    if (it != getCache().constEnd() && it->validUntil > QDateTime::currentDateTimeUtc())
    {
        return "Bearer " + it->accessToken;
    }

    return QByteArray();
}

QString getClientSecret(const PacsDevice &pacs)
{
    if (!pacs.getOAuthClientSecret().isEmpty())
    {
        return pacs.getOAuthClientSecret();
    }

    return PacsCredentialStore::load(pacs.getOAuthTokenUri()).password;
}

// Asks the token endpoint for a token and keeps it. Returns the Authorization header, or an empty array and the reason in errorText.
QByteArray fetchToken(const PacsDevice &pacs, const QString &key, QString &errorText)
{
    QNetworkRequest request(pacs.getOAuthTokenUri());
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    // RFC 6749 section 2.3.1: the client ID and secret are form-encoded before they go in the Basic header
    QByteArray clientAuthentication = QUrl::toPercentEncoding(pacs.getOAuthClientId()) + ":" + QUrl::toPercentEncoding(getClientSecret(pacs));
    request.setRawHeader("Authorization", "Basic " + clientAuthentication.toBase64());

    if (!pacs.getCaCertificateFile().isEmpty())
    {
        QList<QSslCertificate> certificates = QSslCertificate::fromPath(pacs.getCaCertificateFile(), QSsl::Pem, QSslCertificate::PatternSyntax::FixedString);
        QSslConfiguration sslConfiguration = QSslConfiguration::defaultConfiguration();
        sslConfiguration.setCaCertificates(QSslConfiguration::systemCaCertificates() + certificates);
        request.setSslConfiguration(sslConfiguration);
    }

    QUrlQuery body;
    body.addQueryItem("grant_type", "client_credentials");

    QNetworkAccessManager networkAccessManager;
    QNetworkReply *reply = networkAccessManager.post(request, body.toString(QUrl::FullyEncoded).toUtf8());
    QEventLoop eventLoop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(reply, &QNetworkReply::finished, &eventLoop, &QEventLoop::quit);
    QObject::connect(&timer, &QTimer::timeout, &eventLoop, &QEventLoop::quit);
    timer.start(TokenRequestTimeoutMs);
    eventLoop.exec();

    QByteArray header;

    if (!reply->isFinished())
    {
        reply->abort();
        errorText = QObject::tr("The token server did not answer in time.");
    }
    else if (reply->error() != QNetworkReply::NoError)
    {
        int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        if (status == 400 || status == 401)
        {
            errorText = QObject::tr("The token server refused the client ID and secret.");
        }
        else
        {
            errorText = QObject::tr("The token server could not be reached: %1").arg(reply->errorString());
        }
    }
    else
    {
        QJsonObject answer = QJsonDocument::fromJson(reply->readAll()).object();
        QByteArray accessToken = answer.value("access_token").toString().toUtf8();

        if (accessToken.isEmpty() || answer.value("token_type").toString().compare("Bearer", Qt::CaseInsensitive) != 0)
        {
            errorText = QObject::tr("The token server did not give a bearer token.");
        }
        else
        {
            int lifetimeSeconds = answer.contains("expires_in") ? answer.value("expires_in").toInt(DefaultLifetimeSeconds) : DefaultLifetimeSeconds;
            qint64 marginMs = qMin<qint64>(ExpiryMarginMs, lifetimeSeconds * 500LL);
            CachedToken cached{accessToken, QDateTime::currentDateTimeUtc().addMSecs(lifetimeSeconds * 1000LL - marginMs)};

            {
                QMutexLocker locker(&getCacheMutex());
                getCache().insert(key, cached);
            }

            INFO_LOG(QString("Got an OAuth access token from %1 that is valid for %2 s.").arg(pacs.getOAuthTokenUri().toString(), QString::number(lifetimeSeconds)));
            header = "Bearer " + accessToken;
        }
    }

    reply->deleteLater();
    return header;
}

}

QByteArray PacsOAuthTokenProvider::getAuthorizationHeader(const PacsDevice &pacs, QString *errorText)
{
    QString key = getCacheKey(pacs);
    QByteArray header = getCachedHeader(key);

    if (!header.isEmpty())
    {
        return header;
    }

    QMutexLocker fetchLocker(&getFetchMutex());

    // Another thread may have got one while this one waited
    header = getCachedHeader(key);

    if (!header.isEmpty())
    {
        return header;
    }

    QString error;
    header = fetchToken(pacs, key, error);

    if (header.isEmpty())
    {
        ERROR_LOG(QString("Could not get an OAuth access token from %1: %2").arg(pacs.getOAuthTokenUri().toString(), error));

        if (errorText)
        {
            *errorText = error;
        }
    }

    return header;
}

void PacsOAuthTokenProvider::forget(const QByteArray &authorizationHeader)
{
    if (!authorizationHeader.startsWith("Bearer "))
    {
        return;
    }

    QByteArray accessToken = authorizationHeader.mid(7);
    QMutexLocker locker(&getCacheMutex());

    for (auto it = getCache().begin(); it != getCache().end();)
    {
        if (it->accessToken == accessToken)
        {
            it = getCache().erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void PacsOAuthTokenProvider::clear()
{
    QMutexLocker locker(&getCacheMutex());
    getCache().clear();
}

}
