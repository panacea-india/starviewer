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

#include "wadorequest.h"

#include "logging.h"
#include "pacscredentialstore.h"
#include "pacsoauthtokenprovider.h"

#include <QApplication>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSslCertificate>
#include <QSslError>

namespace udg {

namespace {

// Dynamic property of a QNetworkReply that holds its TLS errors.
const char *SslErrorsProperty = "starviewerSslErrors";

}

WadoRequest::WadoRequest(const PacsDevice &pacs, QObject *parent)
    : QObject(parent), m_networkAccessManager(nullptr), m_reply(nullptr), m_status(Status::Clean), m_pacs(pacs)
{
    if (!m_pacs.getCaCertificateFile().isEmpty())
    {
        QList<QSslCertificate> certificates = QSslCertificate::fromPath(m_pacs.getCaCertificateFile(), QSsl::Pem, QSslCertificate::PatternSyntax::FixedString);

        if (certificates.isEmpty())
        {
            ERROR_LOG("No certificates could be read from the CA certificate file " + m_pacs.getCaCertificateFile());
        }

        // Added to the system's, because setting a list replaces the loading of system certificates when they are needed.
        m_sslConfiguration = QSslConfiguration::defaultConfiguration();
        m_sslConfiguration.setCaCertificates(QSslConfiguration::systemCaCertificates() + certificates);
    }
}

WadoRequest::~WadoRequest()
{
    if (m_reply && QApplication::activeWindow())    // if there is no active window, m_reply may have been deleted in another thread
    {
        m_reply->deleteLater();
    }
}

const PacsDevice& WadoRequest::getPacsDevice() const
{
    return m_pacs;
}

QNetworkRequest WadoRequest::createNetworkRequest(const QUrl &url) const
{
    QNetworkRequest request(url);
    QString authorizationError;
    QByteArray authorization = PacsCredentialStore::getAuthorizationHeader(m_pacs, url, &authorizationError);

    if (!authorization.isEmpty())
    {
        request.setRawHeader("Authorization", authorization);
    }
    else if (!authorizationError.isEmpty())
    {
        // So that getErrorString() can say why there was no token, instead of that the PACS wants a login
        request.setAttribute(QNetworkRequest::User, authorizationError);
    }

    if (!m_sslConfiguration.isNull())
    {
        request.setSslConfiguration(m_sslConfiguration);
    }

    return request;
}

void WadoRequest::keepSslErrors(QNetworkReply *reply)
{
    connect(reply, &QNetworkReply::sslErrors, reply, [reply](const QList<QSslError> &errors) {
        QStringList descriptions;

        for (const QSslError &error : errors)
        {
            descriptions << error.errorString();
        }

        reply->setProperty(SslErrorsProperty, descriptions.join("; "));
    });
}

QString WadoRequest::getErrorString(const QNetworkReply *reply)
{
    // Qt says "Host requires authentication" both when no login was sent and when it was refused
    if (reply->error() == QNetworkReply::AuthenticationRequiredError)
    {
        QByteArray authorization = reply->request().rawHeader("Authorization");

        if (authorization.startsWith("Bearer "))
        {
            // The token may have been revoked before it expired. The next request gets a new one.
            PacsOAuthTokenProvider::forget(authorization);
            return tr("The PACS refused the access token. Try again, and check the OAuth settings in the PACS settings if it persists.");
        }

        if (!authorization.isEmpty())
        {
            return tr("The PACS refused the login. Check the user name and password in the PACS settings.");
        }

        QString tokenError = reply->request().attribute(QNetworkRequest::User).toString();

        if (!tokenError.isEmpty())
        {
            return tr("No access token could be got for the PACS. %1").arg(tokenError);
        }

        return tr("The PACS requires a login. Enter a user name and password in the PACS settings.");
    }

    QString sslErrors = reply->property(SslErrorsProperty).toString();

    if (sslErrors.isEmpty())
    {
        return reply->errorString();
    }

    return QString("%1 (%2)").arg(reply->errorString(), sslErrors);
}

// This is run in the WADO thread.
void WadoRequest::start(QNetworkAccessManager *networkAccessManager)
{
    Q_ASSERT(networkAccessManager);
    m_networkAccessManager = networkAccessManager;
    startInternal();
}

void WadoRequest::cancel()
{
    if (m_reply && QApplication::activeWindow())    // if there is no active window, m_reply may have been deleted in another thread
    {
        QMetaObject::invokeMethod(m_reply, &QNetworkReply::abort);  // indirect call because m_reply lives in another thread
    }
}

WadoRequest::Status WadoRequest::getStatus() const
{
    return m_status;
}

const QString& WadoRequest::getErrorsDescription() const
{
    return m_errorsDescription;
}

} // namespace udg
