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

#ifndef UDGRTSTRUCTUREFILLERSTEP_H
#define UDGRTSTRUCTUREFILLERSTEP_H

#include "patientfillerstep.h"

namespace udg {

/**
    Reads the RT Structure Sets and keeps them in their study, so that their contours can be drawn on the images of the same frame of reference.
    A prerequisite is that the DICOMFileClassifierFillerStep has been passed. Files that are not structure sets are ignored.
 */
class RtStructureFillerStep : public PatientFillerStep {
public:
    RtStructureFillerStep();
    ~RtStructureFillerStep();

    bool fillIndividually() override;
};

}

#endif
