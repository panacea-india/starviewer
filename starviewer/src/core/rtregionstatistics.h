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

#ifndef UDGRTREGIONSTATISTICS_H
#define UDGRTREGIONSTATISTICS_H

#include "rtstructureset.h"

namespace udg {

/**
    The volume of a region of an RT structure set, from its contours: the area of the closed contours of each plane, times the distance between planes (the middle distance of the planes,
    so a plane that was left out does not change it much). Open contours are left out. A contour that is inside another one of the same plane is a hole (inside a hole, a piece again, and so on), and its area is taken away. The numbers are in mm and mL (1 mL = 1000 mm3).
 */
class RtRegionStatistics {
public:
    struct Result {
        /// Closed contours used, and the planes they are in
        int contours = 0;
        int planes = 0;
        /// The distance between planes used, in mm (0 if there is one plane or none)
        double planeSpacingMm = 0.0;
        /// 0 if there is less than two planes: the thickness of a single plane is not known
        double volumeMl = 0.0;
    };

    static Result compute(const RtStructureSet::Roi &roi);

    /// The area in mm2 of a closed planar polygon in space (the contour is closed by the last point and the first one). 0 for fewer than 3 points.
    static double area(const QList<std::array<double, 3>> &points);

    /// True if the point (projected on the plane of the contour) is inside the contour.
    static bool contains(const RtStructureSet::Contour &contour, const std::array<double, 3> &point);
};

} // End namespace udg

#endif
