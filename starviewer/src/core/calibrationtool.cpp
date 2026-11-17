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

#include "calibrationtool.h"

#include <cmath>

#include <QInputDialog>
#include <QMessageBox>

#include <vtkCommand.h>
#include <vtkRenderWindowInteractor.h>

#include "drawer.h"
#include "drawerline.h"
#include "drawerpoint.h"
#include "image.h"
#include "logging.h"
#include "pixelspacing2d.h"
#include "q2dviewer.h"
#include "starviewerapplication.h"
#include "volume.h"

namespace udg {

CalibrationTool::CalibrationTool(QViewer *viewer, QObject *parent)
    : Tool(viewer, parent), m_numberOfPoints(0)
{
    m_toolName = "CalibrationTool";
    m_2DViewer = Q2DViewer::castFromQViewer(viewer);
    connect(m_2DViewer, SIGNAL(volumeChanged(Volume*)), SLOT(reset()));
}

CalibrationTool::~CalibrationTool()
{
}

double CalibrationTool::pixelSizeFor(const double first[3], const double second[3], const double dataSpacing[3], double realLengthMillimeters)
{
    if (realLengthMillimeters <= 0.0 || !std::isfinite(realLengthMillimeters) || dataSpacing[0] <= 0.0 || dataSpacing[1] <= 0.0)
    {
        return 0.0;
    }

    double dx = (second[0] - first[0]) / dataSpacing[0];
    double dy = (second[1] - first[1]) / dataSpacing[1];
    double pixels = std::sqrt(dx * dx + dy * dy);
    return pixels > 1e-9 ? realLengthMillimeters / pixels : 0.0;
}

void CalibrationTool::reset()
{
    m_numberOfPoints = 0;

    for (DrawerPrimitive *mark : m_marks)
    {
        delete mark;
    }

    m_marks.clear();
}

void CalibrationTool::addPoint()
{
    double point[3];

    if (m_numberOfPoints >= 2 || !m_2DViewer->getCurrentCursorImageCoordinateOnInput(point, 0))
    {
        return;
    }

    for (int i = 0; i < 3; i++)
    {
        m_points[m_numberOfPoints][i] = point[i];
    }

    m_numberOfPoints++;

    DrawerPoint *mark = new DrawerPoint();
    mark->setPosition(point);
    mark->setErasable(false);
    m_2DViewer->getDrawer()->draw(mark, m_2DViewer->getView(), m_2DViewer->getCurrentSlice());
    m_marks << mark;

    if (m_numberOfPoints == 2)
    {
        DrawerLine *line = new DrawerLine();
        line->setFirstPoint(m_points[0]);
        line->setSecondPoint(m_points[1]);
        line->setErasable(false);
        m_2DViewer->getDrawer()->draw(line, m_2DViewer->getView(), m_2DViewer->getCurrentSlice());
        m_marks << line;
        finish();
    }
}

void CalibrationTool::finish()
{
    Volume *volume = m_2DViewer->getMainInput();
    Image *current = m_2DViewer->getCurrentDisplayedImage();

    if (!volume || !current)
    {
        reset();
        return;
    }

    // Only images with no size of pixels of their own are calibrated: the others have it from the DICOM file
    if (current->getPixelSpacing().isValid() && !current->isManuallyCalibrated())
    {
        QMessageBox::information(m_2DViewer, ApplicationNameString, tr("These images already have the size of their pixels, from the DICOM file. They are not calibrated by hand."));
        reset();
        return;
    }

    bool accepted = false;
    double length = QInputDialog::getDouble(m_2DViewer, tr("Calibrate"), tr("The real length of what you marked, in mm:"), 10.0, 0.001, 100000.0, 3, &accepted);

    if (!accepted)
    {
        reset();
        return;
    }

    double spacing[3];
    volume->getSpacing(spacing);
    double size = pixelSizeFor(m_points[0], m_points[1], spacing, length);
    reset();

    if (size <= 0.0)
    {
        QMessageBox::warning(m_2DViewer, ApplicationNameString, tr("The calibration could not be done: the two points are the same."));
        return;
    }

    for (Image *image : volume->getImages())
    {
        image->setPixelSpacing(size, size);
        image->setManuallyCalibrated(true);
    }

    INFO_LOG(QString("Calibrated %1 images by hand: pixels of %2 mm").arg(volume->getImages().size()).arg(size));
    QMessageBox::information(m_2DViewer, ApplicationNameString, tr("The pixels are now %1 mm. The new measurements of these images are in mm and are marked calibrated. "
                                                                     "Measurements made before are not changed: draw them again.").arg(size, 0, 'f', 4));
    m_2DViewer->render();
}

void CalibrationTool::handleEvent(unsigned long eventId)
{
    if (!m_2DViewer || !m_2DViewer->hasInput())
    {
        return;
    }

    switch (eventId)
    {
        case vtkCommand::LeftButtonPressEvent:
            addPoint();
            break;

        case vtkCommand::KeyPressEvent:
            if (m_2DViewer->getInteractor()->GetKeyCode() == 27)
            {
                reset();
            }

            break;
    }
}

} // End namespace udg
