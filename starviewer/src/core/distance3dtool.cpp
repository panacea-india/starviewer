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

#include "distance3dtool.h"

#include <cmath>

#include <vtkCommand.h>
#include <vtkRenderWindowInteractor.h>

#include "drawer.h"
#include "drawerpoint.h"
#include "drawertext.h"
#include "measurement3d.h"
#include "q2dviewer.h"

namespace udg {

Distance3DTool::Distance3DTool(QViewer *viewer, QObject *parent)
    : Tool(viewer, parent), m_hasFirstPoint(false)
{
    m_toolName = "Distance3DTool";
    m_2DViewer = Q2DViewer::castFromQViewer(viewer);
    m_firstPoint[0] = m_firstPoint[1] = m_firstPoint[2] = 0.0;

    connect(m_2DViewer, SIGNAL(volumeChanged(Volume*)), SLOT(reset()));
}

Distance3DTool::~Distance3DTool()
{
}

void Distance3DTool::reset()
{
    m_hasFirstPoint = false;
}

void Distance3DTool::placeMark(const double point[3], int slice)
{
    DrawerPoint *mark = new DrawerPoint();
    double position[3] = {point[0], point[1], point[2]};
    mark->setPosition(position);
    mark->setRadius(1.5);
    m_2DViewer->getDrawer()->draw(mark, m_2DViewer->getView(), slice);
}

void Distance3DTool::handleEvent(unsigned long eventId)
{
    if (!m_2DViewer || !m_2DViewer->hasInput())
    {
        return;
    }

    if (eventId == vtkCommand::KeyPressEvent)
    {
        if (m_2DViewer->getInteractor()->GetKeyCode() == 27)
        {
            reset();
        }

        return;
    }

    if (eventId != vtkCommand::LeftButtonPressEvent)
    {
        return;
    }

    // The point under the cursor in the coordinates of the volume, with the depth of the slice that is shown
    double point[3];

    if (!m_2DViewer->getCurrentCursorImageCoordinateOnInput(point, 0))
    {
        return;
    }

    int slice = m_2DViewer->getCurrentSlice();

    if (!m_hasFirstPoint)
    {
        m_firstPoint[0] = point[0];
        m_firstPoint[1] = point[1];
        m_firstPoint[2] = point[2];
        m_hasFirstPoint = true;
        placeMark(point, slice);
        return;
    }

    placeMark(point, slice);

    double distance = Measurement3D::distance(m_firstPoint, point);
    DrawerText *text = new DrawerText();
    text->setText(tr("%1 mm (%2, %3, %4)").arg(distance, 0, 'f', 1).arg(std::abs(point[0] - m_firstPoint[0]), 0, 'f', 1)
                  .arg(std::abs(point[1] - m_firstPoint[1]), 0, 'f', 1).arg(std::abs(point[2] - m_firstPoint[2]), 0, 'f', 1));
    double position[3] = {point[0], point[1], point[2]};
    text->setAttachmentPoint(position);
    m_2DViewer->getDrawer()->draw(text, m_2DViewer->getView(), slice);

    m_hasFirstPoint = false;
}

} // End namespace udg
