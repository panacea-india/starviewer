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

#include "savedviewstore.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>

namespace udg {

QList<SavedView> SavedViewStore::load(const QString &filePath)
{
    QList<SavedView> views;
    QFile file(filePath);

    if (!file.open(QIODevice::ReadOnly))
    {
        return views;
    }

    QJsonDocument document = QJsonDocument::fromJson(file.readAll());

    foreach (const QJsonValue &value, document.object()["views"].toArray())
    {
        bool valid = false;
        SavedView view = SavedView::fromJson(value.toObject(), &valid);

        if (valid)
        {
            views << view;
        }
    }

    return views;
}

bool SavedViewStore::save(const QString &filePath, const QList<SavedView> &views)
{
    QDir().mkpath(QFileInfo(filePath).absolutePath());

    QJsonArray array;

    foreach (const SavedView &view, views)
    {
        array << view.toJson();
    }

    QJsonObject root;
    root["version"] = 1;
    root["views"] = array;

    QSaveFile file(filePath);

    if (!file.open(QIODevice::WriteOnly))
    {
        return false;
    }

    file.write(QJsonDocument(root).toJson());
    return file.commit();
}

QString SavedViewStore::fileNameForSeries(const QString &seriesInstanceUid)
{
    return QString::fromLatin1(QCryptographicHash::hash(seriesInstanceUid.toUtf8(), QCryptographicHash::Sha256).toHex().left(32)) + ".json";
}

} // End namespace udg
