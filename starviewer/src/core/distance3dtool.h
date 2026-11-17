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

#ifndef UDGDISTANCE3DTOOL_H
#define UDGDISTANCE3DTOOL_H

#include <QPointer>

#include "tool.h"

namespace udg {

class DrawerPoint;
class DrawerText;
class Q2DViewer;

/**
    Measures the distance in 3D between two points, which can be on different slices: the user clicks the first point, scrolls to another slice, and clicks the second. The
    distance, and how much of it is in x, y and z, is written next to the second point. A mark stays on the slice of each point, and the eraser takes them away.
    Escape forgets the first point.
 */
class Distance3DTool : public Tool {
    Q_OBJECT

public:
    explicit Distance3DTool(QViewer *viewer, QObject *parent = nullptr);
    ~Distance3DTool() override;

    void handleEvent(unsigned long eventId) override;

private slots:
    /// Forgets the first point, as the volume has changed
    void reset();

private:
    void placeMark(const double point[3], int slice);

private:
    Q2DViewer *m_2DViewer;
    bool m_hasFirstPoint;
    double m_firstPoint[3];
    QPointer<DrawerPoint> m_firstMark;
};

} // End namespace udg

#endif
