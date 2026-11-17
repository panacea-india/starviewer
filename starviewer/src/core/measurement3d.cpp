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

#include "measurement3d.h"

#include <algorithm>
#include <cmath>

namespace udg {

namespace {

const double Pi = 3.14159265358979323846;

}

double Measurement3D::distance(const double a[3], const double b[3])
{
    double dx = a[0] - b[0];
    double dy = a[1] - b[1];
    double dz = a[2] - b[2];
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

double Measurement3D::angleDegrees(const double vertex[3], const double a[3], const double b[3])
{
    double u[3] = {a[0] - vertex[0], a[1] - vertex[1], a[2] - vertex[2]};
    double v[3] = {b[0] - vertex[0], b[1] - vertex[1], b[2] - vertex[2]};
    double lengthU = std::sqrt(u[0] * u[0] + u[1] * u[1] + u[2] * u[2]);
    double lengthV = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);

    if (lengthU == 0.0 || lengthV == 0.0)
    {
        return -1.0;
    }

    double cosine = (u[0] * v[0] + u[1] * v[1] + u[2] * v[2]) / (lengthU * lengthV);
    // Rounding can put the cosine of parallel segments just beyond 1
    cosine = std::max(-1.0, std::min(1.0, cosine));
    return std::acos(cosine) * 180.0 / Pi;
}

double Measurement3D::pathLength(const double *points, int numberOfPoints)
{
    double length = 0.0;

    for (int i = 1; i < numberOfPoints; i++)
    {
        length += distance(points + 3 * (i - 1), points + 3 * i);
    }

    return length;
}

} // End namespace udg
