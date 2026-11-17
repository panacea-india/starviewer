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

#ifndef UDGRIGIDREGISTRATION_H
#define UDGRIGIDREGISTRATION_H

#include <QString>

#include <vtkSmartPointer.h>

class vtkImageData;

namespace udg {

/**
    Rigid registration of two volumes: it finds the rotation and the translation that put the moving volume over the fixed one. It is the
    base of the fusion of volumes that were not acquired together. It knows nothing about the interface and works on vtkImageData, in the
    coordinates of the volumes (origin and spacing, in mm).

    The method is ITK's: the centers of the volumes are aligned, and a rigid transform (a versor and a translation) is optimized in three resolutions,
    from a quarter of the voxels to all of them. The metric is mutual information, which works between modalities (MR T1 with MR T2, CT with PET), or
    the mean of the squared differences, which is faster and only right for images of the same kind.

    A result can be wrong without failing, as the optimization finds the nearest good match: the quality of the match is in the result, and whoever
    uses it has to show the user that the volume is registered and not the original.
 */
class RigidRegistration {
public:
    enum class Metric { MutualInformation, MeanSquares };

    struct Options {
        Options();

        Metric metric;
        /// The number of resolution levels, from the coarsest (a factor of 2^(levels-1)) to all the voxels. From 1 to 4.
        int levels;
        /// The most iterations of the optimizer in the coarsest level, and in the finest one the least
        int maximumIterationsCoarse;
        int maximumIterationsFine;
        /// The fraction of the voxels of the fixed volume that the metric looks at, from 0.01 to 1
        double samplingFraction;
        /// The number of bins of the histogram of mutual information
        int histogramBins;
    };

    struct Result {
        Result();

        /// True if the optimization ran and gave a transform. It does not say that the volumes match: see the metric and the transform.
        bool success;
        /// What went wrong, if it did not succeed.
        QString message;
        /// The transform from points of the fixed volume to points of the moving volume, as a 4 x 4 matrix by rows (the last row is 0 0 0 1).
        double matrix[16];
        /// The rotation of the transform in degrees, and the length of its translation in mm.
        double rotationDegrees;
        double translationMillimeters;
        /// The value of the metric at the end, where lower is better (mutual information is negated).
        double metricValue;
        /// The iterations that the finest level took.
        int iterations;
    };

    /// Registers the moving volume to the fixed one. Both must have a single component, and cover the same patient region at least in part.
    static Result run(vtkImageData *fixed, vtkImageData *moving, const Options &options = Options());

    /// Returns the point of the moving volume that corresponds to a point of the fixed one, by the matrix of a result.
    static void transformPoint(const double matrix[16], const double fixedPoint[3], double movingPoint[3]);

    /// Returns the moving volume seen from the fixed one: a volume with the grid of the fixed one (origin, spacing and size) where each voxel has the
    /// value of the moving volume at the point that the matrix gives, by linear interpolation. Where the moving volume does not reach, the value is the
    /// smallest one of the moving volume. The type of the scalars is the one of the moving volume.
    static vtkSmartPointer<vtkImageData> resample(vtkImageData *moving, vtkImageData *fixed, const double matrix[16]);
};

} // End namespace udg

#endif
