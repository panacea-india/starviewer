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

#ifndef UDGCURVEDREFORMATION_H
#define UDGCURVEDREFORMATION_H

#include <QString>
#include <QVector>

#include <vtkSmartPointer.h>

class vtkImageData;

namespace udg {

/**
    Curved planar reformation (curved MPR): the volume straightened along a path, so that a winding structure (a vessel, a bowel, the spine, a root canal) is seen
    along its whole length in one image. It knows nothing about the interface and works on vtkImageData, in the coordinates of the volume (mm).

    The path goes through the given points as a smooth curve (a centripetal Catmull-Rom spline), and is sampled at even steps. At each step the volume is read along a line
    perpendicular to the path, in two directions: the normal (which is the one of a reference vector, the z of the volume by default, taken perpendicular to the path) and
    the binormal (perpendicular to both). That gives two images, which are two views of the structure turned a quarter of a turn from each other.

    The images are straight: the position along the path is the row, and the distance to the path is the column, with the path in the middle column. Distances along the
    path are exact (the spacing of the rows is the step), but the distances across are only so in the plane of each image, and the reformation does not show how the path bends.
 */
class CurvedReformation {
public:
    struct Options {
        Options();

        /// The distance between two rows of the images, in mm. If it is 0 or less, the smallest spacing of the volume.
        double step;
        /// The width of the images, in mm, and the distance between two columns (0 or less is the same as the step)
        double width;
        double columnSpacing;
        /// The direction that the normal is taken from, which must not be the one of the path (if it is, another one is used there)
        double reference[3];
    };

    struct Result {
        Result();

        bool success;
        /// What went wrong, if it did not succeed.
        QString message;
        /// The images, of one component of floats, with the columns across and the rows along the path. The spacing is the one of the options, and the origin of the
        /// columns is -width / 2, so that the path is at 0.
        vtkSmartPointer<vtkImageData> normalImage;
        vtkSmartPointer<vtkImageData> binormalImage;
        /// The length of the curve, in mm
        double length;
        /// The points of the path that each row reads from, as x, y, z one after the other, and the unit tangent of the path there
        QVector<double> centers;
        QVector<double> tangents;
        int rows;
        int columns;
    };

    /// Straightens the volume along the path through the given points (x, y, z of each one, one after the other, at least two). The points are in the coordinates of the volume.
    static Result straighten(vtkImageData *volume, const double *points, int numberOfPoints, const Options &options = Options());

    /// Returns the length of the curve through the points, in mm, as the reformation takes it.
    static double curveLength(const double *points, int numberOfPoints);
};

} // End namespace udg

#endif
