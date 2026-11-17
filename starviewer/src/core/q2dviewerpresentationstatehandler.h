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

#ifndef UDGQ2DVIEWERPRESENTATIONSTATEHANDLER_H
#define UDGQ2DVIEWERPRESENTATIONSTATEHANDLER_H

#include <QList>
#include <QObject>
#include <QPointer>

#include "graphicpresentationstate.h"

namespace udg {

class DrawerPrimitive;
class Image;
class Q2DViewer;

/**
    Draws the graphic annotations of the Softcopy Presentation States on the images of the first input of a Q2DViewer that they refer to.
    The Drawer shows each primitive only on its slice, so it is drawn once when the volume is shown. The annotations can be turned off and on.
    They are drawn only in the plane of the acquisition (the one that the images were stored in), as the coordinates are those of the images.
 */
class Q2DViewerPresentationStateHandler : public QObject {
    Q_OBJECT

public:
    explicit Q2DViewerPresentationStateHandler(Q2DViewer *viewer);
    ~Q2DViewerPresentationStateHandler();

    /// Turns the annotations on or off.
    void setEnabled(bool enabled);
    bool isEnabled() const;

public slots:
    /// Draws the annotations of the images of the first input again. To be called when new volumes have been shown.
    void rebuild();

private:
    void clear();

    /// Draws the annotations of the given image, which is the slice-th of the volume.
    void drawAnnotations(Image *image, int slice, const double origin[3], const double spacing[3]);

    /// Returns the given point in pixels (the top left corner of the top left pixel is 0, 0).
    QPointF toPixels(const QPointF &point, GraphicPresentationState::Units units, const Image *image) const;

    void addPrimitive(DrawerPrimitive *primitive, int slice);

private:
    Q2DViewer *m_viewer;
    QList<QPointer<DrawerPrimitive> > m_primitives;
    bool m_enabled;
};

} // End namespace udg

#endif
