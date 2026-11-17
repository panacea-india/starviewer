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

#ifndef UDGCALIBRATIONTOOL_H
#define UDGCALIBRATIONTOOL_H

#include <QList>

#include "tool.h"

namespace udg {

class DrawerPrimitive;
class Q2DViewer;

/**
    Sets the size of the pixels of images that have none (no Pixel Spacing and no Imager Pixel Spacing), by measuring something of known size in them (a ruler, a calibration ball):
    the user clicks its two ends and gives its real length in mm. The measurements of those images are then in mm, and are marked "calibrated". It is not for images that have a pixel
    size from the DICOM file: those are left as they are. Escape forgets the points.
 */
class CalibrationTool : public Tool {
    Q_OBJECT

public:
    explicit CalibrationTool(QViewer *viewer, QObject *parent = nullptr);
    ~CalibrationTool() override;

    void handleEvent(unsigned long eventId) override;

    /// The size of a pixel (mm) that makes the segment between the two points (in the coordinates of the volume, whose pixels have the given spacing) as long as realLengthMillimeters.
    /// Returns 0 if the points are the same or the length is not positive.
    static double pixelSizeFor(const double first[3], const double second[3], const double dataSpacing[3], double realLengthMillimeters);

private slots:
    void reset();

private:
    void addPoint();
    void finish();

private:
    Q2DViewer *m_2DViewer;
    double m_points[2][3];
    int m_numberOfPoints;
    QList<DrawerPrimitive*> m_marks;
};

} // End namespace udg

#endif
