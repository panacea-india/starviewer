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

#include "timeintensitycurvetool.h"

#include <vtkCommand.h>
#include <vtkImageData.h>

#include <QMessageBox>

#include "orthogonalplane.h"
#include "q2dviewer.h"
#include "qtimeintensitycurvedialog.h"
#include "starviewerapplication.h"
#include "volume.h"

namespace udg {

TimeIntensityCurveTool::TimeIntensityCurveTool(QViewer *viewer, QObject *parent)
    : Tool(viewer, parent)
{
    m_toolName = "TimeIntensityCurveTool";
    m_2DViewer = Q2DViewer::castFromQViewer(viewer);
}

TimeIntensityCurveTool::~TimeIntensityCurveTool()
{
}

void TimeIntensityCurveTool::showCurve()
{
    Volume *volume = m_2DViewer->getMainInput();
    double point[3];

    if (!volume || !m_2DViewer->getCurrentCursorImageCoordinateOnInput(point, 0))
    {
        return;
    }

    if (volume->getNumberOfPhases() < 2 || m_2DViewer->getView() != OrthogonalPlane::XYPlane)
    {
        QMessageBox::information(m_2DViewer, ApplicationNameString, tr("The time-intensity curve needs a series with several phases (a dynamic study), in the axial view of the images."));
        return;
    }

    double origin[3];
    double spacing[3];
    volume->getOrigin(origin);
    volume->getSpacing(spacing);

    double x = (point[0] - origin[0]) / spacing[0];
    double y = (point[1] - origin[1]) / spacing[1];

    QTimeIntensityCurveDialog dialog(volume->getVtkData(), volume->getNumberOfPhases(), m_2DViewer->getCurrentSlice(), x, y, spacing[0], spacing[1], m_2DViewer);
    dialog.exec();
}

void TimeIntensityCurveTool::handleEvent(unsigned long eventId)
{
    if (!m_2DViewer || !m_2DViewer->hasInput())
    {
        return;
    }

    if (eventId == vtkCommand::LeftButtonPressEvent)
    {
        showCurve();
    }
}

} // End namespace udg
