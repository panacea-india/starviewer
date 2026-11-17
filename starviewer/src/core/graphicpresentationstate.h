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

#ifndef UDGGRAPHICPRESENTATIONSTATE_H
#define UDGGRAPHICPRESENTATIONSTATE_H

#include <QList>
#include <QPointF>
#include <QSet>
#include <QString>
#include <QStringList>

namespace udg {

class DICOMTagReader;

/**
    A Softcopy Presentation State (GSPS): how to show some images, with the graphic annotations that someone drew on them.
    It is read from the DICOM file and kept here, with no link to the interface.

    What the class reads: which images it refers to, the graphic objects (points, polylines, circles and ellipses) and the texts.
    What it only detects, so that the interface can say that it is not applied: shutters, a rotation or flip, a window and level,
    a presentation LUT, a zoom or pan, and a mask.
 */
class GraphicPresentationState {
public:
    /// The unit of the coordinates of an object. Pixel: the top left corner of the top left pixel is 0\0 and the bottom right corner of the
    /// bottom right pixel is columns\rows. Display: the same, but from 0 to 1 over the area that is displayed.
    enum class Units { Pixel, Display };

    struct ReferencedImage {
        QString sopInstanceUid;
        /// The frames that are referenced, starting at 1. Empty if the whole instance is.
        QList<int> frames;
    };

    struct GraphicObject {
        enum class Type { Point, Polyline, Interpolated, Circle, Ellipse };

        Type type;
        Units units;
        /// A point has one point. A circle has its center and a point on it. An ellipse has the two ends of its major axis and then the two of its minor axis.
        QList<QPointF> points;
        bool filled;
    };

    struct TextObject {
        QString text;
        bool hasAnchor;
        Units anchorUnits;
        QPointF anchor;
        bool hasBox;
        Units boxUnits;
        QPointF boxTopLeft;
        QPointF boxBottomRight;
    };

    struct Annotation {
        QString layer;
        /// The images that it applies to. If it is empty, it applies to all the images that the presentation state refers to.
        QList<ReferencedImage> images;
        QList<GraphicObject> graphics;
        QList<TextObject> texts;
    };

    GraphicPresentationState();

    /// Reads the Presentation State of the given reader. If the file is not a grayscale, color or pseudo-color softcopy presentation state, or refers to no image,
    /// it returns a null object and, if error is not null, says why.
    static GraphicPresentationState fromDICOM(const DICOMTagReader &reader, QString *error = nullptr);

    bool isNull() const;

    const QString& getSopInstanceUid() const;
    const QString& getLabel() const;
    const QString& getDescription() const;
    const QList<ReferencedImage>& getReferencedImages() const;

    /// Returns true if the presentation state refers to the instance with the given SOP Instance UID.
    bool references(const QString &sopInstanceUid) const;

    /// Returns the references to instances that are not in the given set of SOP Instance UIDs.
    QList<ReferencedImage> getUnresolvedReferences(const QSet<QString> &availableSopInstanceUids) const;

    /// Returns the annotations that apply to the given frame (starting at 1) of the instance with the given SOP Instance UID.
    QList<Annotation> getAnnotationsFor(const QString &sopInstanceUid, int frame = 1) const;

    /// Returns what the presentation state asks for that is not applied, in words, like "Display shutter". Empty if it is all applied.
    const QStringList& getNotApplied() const;

private:
    QString m_sopInstanceUid;
    QString m_label;
    QString m_description;
    QList<ReferencedImage> m_referencedImages;
    QList<Annotation> m_annotations;
    QStringList m_notApplied;
    bool m_null;
};

} // End namespace udg

#endif
