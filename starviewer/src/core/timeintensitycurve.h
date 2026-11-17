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

#ifndef UDGTIMEINTENSITYCURVE_H
#define UDGTIMEINTENSITYCURVE_H

#include <QString>
#include <QVector>

class vtkImageData;

namespace udg {

/**
    The time-intensity curve of a region in a series with several phases (a dynamic study: perfusion, contrast uptake, a heart cycle): for each phase, the mean of the values of the
    pixels of a disc in one slice, and their standard deviation. The values are the ones of the pixels (HU, signal): nothing is converted.
    The phases of a volume are stored one after the other for each slice: the image of slice s and phase p is the z index s * numberOfPhases + p.
 */
class TimeIntensityCurve {
public:
    /// One value per phase. Empty if there is nothing to measure (see compute).
    QVector<double> mean;
    QVector<double> standardDeviation;

    /// Number of pixels of the disc that are inside the image
    int pixels = 0;

    /// Measures the pixels that are at most radiusMillimeters from the center (given in pixel indexes, which may have decimals) in the given slice.
    /// The spacing is the size of the pixels in mm. Gives an empty curve if the data is null, has more than one component, has fewer z planes than
    /// numberOfSlices * numberOfPhases, the slice or the phases are not valid or the disc has no pixel inside the image.
    static TimeIntensityCurve compute(vtkImageData *data, int numberOfPhases, int slice, double centerX, double centerY, double radiusMillimeters, double spacingX, double spacingY);

    bool isEmpty() const;

    /// The phase (0 based) with the highest mean, the first one if there is a tie. -1 for an empty curve.
    int peakPhase() const;

    /// The phase where the mean first reaches the given fraction (0 to 1) of the way between its minimum and its maximum, or -1 for an empty curve or a flat one.
    /// With 0.5 it is the "time to half the rise", a simple way to compare the arrival of the contrast.
    int firstPhaseReaching(double fraction) const;

    /// Phase number (starting at 1), mean and standard deviation, as comma separated lines with a header.
    QString toCsv() const;
};

} // End namespace udg

#endif
