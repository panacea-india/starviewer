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

#include "volumesculptor.h"

#include <cmath>
#include <vector>

#include <vtkDataArray.h>
#include <vtkImageData.h>
#include <vtkPointData.h>

namespace udg {

namespace {

VolumeSculptor::Result copyOf(vtkImageData *volume, double &fill)
{
    VolumeSculptor::Result result;

    if (!volume || !volume->GetPointData() || !volume->GetPointData()->GetScalars())
    {
        return result;
    }

    result.image = vtkSmartPointer<vtkImageData>::New();
    result.image->DeepCopy(volume);
    double range[2];
    volume->GetPointData()->GetScalars()->GetRange(range);
    fill = range[0];
    return result;
}

void clearVoxel(vtkDataArray *scalars, long long index, double fill)
{
    for (int c = 0; c < scalars->GetNumberOfComponents(); c++)
    {
        scalars->SetComponent(index, c, fill);
    }
}

}

VolumeSculptor::Result VolumeSculptor::keepLargestPiece(vtkImageData *volume, double threshold)
{
    double fill = 0.0;
    Result result = copyOf(volume, fill);

    if (!result.image || result.image->GetNumberOfScalarComponents() != 1)
    {
        result.image = nullptr;
        return result;
    }

    int dimensions[3];
    volume->GetDimensions(dimensions);
    vtkDataArray *scalars = result.image->GetPointData()->GetScalars();
    long long total = static_cast<long long>(dimensions[0]) * dimensions[1] * dimensions[2];
    long long strideY = dimensions[0];
    long long strideZ = strideY * dimensions[1];

    // Pass 1: the size of each group, and where the largest one starts
    std::vector<unsigned char> visited(total, 0);
    std::vector<long long> stack;
    long long largestSize = 0;
    long long largestSeed = -1;

    auto neighbors = [&](long long index, long long out[6]) {
        int x = static_cast<int>(index % strideY);
        int y = static_cast<int>((index / strideY) % dimensions[1]);
        int z = static_cast<int>(index / strideZ);
        out[0] = x > 0 ? index - 1 : -1;
        out[1] = x < dimensions[0] - 1 ? index + 1 : -1;
        out[2] = y > 0 ? index - strideY : -1;
        out[3] = y < dimensions[1] - 1 ? index + strideY : -1;
        out[4] = z > 0 ? index - strideZ : -1;
        out[5] = z < dimensions[2] - 1 ? index + strideZ : -1;
    };

    for (long long seed = 0; seed < total; seed++)
    {
        if (visited[seed] || scalars->GetTuple1(seed) < threshold)
        {
            continue;
        }

        long long size = 0;
        stack.push_back(seed);
        visited[seed] = 1;

        while (!stack.empty())
        {
            long long current = stack.back();
            stack.pop_back();
            size++;
            long long around[6];
            neighbors(current, around);

            for (int i = 0; i < 6; i++)
            {
                if (around[i] >= 0 && !visited[around[i]] && scalars->GetTuple1(around[i]) >= threshold)
                {
                    visited[around[i]] = 1;
                    stack.push_back(around[i]);
                }
            }
        }

        if (size > largestSize)
        {
            largestSize = size;
            largestSeed = seed;
        }
    }

    if (largestSeed < 0)
    {
        // Nothing reaches the threshold: nothing to keep
        result.image = nullptr;
        return result;
    }

    // Pass 2: mark the largest group, and take out the rest of the voxels that were at or above the threshold
    std::vector<unsigned char> keep(total, 0);
    stack.push_back(largestSeed);
    keep[largestSeed] = 1;

    while (!stack.empty())
    {
        long long current = stack.back();
        stack.pop_back();
        long long around[6];
        neighbors(current, around);

        for (int i = 0; i < 6; i++)
        {
            if (around[i] >= 0 && !keep[around[i]] && scalars->GetTuple1(around[i]) >= threshold)
            {
                keep[around[i]] = 1;
                stack.push_back(around[i]);
            }
        }
    }

    for (long long index = 0; index < total; index++)
    {
        if (keep[index])
        {
            result.keptVoxels++;
        }
        else if (scalars->GetTuple1(index) >= threshold)
        {
            scalars->SetTuple1(index, fill);
            result.removedVoxels++;
        }
    }

    scalars->Modified();
    return result;
}

VolumeSculptor::Result VolumeSculptor::eraseSphere(vtkImageData *volume, const double center[3], double radius)
{
    double fill = 0.0;
    Result result = copyOf(volume, fill);

    if (!result.image)
    {
        return result;
    }

    int dimensions[3];
    volume->GetDimensions(dimensions);
    double origin[3];
    double spacing[3];
    volume->GetOrigin(origin);
    volume->GetSpacing(spacing);
    vtkDataArray *scalars = result.image->GetPointData()->GetScalars();
    double radiusSquared = radius * radius;
    long long index = 0;

    for (int z = 0; z < dimensions[2]; z++)
    {
        double dz = origin[2] + z * spacing[2] - center[2];

        for (int y = 0; y < dimensions[1]; y++)
        {
            double dy = origin[1] + y * spacing[1] - center[1];

            for (int x = 0; x < dimensions[0]; x++, index++)
            {
                double dx = origin[0] + x * spacing[0] - center[0];

                if (dx * dx + dy * dy + dz * dz <= radiusSquared)
                {
                    clearVoxel(scalars, index, fill);
                    result.removedVoxels++;
                }
                else
                {
                    result.keptVoxels++;
                }
            }
        }
    }

    scalars->Modified();
    return result;
}

VolumeSculptor::Result VolumeSculptor::keepBox(vtkImageData *volume, const double bounds[6])
{
    double fill = 0.0;
    Result result = copyOf(volume, fill);

    if (!result.image)
    {
        return result;
    }

    int dimensions[3];
    volume->GetDimensions(dimensions);
    double origin[3];
    double spacing[3];
    volume->GetOrigin(origin);
    volume->GetSpacing(spacing);
    vtkDataArray *scalars = result.image->GetPointData()->GetScalars();
    long long index = 0;

    for (int z = 0; z < dimensions[2]; z++)
    {
        double pz = origin[2] + z * spacing[2];

        for (int y = 0; y < dimensions[1]; y++)
        {
            double py = origin[1] + y * spacing[1];

            for (int x = 0; x < dimensions[0]; x++, index++)
            {
                double px = origin[0] + x * spacing[0];
                bool inside = px >= bounds[0] && px <= bounds[1] && py >= bounds[2] && py <= bounds[3] && pz >= bounds[4] && pz <= bounds[5];

                if (inside)
                {
                    result.keptVoxels++;
                }
                else
                {
                    clearVoxel(scalars, index, fill);
                    result.removedVoxels++;
                }
            }
        }
    }

    scalars->Modified();
    return result;
}

} // End namespace udg
