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

#include "pacsconnection.h"

#include <dimse.h>
#include <ofcond.h>
#include <assoc.h>
#include <tlslayer.h>
#ifdef WITH_OPENSSL
#include <openssl/ssl.h>
#include <openssl/x509v3.h>
#endif
#include <QFile>
#include <QHostAddress>
#include <QHostInfo>
#include <QScopedPointer>
#include <QStringList>

#include "logging.h"
#include "inputoutputsettings.h"

namespace udg {

PACSConnection::PACSConnection(PacsDevice pacsDevice)
{
    // Variable global de dcmtk per evitar el dnslookup, que dona problemes de lentitu a windows.
    // TODO: Al fer refactoring aquesta inicialització hauria de quedar en un lloc central de configuracions per dcmtk.
    dcmDisableGethostbyaddr.set(OFTrue);

    m_pacs = pacsDevice;
    m_associationNetwork = NULL;
    m_associationParameters = NULL;
    m_dicomAssociation = NULL;
    m_associationAborted = false;
}

PACSConnection::~PACSConnection()
{
}

bool PACSConnection::isDicomTlsAvailable()
{
#ifdef WITH_OPENSSL
    return true;
#else
    return false;
#endif
}

bool PACSConnection::setUpDicomTls()
{
#ifdef WITH_OPENSSL
    if (m_pacs.getCaCertificateFile().isEmpty())
    {
        ERROR_LOG("DICOM TLS needs a CA certificate file. AE Title: " + m_pacs.getAETitle());
        return false;
    }

    QScopedPointer<DcmTLSTransportLayer> tlsLayer(new DcmTLSTransportLayer(NET_REQUESTOR, nullptr, OFTrue));

    if (!*tlsLayer)
    {
        ERROR_LOG("The DICOM TLS layer could not be created. AE Title: " + m_pacs.getAETitle());
        return false;
    }

    // Without this, OpenSSL would ask for the password of an encrypted private key on a console that nobody sees.
    tlsLayer->setPrivateKeyPasswd("");

    OFCondition condition = tlsLayer->setTLSProfile(TSP_Profile_BCP195);

    if (condition.good())
    {
        condition = tlsLayer->activateCipherSuites();
    }
    if (condition.good())
    {
        condition = tlsLayer->addTrustedCertificateFile(QFile::encodeName(m_pacs.getCaCertificateFile()).constData(), DCF_Filetype_PEM);
    }
    if (condition.good() && !m_pacs.getClientCertificateFile().isEmpty())
    {
        condition = tlsLayer->setPrivateKeyFile(QFile::encodeName(m_pacs.getClientPrivateKeyFile()).constData(), DCF_Filetype_PEM);

        if (condition.good())
        {
            condition = tlsLayer->setCertificateFile(QFile::encodeName(m_pacs.getClientCertificateFile()).constData(), DCF_Filetype_PEM);
        }
        if (condition.good() && !tlsLayer->checkPrivateKeyMatchesCertificate())
        {
            ERROR_LOG("The client private key does not match its certificate. AE Title: " + m_pacs.getAETitle());
            return false;
        }
    }
    if (condition.bad())
    {
        ERROR_LOG("DICOM TLS could not be configured. AE Title: " + m_pacs.getAETitle() + ". Error description: " + QString(condition.text()));
        return false;
    }

    tlsLayer->setCertificateVerification(DCV_requireCertificate);

    // DCMTK only checks that a trusted CA signed the certificate. This also makes OpenSSL check that the certificate is for this PACS.
    X509_VERIFY_PARAM *verifyParameters = SSL_CTX_get0_param(tlsLayer->getNativeHandle());
    QByteArray address = m_pacs.getAddress().toUtf8();
    int isSet = QHostAddress().setAddress(m_pacs.getAddress()) ? X509_VERIFY_PARAM_set1_ip_asc(verifyParameters, address.constData())
                                                               : X509_VERIFY_PARAM_set1_host(verifyParameters, address.constData(), 0);
    if (isSet != 1)
    {
        ERROR_LOG("The name of the PACS could not be set for the check of its certificate. AE Title: " + m_pacs.getAETitle());
        return false;
    }

    condition = ASC_setTransportLayer(m_associationNetwork, tlsLayer.data(), 1);

    if (condition.bad())
    {
        ERROR_LOG("The DICOM TLS layer could not be set. AE Title: " + m_pacs.getAETitle() + ". Error description: " + QString(condition.text()));
        return false;
    }

    tlsLayer.take();    // the association network owns it now
    return true;
#else
    ERROR_LOG("This build of Starviewer cannot use DICOM TLS. AE Title: " + m_pacs.getAETitle());
    return false;
#endif
}

OFCondition PACSConnection::configureGet()
{
    const char *transferSyntaxes[] = { NULL, NULL, NULL };

    getTransferSyntaxForFindOrMoveConnection(transferSyntaxes);

    OFCondition condition = ASC_addPresentationContext(m_associationParameters, 1, UID_GETStudyRootQueryRetrieveInformationModel, transferSyntaxes,
                                                       DIM_OF(transferSyntaxes));

    // Uncompressed first, and lossy last, because a PACS may just take the first one and convert the images to it. The lossy ones are needed for images
    // that are already stored that way.
    QList<const char*> storageTransferSyntaxes;
    storageTransferSyntaxes << UID_LittleEndianExplicitTransferSyntax << UID_LittleEndianImplicitTransferSyntax << UID_BigEndianExplicitTransferSyntax
                            << UID_JPEGProcess14SV1TransferSyntax << UID_JPEGProcess14TransferSyntax << UID_JPEGLSLosslessTransferSyntax
                            << UID_JPEG2000LosslessOnlyTransferSyntax << UID_RLELosslessTransferSyntax << UID_DeflatedExplicitVRLittleEndianTransferSyntax
                            << UID_JPEGProcess1TransferSyntax << UID_JPEGProcess2_4TransferSyntax << UID_JPEGLSLossyTransferSyntax
                            << UID_JPEG2000TransferSyntax;

    // The short list leaves room for the presentation contexts: there can only be 128 in an association.
    QStringList sopClasses;
    int presentationContextID = 3;

    for (int i = 0; i < numberOfDcmShortSCUStorageSOPClassUIDs && condition.good() && presentationContextID <= 255; i++)
    {
        if (!sopClasses.contains(QString(dcmShortSCUStorageSOPClassUIDs[i])))
        {
            sopClasses.append(QString(dcmShortSCUStorageSOPClassUIDs[i]));
            condition = addPresentationContext(presentationContextID, sopClasses.last(), storageTransferSyntaxes, ASC_SC_ROLE_SCP);
            // Only odd presentation context id's
            presentationContextID += 2;
        }
    }

    return condition;
}

OFCondition PACSConnection::configureEcho()
{
    const char *transferSyntaxes[] = { UID_LittleEndianImplicitTransferSyntax };
    // PresentationContextID always has to be odd
    int presentationContextID = 1;

    return ASC_addPresentationContext(m_associationParameters, presentationContextID, UID_VerificationSOPClass, transferSyntaxes, DIM_OF(transferSyntaxes));
}

OFCondition PACSConnection::configureFind()
{
    const char *transferSyntaxes[] = { NULL, NULL, NULL };
    // Sempre ha de ser imparell, el seu valor és 1 perquè només passem un presentation context
    int presentationContextID = 1;

    getTransferSyntaxForFindOrMoveConnection(transferSyntaxes);

    return ASC_addPresentationContext(m_associationParameters, presentationContextID, UID_FINDStudyRootQueryRetrieveInformationModel, transferSyntaxes,
        DIM_OF(transferSyntaxes));
}

OFCondition PACSConnection::configureMove()
{
    T_ASC_PresentationContextID associationPresentationContextID = 1;
    const char *transferSyntaxes[] = { NULL, NULL, NULL };

    getTransferSyntaxForFindOrMoveConnection(transferSyntaxes);

    return ASC_addPresentationContext(m_associationParameters, associationPresentationContextID, UID_MOVEStudyRootQueryRetrieveInformationModel,
                                      transferSyntaxes, 3 /*number of TransferSyntaxes*/);
}

// TODO Estudiar si el millor transferSyntax per defecte és UID_LittleEndianExplicitTransferSyntax o com els cas del move és el JPegLossLess
OFCondition PACSConnection::configureStore()
{
    // Each SOP Class will be proposed in two presentation contexts (unless the opt_combineProposedTransferSyntaxes global variable is true).
    // The command line specified a preferred transfer syntax to use. This prefered transfer syntax will be proposed in one presentation context
    // and a set of alternative (fallback) transfer syntaxes will be proposed in a different presentation context.

    // Generally, we prefer to use Explicitly encoded transfer syntaxes and if running on a Little Endian machine we prefer LittleEndianExplicitTransferSyntax
    // to BigEndianTransferSyntax. Some SCP implementations will just select the first transfer syntax they support (this is not part of the standard) so
    // organise the proposed transfer syntaxes to take advantage of such behaviour.

    /// Indiquem que con Transfer syntax preferida volem utilitzar JpegLossless
    const char *preferredTransferSyntax = UID_JPEGProcess14SV1TransferSyntax;

    QList<const char*> fallbackSyntaxes;
    fallbackSyntaxes.append(UID_LittleEndianExplicitTransferSyntax);
    fallbackSyntaxes.append(UID_BigEndianExplicitTransferSyntax);
    fallbackSyntaxes.append(UID_LittleEndianImplicitTransferSyntax);

    // Afegim totes les classes SOP de transfarència d'imatges. com que desconeixem de quina modalitat són
    // les imatges alhora de preparar la connexió les hi incloem totes les modalitats. Si alhora de connectar sabèssim de quina modalitat és
    // l'estudi només caldria afegir-hi la de la motalitat de l'estudi

    // Les sopClass o també conegudes com AbstractSyntax és equivalent amb el Move quina acció volem fer per exemple
    // UID_MOVEStudyRootQueryRetrieveInformationModel, en el case del move, en el cas de StoreScu, el sopClass que tenim van en funció del tipus d'imatge
    // per exemple tenim ComputedRadiographyImageStorage, CTImageStore, etc.. aquestes sopClass indiquen  quin tipus d'imatge anirem a guardar, per això sinó
    // sabem de quin tipus de SOPClass són les imatges que anem a guardar al PACS, li indiquem una llista per defecte que cobreix la gran majoria i més
    // comuns de SOPClass que existeixen

    // Amb l'Abstract syntax o SOPClass definim quina operació volem fer, i amb els transfer syntax indiquem amb quin protocol es farà
    // la transfarència de les dades, LittleEndian, JPegLossLess, etc..

    // TODO Si que la podem arribar a saber la transfer syntax, només hem de mirar la SOPClassUID de cada imatge a enviar, mirar
    // codi storescu.cc a partir de la línia 639
    QStringList sopClasses;
    for (int i = 0; i < numberOfDcmShortSCUStorageSOPClassUIDs; i++)
    {
        // Comprovem que no hi hagi que cap SOPClas duplicada
        if (!sopClasses.contains(QString(dcmShortSCUStorageSOPClassUIDs[i])))
        {
            sopClasses.append(QString(dcmShortSCUStorageSOPClassUIDs[i]));
        }
    }

    OFCondition condition = EC_Normal;
    int presentationContextID = 1;

    // Creem un presentation context per cada SOPClass que tinguem, indicant per cada SOPClass quina transfer syntax utilitzarem
    // En el cas del Store amb el presentation Context indiquem que per cada tipus d'imatge que volem guardar SOPClass amb quins
    // transfer syntax ens podem comunicar, llavors el PACS ens indicarà si ell pot guardar aquest tipus de SOPClass, i amb quin
    // transfer syntax li hem d'enviar la imatge

    foreach (const QString &sopClass, sopClasses)
    {
        // No poden haver més de 255 presentation context
        if (presentationContextID > 255)
        {
            return ASC_BADPRESENTATIONCONTEXTID;
        }

        // Sop class with preferred transfer syntax
        condition = ASC_addPresentationContext(m_associationParameters, presentationContextID, qPrintable(sopClass), &preferredTransferSyntax, 1,
                                               ASC_SC_ROLE_DEFAULT);
        // Only odd presentation context id's
        presentationContextID += 2;
        if (!condition.good())
        {
            break;
        }

        if (fallbackSyntaxes.size() > 0)
        {
            if (presentationContextID > 255)
            {
                return ASC_BADPRESENTATIONCONTEXTID;
            }

            // Sop class with fallback transfer syntax
            condition = addPresentationContext(presentationContextID, sopClass, fallbackSyntaxes);
            // Only odd presentation context id's
            presentationContextID += 2;
            if (!condition.good())
            {
                break;
            }
        }
    }

    return condition;
}

OFCondition PACSConnection::addPresentationContext(int presentationContextId, const QString &abstractSyntax, QList<const char*> transferSyntaxList,
                                                   T_ASC_SC_ROLE role)
{
    // Create an array of supported/possible transfer syntaxes
    const char **transferSyntaxes = new const char*[transferSyntaxList.size()];
    int transferSyntaxCount = 0;

    foreach (const char *transferSyntax, transferSyntaxList)
    {
        transferSyntaxes[transferSyntaxCount++] = transferSyntax;
    }

    OFCondition condition = ASC_addPresentationContext(m_associationParameters, presentationContextId, qPrintable(abstractSyntax), transferSyntaxes,
        transferSyntaxCount, role);

    delete[] transferSyntaxes;
    return condition;
}

bool PACSConnection::connectToPACS(PACSServiceToRequest pacsServiceToRequest)
{
    // Hi ha invocacions de mètodes de dcmtk que no se'ls hi comprova el condition que retornen, perquè se'ls hi ha mirat el codi i sempre retornen EC_NORMAL
    Settings settings;

    // Create the parameters of the connection
    OFCondition condition = ASC_createAssociationParameters(&m_associationParameters, ASC_DEFAULTMAXPDU);
    if (!condition.good())
    {
        ERROR_LOG("Error creating the association parameters, error description: " + QString(condition.text()));
        return false;
    }

    // Set calling and called AE titles
    ASC_setAPTitles(m_associationParameters, qPrintable(settings.getValue(InputOutputSettings::LocalAETitle).toString()), qPrintable(m_pacs.getAETitle()),
                    NULL);

    ASC_setTransportLayerType(m_associationParameters, m_pacs.isDicomTlsEnabled() ? OFTrue : OFFalse);

    ASC_setPresentationAddresses(m_associationParameters, qPrintable(QHostInfo::localHostName()),
        qPrintable(constructPacsServerAddress(pacsServiceToRequest, m_pacs)));

    // Especifiquem el timeout de connexió, si amb aquest temps no rebem resposta donem error per time out
    dcmConnectionTimeout.set(settings.getValue(InputOutputSettings::PACSConnectionTimeout).toInt());

    switch (pacsServiceToRequest)
    {
        case Echo:
            condition = configureEcho();
            break;
        case Query:
            condition = configureFind();
            break;
        case RetrieveDICOMFiles:
            condition = m_pacs.usesCGet() ? configureGet() : configureMove();
            break;
        case SendDICOMFiles:
            condition = configureStore();
            break;
    }

    if (!condition.good())
    {
        ERROR_LOG("An error occurred configuring the connection. AE Title: " + m_pacs.getAETitle() + ", address: " +
            constructPacsServerAddress(pacsServiceToRequest, m_pacs) + ". Error description: " + QString(condition.text()));
        return false;
    }

    // Inicialitzem l'objecte network però la connexió no s'obre fins a l'invocacació del mètode ASC_requestAssociation
    m_associationNetwork = initializeAssociationNetwork(pacsServiceToRequest);

    if (m_associationNetwork == NULL)
    {
        ERROR_LOG("An error occurred initializing the connection parameters. AE Title: " + m_pacs.getAETitle() + ", address: " +
            constructPacsServerAddress(pacsServiceToRequest, m_pacs));
        return false;
    }

    if (m_pacs.isDicomTlsEnabled() && !setUpDicomTls())
    {
        ASC_dropNetwork(&m_associationNetwork);
        return false;
    }

    // Intentem connectar
    condition = ASC_requestAssociation(m_associationNetwork, m_associationParameters, &m_dicomAssociation);

    if (condition.good())
    {
        if (ASC_countAcceptedPresentationContexts(m_associationParameters) == 0)
        {
            ERROR_LOG("The PACS did not accept any of the Presentation Contexts presented. AE Title: " + m_pacs.getAETitle() + ", address: " +
                constructPacsServerAddress(pacsServiceToRequest, m_pacs));
            return false;
        }
    }
    else
    {
        ERROR_LOG("An error occurred trying to connect to the PACS. AE Title: " + m_pacs.getAETitle() + ", address: " +
            constructPacsServerAddress(pacsServiceToRequest, m_pacs) + ". Error description: " + QString(condition.text()));

        // Si no hem pogut connectar al PACS i és una descàrrega haurem obert el port per rebre connexions entrants DICOM, com no que podrem descarregar
        // les imatges perquè no hem pogut connectar amb el PACS per sol·licitar-ne la descarrega, tanquem el port local que espera per connexions entrants.
        if (pacsServiceToRequest == RetrieveDICOMFiles)
        {
            disconnect();
        }

        return false;
    }

    return true;
}

OFCondition PACSConnection::abort()
{
    if (!m_dicomAssociation || m_associationAborted)
    {
        return EC_Normal;
    }

    OFCondition condition = ASC_abortAssociation(m_dicomAssociation);
    m_associationAborted = true;
    return condition;
}

void PACSConnection::disconnect()
{
    // Only an association that was established and not aborted can be released. If the PACS could not be reached, DCMTK has created the
    // association but not its upper layer association.
    if (m_dicomAssociation && m_dicomAssociation->DULassociation && !m_associationAborted)
    {
        OFCondition condition = ASC_releaseAssociation(m_dicomAssociation);
        if (condition.bad())
        {
            ERROR_LOG("Could not disconnect from the PACS, error description: " + QString(condition.text()));
        }
    }

    if (m_dicomAssociation)
    {
        OFCondition condition = ASC_destroyAssociation(&m_dicomAssociation);
        if (condition.bad())
        {
            ERROR_LOG("Error destroying the connection with the PACS, error description: " + QString(condition.text()));
        }
    }

    // Destrueix l'objecte i tanca el socket obert, fins que no es fa el drop de l'objecte no es tanca el socket
    if (m_associationNetwork)
    {
        OFCondition condition = ASC_dropNetwork(&m_associationNetwork);
        if (condition.bad())
        {
            ERROR_LOG("Error closing the incoming connections port, error description: " + QString(condition.text()));
        }
    }

    m_dicomAssociation = NULL;
    m_associationNetwork = NULL;
    m_associationAborted = false;
}

QString PACSConnection::constructPacsServerAddress(PACSServiceToRequest pacsServiceToRequest, PacsDevice pacsDevice)
{
    // The format is "server:port"
    QString pacsServerAddress = pacsDevice.getAddress() + ":";

    switch (pacsServiceToRequest)
    {
        case PACSConnection::Query:
        case PACSConnection::RetrieveDICOMFiles:
            pacsServerAddress += QString().setNum(pacsDevice.getQueryRetrieveServicePort());
            break;
        case PACSConnection::SendDICOMFiles:
            pacsServerAddress += QString().setNum(pacsDevice.getStoreServicePort());
            break;
        case PACSConnection::Echo:
            if (pacsDevice.isQueryRetrieveServiceEnabled())
            {
                pacsServerAddress += QString().setNum(pacsDevice.getQueryRetrieveServicePort());
            }
            else if (pacsDevice.isStoreServiceEnabled())
            {
                pacsServerAddress += QString().setNum(pacsDevice.getStoreServicePort());
            }
            else
            {
                ERROR_LOG("Could not configure which port to use for the echo because the PACS " + pacsDevice.getAETitle() + " has no service enabled");
            }
            break;
        default:
            ERROR_LOG("Could not configure which port to use for the echo to the PACS " + pacsDevice.getAETitle() + " because the connection mode is invalid");
    }

    INFO_LOG("PACS Adress build:" + pacsServerAddress);

    return pacsServerAddress;
}

T_ASC_Network* PACSConnection::initializeAssociationNetwork(PACSServiceToRequest pacsServiceToRequest)
{
    Settings settings;
    // C-GET receives the images on the association that it opens, so only C-MOVE listens on a port.
    bool listens = pacsServiceToRequest == RetrieveDICOMFiles && !m_pacs.usesCGet();
    // Si no es tracta d'una descarrega indiquem port 0
    int networkPort = listens ? settings.getValue(InputOutputSettings::IncomingDICOMConnectionsPort).toInt() : 0;
    int timeout = settings.getValue(InputOutputSettings::PACSConnectionTimeout).toInt();
    T_ASC_NetworkRole networkRole = listens ? NET_ACCEPTORREQUESTOR : NET_REQUESTOR;
    T_ASC_Network *associationNetwork;

    OFCondition condition = ASC_initializeNetwork(networkRole, networkPort, timeout, &associationNetwork);
    if (!condition.good())
    {
        ERROR_LOG("Could not initialize the network object, error description" + QString(condition.text()));
        return NULL;
    }

    return associationNetwork;
}

void PACSConnection::getTransferSyntaxForFindOrMoveConnection(const char *transferSyntaxes[3])
{
    // We prefer to use Explicitly encoded transfer syntaxes. If we are running on a Little Endian machine we prefer LittleEndianExplicitTransferSyntax
    // to BigEndianTransferSyntax. Some SCP implementations will just select the first transfer syntax they support (this is not part of the standard) so
    // organise the proposed transfer syntaxes to take advantage of such behaviour.
    // The presentation presentationContextIDs proposed here are only used for C-FIND and C-MOVE, so there is no need to support compressed transmission.

    // gLocalByteOrder is defined in dcxfer.h
    if (gLocalByteOrder == EBO_LittleEndian)
    {
        // We are on a little endian machine
        transferSyntaxes[0] = UID_LittleEndianExplicitTransferSyntax;
        transferSyntaxes[1] = UID_BigEndianExplicitTransferSyntax;
    }
    else
    {
        // We are on a big endian machine
        transferSyntaxes[0] = UID_BigEndianExplicitTransferSyntax;
        transferSyntaxes[1] = UID_LittleEndianExplicitTransferSyntax;
    }

    transferSyntaxes[2] = UID_LittleEndianImplicitTransferSyntax;
}

PacsDevice PACSConnection::getPacs()
{
    return m_pacs;
}

T_ASC_Association* PACSConnection::getConnection()
{
    return m_dicomAssociation;
}

T_ASC_Network* PACSConnection::getNetwork()
{
    return m_associationNetwork;
}

}
