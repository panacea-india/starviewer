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

#include "gantrytiltcorrector.h"

#include <algorithm>
#include <cmath>

#include <vtkDataArray.h>
#include <vtkImageData.h>
#include <vtkPointData.h>
#include <vtkSmartPointer.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace udg {

namespace {

double dot(const GantryTiltCorrector::Vector &a, const GantryTiltCorrector::Vector &b)
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

GantryTiltCorrector::Vector cross(const GantryTiltCorrector::Vector &a, const GantryTiltCorrector::Vector &b)
{
    return {{a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]}};
}

}

GantryTiltCorrector::Analysis GantryTiltCorrector::analyse(const std::vector<Vector> &positions, const Vector &rowDirection, const Vector &columnDirection,
                                                           double pixelSpacingX, double pixelSpacingY)
{
    Analysis analysis;

    if (positions.size() < 2 || pixelSpacingX <= 0.0 || pixelSpacingY <= 0.0)
    {
        return analysis;
    }

    Vector normal = cross(rowDirection, columnDirection);
    double normalLength = std::sqrt(dot(normal, normal));

    if (normalLength < 1e-9)
    {
        return analysis;
    }

    for (double &component : normal)
    {
        component /= normalLength;
    }

    double maximumOffsetInPixels = 0.0;

    for (const Vector &position : positions)
    {
        Vector difference = {{position[0] - positions.front()[0], position[1] - positions.front()[1], position[2] - positions.front()[2]}};
        double depth = dot(difference, normal);
        Vector inPlane = {{difference[0] - depth * normal[0], difference[1] - depth * normal[1], difference[2] - depth * normal[2]}};
        std::array<double, 2> offset = {{dot(inPlane, rowDirection), dot(inPlane, columnDirection)}};

        analysis.depths.push_back(depth);
        analysis.offsets.push_back(offset);
        maximumOffsetInPixels = std::max(maximumOffsetInPixels, std::max(std::abs(offset[0]) / pixelSpacingX, std::abs(offset[1]) / pixelSpacingY));
    }

    double lastDepth = analysis.depths.back();
    double lastOffset = std::sqrt(analysis.offsets.back()[0] * analysis.offsets.back()[0] + analysis.offsets.back()[1] * analysis.offsets.back()[1]);
    analysis.tiltDegrees = std::atan2(lastOffset, std::abs(lastDepth)) * 180.0 / M_PI;
    analysis.tilted = maximumOffsetInPixels > 0.5;
    return analysis;
}

bool GantryTiltCorrector::correct(vtkImageData *volume, const Analysis &analysis)
{
    if (!volume || !volume->GetPointData() || !volume->GetPointData()->GetScalars())
    {
        return false;
    }

    int dimensions[3];
    volume->GetDimensions(dimensions);

    if (static_cast<size_t>(dimensions[2]) != analysis.offsets.size() || volume->GetNumberOfScalarComponents() != 1)
    {
        return false;
    }

    double spacing[3];
    volume->GetSpacing(spacing);
    vtkDataArray *scalars = volume->GetPointData()->GetScalars();
    double range[2];
    scalars->GetRange(range);
    double fill = range[0];

    vtkSmartPointer<vtkDataArray> copy;
    copy.TakeReference(scalars->NewInstance());
    copy->SetNumberOfComponents(1);
    copy->SetNumberOfTuples(scalars->GetNumberOfTuples());
    long long sliceSize = static_cast<long long>(dimensions[0]) * dimensions[1];

    for (int z = 0; z < dimensions[2]; z++)
    {
        // The pixel (x, y) of the straight stack comes from the pixel (x - shiftX, y - shiftY) of the slice as it was read
        double shiftX = analysis.offsets[z][0] / spacing[0];
        double shiftY = analysis.offsets[z][1] / spacing[1];
        long long base = z * sliceSize;

        for (int y = 0; y < dimensions[1]; y++)
        {
            double sourceY = y - shiftY;
            int y0 = static_cast<int>(std::floor(sourceY));
            double fy = sourceY - y0;

            for (int x = 0; x < dimensions[0]; x++)
            {
                double sourceX = x - shiftX;
                int x0 = static_cast<int>(std::floor(sourceX));
                double fx = sourceX - x0;
                double value = 0.0;
                double weightSum = 0.0;

                for (int j = 0; j < 2; j++)
                {
                    for (int i = 0; i < 2; i++)
                    {
                        double weight = (i ? fx : 1.0 - fx) * (j ? fy : 1.0 - fy);

                        if (weight <= 0.0)
                        {
                            continue;
                        }

                        int sx = x0 + i;
                        int sy = y0 + j;
                        double sample = (sx >= 0 && sx < dimensions[0] && sy >= 0 && sy < dimensions[1]) ? scalars->GetTuple1(base + sy * dimensions[0] + sx) : fill;
                        value += weight * sample;
                        weightSum += weight;
                    }
                }

                copy->SetTuple1(base + y * dimensions[0] + x, weightSum > 0.0 ? value / weightSum : fill);
            }
        }
    }

    scalars->DeepCopy(copy);
    scalars->Modified();
    return true;
}

} // End namespace udg
