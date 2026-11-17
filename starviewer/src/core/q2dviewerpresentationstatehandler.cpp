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

#include "q2dviewerpresentationstatehandler.h"

#include <cmath>

#include <QColor>

#include "drawer.h"
#include "drawerpoint.h"
#include "drawerpolygon.h"
#include "drawerpolyline.h"
#include "drawertext.h"
#include "image.h"
#include "logging.h"
#include "orthogonalplane.h"
#include "q2dviewer.h"
#include "series.h"
#include "study.h"
#include "volume.h"

namespace udg {

namespace {

const char* const GroupName = "PresentationState";
const QColor AnnotationColor(255, 255, 0);
const int EllipseSegments = 72;
const double Pi = 3.14159265358979323846;

}

Q2DViewerPresentationStateHandler::Q2DViewerPresentationStateHandler(Q2DViewer *viewer)
    : QObject(viewer), m_viewer(viewer), m_enabled(true)
{
}

Q2DViewerPresentationStateHandler::~Q2DViewerPresentationStateHandler()
{
}

void Q2DViewerPresentationStateHandler::setEnabled(bool enabled)
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

bool Q2DViewerPresentationStateHandler::isEnabled() const
{
    return m_enabled;
}

void Q2DViewerPresentationStateHandler::clear()
{
    for (const QPointer<DrawerPrimitive> &primitive : m_primitives)
    {
        // Deleting it takes it out of the Drawer
        delete primitive.data();
    }

    m_primitives.clear();
}

void Q2DViewerPresentationStateHandler::rebuild()
{
    clear();

    Volume *volume = m_viewer->getInput(0);

    if (!volume || volume->getNumberOfPhases() != 1)
    {
        return;
    }

    double origin[3];
    double spacing[3];
    volume->getOrigin(origin);
    volume->getSpacing(spacing);

    QList<Image*> images = volume->getImages();

    for (int slice = 0; slice < images.size(); slice++)
    {
        drawAnnotations(images.at(slice), slice, origin, spacing);
    }

    setEnabled(m_enabled);
}

QPointF Q2DViewerPresentationStateHandler::toPixels(const QPointF &point, GraphicPresentationState::Units units, const Image *image) const
{
    if (units == GraphicPresentationState::Units::Display)
    {
        // Display units run from 0 to 1 over the displayed area, which is the whole image here
        return QPointF(point.x() * image->getColumns(), point.y() * image->getRows());
    }

    return point;
}

void Q2DViewerPresentationStateHandler::addPrimitive(DrawerPrimitive *primitive, int slice)
{
    primitive->setColor(AnnotationColor);
    primitive->setErasable(false);
    m_viewer->getDrawer()->draw(primitive, OrthogonalPlane::XYPlane, slice);
    m_viewer->getDrawer()->addToGroup(primitive, GroupName);
    m_primitives << QPointer<DrawerPrimitive>(primitive);
}

void Q2DViewerPresentationStateHandler::drawAnnotations(Image *image, int slice, const double origin[3], const double spacing[3])
{
    Series *series = image->getParentSeries();
    Study *study = series ? series->getParentStudy() : nullptr;

    if (!study)
    {
        return;
    }

    int frame = image->getFrameNumber() + 1;

    // The image is in the volume with its pixel (0, 0) in the origin, and the pixel centers are half a pixel inside the coordinates of the presentation state
    auto toWorld = [&](const QPointF &pixel, double world[3]) {
        world[0] = origin[0] + (pixel.x() - 0.5) * spacing[0];
        world[1] = origin[1] + (pixel.y() - 0.5) * spacing[1];
        world[2] = origin[2] + slice * spacing[2];
    };

    for (const GraphicPresentationState &state : study->getPresentationStatesReferencing(image->getSOPInstanceUID()))
    {
        for (const GraphicPresentationState::Annotation &annotation : state.getAnnotationsFor(image->getSOPInstanceUID(), frame))
        {
            for (const GraphicPresentationState::GraphicObject &object : annotation.graphics)
            {
                QList<QPointF> pixels;

                for (const QPointF &point : object.points)
                {
                    pixels << toPixels(point, object.units, image);
                }

                double world[3];

                switch (object.type)
                {
                    case GraphicPresentationState::GraphicObject::Type::Point:
                    {
                        DrawerPoint *point = new DrawerPoint();
                        toWorld(pixels.first(), world);
                        point->setPosition(world);
                        addPrimitive(point, slice);
                        break;
                    }

                    case GraphicPresentationState::GraphicObject::Type::Polyline:
                    case GraphicPresentationState::GraphicObject::Type::Interpolated:
                    {
                        bool closed = pixels.size() > 2 && pixels.first() == pixels.last();

                        if (object.filled && closed)
                        {
                            DrawerPolygon *polygon = new DrawerPolygon();

                            for (const QPointF &pixel : pixels)
                            {
                                toWorld(pixel, world);
                                polygon->addVertix(world);
                            }

                            polygon->setFilled(true);
                            addPrimitive(polygon, slice);
                        }
                        else
                        {
                            DrawerPolyline *polyline = new DrawerPolyline();

                            for (const QPointF &pixel : pixels)
                            {
                                toWorld(pixel, world);
                                polyline->addPoint(world);
                            }

                            addPrimitive(polyline, slice);
                        }

                        break;
                    }

                    case GraphicPresentationState::GraphicObject::Type::Circle:
                    case GraphicPresentationState::GraphicObject::Type::Ellipse:
                    {
                        // A circle has its center and a point on it. An ellipse has the ends of its major axis, then the ends of its minor axis.
                        QPointF center;
                        QPointF majorHalf;
                        QPointF minorHalf;

                        if (object.type == GraphicPresentationState::GraphicObject::Type::Circle)
                        {
                            center = pixels.at(0);
                            majorHalf = pixels.at(1) - center;
                            minorHalf = QPointF(-majorHalf.y(), majorHalf.x());
                        }
                        else
                        {
                            center = (pixels.at(0) + pixels.at(1)) / 2.0;
                            majorHalf = (pixels.at(1) - pixels.at(0)) / 2.0;
                            minorHalf = (pixels.at(3) - pixels.at(2)) / 2.0;
                        }

                        DrawerPolyline *outline = new DrawerPolyline();

                        for (int i = 0; i <= EllipseSegments; i++)
                        {
                            double angle = 2.0 * Pi * i / EllipseSegments;
                            QPointF pixel = center + majorHalf * std::cos(angle) + minorHalf * std::sin(angle);
                            toWorld(pixel, world);
                            outline->addPoint(world);
                        }

                        addPrimitive(outline, slice);
                        break;
                    }
                }
            }

            for (const GraphicPresentationState::TextObject &textObject : annotation.texts)
            {
                if (textObject.text.isEmpty() || (!textObject.hasAnchor && !textObject.hasBox))
                {
                    continue;
                }

                QPointF pixel = textObject.hasAnchor ? toPixels(textObject.anchor, textObject.anchorUnits, image)
                                                     : toPixels(textObject.boxTopLeft, textObject.boxUnits, image);
                double world[3];
                toWorld(pixel, world);

                DrawerText *text = new DrawerText();
                text->setText(textObject.text);
                text->setAttachmentPoint(world);
                addPrimitive(text, slice);
            }
        }
    }
}

} // End namespace udg
