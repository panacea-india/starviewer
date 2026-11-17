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

#include "timeintensitycurve.h"

#include <algorithm>
#include <cmath>

#include <vtkImageData.h>

namespace udg {

TimeIntensityCurve TimeIntensityCurve::compute(vtkImageData *data, int numberOfPhases, int slice, double centerX, double centerY, double radiusMillimeters, double spacingX,
                                               double spacingY)
{
    TimeIntensityCurve curve;

    if (!data || numberOfPhases < 1 || slice < 0 || spacingX <= 0.0 || spacingY <= 0.0 || radiusMillimeters < 0.0 || !std::isfinite(radiusMillimeters)
        || !std::isfinite(centerX) || !std::isfinite(centerY) || data->GetNumberOfScalarComponents() != 1)
    {
        return curve;
    }

    int extent[6];
    data->GetExtent(extent);
    int planes = extent[5] - extent[4] + 1;

    if (planes < numberOfPhases || (static_cast<qint64>(slice) + 1) * numberOfPhases > planes)
    {
        return curve;
    }

    // The pixels of the disc that are inside the image
    QVector<int> xs;
    QVector<int> ys;
    int fromX = static_cast<int>(std::floor(centerX - radiusMillimeters / spacingX));
    int toX = static_cast<int>(std::ceil(centerX + radiusMillimeters / spacingX));
    int fromY = static_cast<int>(std::floor(centerY - radiusMillimeters / spacingY));
    int toY = static_cast<int>(std::ceil(centerY + radiusMillimeters / spacingY));

    for (int y = std::max(fromY, extent[2]); y <= std::min(toY, extent[3]); y++)
    {
        for (int x = std::max(fromX, extent[0]); x <= std::min(toX, extent[1]); x++)
        {
            double dx = (x - centerX) * spacingX;
            double dy = (y - centerY) * spacingY;

            if (dx * dx + dy * dy <= radiusMillimeters * radiusMillimeters)
            {
                xs << x;
                ys << y;
            }
        }
    }

    if (xs.isEmpty())
    {
        return curve;
    }

    curve.pixels = xs.size();
    curve.mean.resize(numberOfPhases);
    curve.standardDeviation.resize(numberOfPhases);

    for (int phase = 0; phase < numberOfPhases; phase++)
    {
        int z = extent[4] + slice * numberOfPhases + phase;
        double sum = 0.0;
        double sumOfSquares = 0.0;

        for (int i = 0; i < xs.size(); i++)
        {
            double value = data->GetScalarComponentAsDouble(xs[i], ys[i], z, 0);
            sum += value;
            sumOfSquares += value * value;
        }

        double mean = sum / xs.size();
        curve.mean[phase] = mean;
        curve.standardDeviation[phase] = std::sqrt(std::max(0.0, sumOfSquares / xs.size() - mean * mean));
    }

    return curve;
}

bool TimeIntensityCurve::isEmpty() const
{
    return mean.isEmpty();
}

int TimeIntensityCurve::peakPhase() const
{
    if (mean.isEmpty())
    {
        return -1;
    }

    return static_cast<int>(std::max_element(mean.begin(), mean.end()) - mean.begin());
}

int TimeIntensityCurve::firstPhaseReaching(double fraction) const
{
    if (mean.isEmpty() || fraction < 0.0 || fraction > 1.0)
    {
        return -1;
    }

    double low = *std::min_element(mean.begin(), mean.end());
    double high = *std::max_element(mean.begin(), mean.end());

    if (high - low <= 0.0)
    {
        return -1;
    }

    double target = low + fraction * (high - low);

    for (int i = 0; i < mean.size(); i++)
    {
        if (mean[i] >= target)
        {
            return i;
        }
    }

    return -1;
}

QString TimeIntensityCurve::toCsv() const
{
    QString text = "phase,mean,standard deviation\n";

    for (int i = 0; i < mean.size(); i++)
    {
        text += QString("%1,%2,%3\n").arg(i + 1).arg(mean[i], 0, 'f', 4).arg(standardDeviation[i], 0, 'f', 4);
    }

    return text;
}

} // End namespace udg
