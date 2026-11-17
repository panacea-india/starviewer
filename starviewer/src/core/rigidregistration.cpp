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

#include "rigidregistration.h"

#include <algorithm>
#include <cmath>

#include <QObject>

#include <vtkImageCast.h>
#include <vtkImageData.h>
#include <vtkImageReslice.h>
#include <vtkMatrix4x4.h>
#include <vtkSmartPointer.h>
#include <vtkTransform.h>

#include <itkCenteredTransformInitializer.h>
#include <itkImage.h>
#include <itkImageRegistrationMethod.h>
#include <itkLinearInterpolateImageFunction.h>
#include <itkMattesMutualInformationImageToImageMetric.h>
#include <itkMeanSquaresImageToImageMetric.h>
#include <itkShrinkImageFilter.h>
#include <itkSmoothingRecursiveGaussianImageFilter.h>
#include <itkVersorRigid3DTransform.h>
#include <itkVersorRigid3DTransformOptimizer.h>
#include <itkVTKImageToImageFilter.h>

namespace udg {

namespace {

using ImageType = itk::Image<float, 3>;
using TransformType = itk::VersorRigid3DTransform<double>;
using OptimizerType = itk::VersorRigid3DTransformOptimizer;
using InterpolatorType = itk::LinearInterpolateImageFunction<ImageType, double>;
using RegistrationType = itk::ImageRegistrationMethod<ImageType, ImageType>;
using MutualInformationType = itk::MattesMutualInformationImageToImageMetric<ImageType, ImageType>;
using MeanSquaresType = itk::MeanSquaresImageToImageMetric<ImageType, ImageType>;

const double Pi = 3.14159265358979323846;

// Converts the volume to an ITK image of floats, with its origin and spacing
ImageType::Pointer toItkImage(vtkImageData *volume)
{
    vtkSmartPointer<vtkImageCast> cast = vtkSmartPointer<vtkImageCast>::New();
    cast->SetInputData(volume);
    cast->SetOutputScalarTypeToFloat();
    cast->Update();

    using ConnectorType = itk::VTKImageToImageFilter<ImageType>;
    ConnectorType::Pointer connector = ConnectorType::New();
    connector->SetInput(cast->GetOutput());
    connector->Update();

    // The connector shares the memory of the VTK image, which goes when this function ends: keep a copy
    ImageType::Pointer image = ImageType::New();
    image->CopyInformation(connector->GetOutput());
    image->SetRegions(connector->GetOutput()->GetBufferedRegion());
    image->Allocate();
    std::copy(connector->GetOutput()->GetBufferPointer(), connector->GetOutput()->GetBufferPointer() + connector->GetOutput()->GetBufferedRegion().GetNumberOfPixels(),
              image->GetBufferPointer());
    return image;
}

// Smooths the image, so that the metric changes gently with the transform, and then keeps one voxel of each factor. This is the way of a resolution pyramid.
ImageType::Pointer smoothAndShrink(ImageType *image, unsigned int factor)
{
    double smallestSpacing = std::min(image->GetSpacing()[0], std::min(image->GetSpacing()[1], image->GetSpacing()[2]));

    using SmoothingType = itk::SmoothingRecursiveGaussianImageFilter<ImageType, ImageType>;
    SmoothingType::Pointer smoothing = SmoothingType::New();
    smoothing->SetInput(image);
    smoothing->SetSigma(0.5 * factor * smallestSpacing);
    smoothing->Update();

    if (factor <= 1)
    {
        return smoothing->GetOutput();
    }

    using ShrinkType = itk::ShrinkImageFilter<ImageType, ImageType>;
    ShrinkType::Pointer filter = ShrinkType::New();
    filter->SetInput(smoothing->GetOutput());
    filter->SetShrinkFactors(factor);
    filter->Update();
    return filter->GetOutput();
}

// The angle in degrees of the rotation of a versor
double rotationAngleDegrees(const TransformType *transform)
{
    return transform->GetVersor().GetAngle() * 180.0 / Pi;
}

}

RigidRegistration::Options::Options()
    : metric(Metric::MutualInformation), levels(3), maximumIterationsCoarse(120), maximumIterationsFine(40), samplingFraction(0.2), histogramBins(50)
{
}

RigidRegistration::Result::Result()
    : success(false), rotationDegrees(0.0), translationMillimeters(0.0), metricValue(0.0), iterations(0)
{
    for (int i = 0; i < 16; i++)
    {
        matrix[i] = (i % 5 == 0) ? 1.0 : 0.0;
    }
}

RigidRegistration::Result RigidRegistration::run(vtkImageData *fixed, vtkImageData *moving, const Options &options)
{
    Result result;

    if (!fixed || !moving)
    {
        result.message = QObject::tr("A volume is missing.");
        return result;
    }

    if (fixed->GetNumberOfScalarComponents() != 1 || moving->GetNumberOfScalarComponents() != 1)
    {
        result.message = QObject::tr("The volumes must have a single component.");
        return result;
    }

    int levels = std::max(1, std::min(4, options.levels));

    try
    {
        ImageType::Pointer fixedImage = toItkImage(fixed);
        ImageType::Pointer movingImage = toItkImage(moving);

        TransformType::Pointer transform = TransformType::New();
        transform->SetIdentity();

        // Start with the centers of the volumes on top of each other
        using InitializerType = itk::CenteredTransformInitializer<TransformType, ImageType, ImageType>;
        InitializerType::Pointer initializer = InitializerType::New();
        initializer->SetTransform(transform);
        initializer->SetFixedImage(fixedImage);
        initializer->SetMovingImage(movingImage);
        initializer->GeometryOn();
        initializer->InitializeTransform();

        TransformType::ParametersType parameters = transform->GetParameters();
        double finalMetric = 0.0;
        unsigned int finalIterations = 0;

        for (int level = levels - 1; level >= 0; level--)
        {
            unsigned int factor = 1u << level;
            ImageType::Pointer fixedLevel = smoothAndShrink(fixedImage, factor);
            ImageType::Pointer movingLevel = smoothAndShrink(movingImage, factor);

            RegistrationType::Pointer registration = RegistrationType::New();
            OptimizerType::Pointer optimizer = OptimizerType::New();
            InterpolatorType::Pointer interpolator = InterpolatorType::New();
            TransformType::Pointer levelTransform = TransformType::New();
            levelTransform->SetFixedParameters(transform->GetFixedParameters());
            levelTransform->SetParameters(parameters);

            unsigned long samples = static_cast<unsigned long>(std::max<double>(2000.0, fixedLevel->GetBufferedRegion().GetNumberOfPixels()
                                                                                  * std::min(1.0, std::max(0.01, options.samplingFraction))));

            if (options.metric == Metric::MutualInformation)
            {
                MutualInformationType::Pointer metric = MutualInformationType::New();
                metric->SetNumberOfHistogramBins(options.histogramBins);
                // A small volume is looked at in full, which is exact and costs little. A large one is sampled, with a fixed seed so that the
                // same volumes give the same result.
                if (fixedLevel->GetBufferedRegion().GetNumberOfPixels() <= 70000)
                {
                    metric->UseAllPixelsOn();
                }
                else
                {
                    metric->SetNumberOfSpatialSamples(samples);
                }

                metric->ReinitializeSeed(76926294);
                registration->SetMetric(metric);
            }
            else
            {
                MeanSquaresType::Pointer metric = MeanSquaresType::New();
                registration->SetMetric(metric);
            }

            // The rotation (a versor) and the translation (mm) have different sizes
            OptimizerType::ScalesType scales(levelTransform->GetNumberOfParameters());
            const double translationScale = 1.0 / 100.0;
            scales[0] = scales[1] = scales[2] = 1.0;
            scales[3] = scales[4] = scales[5] = translationScale;
            optimizer->SetScales(scales);
            // Large steps at the coarse level, where the volumes are small and blurred, and small ones at the fine level, where a large step would only
            // throw away what the coarse levels found
            optimizer->SetMaximumStepLength(0.025 * factor);
            optimizer->SetMinimumStepLength(0.00001);
            optimizer->SetRelaxationFactor(0.7);
            int maximumIterations = options.maximumIterationsFine + (options.maximumIterationsCoarse - options.maximumIterationsFine) * level / std::max(1, levels - 1);
            optimizer->SetNumberOfIterations(std::max(1, maximumIterations));
            optimizer->MinimizeOn();

            registration->SetOptimizer(optimizer);
            registration->SetTransform(levelTransform);
            registration->SetInterpolator(interpolator);
            registration->SetFixedImage(fixedLevel);
            registration->SetMovingImage(movingLevel);
            registration->SetFixedImageRegion(fixedLevel->GetBufferedRegion());
            registration->SetInitialTransformParameters(parameters);
            registration->Update();

            // Do no harm: the optimization is only taken if the metric, on the same voxels, is better than where the level started. A volume that
            // was already in place (the centers of two series of a patient usually put them there) must not be turned by the noise of the metric.
            RegistrationType::ParametersType optimized = registration->GetLastTransformParameters();
            double startValue = registration->GetMetric()->GetValue(parameters);
            double optimizedValue = registration->GetMetric()->GetValue(optimized);

            if (optimizedValue < startValue - 0.005 * std::abs(startValue))
            {
                parameters = optimized;
                finalMetric = optimizedValue;
            }
            else
            {
                finalMetric = startValue;
            }

            finalIterations = optimizer->GetCurrentIteration();
        }

        transform->SetParameters(parameters);

        // The matrix by rows: the images of the axes, and of the origin
        ImageType::PointType origin;
        origin.Fill(0.0);
        ImageType::PointType movedOrigin = transform->TransformPoint(origin);

        for (int column = 0; column < 3; column++)
        {
            ImageType::PointType axis;
            axis.Fill(0.0);
            axis[column] = 1.0;
            ImageType::PointType movedAxis = transform->TransformPoint(axis);

            for (int row = 0; row < 3; row++)
            {
                result.matrix[row * 4 + column] = movedAxis[row] - movedOrigin[row];
            }
        }

        for (int row = 0; row < 3; row++)
        {
            result.matrix[row * 4 + 3] = movedOrigin[row];
        }

        result.rotationDegrees = rotationAngleDegrees(transform);
        result.translationMillimeters = std::sqrt(movedOrigin[0] * movedOrigin[0] + movedOrigin[1] * movedOrigin[1] + movedOrigin[2] * movedOrigin[2]);
        result.metricValue = finalMetric;
        result.iterations = static_cast<int>(finalIterations);
        result.success = true;
    }
    catch (const itk::ExceptionObject &exception)
    {
        result.success = false;
        result.message = QObject::tr("The registration failed: %1").arg(QString::fromLatin1(exception.GetDescription()));
    }
    catch (const std::exception &exception)
    {
        result.success = false;
        result.message = QObject::tr("The registration failed: %1").arg(QString::fromLatin1(exception.what()));
    }

    return result;
}

vtkSmartPointer<vtkImageData> RigidRegistration::resample(vtkImageData *moving, vtkImageData *fixed, const double matrix[16])
{
    vtkSmartPointer<vtkMatrix4x4> vtkMatrix = vtkSmartPointer<vtkMatrix4x4>::New();

    for (int row = 0; row < 4; row++)
    {
        for (int column = 0; column < 4; column++)
        {
            vtkMatrix->SetElement(row, column, matrix[row * 4 + column]);
        }
    }

    // The reslice transform sends the points of the output (the fixed grid) to the input (the moving volume), as the matrix does
    vtkSmartPointer<vtkTransform> transform = vtkSmartPointer<vtkTransform>::New();
    transform->SetMatrix(vtkMatrix);

    double range[2];
    moving->GetScalarRange(range);

    int extent[6];
    double origin[3];
    double spacing[3];
    fixed->GetExtent(extent);
    fixed->GetOrigin(origin);
    fixed->GetSpacing(spacing);

    vtkSmartPointer<vtkImageReslice> reslice = vtkSmartPointer<vtkImageReslice>::New();
    reslice->SetInputData(moving);
    reslice->SetResliceTransform(transform);
    reslice->SetInterpolationModeToLinear();
    reslice->SetOutputExtent(extent);
    reslice->SetOutputOrigin(origin);
    reslice->SetOutputSpacing(spacing);
    reslice->SetBackgroundLevel(range[0]);
    reslice->Update();

    vtkSmartPointer<vtkImageData> result = vtkSmartPointer<vtkImageData>::New();
    result->DeepCopy(reslice->GetOutput());
    return result;
}

void RigidRegistration::transformPoint(const double matrix[16], const double fixedPoint[3], double movingPoint[3])
{
    for (int row = 0; row < 3; row++)
    {
        movingPoint[row] = matrix[row * 4] * fixedPoint[0] + matrix[row * 4 + 1] * fixedPoint[1] + matrix[row * 4 + 2] * fixedPoint[2] + matrix[row * 4 + 3];
    }
}

} // End namespace udg
