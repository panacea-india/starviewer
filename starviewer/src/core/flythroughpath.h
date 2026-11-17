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

#ifndef UDGFLYTHROUGHPATH_H
#define UDGFLYTHROUGHPATH_H

#include <QList>
#include <QVector>

#include "savedview.h"

namespace udg {

/**
    The cameras of a flight along a path (a virtual endoscopy): the path is a smooth curve through the given points (x, y, z in mm, three numbers per point), the camera goes along it at an even
    speed looking ahead along the curve, and its up direction turns as little as possible (so the picture does not spin). The path comes from the points the user clicked with the curved MPR tool.
    It holds no pointers, and so it is tested without a window.
 */
class FlythroughPath {
public:
    /// The cameras, one per frame, from the first point to the last. The camera looks lookAheadMillimeters ahead along the direction of the path (a perspective camera with the given view angle in degrees).
    /// Empty if there are fewer than 2 different points, fewer than 2 frames, or the look-ahead or the angle are not positive (and finite).
    static QList<SavedView> views(const QVector<double> &points, int frames, double lookAheadMillimeters, double viewAngleDegrees);

    /// The length in mm of the smooth curve through the points (0 if there are fewer than 2 different points).
    static double length(const QVector<double> &points);

    /// The last path that was drawn with the curved MPR tool, to fly along it in the 3D viewer. Empty if there is none.
    static void setLastPath(const QVector<double> &points);
    static QVector<double> lastPath();
};

} // End namespace udg

#endif
