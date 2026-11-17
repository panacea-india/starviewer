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

#include "registeredvolumes.h"

#include <QHash>
#include <QObject>

#include "volume.h"

namespace udg {

namespace {

QHash<const Volume*, QString>& notes()
{
    static QHash<const Volume*, QString> registry;
    return registry;
}

}

void RegisteredVolumes::add(Volume *volume, const QString &note)
{
    if (!volume)
    {
        return;
    }

    notes().insert(volume, note.isEmpty() ? QObject::tr("registered") : note);
    QObject::connect(volume, &QObject::destroyed, [](QObject *object) { notes().remove(static_cast<const Volume*>(object)); });
}

QString RegisteredVolumes::getNote(const Volume *volume)
{
    return notes().value(volume);
}

bool RegisteredVolumes::isRegistered(const Volume *volume)
{
    return notes().contains(volume);
}

} // End namespace udg
