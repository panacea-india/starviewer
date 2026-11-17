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

#ifndef UDGQ2DVIEWERRTSTRUCTUREHANDLER_H
#define UDGQ2DVIEWERRTSTRUCTUREHANDLER_H

#include <QColor>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QString>

namespace udg {

class DrawerPrimitive;
class Q2DViewer;

/**
    Draws the contours of the RT Structure Sets of the study on the images of the first input of a Q2DViewer, in the color of each region. A contour goes on the slice that is
    nearest to it, if it is within half the distance between slices, and only in the plane of the acquisition. The contours are drawn once, when the volume is shown. They can
    be turned off and on. The contours are in the patient coordinates, so they follow the images even if the volume was straightened for a gantry tilt.
 */
class Q2DViewerRtStructureHandler : public QObject {
    Q_OBJECT

public:
    explicit Q2DViewerRtStructureHandler(Q2DViewer *viewer);
    ~Q2DViewerRtStructureHandler();

    void setEnabled(bool enabled);
    bool isEnabled() const;

    /// The number of contours that were drawn for the volume that is shown (all slices).
    int getNumberOfDrawnContours() const;

    /// A region (organ or target) of the structure sets that have contours on the volume that is shown. Regions with the same number in two sets are one region here.
    struct Region {
        int number = 0;
        QString name;
        QColor color;
        int contours = 0;
    };

    /// The regions that have contours drawn, ordered by number.
    QList<Region> getRegions() const;

    /// Shows or hides the contours of one region (all are shown at first, and after the volume changes).
    void setRegionVisible(int number, bool visible);
    bool isRegionVisible(int number) const;

    /// Draws the closed contours filled with their color and some transparency (or only the outline, which is the default).
    void setFilled(bool filled);
    bool isFilled() const;

public slots:
    /// Draws the contours again for the first input. To be called when new volumes have been shown.
    void rebuild();

private:
    void clear();
    void applyVisibility();
    static QString groupName(int regionNumber);

private:
    Q2DViewer *m_viewer;
    QList<QPointer<DrawerPrimitive> > m_primitives;
    bool m_enabled;
    int m_drawnContours;
    bool m_filled;
    QSet<int> m_hiddenRegions;
    QList<Region> m_regions;
};

} // End namespace udg

#endif
