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

#ifndef UDGDIGITALSUBTRACTION_H
#define UDGDIGITALSUBTRACTION_H

#include <QPoint>
#include <QVector>

#include <vtkSmartPointer.h>

class vtkImageData;

namespace udg {

/**
    Digital subtraction (DSA): the image taken before the contrast (the mask) is taken away from each image of a series, so that what stays is what the contrast fills (the vessels).
    The result is a copy with signed 16 bit values: the difference of each pixel, which can be negative, kept in the range of that type. It is a difference of the stored pixel values,
    not a value in HU or in any other unit, and it is not to be measured as such.
    If the patient or the table moved between the mask and an image, the mask can be moved by a whole number of pixels to fit that image (see findShift).
 */
class DigitalSubtraction {
public:
    /// The images are the slices (z planes) of the volume. For each slice, the pixel (x, y) of the result is the slice's pixel minus the pixel (x - dx, y - dy) of the mask slice, where
    /// (dx, dy) is the shift of that slice; the pixels whose mask pixel is outside the image are 0. The shifts are either empty (no shift) or one per slice.
    /// Null if there is no volume, it has more than one component, the mask slice is not in the volume or the shifts have the wrong size.
    static vtkSmartPointer<vtkImageData> subtract(vtkImageData *volume, int maskSlice, const QVector<QPoint> &shifts = QVector<QPoint>());

    /// The shift (dx, dy), at most maxShift pixels in each direction, that makes the mask slice most like the given slice: the one with the least mean absolute difference in the
    /// central part of the image (the border is where the table and the corners are). A tie goes to the smallest shift. (0, 0) if the slices are not valid.
    static QPoint findShift(vtkImageData *volume, int slice, int maskSlice, int maxShift);

    /// findShift for every slice of the volume (the mask slice itself gets no shift).
    static QVector<QPoint> findShifts(vtkImageData *volume, int maskSlice, int maxShift);
};

} // End namespace udg

#endif
