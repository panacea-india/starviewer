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

#ifndef UDGGANTRYTILTCORRECTOR_H
#define UDGGANTRYTILTCORRECTOR_H

#include <array>
#include <vector>

class vtkImageData;

namespace udg {

/**
    Corrects the stack of a CT that was acquired with a tilted gantry.

    The slices of such a CT are planes with a tilted orientation, but the table moves them along its own axis, so the position of each slice is shifted along the slice
    from the one before. A reader that stacks the slices by their order and the distance along the normal makes a volume with slices that are all in the same place, while
    the patient is not: the head looks sheared in the sagittal and coronal views and in 3D. The correction shifts each slice back by the part of the position that is in the
    plane, so that the stack is straight (the voxels are at p0 + x row + y column + z normal), which is what the rest of the application assumes.

    The calculations take only numbers and an image, so they are tested without a window.
 */
class GantryTiltCorrector {
public:
    typedef std::array<double, 3> Vector;

    struct Analysis {
        /// True if the shift of the slices is large enough to correct (more than half a pixel along the stack)
        bool tilted = false;
        /// The angle between the normal of the slices and the direction that the stack goes in, in degrees
        double tiltDegrees = 0.0;
        /// The distance from the first slice along the normal of each slice, in mm
        std::vector<double> depths;
        /// The shift of each slice from the first one in the plane: along the row direction and along the column direction, in mm
        std::vector<std::array<double, 2>> offsets;
    };

    /// Works out the tilt from the position of each slice (ImagePositionPatient, in the order of the stack), the directions of the rows and columns, and the size of a pixel.
    static Analysis analyse(const std::vector<Vector> &positions, const Vector &rowDirection, const Vector &columnDirection, double pixelSpacingX, double pixelSpacingY);

    /// Shifts each slice of the volume back by its offset, with linear interpolation. The voxels that come from outside the slice get the lowest value of the volume.
    /// The volume is changed in place. The number of slices of the volume has to be the number of offsets of the analysis, or nothing is done and false is returned.
    static bool correct(vtkImageData *volume, const Analysis &analysis);
};

} // End namespace udg

#endif
