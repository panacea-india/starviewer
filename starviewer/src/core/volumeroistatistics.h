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

#ifndef UDGVOLUMEROISTATISTICS_H
#define UDGVOLUMEROISTATISTICS_H

#include <vtkSmartPointer.h>

class vtkImageData;

namespace udg {

/**
    Statistics of a region of a whole volume (a VOI): its volume, and the mean, the extremes and the deviation of the values in it. It is the 3D
    counterpart of the ROI of a slice, and the base of the SUV statistics of PET (SUVmax, SUVmean, SUVpeak, metabolic volume). It knows nothing about the interface.

    The region is a mask, a volume of the same size as the values where a voxel that is not zero belongs to it. The values can be scaled while they are
    read, for example by the factor that turns the stored values of a PET into SUV.
 */
class VolumeRoiStatistics {
public:
    struct Statistics {
        Statistics();

        /// The number of voxels of the region, and its volume in milliliters
        long long voxels;
        double volumeMilliliters;
        double mean;
        double minimum;
        double maximum;
        double standardDeviation;
        /// The mean of the values in the sphere of 1 mL that has the highest mean, around a voxel of the region (SUVpeak). It is 0 if it was not asked for.
        double peak;
        /// The center of the voxel of the highest value, in the coordinates of the volume
        double maximumPosition[3];
    };

    /// The statistics of the voxels of the volume that the mask has not set to zero. The mask must have the same dimensions as the values. Each value is
    /// multiplied by the scale before it is used. With peak, SUVpeak is computed too, which takes longer. It returns null statistics (0 voxels) if the region is empty
    /// or the mask does not match the values.
    static Statistics compute(vtkImageData *values, vtkImageData *mask, double scale = 1.0, bool peak = false);

    /// Returns a mask of the voxels of the volume whose centers are in the sphere of the given center (in the coordinates of the volume) and radius, both in mm.
    static vtkSmartPointer<vtkImageData> createSphereMask(vtkImageData *like, const double center[3], double radius);

    /// Returns a mask of the voxels of the region that have a value (scaled) of at least the threshold. Typically the region is a sphere around a lesion, and the
    /// threshold a fraction of its maximum, which gives the metabolic volume of the lesion.
    static vtkSmartPointer<vtkImageData> createThresholdMask(vtkImageData *values, vtkImageData *region, double scale, double threshold);
};

} // End namespace udg

#endif
