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

#include "rtstructureset.h"

#include <cmath>

#include <QMap>
#include <QObject>

#include "dicomsequenceattribute.h"
#include "dicomsequenceitem.h"
#include "dicomtagreader.h"

namespace udg {

namespace {

const char* const RtStructureSetClass = "1.2.840.10008.5.1.4.1.1.481.3";

QList<double> parseNumbers(const QString &value)
{
    QList<double> numbers;

    for (const QString &part : value.split('\\', QString::SkipEmptyParts))
    {
        bool ok = false;
        double number = part.trimmed().toDouble(&ok);

        if (ok && std::isfinite(number))
        {
            numbers << number;
        }
    }

    return numbers;
}

QColor parseColor(const QString &value, const QColor &fallback)
{
    QList<double> rgb = parseNumbers(value);

    if (rgb.size() != 3)
    {
        return fallback;
    }

    for (double component : rgb)
    {
        if (component < 0.0 || component > 255.0)
        {
            return fallback;
        }
    }

    return QColor(static_cast<int>(rgb.at(0)), static_cast<int>(rgb.at(1)), static_cast<int>(rgb.at(2)));
}

}

RtStructureSet::RtStructureSet()
{
}

RtStructureSet RtStructureSet::fromDICOM(const DICOMTagReader &reader, QString *error)
{
    auto fail = [error](const QString &reason) {
        if (error)
        {
            *error = reason;
        }

        return RtStructureSet();
    };

    if (reader.getValueAttributeAsQString(DICOMSOPClassUID) != RtStructureSetClass)
    {
        return fail(QObject::tr("It is not an RT structure set."));
    }

    RtStructureSet set;
    set.m_sopInstanceUid = reader.getValueAttributeAsQString(DICOMSOPInstanceUID);
    set.m_label = reader.getValueAttributeAsQString(DICOMStructureSetLabel).trimmed();

    // The frames of reference that the contours are in
    DICOMSequenceAttribute *frames = reader.getSequenceAttribute(DICOMReferencedFrameOfReferenceSequence);

    if (frames)
    {
        for (DICOMSequenceItem *item : frames->getItems())
        {
            QString uid = item->getValueAttributeAsQString(DICOMFrameOfReferenceUID).trimmed();

            if (!uid.isEmpty() && !set.m_frameOfReferenceUids.contains(uid))
            {
                set.m_frameOfReferenceUids << uid;
            }
        }
    }

    QString ownFrame = reader.getValueAttributeAsQString(DICOMFrameOfReferenceUID).trimmed();

    if (!ownFrame.isEmpty() && !set.m_frameOfReferenceUids.contains(ownFrame))
    {
        set.m_frameOfReferenceUids << ownFrame;
    }

    // The names of the regions
    QMap<int, Roi> byNumber;
    DICOMSequenceAttribute *structures = reader.getSequenceAttribute(DICOMStructureSetROISequence);

    if (structures)
    {
        for (DICOMSequenceItem *item : structures->getItems())
        {
            bool ok = false;
            int number = item->getValueAttributeAsQString(DICOMROINumber).trimmed().toInt(&ok);

            if (ok)
            {
                Roi roi;
                roi.number = number;
                roi.name = item->getValueAttributeAsQString(DICOMROIName).trimmed();
                byNumber.insert(number, roi);
            }
        }
    }

    // The contours of each region, and its color
    DICOMSequenceAttribute *contours = reader.getSequenceAttribute(DICOMROIContourSequence);

    if (contours)
    {
        for (DICOMSequenceItem *item : contours->getItems())
        {
            bool ok = false;
            int number = item->getValueAttributeAsQString(DICOMReferencedROINumber).trimmed().toInt(&ok);

            if (!ok || !byNumber.contains(number))
            {
                continue;
            }

            Roi &roi = byNumber[number];
            roi.color = parseColor(item->getValueAttributeAsQString(DICOMROIDisplayColor), roi.color);
            DICOMSequenceAttribute *sequence = item->getSequenceAttribute(DICOMContourSequence);

            if (!sequence)
            {
                continue;
            }

            for (DICOMSequenceItem *contourItem : sequence->getItems())
            {
                QString type = contourItem->getValueAttributeAsQString(DICOMContourGeometricType).trimmed();

                // A single point is not an outline
                if (type != "CLOSED_PLANAR" && type != "OPEN_PLANAR" && type != "OPEN_NONPLANAR")
                {
                    continue;
                }

                QList<double> numbers = parseNumbers(contourItem->getValueAttributeAsQString(DICOMContourData));
                Contour contour;
                contour.closed = type == "CLOSED_PLANAR";

                for (int i = 0; i + 2 < numbers.size(); i += 3)
                {
                    contour.points << std::array<double, 3>{{numbers.at(i), numbers.at(i + 1), numbers.at(i + 2)}};
                }

                if (contour.points.size() >= 2)
                {
                    roi.contours << contour;
                }
            }
        }
    }

    for (const Roi &roi : byNumber)
    {
        if (!roi.contours.isEmpty())
        {
            set.m_rois << roi;
        }
    }

    if (set.m_rois.isEmpty())
    {
        return fail(QObject::tr("The structure set has no contours."));
    }

    return set;
}

bool RtStructureSet::isNull() const
{
    return m_rois.isEmpty();
}

QString RtStructureSet::getSopInstanceUid() const
{
    return m_sopInstanceUid;
}

QString RtStructureSet::getLabel() const
{
    return m_label;
}

const QStringList& RtStructureSet::getFrameOfReferenceUids() const
{
    return m_frameOfReferenceUids;
}

const QList<RtStructureSet::Roi>& RtStructureSet::getRois() const
{
    return m_rois;
}

int RtStructureSet::getNumberOfContours() const
{
    int count = 0;

    for (const Roi &roi : m_rois)
    {
        count += roi.contours.size();
    }

    return count;
}

bool RtStructureSet::isInFrameOfReference(const QString &uid) const
{
    return !uid.isEmpty() && m_frameOfReferenceUids.contains(uid.trimmed());
}

double RtStructureSet::depthOf(const PlaneGeometry &geometry, const std::array<double, 3> &point)
{
    std::array<double, 3> normal = {{geometry.row[1] * geometry.column[2] - geometry.row[2] * geometry.column[1],
                                     geometry.row[2] * geometry.column[0] - geometry.row[0] * geometry.column[2],
                                     geometry.row[0] * geometry.column[1] - geometry.row[1] * geometry.column[0]}};

    double depth = 0.0;

    for (int i = 0; i < 3; i++)
    {
        depth += (point[i] - geometry.origin[i]) * normal[i];
    }

    return depth;
}

QPointF RtStructureSet::toPixel(const PlaneGeometry &geometry, const std::array<double, 3> &point)
{
    double x = 0.0;
    double y = 0.0;

    for (int i = 0; i < 3; i++)
    {
        double d = point[i] - geometry.origin[i];
        x += d * geometry.row[i];
        y += d * geometry.column[i];
    }

    return QPointF(x / geometry.spacingX, y / geometry.spacingY);
}

QList<RtStructureSet::DrawnContour> RtStructureSet::contoursAtDepth(const PlaneGeometry &geometry, double depth, double tolerance) const
{
    QList<DrawnContour> drawn;

    for (const Roi &roi : m_rois)
    {
        for (const Contour &contour : roi.contours)
        {
            if (std::abs(depthOf(geometry, contour.points.first()) - depth) > tolerance)
            {
                continue;
            }

            DrawnContour result;
            result.roiNumber = roi.number;
            result.roiName = roi.name;
            result.color = roi.color;
            result.closed = contour.closed;

            for (const std::array<double, 3> &point : contour.points)
            {
                result.pixels << toPixel(geometry, point);
            }

            if (result.closed)
            {
                result.pixels << result.pixels.first();
            }

            drawn << result;
        }
    }

    return drawn;
}

} // End namespace udg
