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

#include "listenrisrequests.h"

#include <QTcpServer>
#include <QHostAddress>
#include <QTcpSocket>
#include <QMetaType>

#include "parsexmlrispierrequest.h"
#include "logging.h"
#include "inputoutputsettings.h"
#include "starviewerapplication.h"

namespace udg {

const int ListenRISRequests::TimeOutToReadData = 15000;

ListenRISRequests::ListenRISRequests()
{
    // Registrem la classe DicomMask per poder-ne fer un signal
    qRegisterMetaType<DicomMask>("DicomMask");
    qRegisterMetaType<ListenRISRequests::ListenRISRequestsError>("ListenRISRequests::ListenRISRequestsError");
}

void ListenRISRequests::stopListen()
{
    if (m_tcpRISServer->isListening())
    {
        m_tcpRISServer->close();
    }
}

void ListenRISRequests::listen()
{
    m_tcpRISServer = new QTcpServer();

    if (!m_tcpRISServer->listen(QHostAddress::Any, Settings().getValue(InputOutputSettings::RISRequestsPort).toUInt()))
    {
        networkError(m_tcpRISServer);
        return;
    }

    connect(m_tcpRISServer, SIGNAL(newConnection()), SLOT(newRISRequest()));
}

void ListenRISRequests::newRISRequest()
{
    QTcpSocket *tcpSocket = m_tcpRISServer->nextPendingConnection();
    QString risRequestData;

    INFO_LOG("Request received from IP " + tcpSocket->peerAddress().toString());
    if (tcpSocket->waitForReadyRead(TimeOutToReadData))
    {
        risRequestData = QString(tcpSocket->readAll());
        INFO_LOG("Data received: " + risRequestData);
    }
    else
    {
        INFO_LOG("No data was received, error: " + tcpSocket->errorString());
    }

    INFO_LOG("Tanco socket");
    tcpSocket->disconnectFromHost();
    INFO_LOG("Deleting the socket");
    delete tcpSocket;

    if (!risRequestData.isEmpty())
    {
        processRequest(risRequestData);
    }
}

void ListenRISRequests::processRequest(QString risRequestData)
{
    // Com ara mateix només rebrem peticions del RIS PIER del IDI, no cal esbrinar quin tipus de petició és per defecte entenem que és petició del RIS PIER
    INFO_LOG("An attempt will be made to process the received request as XML");

    ParseXmlRisPIERRequest parseXml;

    DicomMask mask = parseXml.parseXml(risRequestData);

    if (!parseXml.error())
    {
        INFO_LOG("Process correcte faig signal");
        emit requestRetrieveStudy(mask);
    }
}

void ListenRISRequests::networkError(QTcpServer *tcpServer)
{
    ERROR_LOG("Cannot listen for RIS requests on port " + QString().setNum(Settings().getValue(InputOutputSettings::RISRequestsPort).toUInt()) +
              ", error " + tcpServer->errorString());

    switch (tcpServer->serverError())
    {
        case QAbstractSocket::AddressInUseError:
            emit errorListening(RisPortInUse);
            break;
        default:
            emit errorListening(UnknownNetworkError);
            break;
    }
}

}
