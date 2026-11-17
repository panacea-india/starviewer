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

#include "imagefilters.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include <vtkDataArray.h>
#include <vtkImageData.h>
#include <vtkPointData.h>

namespace udg {

namespace {

std::vector<double> kernel(double sigmaPixels)
{
    int radius = std::max(1, static_cast<int>(std::ceil(3.0 * sigmaPixels)));
    std::vector<double> weights(2 * radius + 1);
    double sum = 0.0;

    for (int i = -radius; i <= radius; i++)
    {
        weights[i + radius] = std::exp(-0.5 * i * i / (sigmaPixels * sigmaPixels));
        sum += weights[i + radius];
    }

    for (double &weight : weights)
    {
        weight /= sum;
    }

    return weights;
}

// The slices blurred, as doubles, along x then y (the edge pixels are repeated)
std::vector<double> blurred(vtkImageData *volume, double sigmaX, double sigmaY)
{
    int d[3];
    volume->GetDimensions(d);
    vtkDataArray *scalars = volume->GetPointData()->GetScalars();
    long long sliceSize = static_cast<long long>(d[0]) * d[1];
    std::vector<double> input(static_cast<size_t>(sliceSize) * d[2]);

    for (long long i = 0; i < static_cast<long long>(input.size()); i++)
    {
        input[i] = scalars->GetTuple1(i);
    }

    std::vector<double> kx = kernel(sigmaX);
    std::vector<double> ky = kernel(sigmaY);
    int rx = static_cast<int>(kx.size() / 2);
    int ry = static_cast<int>(ky.size() / 2);
    std::vector<double> temp(static_cast<size_t>(sliceSize));
    std::vector<double> output(input.size());

    for (int z = 0; z < d[2]; z++)
    {
        const double *in = &input[static_cast<size_t>(z) * sliceSize];

        for (int y = 0; y < d[1]; y++)
        {
            for (int x = 0; x < d[0]; x++)
            {
                double sum = 0.0;

                for (int k = -rx; k <= rx; k++)
                {
                    sum += kx[k + rx] * in[y * d[0] + std::min(std::max(x + k, 0), d[0] - 1)];
                }

                temp[y * d[0] + x] = sum;
            }
        }

        double *out = &output[static_cast<size_t>(z) * sliceSize];

        for (int y = 0; y < d[1]; y++)
        {
            for (int x = 0; x < d[0]; x++)
            {
                double sum = 0.0;

                for (int k = -ry; k <= ry; k++)
                {
                    sum += ky[k + ry] * temp[std::min(std::max(y + k, 0), d[1] - 1) * d[0] + x];
                }

                out[y * d[0] + x] = sum;
            }
        }
    }

    return output;
}

bool valid(vtkImageData *volume, double sigma)
{
    return volume && volume->GetPointData() && volume->GetPointData()->GetScalars() && volume->GetNumberOfScalarComponents() == 1 && sigma > 0.0
           && std::isfinite(sigma);
}

vtkSmartPointer<vtkImageData> store(vtkImageData *volume, const std::vector<double> &values)
{
    vtkSmartPointer<vtkImageData> result = vtkSmartPointer<vtkImageData>::New();
    result->DeepCopy(volume);
    vtkDataArray *scalars = result->GetPointData()->GetScalars();
    double low = scalars->GetDataTypeMin();
    double high = scalars->GetDataTypeMax();
    bool integer = scalars->GetDataType() != VTK_FLOAT && scalars->GetDataType() != VTK_DOUBLE;

    for (long long i = 0; i < static_cast<long long>(values.size()); i++)
    {
        double value = std::min(std::max(values[i], low), high);
        scalars->SetTuple1(i, integer ? std::floor(value + 0.5) : value);
    }

    scalars->Modified();
    return result;
}

}

vtkSmartPointer<vtkImageData> ImageFilters::gaussianSmooth(vtkImageData *volume, double sigmaMillimeters)
{
    if (!valid(volume, sigmaMillimeters))
    {
        return nullptr;
    }

    double spacing[3];
    volume->GetSpacing(spacing);
    return store(volume, blurred(volume, std::max(sigmaMillimeters / std::abs(spacing[0]), 1e-3), std::max(sigmaMillimeters / std::abs(spacing[1]), 1e-3)));
}

vtkSmartPointer<vtkImageData> ImageFilters::unsharpMask(vtkImageData *volume, double sigmaMillimeters, double amount)
{
    if (!valid(volume, sigmaMillimeters) || amount < 0.0)
    {
        return nullptr;
    }

    double spacing[3];
    volume->GetSpacing(spacing);
    std::vector<double> values = blurred(volume, std::max(sigmaMillimeters / std::abs(spacing[0]), 1e-3), std::max(sigmaMillimeters / std::abs(spacing[1]), 1e-3));
    vtkDataArray *scalars = volume->GetPointData()->GetScalars();

    for (long long i = 0; i < static_cast<long long>(values.size()); i++)
    {
        double original = scalars->GetTuple1(i);
        values[i] = original + amount * (original - values[i]);
    }

    return store(volume, values);
}

} // End namespace udg
