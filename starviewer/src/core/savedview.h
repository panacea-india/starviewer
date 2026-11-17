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

#ifndef UDGSAVEDVIEW_H
#define UDGSAVEDVIEW_H

#include <array>

#include <QJsonObject>
#include <QList>
#include <QString>

namespace udg {

/**
    A camera of a 3D view that has a name, so that the user can come back to it, and the geometry to move a camera from one view to another or around a volume
    (for a movie). It holds no pointers, and so it is tested without a window.
 */
class SavedView {
public:
    SavedView();

    QString name;
    std::array<double, 3> position;
    std::array<double, 3> focalPoint;
    std::array<double, 3> viewUp;
    double viewAngle;
    bool parallelProjection;
    double parallelScale;

    /// The view as a JSON object, and the view read back from one. The view read is valid only if the object has all the fields and the numbers are finite.
    QJsonObject toJson() const;
    static SavedView fromJson(const QJsonObject &object, bool *valid = nullptr);

    /// The view between two views, for t from 0 (the first) to 1 (the second). The focal point moves in a line, and the camera moves around it in an arc with a
    /// distance that goes from one to the other, so that two views on a circle around the volume have the arc between them and not the chord.
    static SavedView interpolate(const SavedView &first, const SavedView &second, double t);

    /// The view turned around the focal point, around the up direction, by the given number of degrees (as seen from above, positive is counterclockwise).
    SavedView orbited(double degrees) const;

    /// The views of a movie that goes through the given views, with the given number of frames in all, spread evenly along the path (by the parameter, with the
    /// same number of frames between each two). With one view, all frames are that view.
    static QList<SavedView> path(const QList<SavedView> &keys, int frames);

    /// The views of a movie that turns once around the volume, with the given number of frames; the last one is not the first again, so that the movie can loop.
    QList<SavedView> turntable(int frames) const;
};

} // End namespace udg

#endif
