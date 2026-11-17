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

#include "videofillerstep.h"

#include "dicomtagreader.h"
#include "logging.h"
#include "patientfillerinput.h"
#include "series.h"
#include "study.h"
#include "videoobject.h"

namespace udg {

VideoFillerStep::VideoFillerStep()
 : PatientFillerStep()
{
}

VideoFillerStep::~VideoFillerStep()
{
}

bool VideoFillerStep::fillIndividually()
{
    const DICOMTagReader *dicomReader = m_input->getDICOMFile();

    if (!dicomReader || !VideoObject::isVideoTransferSyntax(dicomReader->getValueAttributeAsQString(DICOMTransferSyntaxUID).trimmed()))
    {
        return true;
    }

    QString error;
    VideoObject video = VideoObject::fromDICOM(*dicomReader, &error);

    if (video.isNull())
    {
        WARN_LOG(QString("The video %1 was not used: %2").arg(dicomReader->getFileName()).arg(error));
        return true;
    }

    Series *series = m_input->getCurrentSeries();
    Study *study = series ? series->getParentStudy() : nullptr;

    if (study && study->addVideo(video))
    {
        INFO_LOG(QString("Video %1 (%2, %3 frames)").arg(video.getSopInstanceUid()).arg(video.getFormatName()).arg(video.getNumberOfFrames()));
    }

    return true;
}

} // End namespace udg
