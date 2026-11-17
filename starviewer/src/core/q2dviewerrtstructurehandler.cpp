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

#include "q2dviewerrtstructurehandler.h"

#include <cmath>

#include "drawer.h"
#include "drawerpolyline.h"
#include "drawertext.h"
#include "image.h"
#include "imageorientation.h"
#include "logging.h"
#include "orthogonalplane.h"
#include "q2dviewer.h"
#include "rtstructureset.h"
#include "series.h"
#include "study.h"
#include "volume.h"

namespace udg {

namespace {

const char* const GroupName = "RtStructure";

}

Q2DViewerRtStructureHandler::Q2DViewerRtStructureHandler(Q2DViewer *viewer)
    : QObject(viewer), m_viewer(viewer), m_enabled(true), m_drawnContours(0)
{
}

Q2DViewerRtStructureHandler::~Q2DViewerRtStructureHandler()
{
}

void Q2DViewerRtStructureHandler::setEnabled(bool enabled)
{
    m_enabled = enabled;

    if (m_enabled)
    {
        m_viewer->getDrawer()->enableGroup(GroupName);
    }
    else
    {
        m_viewer->getDrawer()->disableGroup(GroupName);
    }

    m_viewer->render();
}

bool Q2DViewerRtStructureHandler::isEnabled() const
{
    return m_enabled;
}

int Q2DViewerRtStructureHandler::getNumberOfDrawnContours() const
{
    return m_drawnContours;
}

void Q2DViewerRtStructureHandler::clear()
{
    for (const QPointer<DrawerPrimitive> &primitive : m_primitives)
    {
        // Deleting it takes it out of the Drawer
        delete primitive.data();
    }

    m_primitives.clear();
    m_drawnContours = 0;
}

void Q2DViewerRtStructureHandler::rebuild()
{
    clear();

    Volume *volume = m_viewer->getInput(0);

    if (!volume || volume->getNumberOfPhases() != 1 || volume->getImages().isEmpty())
    {
        return;
    }

    Image *first = volume->getImage(0);
    Series *series = first ? first->getParentSeries() : nullptr;
    Study *study = series ? series->getParentStudy() : nullptr;

    if (!study || study->getRtStructureSets().isEmpty() || !first->getPixelSpacing().isValid())
    {
        return;
    }

    // The plane of the first image is the reference for every slice: the volume has all its slices with the same in-plane position, also when they were straightened
    RtStructureSet::PlaneGeometry geometry;
    const double *position = first->getImagePositionPatient();
    ImageOrientation orientation = first->getImageOrientationPatient();

    for (int i = 0; i < 3; i++)
    {
        geometry.origin[i] = position[i];
    }

    geometry.row = {{orientation.getRowVector().x(), orientation.getRowVector().y(), orientation.getRowVector().z()}};
    geometry.column = {{orientation.getColumnVector().x(), orientation.getColumnVector().y(), orientation.getColumnVector().z()}};
    geometry.spacingX = first->getPixelSpacing().x();
    geometry.spacingY = first->getPixelSpacing().y();

    double origin[3];
    double spacing[3];
    volume->getOrigin(origin);
    volume->getSpacing(spacing);

    QList<Image*> images = volume->getImages();
    QString frameOfReference = series->getFrameOfReferenceUID();

    for (const RtStructureSet &set : study->getRtStructureSets())
    {
        if (!set.isInFrameOfReference(frameOfReference))
        {
            continue;
        }

        for (int slice = 0; slice < images.size(); slice++)
        {
            const double *slicePosition = images.at(slice)->getImagePositionPatient();
            std::array<double, 3> point = {{slicePosition[0], slicePosition[1], slicePosition[2]}};
            double depth = RtStructureSet::depthOf(geometry, point);
            // A contour belongs to the slice that is nearest, so each one is on a single slice
            double tolerance = 0.5 * std::abs(spacing[2]) * 1.001;

            for (const RtStructureSet::DrawnContour &contour : set.contoursAtDepth(geometry, depth, tolerance))
            {
                DrawerPolyline *polyline = new DrawerPolyline();

                for (const QPointF &pixel : contour.pixels)
                {
                    double world[3] = {origin[0] + pixel.x() * spacing[0], origin[1] + pixel.y() * spacing[1], origin[2] + slice * spacing[2]};
                    polyline->addPoint(world);
                }

                polyline->setColor(contour.color);
                polyline->setErasable(false);
                m_viewer->getDrawer()->draw(polyline, OrthogonalPlane::XYPlane, slice);
                m_viewer->getDrawer()->addToGroup(polyline, GroupName);
                m_primitives << QPointer<DrawerPrimitive>(polyline);
                m_drawnContours++;
            }
        }
    }

    INFO_LOG(QString("Drew %1 contours of the RT structure sets on the images").arg(m_drawnContours));
    setEnabled(m_enabled);
}

} // End namespace udg
