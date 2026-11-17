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

#ifndef UDGVIDEOFILLERSTEP_H
#define UDGVIDEOFILLERSTEP_H

#include "patientfillerstep.h"

namespace udg {

/**
    Keeps the DICOM videos in their study, so that the video extension can list and play them. A prerequisite is that the
    DICOMFileClassifierFillerStep has been passed. The ImageFillerStep ignores the videos, as they have no pixels to read.
 */
class VideoFillerStep : public PatientFillerStep {
public:
    VideoFillerStep();
    ~VideoFillerStep();

    bool fillIndividually() override;
};

}

#endif
