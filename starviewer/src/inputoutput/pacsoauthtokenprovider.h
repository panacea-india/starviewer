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

#ifndef UDG_PACSOAUTHTOKENPROVIDER_H
#define UDG_PACSOAUTHTOKENPROVIDER_H

#include <QByteArray>
#include <QString>

namespace udg {

class PacsDevice;

/**
 * @brief The PacsOAuthTokenProvider class gets the OAuth 2 access tokens that DICOMweb PACS ask for.
 *
 * It uses the client credentials grant: the application identifies itself to the token endpoint of the PACS with a client ID and a client secret, and gets
 * a bearer token that it sends with each request. Tokens are kept in memory until shortly before they expire, so that requests don't ask for one each time.
 * The client secret is the one given to the PacsDevice or, if there is none, the one kept in PacsCredentialStore under the token endpoint.
 *
 * Getting a token blocks the calling thread, with a local event loop, for as long as the token endpoint takes to answer, up to a time limit.
 * All methods can be called from any thread. Tokens and secrets are never written to the log.
 */
class PacsOAuthTokenProvider
{
public:
    /// Returns the value of the Authorization header ("Bearer " and a token) for the given PACS, which must use OAuth. It's empty if no token could be
    /// obtained, and then the given error text, if not null, is filled with the reason.
    static QByteArray getAuthorizationHeader(const PacsDevice &pacs, QString *errorText = nullptr);

    /// Forgets the token of the given Authorization header, so that the next request gets a new one. To be called when a PACS refuses a token, which can
    /// happen before the token expires if the PACS revoked it.
    static void forget(const QByteArray &authorizationHeader);

    /// Forgets all tokens.
    static void clear();
};

}

#endif
