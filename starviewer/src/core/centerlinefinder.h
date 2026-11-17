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

#ifndef UDGCENTERLINEFINDER_H
#define UDGCENTERLINEFINDER_H

#include <QString>
#include <QVector>

class vtkImageData;

namespace udg {

/**
    Finds the center line of a vessel, an airway or another tube-like structure between two points, so that the user gives only its two ends. The structure is what is connected to the
    start point and has values in a range (a vessel full of contrast is all brighter than a limit, for example). The path is the cheapest one through those voxels, where it is cheaper
    the farther it is from the border, so that it keeps to the middle and does not cut the corners. It works on vtkImageData in the coordinates of the volume (mm) and knows nothing of the interface.
 */
class CenterlineFinder {
public:
    struct Result {
        bool success = false;
        /// What went wrong, if it did not succeed.
        QString message;
        /// The path as x, y, z of each point one after the other (mm), from the start to the end, with about one point every 2 mm
        QVector<double> points;
        /// The length of the path in mm
        double lengthMm = 0.0;
        /// The number of voxels of the structure that is connected to the start point
        qint64 structureVoxels = 0;
    };

    /// Finds the path from start to end (x, y, z in mm), through the voxels with a value from lower to upper (both included). A point that is not in that range is moved to the nearest voxel that is,
    /// if there is one within 3 voxels. It fails if there is none, if the end is not connected to the start through such voxels, or if the data has not one component.
    static Result find(vtkImageData *data, const double start[3], const double end[3], double lower, double upper);

    /// find() with the range taken from the value around the start point, plus and minus the tolerance and, if that does not work (a point at the very end or at the border of the structure
    /// can have mostly what is next to it around), from the value around the end point; if both give a path, the one through the smaller structure is used (the other one is likely the tissue around it). The message of the first failure is kept if both fail.
    static Result findAutomatically(vtkImageData *data, const double start[3], const double end[3], double tolerance);

    /// The range of values to use for a structure from the value at a point: the median of the voxels around it (3 x 3 x 3) plus and minus the tolerance. Returns false if the point is outside the volume.
    static bool rangeAround(vtkImageData *data, const double point[3], double tolerance, double *lower, double *upper);
};

} // End namespace udg

#endif
