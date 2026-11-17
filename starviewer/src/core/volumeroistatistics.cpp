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

#include "volumeroistatistics.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

#include <vtkDataArray.h>
#include <vtkImageData.h>
#include <vtkPointData.h>

namespace udg {

namespace {

const double Pi = 3.14159265358979323846;

// A radius of 6.2035 mm is a sphere of 1 mL
const double OneMillilitreRadius = 6.203504908994;

bool sameDimensions(vtkImageData *a, vtkImageData *b)
{
    int da[3];
    int db[3];
    a->GetDimensions(da);
    b->GetDimensions(db);
    return da[0] == db[0] && da[1] == db[1] && da[2] == db[2];
}

double valueAt(vtkImageData *values, vtkIdType index)
{
    return values->GetPointData()->GetScalars()->GetComponent(index, 0);
}

}

VolumeRoiStatistics::Statistics::Statistics()
    : voxels(0), volumeMilliliters(0.0), mean(0.0), minimum(0.0), maximum(0.0), standardDeviation(0.0), peak(0.0)
{
    maximumPosition[0] = maximumPosition[1] = maximumPosition[2] = 0.0;
}

VolumeRoiStatistics::Statistics VolumeRoiStatistics::compute(vtkImageData *values, vtkImageData *mask, double scale, bool peak)
{
    Statistics statistics;

    if (!values || !mask || values->GetNumberOfScalarComponents() != 1 || mask->GetNumberOfScalarComponents() != 1 || !sameDimensions(values, mask))
    {
        return statistics;
    }

    int dimensions[3];
    values->GetDimensions(dimensions);
    double spacing[3];
    double origin[3];
    values->GetSpacing(spacing);
    values->GetOrigin(origin);

    double sum = 0.0;
    double sumOfSquares = 0.0;
    double minimum = std::numeric_limits<double>::max();
    double maximum = -std::numeric_limits<double>::max();
    vtkIdType maximumIndex = 0;
    long long count = 0;
    vtkIdType total = static_cast<vtkIdType>(dimensions[0]) * dimensions[1] * dimensions[2];

    for (vtkIdType index = 0; index < total; index++)
    {
        if (valueAt(mask, index) == 0.0)
        {
            continue;
        }

        double value = valueAt(values, index) * scale;
        sum += value;
        sumOfSquares += value * value;
        count++;

        if (value < minimum)
        {
            minimum = value;
        }
        if (value > maximum)
        {
            maximum = value;
            maximumIndex = index;
        }
    }

    if (count == 0)
    {
        return statistics;
    }

    statistics.voxels = count;
    statistics.volumeMilliliters = count * spacing[0] * spacing[1] * spacing[2] / 1000.0;
    statistics.mean = sum / count;
    statistics.minimum = minimum;
    statistics.maximum = maximum;
    double variance = sumOfSquares / count - statistics.mean * statistics.mean;
    statistics.standardDeviation = variance > 0.0 ? std::sqrt(variance) : 0.0;

    int ci = static_cast<int>(maximumIndex % dimensions[0]);
    int cj = static_cast<int>((maximumIndex / dimensions[0]) % dimensions[1]);
    int ck = static_cast<int>(maximumIndex / (static_cast<vtkIdType>(dimensions[0]) * dimensions[1]));
    statistics.maximumPosition[0] = origin[0] + ci * spacing[0];
    statistics.maximumPosition[1] = origin[1] + cj * spacing[1];
    statistics.maximumPosition[2] = origin[2] + ck * spacing[2];

    if (peak)
    {
        // The sphere of 1 mL with the highest mean, around the voxels of the region that are near the maximum (half of it or more: the sphere with the
        // highest mean is always around one of them, as a voxel of low value cannot be the center of the hottest 1 mL)
        int reach[3] = {static_cast<int>(std::ceil(OneMillilitreRadius / spacing[0])), static_cast<int>(std::ceil(OneMillilitreRadius / spacing[1])),
                        static_cast<int>(std::ceil(OneMillilitreRadius / spacing[2]))};
        double best = -std::numeric_limits<double>::max();

        for (vtkIdType index = 0; index < total; index++)
        {
            if (valueAt(mask, index) == 0.0 || valueAt(values, index) * scale < 0.5 * maximum)
            {
                continue;
            }

            int i = static_cast<int>(index % dimensions[0]);
            int j = static_cast<int>((index / dimensions[0]) % dimensions[1]);
            int k = static_cast<int>(index / (static_cast<vtkIdType>(dimensions[0]) * dimensions[1]));
            double inside = 0.0;
            int voxelsInside = 0;

            for (int dk = -reach[2]; dk <= reach[2]; dk++)
            {
                for (int dj = -reach[1]; dj <= reach[1]; dj++)
                {
                    for (int di = -reach[0]; di <= reach[0]; di++)
                    {
                        double dx = di * spacing[0];
                        double dy = dj * spacing[1];
                        double dz = dk * spacing[2];

                        if (dx * dx + dy * dy + dz * dz > OneMillilitreRadius * OneMillilitreRadius)
                        {
                            continue;
                        }

                        int ni = i + di;
                        int nj = j + dj;
                        int nk = k + dk;

                        if (ni < 0 || nj < 0 || nk < 0 || ni >= dimensions[0] || nj >= dimensions[1] || nk >= dimensions[2])
                        {
                            continue;
                        }

                        inside += valueAt(values, (static_cast<vtkIdType>(nk) * dimensions[1] + nj) * dimensions[0] + ni) * scale;
                        voxelsInside++;
                    }
                }
            }

            if (voxelsInside > 0)
            {
                best = std::max(best, inside / voxelsInside);
            }
        }

        statistics.peak = best;
    }

    return statistics;
}

vtkSmartPointer<vtkImageData> VolumeRoiStatistics::createSphereMask(vtkImageData *like, const double center[3], double radius)
{
    vtkSmartPointer<vtkImageData> mask = vtkSmartPointer<vtkImageData>::New();
    mask->SetDimensions(like->GetDimensions());
    mask->SetSpacing(like->GetSpacing());
    mask->SetOrigin(like->GetOrigin());
    mask->AllocateScalars(VTK_UNSIGNED_CHAR, 1);

    int dimensions[3];
    like->GetDimensions(dimensions);
    double spacing[3];
    double origin[3];
    like->GetSpacing(spacing);
    like->GetOrigin(origin);
    unsigned char *voxels = static_cast<unsigned char*>(mask->GetScalarPointer());

    for (int k = 0; k < dimensions[2]; k++)
    {
        for (int j = 0; j < dimensions[1]; j++)
        {
            for (int i = 0; i < dimensions[0]; i++)
            {
                double dx = origin[0] + i * spacing[0] - center[0];
                double dy = origin[1] + j * spacing[1] - center[1];
                double dz = origin[2] + k * spacing[2] - center[2];
                voxels[(static_cast<size_t>(k) * dimensions[1] + j) * dimensions[0] + i] = (dx * dx + dy * dy + dz * dz <= radius * radius) ? 1 : 0;
            }
        }
    }

    return mask;
}

vtkSmartPointer<vtkImageData> VolumeRoiStatistics::createThresholdMask(vtkImageData *values, vtkImageData *region, double scale, double threshold)
{
    vtkSmartPointer<vtkImageData> mask = vtkSmartPointer<vtkImageData>::New();
    mask->SetDimensions(values->GetDimensions());
    mask->SetSpacing(values->GetSpacing());
    mask->SetOrigin(values->GetOrigin());
    mask->AllocateScalars(VTK_UNSIGNED_CHAR, 1);

    int dimensions[3];
    values->GetDimensions(dimensions);
    vtkIdType total = static_cast<vtkIdType>(dimensions[0]) * dimensions[1] * dimensions[2];
    unsigned char *voxels = static_cast<unsigned char*>(mask->GetScalarPointer());
    bool matches = region && sameDimensions(values, region);

    for (vtkIdType index = 0; index < total; index++)
    {
        bool inRegion = !matches || valueAt(region, index) != 0.0;
        voxels[index] = (inRegion && valueAt(values, index) * scale >= threshold) ? 1 : 0;
    }

    return mask;
}

} // End namespace udg
