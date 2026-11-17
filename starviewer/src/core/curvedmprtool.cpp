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

#include "curvedmprtool.h"

#include <vtkCommand.h>
#include <vtkImageData.h>
#include <vtkRenderWindowInteractor.h>

#include <QApplication>
#include <QMessageBox>

#include "curvedreformation.h"
#include "flythroughpath.h"
#include "drawer.h"
#include "drawerpoint.h"
#include "q2dviewer.h"
#include "qcurvedmprdialog.h"
#include "starviewerapplication.h"
#include "volume.h"
#include "voilut.h"

namespace udg {

CurvedMprTool::CurvedMprTool(QViewer *viewer, QObject *parent)
    : Tool(viewer, parent)
{
    m_toolName = "CurvedMprTool";
    m_2DViewer = Q2DViewer::castFromQViewer(viewer);

    connect(m_2DViewer, SIGNAL(volumeChanged(Volume*)), SLOT(reset()));
}

CurvedMprTool::~CurvedMprTool()
{
}

void CurvedMprTool::reset()
{
    m_points.clear();
}

void CurvedMprTool::addPoint()
{
    double point[3];

    // The point under the cursor in the coordinates of the volume, with the depth of the slice that is shown
    if (!m_2DViewer->getCurrentCursorImageCoordinateOnInput(point, 0))
    {
        return;
    }

    m_points << point[0] << point[1] << point[2];

    DrawerPoint *mark = new DrawerPoint();
    mark->setPosition(point);
    mark->setRadius(1.5);
    m_2DViewer->getDrawer()->draw(mark, m_2DViewer->getView(), m_2DViewer->getCurrentSlice());
}

void CurvedMprTool::finish()
{
    Volume *volume = m_2DViewer->getMainInput();

    if (m_points.size() < 6 || !volume || !volume->getVtkData())
    {
        return;
    }

    // The 3D viewer can fly along this path
    FlythroughPath::setLastPath(m_points);

    QApplication::setOverrideCursor(Qt::WaitCursor);
    CurvedReformation::Result result = CurvedReformation::straighten(volume->getVtkData(), m_points.constData(), m_points.size() / 3);
    QApplication::restoreOverrideCursor();

    m_points.clear();

    if (!result.success)
    {
        QMessageBox::warning(m_2DViewer, ApplicationNameString, tr("The curved MPR could not be made: %1").arg(result.message));
        return;
    }

    WindowLevel windowLevel = m_2DViewer->getCurrentVoiLut().getWindowLevel();
    QCurvedMprDialog dialog(result, windowLevel.getWidth(), windowLevel.getCenter(), m_2DViewer);
    dialog.exec();
}

void CurvedMprTool::handleEvent(unsigned long eventId)
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

        case vtkCommand::LeftButtonDoubleClickEvent:
            // The first click of the double click has added the point already
            finish();
            break;

        case vtkCommand::KeyPressEvent:
        {
            int keyCode = m_2DViewer->getInteractor()->GetKeyCode();

            if (keyCode == 13)
            {
                finish();
            }
            else if (keyCode == 27)
            {
                reset();
            }

            break;
        }
    }
}

} // End namespace udg
