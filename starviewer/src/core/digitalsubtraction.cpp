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

#include "digitalsubtraction.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include <vtkImageData.h>

namespace udg {

namespace {

bool isUsable(vtkImageData *volume)
{
    if (!volume || volume->GetNumberOfScalarComponents() != 1)
    {
        return false;
    }

    int type = volume->GetScalarType();
    return type != VTK_STRING && type != VTK_BIT;
}

}

vtkSmartPointer<vtkImageData> DigitalSubtraction::subtract(vtkImageData *volume, int maskSlice, const QVector<QPoint> &shifts)
{
    if (!isUsable(volume))
    {
        return nullptr;
    }

    int extent[6];
    volume->GetExtent(extent);
    int slices = extent[5] - extent[4] + 1;

    if (maskSlice < 0 || maskSlice >= slices || (!shifts.isEmpty() && shifts.size() != slices))
    {
        return nullptr;
    }

    vtkSmartPointer<vtkImageData> result = vtkSmartPointer<vtkImageData>::New();
    result->SetExtent(extent);
    result->SetSpacing(volume->GetSpacing());
    result->SetOrigin(volume->GetOrigin());
    result->AllocateScalars(VTK_SHORT, 1);

    int maskZ = extent[4] + maskSlice;

    for (int s = 0; s < slices; s++)
    {
        int z = extent[4] + s;
        QPoint shift = shifts.isEmpty() ? QPoint(0, 0) : shifts[s];

        for (int y = extent[2]; y <= extent[3]; y++)
        {
            for (int x = extent[0]; x <= extent[1]; x++)
            {
                int maskX = x - shift.x();
                int maskY = y - shift.y();
                double difference = 0.0;

                if (maskX >= extent[0] && maskX <= extent[1] && maskY >= extent[2] && maskY <= extent[3])
                {
                    difference = volume->GetScalarComponentAsDouble(x, y, z, 0) - volume->GetScalarComponentAsDouble(maskX, maskY, maskZ, 0);
                }

                result->SetScalarComponentFromDouble(x, y, z, 0, std::max(-32768.0, std::min(32767.0, difference)));
            }
        }
    }

    return result;
}

QPoint DigitalSubtraction::findShift(vtkImageData *volume, int slice, int maskSlice, int maxShift)
{
    if (!isUsable(volume) || maxShift < 0)
    {
        return QPoint(0, 0);
    }

    int extent[6];
    volume->GetExtent(extent);
    int slices = extent[5] - extent[4] + 1;

    if (slice < 0 || slice >= slices || maskSlice < 0 || maskSlice >= slices || slice == maskSlice)
    {
        return QPoint(0, 0);
    }

    int width = extent[1] - extent[0] + 1;
    int height = extent[3] - extent[2] + 1;

    // The central part of the image, leaving room for the shifts, looking at every second pixel to keep it fast
    int marginX = std::max(width / 4, maxShift);
    int marginY = std::max(height / 4, maxShift);
    int fromX = extent[0] + marginX;
    int toX = extent[1] - marginX;
    int fromY = extent[2] + marginY;
    int toY = extent[3] - marginY;

    if (fromX > toX || fromY > toY)
    {
        return QPoint(0, 0);
    }

    int z = extent[4] + slice;
    int maskZ = extent[4] + maskSlice;
    QPoint best(0, 0);
    double bestCost = std::numeric_limits<double>::max();

    // The shifts are tried from the smallest, so that a tie keeps the smallest
    QVector<QPoint> candidates;

    for (int dy = -maxShift; dy <= maxShift; dy++)
    {
        for (int dx = -maxShift; dx <= maxShift; dx++)
        {
            candidates << QPoint(dx, dy);
        }
    }

    std::stable_sort(candidates.begin(), candidates.end(), [](const QPoint &a, const QPoint &b) { return a.manhattanLength() < b.manhattanLength(); });

    for (const QPoint &candidate : candidates)
    {
        double cost = 0.0;
        int count = 0;

        for (int y = fromY; y <= toY; y += 2)
        {
            for (int x = fromX; x <= toX; x += 2)
            {
                cost += std::abs(volume->GetScalarComponentAsDouble(x, y, z, 0) - volume->GetScalarComponentAsDouble(x - candidate.x(), y - candidate.y(), maskZ, 0));
                count++;
            }
        }

        cost /= count;

        if (cost < bestCost - 1e-12)
        {
            bestCost = cost;
            best = candidate;
        }
    }

    return best;
}

QVector<QPoint> DigitalSubtraction::findShifts(vtkImageData *volume, int maskSlice, int maxShift)
{
    QVector<QPoint> shifts;

    if (!isUsable(volume))
    {
        return shifts;
    }

    int extent[6];
    volume->GetExtent(extent);
    int slices = extent[5] - extent[4] + 1;

    for (int s = 0; s < slices; s++)
    {
        shifts << findShift(volume, s, maskSlice, maxShift);
    }

    return shifts;
}

} // End namespace udg
