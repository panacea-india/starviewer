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

#include "videoextensionmediator.h"

#include "extensioncontext.h"

namespace udg {

VideoExtensionMediator::VideoExtensionMediator(QObject *parent)
    : ExtensionMediator(parent)
{
}

VideoExtensionMediator::~VideoExtensionMediator()
{
}

bool VideoExtensionMediator::initializeExtension(QWidget *extension, const ExtensionContext &extensionContext)
{
    QVideoExtension *videoExtension = qobject_cast<QVideoExtension*>(extension);

    if (!videoExtension)
    {
        return false;
    }

    videoExtension->setPatient(extensionContext.getPatient());

    return true;
}

DisplayableID VideoExtensionMediator::getExtensionID() const
{
    return DisplayableID("VideoExtension", tr("Video"));
}

DicomEntityFlags VideoExtensionMediator::getSupportedDicomEntities() const
{
    return DicomEntity::Video;
}

} // namespace udg
