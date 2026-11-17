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

#include "graphicpresentationstate.h"

#include <QObject>

#include "dicomsequenceattribute.h"
#include "dicomsequenceitem.h"
#include "dicomtagreader.h"

namespace udg {

namespace {

const char* const GrayscalePresentationStateUid = "1.2.840.10008.5.1.4.1.1.11.1";
const char* const ColorPresentationStateUid = "1.2.840.10008.5.1.4.1.1.11.2";
const char* const PseudoColorPresentationStateUid = "1.2.840.10008.5.1.4.1.1.11.3";

QList<double> parseNumbers(const QString &value)
{
    QList<double> numbers;

    for (const QString &part : value.split('\\', QString::SkipEmptyParts))
    {
        bool ok = false;
        double number = part.trimmed().toDouble(&ok);

        if (ok)
        {
            numbers << number;
        }
    }

    return numbers;
}

QList<int> parseFrames(const QString &value)
{
    QList<int> frames;

    for (double number : parseNumbers(value))
    {
        if (number >= 1.0)
        {
            frames << static_cast<int>(number);
        }
    }

    return frames;
}

GraphicPresentationState::Units parseUnits(const QString &value)
{
    return value.trimmed() == "DISPLAY" ? GraphicPresentationState::Units::Display : GraphicPresentationState::Units::Pixel;
}

QList<GraphicPresentationState::ReferencedImage> parseReferencedImages(DICOMSequenceAttribute *sequence)
{
    QList<GraphicPresentationState::ReferencedImage> images;

    if (!sequence)
    {
        return images;
    }

    for (DICOMSequenceItem *item : sequence->getItems())
    {
        GraphicPresentationState::ReferencedImage image;
        image.sopInstanceUid = item->getValueAttributeAsQString(DICOMReferencedSOPInstanceUID);
        image.frames = parseFrames(item->getValueAttributeAsQString(DICOMReferencedFrameNumber));

        if (!image.sopInstanceUid.isEmpty())
        {
            images << image;
        }
    }

    return images;
}

QList<QPointF> toPoints(const QList<double> &numbers)
{
    QList<QPointF> points;

    for (int i = 0; i + 1 < numbers.size(); i += 2)
    {
        points << QPointF(numbers.at(i), numbers.at(i + 1));
    }

    return points;
}

bool parseGraphicObject(DICOMSequenceItem *item, GraphicPresentationState::GraphicObject &object)
{
    QString type = item->getValueAttributeAsQString(DICOMGraphicType).trimmed();

    if (type == "POINT")
    {
        object.type = GraphicPresentationState::GraphicObject::Type::Point;
    }
    else if (type == "POLYLINE")
    {
        object.type = GraphicPresentationState::GraphicObject::Type::Polyline;
    }
    else if (type == "INTERPOLATED")
    {
        object.type = GraphicPresentationState::GraphicObject::Type::Interpolated;
    }
    else if (type == "CIRCLE")
    {
        object.type = GraphicPresentationState::GraphicObject::Type::Circle;
    }
    else if (type == "ELLIPSE")
    {
        object.type = GraphicPresentationState::GraphicObject::Type::Ellipse;
    }
    else
    {
        return false;
    }

    object.units = parseUnits(item->getValueAttributeAsQString(DICOMGraphicAnnotationUnits));
    object.filled = item->getValueAttributeAsQString(DICOMGraphicFilled).trimmed() == "Y";
    object.points = toPoints(parseNumbers(item->getValueAttributeAsQString(DICOMGraphicData)));

    int needed = 1;

    switch (object.type)
    {
        case GraphicPresentationState::GraphicObject::Type::Point:
            needed = 1;
            break;

        case GraphicPresentationState::GraphicObject::Type::Polyline:
        case GraphicPresentationState::GraphicObject::Type::Interpolated:
            needed = 2;
            break;

        case GraphicPresentationState::GraphicObject::Type::Circle:
            needed = 2;
            break;

        case GraphicPresentationState::GraphicObject::Type::Ellipse:
            needed = 4;
            break;
    }

    return object.points.size() >= needed;
}

GraphicPresentationState::TextObject parseTextObject(DICOMSequenceItem *item)
{
    GraphicPresentationState::TextObject text;
    text.text = item->getValueAttributeAsQString(DICOMUnformattedTextValue);

    QList<double> anchor = parseNumbers(item->getValueAttributeAsQString(DICOMAnchorPoint));
    text.hasAnchor = anchor.size() >= 2;
    text.anchor = text.hasAnchor ? QPointF(anchor.at(0), anchor.at(1)) : QPointF();
    text.anchorUnits = parseUnits(item->getValueAttributeAsQString(DICOMAnchorPointAnnotationUnits));

    QList<double> topLeft = parseNumbers(item->getValueAttributeAsQString(DICOMBoundingBoxTopLeftHandCorner));
    QList<double> bottomRight = parseNumbers(item->getValueAttributeAsQString(DICOMBoundingBoxBottomRightHandCorner));
    text.hasBox = topLeft.size() >= 2 && bottomRight.size() >= 2;
    text.boxTopLeft = text.hasBox ? QPointF(topLeft.at(0), topLeft.at(1)) : QPointF();
    text.boxBottomRight = text.hasBox ? QPointF(bottomRight.at(0), bottomRight.at(1)) : QPointF();
    text.boxUnits = parseUnits(item->getValueAttributeAsQString(DICOMBoundingBoxAnnotationUnits));

    return text;
}

}

GraphicPresentationState::GraphicPresentationState()
    : m_null(true)
{
}

GraphicPresentationState GraphicPresentationState::fromDICOM(const DICOMTagReader &reader, QString *error)
{
    auto fail = [error](const QString &reason) {
        if (error)
        {
            *error = reason;
        }
        return GraphicPresentationState();
    };

    QString sopClass = reader.getValueAttributeAsQString(DICOMSOPClassUID);

    if (sopClass != GrayscalePresentationStateUid && sopClass != ColorPresentationStateUid && sopClass != PseudoColorPresentationStateUid)
    {
        return fail(QObject::tr("It is not a softcopy presentation state."));
    }

    GraphicPresentationState state;
    state.m_sopInstanceUid = reader.getValueAttributeAsQString(DICOMSOPInstanceUID);
    state.m_label = reader.getValueAttributeAsQString(DICOMContentLabel).trimmed();
    state.m_description = reader.getValueAttributeAsQString(DICOMContentDescription).trimmed();

    // The images that it refers to, by series
    DICOMSequenceAttribute *series = reader.getSequenceAttribute(DICOMReferencedSeriesSequence);

    if (series)
    {
        for (DICOMSequenceItem *seriesItem : series->getItems())
        {
            state.m_referencedImages << parseReferencedImages(seriesItem->getSequenceAttribute(DICOMReferencedImageSequence));
        }
    }

    if (state.m_referencedImages.isEmpty())
    {
        return fail(QObject::tr("The presentation state does not refer to any image."));
    }

    // The annotations
    DICOMSequenceAttribute *annotations = reader.getSequenceAttribute(DICOMGraphicAnnotationSequence);

    if (annotations)
    {
        for (DICOMSequenceItem *annotationItem : annotations->getItems())
        {
            Annotation annotation;
            annotation.layer = annotationItem->getValueAttributeAsQString(DICOMGraphicLayer).trimmed();
            annotation.images = parseReferencedImages(annotationItem->getSequenceAttribute(DICOMReferencedImageSequence));

            DICOMSequenceAttribute *graphics = annotationItem->getSequenceAttribute(DICOMGraphicObjectSequence);

            if (graphics)
            {
                for (DICOMSequenceItem *graphicItem : graphics->getItems())
                {
                    GraphicObject object;

                    if (parseGraphicObject(graphicItem, object))
                    {
                        annotation.graphics << object;
                    }
                    else if (!state.m_notApplied.contains(QObject::tr("Some graphic objects")))
                    {
                        state.m_notApplied << QObject::tr("Some graphic objects");
                    }
                }
            }

            DICOMSequenceAttribute *texts = annotationItem->getSequenceAttribute(DICOMTextObjectSequence);

            if (texts)
            {
                for (DICOMSequenceItem *textItem : texts->getItems())
                {
                    annotation.texts << parseTextObject(textItem);
                }
            }

            state.m_annotations << annotation;
        }
    }

    // What the presentation state asks for and is not applied
    if (reader.tagExists(DICOMShutterShape))
    {
        state.m_notApplied << QObject::tr("Display shutter");
    }

    bool flipped = reader.getValueAttributeAsQString(DICOMImageHorizontalFlip).trimmed() == "Y";
    int rotation = reader.getValueAttributeAsQString(DICOMImageRotation).trimmed().toInt();

    if (flipped || rotation != 0)
    {
        state.m_notApplied << QObject::tr("Rotation or flip");
    }

    if (reader.getSequenceAttribute(DICOMSoftcopyVOILUTSequence))
    {
        state.m_notApplied << QObject::tr("Window and level");
    }

    if (reader.getSequenceAttribute(DICOMPresentationLUTSequence) || reader.getValueAttributeAsQString(DICOMPresentationLUTShape).trimmed() == "INVERSE")
    {
        state.m_notApplied << QObject::tr("Presentation LUT");
    }

    if (reader.getSequenceAttribute(DICOMDisplayedAreaSelectionSequence))
    {
        DICOMSequenceAttribute *area = reader.getSequenceAttribute(DICOMDisplayedAreaSelectionSequence);

        if (!area->getItems().isEmpty() && area->getItems().first()->getValueAttributeAsQString(DICOMPresentationSizeMode).trimmed() != "SCALE TO FIT")
        {
            state.m_notApplied << QObject::tr("Zoom or pan");
        }
    }

    if (reader.getSequenceAttribute(DICOMMaskSubtractionSequence))
    {
        state.m_notApplied << QObject::tr("Mask subtraction");
    }

    state.m_null = false;
    return state;
}

bool GraphicPresentationState::isNull() const
{
    return m_null;
}

const QString& GraphicPresentationState::getSopInstanceUid() const
{
    return m_sopInstanceUid;
}

const QString& GraphicPresentationState::getLabel() const
{
    return m_label;
}

const QString& GraphicPresentationState::getDescription() const
{
    return m_description;
}

const QList<GraphicPresentationState::ReferencedImage>& GraphicPresentationState::getReferencedImages() const
{
    return m_referencedImages;
}

bool GraphicPresentationState::references(const QString &sopInstanceUid) const
{
    for (const ReferencedImage &image : m_referencedImages)
    {
        if (image.sopInstanceUid == sopInstanceUid)
        {
            return true;
        }
    }

    return false;
}

QList<GraphicPresentationState::ReferencedImage> GraphicPresentationState::getUnresolvedReferences(const QSet<QString> &availableSopInstanceUids) const
{
    QList<ReferencedImage> unresolved;

    for (const ReferencedImage &image : m_referencedImages)
    {
        if (!availableSopInstanceUids.contains(image.sopInstanceUid))
        {
            unresolved << image;
        }
    }

    return unresolved;
}

QList<GraphicPresentationState::Annotation> GraphicPresentationState::getAnnotationsFor(const QString &sopInstanceUid, int frame) const
{
    QList<Annotation> applicable;

    if (!references(sopInstanceUid))
    {
        return applicable;
    }

    for (const Annotation &annotation : m_annotations)
    {
        bool applies = annotation.images.isEmpty();

        for (const ReferencedImage &image : annotation.images)
        {
            if (image.sopInstanceUid == sopInstanceUid && (image.frames.isEmpty() || image.frames.contains(frame)))
            {
                applies = true;
                break;
            }
        }

        if (applies)
        {
            applicable << annotation;
        }
    }

    return applicable;
}

const QStringList& GraphicPresentationState::getNotApplied() const
{
    return m_notApplied;
}

} // End namespace udg
