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

#ifndef UDGMEASUREMENT3D_H
#define UDGMEASUREMENT3D_H

namespace udg {

/**
    Measurements between points in 3D, in the coordinates of a volume (mm): the distance between two points that can be on different slices, the angle at a vertex, and
    the length of a path. It is plain geometry, with no interface, so that its results can be checked against objects of known size.

    The accuracy of a measurement is the one of the points: a point is placed on a voxel, so each end of a distance can be wrong by up to half a voxel in each direction.
 */
class Measurement3D {
public:
    /// Returns the distance between two points, in the unit of the coordinates.
    static double distance(const double a[3], const double b[3]);

    /// Returns the angle at the vertex between the segments to the points a and b, in degrees from 0 to 180. It returns -1 if a segment has no length.
    static double angleDegrees(const double vertex[3], const double a[3], const double b[3]);

    /// Returns the length of the path that goes through the given points in order. The points are given as x, y, z of each one, one after the other.
    static double pathLength(const double *points, int numberOfPoints);
};

} // End namespace udg

#endif
