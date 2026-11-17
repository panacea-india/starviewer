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

#ifndef UDGRTSTRUCTURESET_H
#define UDGRTSTRUCTURESET_H

#include <array>

#include <QColor>
#include <QList>
#include <QPointF>
#include <QString>
#include <QStringList>

namespace udg {

class DICOMTagReader;

/**
    An RT Structure Set: the outlines of the organs and targets of a radiotherapy plan, as contours in the patient coordinates (mm) of a frame of reference.
    It reads the contours and finds the ones that cross a given image, but it draws nothing (see Q2DViewerRtStructureHandler).
 */
class RtStructureSet {
public:
    struct Contour {
        QList<std::array<double, 3>> points;
        /// True for a CLOSED_PLANAR contour, which is a polygon; false for the open ones
        bool closed = false;
    };

    struct Roi {
        int number = 0;
        QString name;
        QColor color = QColor(255, 255, 0);
        QList<Contour> contours;
    };

    /// The plane of an image of the volume, to put the contours on: the position of the center of its first pixel, the directions of the rows and the columns (unit vectors)
    /// and the size of its pixels, all in mm and patient coordinates.
    struct PlaneGeometry {
        std::array<double, 3> origin;
        std::array<double, 3> row;
        std::array<double, 3> column;
        double spacingX = 1.0;
        double spacingY = 1.0;
    };

    /// A contour as it goes on an image: points in pixels (the center of the first pixel is 0, 0).
    struct DrawnContour {
        int roiNumber = 0;
        QString roiName;
        QColor color;
        QList<QPointF> pixels;
        bool closed = false;
    };

    RtStructureSet();

    /// Reads a structure set. A file that is not one, or that has no contours, gives a null set with the reason in `error`.
    static RtStructureSet fromDICOM(const DICOMTagReader &reader, QString *error = nullptr);

    bool isNull() const;
    QString getSopInstanceUid() const;
    QString getLabel() const;
    const QStringList& getFrameOfReferenceUids() const;
    const QList<Roi>& getRois() const;
    int getNumberOfContours() const;

    /// True if the set is in the frame of reference of the images (the same patient coordinates).
    bool isInFrameOfReference(const QString &uid) const;

    /// The distance of a point from the plane of the image, along its normal (positive toward the next slices if the normal is row x column).
    static double depthOf(const PlaneGeometry &geometry, const std::array<double, 3> &point);
    /// The point in pixels in the plane of the image (the part of it along the normal is left out).
    static QPointF toPixel(const PlaneGeometry &geometry, const std::array<double, 3> &point);

    /// The contours that are at the given depth from the plane of the first image, within the tolerance (mm) on each side. A contour is at the depth of its first point.
    QList<DrawnContour> contoursAtDepth(const PlaneGeometry &geometry, double depth, double tolerance) const;

private:
    QString m_sopInstanceUid;
    QString m_label;
    QStringList m_frameOfReferenceUids;
    QList<Roi> m_rois;
};

} // End namespace udg

#endif
