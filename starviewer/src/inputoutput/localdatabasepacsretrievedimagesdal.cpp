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

#include "localdatabasepacsretrievedimagesdal.h"

#include "logging.h"
#include "pacsdevice.h"

#include <QList>
#include <QSqlQuery>
#include <QUrl>
#include <QVariant>

namespace udg {

LocalDatabasePACSRetrievedImagesDAL::LocalDatabasePACSRetrievedImagesDAL(DatabaseConnection &databaseConnection)
    : LocalDatabaseBaseDAL(databaseConnection)
{
}

qlonglong LocalDatabasePACSRetrievedImagesDAL::insert(const PacsDevice &pacsDevice)
{
    QSqlQuery query = getNewQuery();

    if (pacsDevice.getType() == PacsDevice::Type::Dimse)
    {
        query.prepare("INSERT INTO PACSRetrievedImages (Type, AETitle, Address, QueryPort) VALUES (:type, :aeTitle, :address, :queryPort)");
        query.bindValue(":type", "DIMSE");
        query.bindValue(":aeTitle", pacsDevice.getAETitle());
        query.bindValue(":address", pacsDevice.getAddress());
        query.bindValue(":queryPort", pacsDevice.getQueryRetrieveServicePort());
    }
    else if (pacsDevice.getType() == PacsDevice::Type::Wado)
    {
        query.prepare("INSERT INTO PACSRetrievedImages (Type, BaseUri) VALUES (:type, :baseUri)");
        query.bindValue(":type", "WADO");
        query.bindValue(":baseUri", pacsDevice.getBaseUri());
    }
    else    // WADO-URI + DIMSE
    {
        query.prepare("INSERT INTO PACSRetrievedImages (Type, AETitle, Address, QueryPort, BaseUri) VALUES (:type, :aeTitle, :address, :queryPort, :baseUri)");
        query.bindValue(":type", "WADO-URI+DIMSE");
        query.bindValue(":aeTitle", pacsDevice.getAETitle());
        query.bindValue(":address", pacsDevice.getAddress());
        query.bindValue(":queryPort", pacsDevice.getQueryRetrieveServicePort());
        query.bindValue(":baseUri", pacsDevice.getBaseUri());
    }

    if (executeQueryAndLogError(query))
    {
        return query.lastInsertId().toLongLong();
    }
    else
    {
        return -1;
    }
}

PacsDevice LocalDatabasePACSRetrievedImagesDAL::query(qlonglong pacsId)
{
    PacsDevice pacsDevice;
    QSqlQuery query = getNewQuery();
    query.prepare("SELECT ID, Type, AETitle, Address, QueryPort, BaseUri FROM PACSRetrievedImages WHERE ID = :id");
    query.bindValue(":id", pacsId);

    if (executeQueryAndLogError(query) && query.next())
    {
        QString type = query.value("Type").toString();

        if (type == "DIMSE")
        {
            pacsDevice.setID(query.value("ID").toString());
            pacsDevice.setType(PacsDevice::Type::Dimse);
            pacsDevice.setAETitle(query.value("AETitle").toString());
            pacsDevice.setAddress(query.value("Address").toString());
            pacsDevice.setQueryRetrieveServicePort(query.value("QueryPort").toInt());
        }
        else if (type == "WADO")
        {
            pacsDevice.setID(query.value("ID").toString());
            pacsDevice.setType(PacsDevice::Type::Wado);
            pacsDevice.setBaseUri(query.value("BaseUri").toUrl());
        }
        else if (type == "WADO-URI+DIMSE")
        {
            pacsDevice.setID(query.value("ID").toString());
            pacsDevice.setType(PacsDevice::Type::WadoUriDimse);
            pacsDevice.setAETitle(query.value("AETitle").toString());
            pacsDevice.setAddress(query.value("Address").toString());
            pacsDevice.setQueryRetrieveServicePort(query.value("QueryPort").toInt());
            pacsDevice.setBaseUri(query.value("BaseUri").toUrl());
        }
        else
        {
            WARN_LOG(QString("Found PACS in database with unexpected type: %1").arg(type));
        }
    }

    return pacsDevice;
}

QVariant LocalDatabasePACSRetrievedImagesDAL::queryId(const PacsDevice &pacsDevice)
{
    QSqlQuery query = getNewQuery();

    if (pacsDevice.getType() == PacsDevice::Type::Dimse)
    {
        query.prepare("SELECT ID FROM PACSRetrievedImages WHERE AETitle = :aeTitle AND Address = :address AND QueryPort = :queryPort");
        query.bindValue(":aeTitle", pacsDevice.getAETitle());
        query.bindValue(":address", pacsDevice.getAddress());
        query.bindValue(":queryPort", pacsDevice.getQueryRetrieveServicePort());
    }
    else if (pacsDevice.getType() == PacsDevice::Type::Wado)
    {
        query.prepare("SELECT ID FROM PACSRetrievedImages WHERE BaseUri = :baseUri");
        query.bindValue(":baseUri", pacsDevice.getBaseUri());
    }
    else    // WADO-URI + DIMSE
    {
        query.prepare("SELECT ID FROM PACSRetrievedImages WHERE AETitle = :aeTitle AND Address = :address AND QueryPort = :queryPort AND BaseUri = :baseUri");
        query.bindValue(":aeTitle", pacsDevice.getAETitle());
        query.bindValue(":address", pacsDevice.getAddress());
        query.bindValue(":queryPort", pacsDevice.getQueryRetrieveServicePort());
        query.bindValue(":baseUri", pacsDevice.getBaseUri());
    }

    if (executeQueryAndLogError(query) && query.next())
    {
        return query.value("ID");
    }
    else
    {
        return QVariant(QVariant::LongLong);
    }
}

int LocalDatabasePACSRetrievedImagesDAL::removeLoginsFromBaseUris()
{
    struct Row
    {
        qlonglong id;
        QString type;
        QUrl baseUri;
    };

    QList<Row> rows;

    // Rows are read in full first because the table can't be changed while a query on it is active
    {
        QSqlQuery query = getNewQuery();
        query.prepare("SELECT ID, Type, BaseUri FROM PACSRetrievedImages WHERE BaseUri LIKE '%@%'");

        if (!executeQueryAndLogError(query))
        {
            return -1;
        }

        while (query.next())
        {
            QUrl baseUri = query.value("BaseUri").toUrl();

            if (!baseUri.userInfo().isEmpty())
            {
                rows.append({query.value("ID").toLongLong(), query.value("Type").toString(), baseUri.adjusted(QUrl::RemoveUserInfo)});
            }
        }
    }

    for (const Row &row : rows)
    {
        QSqlQuery existing = getNewQuery();
        existing.prepare("SELECT ID FROM PACSRetrievedImages WHERE BaseUri = :baseUri AND Type = :type");
        existing.bindValue(":baseUri", row.baseUri);
        existing.bindValue(":type", row.type);

        if (!executeQueryAndLogError(existing))
        {
            return -1;
        }

        if (existing.next())
        {
            const qlonglong existingId = existing.value("ID").toLongLong();
            existing.finish();

            for (const QString &statement : {QString("UPDATE Image SET RetrievedPACSID = :newId WHERE RetrievedPACSID = :oldId"),
                                             QString("UPDATE EncapsulatedDocument SET RetrievedPacsID = :newId WHERE RetrievedPacsID = :oldId")})
            {
                QSqlQuery move = getNewQuery();
                move.prepare(statement);
                move.bindValue(":newId", existingId);
                move.bindValue(":oldId", row.id);

                if (!executeQueryAndLogError(move))
                {
                    return -1;
                }
            }

            QSqlQuery remove = getNewQuery();
            remove.prepare("DELETE FROM PACSRetrievedImages WHERE ID = :id");
            remove.bindValue(":id", row.id);

            if (!executeQueryAndLogError(remove))
            {
                return -1;
            }
        }
        else
        {
            existing.finish();

            QSqlQuery update = getNewQuery();
            update.prepare("UPDATE PACSRetrievedImages SET BaseUri = :baseUri WHERE ID = :id");
            update.bindValue(":baseUri", row.baseUri);
            update.bindValue(":id", row.id);

            if (!executeQueryAndLogError(update))
            {
                return -1;
            }
        }
    }

    return rows.size();
}

}
