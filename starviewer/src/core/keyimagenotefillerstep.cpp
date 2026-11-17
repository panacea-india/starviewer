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

#include "keyimagenotefillerstep.h"

#include "dicomtagreader.h"
#include "keyobjectselection.h"
#include "logging.h"
#include "patientfillerinput.h"
#include "series.h"
#include "study.h"

namespace udg {

KeyImageNoteFillerStep::KeyImageNoteFillerStep()
 : PatientFillerStep()
{
}

KeyImageNoteFillerStep::~KeyImageNoteFillerStep()
{
}

bool KeyImageNoteFillerStep::fillIndividually()
{
    const DICOMTagReader *dicomReader = m_input->getDICOMFile();

    if (!dicomReader || dicomReader->getValueAttributeAsQString(DICOMSOPClassUID) != UIDKeyObjectSelectionDocumentStorage)
    {
        return true;
    }

    QString error;
    KeyObjectSelection selection = KeyObjectSelection::fromDICOM(*dicomReader, &error);

    if (selection.isNull())
    {
        WARN_LOG(QString("The Key Object Selection Document %1 was not used: %2").arg(dicomReader->getFileName()).arg(error));
        return true;
    }

    Series *series = m_input->getCurrentSeries();
    Study *study = series ? series->getParentStudy() : nullptr;

    if (study && study->addKeyObjectSelection(selection))
    {
        INFO_LOG(QString("Key Object Selection Document %1 (%2) refers to %3 images")
                 .arg(selection.getSopInstanceUid()).arg(selection.getTitleMeaning()).arg(selection.getReferences().size()));
    }

    return true;
}

} // End namespace udg
