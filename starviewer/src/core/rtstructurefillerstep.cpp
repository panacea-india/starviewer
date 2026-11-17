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

#include "rtstructurefillerstep.h"

#include "dicomtagreader.h"
#include "logging.h"
#include "patientfillerinput.h"
#include "rtstructureset.h"
#include "series.h"
#include "study.h"

namespace udg {

RtStructureFillerStep::RtStructureFillerStep()
 : PatientFillerStep()
{
}

RtStructureFillerStep::~RtStructureFillerStep()
{
}

bool RtStructureFillerStep::fillIndividually()
{
    const DICOMTagReader *dicomReader = m_input->getDICOMFile();

    if (!dicomReader || dicomReader->getValueAttributeAsQString(DICOMSOPClassUID) != UIDRTStructureSetStorage)
    {
        return true;
    }

    QString error;
    RtStructureSet set = RtStructureSet::fromDICOM(*dicomReader, &error);

    if (set.isNull())
    {
        WARN_LOG(QString("The RT structure set %1 was not used: %2").arg(dicomReader->getFileName()).arg(error));
        return true;
    }

    Series *series = m_input->getCurrentSeries();
    Study *study = series ? series->getParentStudy() : nullptr;

    if (study && study->addRtStructureSet(set))
    {
        INFO_LOG(QString("RT structure set %1 (%2): %3 regions, %4 contours").arg(set.getSopInstanceUid()).arg(set.getLabel()).arg(set.getRois().size())
                 .arg(set.getNumberOfContours()));
    }

    return true;
}

} // End namespace udg
