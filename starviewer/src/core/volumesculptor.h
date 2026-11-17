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

#ifndef UDGVOLUMESCULPTOR_H
#define UDGVOLUMESCULPTOR_H

#include <vtkSmartPointer.h>

class vtkImageData;

namespace udg {

/**
    Takes parts out of a volume for the 3D view (the table, the cables, the noise around a structure) without changing the volume that was loaded: each operation gives
    a new volume of the same size, and the voxels that are taken out get the lowest value of the volume, which is empty in a 3D view.
 */
class VolumeSculptor {
public:
    struct Result {
        /// The new volume, or null if the operation could not be done (no volume, more than one component, or nothing at or above the threshold)
        vtkSmartPointer<vtkImageData> image;
        /// The number of voxels that were taken out
        long long removedVoxels = 0;
        long long keptVoxels = 0;
    };

    /// Takes out the voxels at or above the threshold that are not part of the largest group of them (voxels are in a group when they touch by a face).
    /// This leaves the body without the table, or the vessel tree without the loose spots.
    static Result keepLargestPiece(vtkImageData *volume, double threshold);

    /// Takes out the voxels whose center is inside a sphere (center and radius in the units of the volume, millimeters).
    static Result eraseSphere(vtkImageData *volume, const double center[3], double radius);

    /// Takes out the voxels whose center is outside an axis aligned box (xMin, xMax, yMin, yMax, zMin, zMax, in millimeters).
    static Result keepBox(vtkImageData *volume, const double bounds[6]);
};

} // End namespace udg

#endif
