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

#include "gantrytiltpostprocessor.h"

#include <vector>

#include "gantrytiltcorrector.h"
#include "image.h"
#include "imageorientation.h"
#include "logging.h"
#include "volume.h"

namespace udg {

void GantryTiltPostprocessor::postprocess(Volume *volume)
{
    if (!volume || volume->getNumberOfPhases() != 1 || volume->getNumberOfSlicesPerPhase() < 2 || !volume->getVtkData())
    {
        return;
    }

    Image *first = volume->getImage(0);

    if (!first || !first->getPixelSpacing().isValid())
    {
        return;
    }

    std::vector<GantryTiltCorrector::Vector> positions;

    for (int i = 0; i < volume->getNumberOfSlicesPerPhase(); i++)
    {
        Image *image = volume->getImage(i);

        if (!image)
        {
            return;
        }

        const double *position = image->getImagePositionPatient();
        positions.push_back({{position[0], position[1], position[2]}});
    }

    ImageOrientation orientation = first->getImageOrientationPatient();
    GantryTiltCorrector::Vector row = {{orientation.getRowVector().x(), orientation.getRowVector().y(), orientation.getRowVector().z()}};
    GantryTiltCorrector::Vector column = {{orientation.getColumnVector().x(), orientation.getColumnVector().y(), orientation.getColumnVector().z()}};

    GantryTiltCorrector::Analysis analysis = GantryTiltCorrector::analyse(positions, row, column, first->getPixelSpacing().x(), first->getPixelSpacing().y());

    if (!analysis.tilted)
    {
        return;
    }

    if (GantryTiltCorrector::correct(volume->getVtkData(), analysis))
    {
        INFO_LOG(QString("Corrected a gantry tilt of %1 degrees in a volume of %2 slices").arg(analysis.tiltDegrees, 0, 'f', 1).arg(positions.size()));
    }
}

} // namespace udg
