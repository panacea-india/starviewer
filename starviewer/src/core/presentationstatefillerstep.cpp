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

#include "presentationstatefillerstep.h"

#include "dicomtagreader.h"
#include "graphicpresentationstate.h"
#include "logging.h"
#include "patientfillerinput.h"
#include "series.h"
#include "study.h"

namespace udg {

PresentationStateFillerStep::PresentationStateFillerStep()
 : PatientFillerStep()
{
}

PresentationStateFillerStep::~PresentationStateFillerStep()
{
}

bool PresentationStateFillerStep::fillIndividually()
{
    const DICOMTagReader *dicomReader = m_input->getDICOMFile();

    if (!dicomReader)
    {
        return true;
    }

    QString sopClass = dicomReader->getValueAttributeAsQString(DICOMSOPClassUID);

    if (sopClass != UIDGrayscaleSoftcopyPresentationStateStorage && sopClass != UIDColorSoftcopyPresentationStateStorage
        && sopClass != UIDPseudoColorSoftcopyPresentationStateStorage)
    {
        return true;
    }

    QString error;
    GraphicPresentationState state = GraphicPresentationState::fromDICOM(*dicomReader, &error);

    if (state.isNull())
    {
        WARN_LOG(QString("The presentation state %1 was not used: %2").arg(dicomReader->getFileName()).arg(error));
        return true;
    }

    Series *series = m_input->getCurrentSeries();
    Study *study = series ? series->getParentStudy() : nullptr;

    if (study && study->addPresentationState(state))
    {
        INFO_LOG(QString("Presentation state %1 (%2) refers to %3 images").arg(state.getSopInstanceUid()).arg(state.getLabel())
                 .arg(state.getReferencedImages().size()));
    }

    return true;
}

} // End namespace udg
