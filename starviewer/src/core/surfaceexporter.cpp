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

#include "surfaceexporter.h"

#include <algorithm>

#include <QObject>

#include <vtkCleanPolyData.h>
#include <vtkFeatureEdges.h>
#include <vtkFlyingEdges3D.h>
#include <vtkImageConstantPad.h>
#include <vtkImageData.h>
#include <vtkMassProperties.h>
#include <vtkPolyData.h>
#include <vtkPolyDataConnectivityFilter.h>
#include <vtkSTLWriter.h>
#include <vtkWindowedSincPolyDataFilter.h>
#include <vtkTriangleFilter.h>

namespace udg {

SurfaceExporter::Options::Options()
    : threshold(0.0), largestPieceOnly(false), smoothingIterations(0), binary(true)
{
}

SurfaceExporter::Result::Result()
    : success(false), triangles(0), volumeMilliliters(0.0), areaSquareCentimeters(0.0), closed(false)
{
    for (int i = 0; i < 6; i++)
    {
        bounds[i] = 0.0;
    }
}

vtkSmartPointer<vtkPolyData> SurfaceExporter::createSurface(vtkImageData *volume, const Options &options, QString *error)
{
    auto fail = [error](const QString &reason) {
        if (error)
        {
            *error = reason;
        }
        return vtkSmartPointer<vtkPolyData>();
    };

    if (!volume || volume->GetNumberOfScalarComponents() != 1)
    {
        return fail(QObject::tr("The volume must have a single component."));
    }

    int extent[6];
    volume->GetExtent(extent);

    if (extent[1] - extent[0] < 1 || extent[3] - extent[2] < 1 || extent[5] - extent[4] < 1)
    {
        return fail(QObject::tr("The volume is too small to have a surface."));
    }

    // One voxel of a value below the threshold all around, so that an object that reaches the edge of the volume still has a closed surface
    double range[2];
    volume->GetScalarRange(range);
    vtkSmartPointer<vtkImageConstantPad> pad = vtkSmartPointer<vtkImageConstantPad>::New();
    pad->SetInputData(volume);
    pad->SetOutputWholeExtent(extent[0] - 1, extent[1] + 1, extent[2] - 1, extent[3] + 1, extent[4] - 1, extent[5] + 1);
    pad->SetConstant(std::min(range[0], options.threshold - 1.0));
    pad->Update();

    vtkSmartPointer<vtkFlyingEdges3D> edges = vtkSmartPointer<vtkFlyingEdges3D>::New();
    edges->SetInputConnection(pad->GetOutputPort());
    edges->SetValue(0, options.threshold);
    edges->ComputeNormalsOff();
    edges->ComputeGradientsOff();
    edges->Update();

    vtkSmartPointer<vtkCleanPolyData> clean = vtkSmartPointer<vtkCleanPolyData>::New();
    clean->SetInputConnection(edges->GetOutputPort());
    clean->Update();

    vtkSmartPointer<vtkPolyData> surface = clean->GetOutput();

    if (surface->GetNumberOfPolys() == 0)
    {
        return fail(QObject::tr("No voxel is above the threshold, so there is no surface."));
    }

    if (options.largestPieceOnly)
    {
        vtkSmartPointer<vtkPolyDataConnectivityFilter> connectivity = vtkSmartPointer<vtkPolyDataConnectivityFilter>::New();
        connectivity->SetInputData(surface);
        connectivity->SetExtractionModeToLargestRegion();
        connectivity->Update();

        vtkSmartPointer<vtkCleanPolyData> cleanRegion = vtkSmartPointer<vtkCleanPolyData>::New();
        cleanRegion->SetInputConnection(connectivity->GetOutputPort());
        cleanRegion->Update();
        surface = cleanRegion->GetOutput();
    }

    int iterations = std::max(0, std::min(50, options.smoothingIterations));

    if (iterations > 0)
    {
        vtkSmartPointer<vtkWindowedSincPolyDataFilter> smoothing = vtkSmartPointer<vtkWindowedSincPolyDataFilter>::New();
        smoothing->SetInputData(surface);
        smoothing->SetNumberOfIterations(iterations);
        smoothing->SetPassBand(0.1);
        smoothing->BoundarySmoothingOff();
        smoothing->FeatureEdgeSmoothingOff();
        smoothing->NonManifoldSmoothingOn();
        smoothing->NormalizeCoordinatesOn();
        smoothing->Update();
        surface = smoothing->GetOutput();
    }

    vtkSmartPointer<vtkTriangleFilter> triangles = vtkSmartPointer<vtkTriangleFilter>::New();
    triangles->SetInputData(surface);
    triangles->Update();
    return triangles->GetOutput();
}

SurfaceExporter::Result SurfaceExporter::exportStl(vtkImageData *volume, const QString &path, const Options &options)
{
    Result result;
    QString error;
    vtkSmartPointer<vtkPolyData> surface = createSurface(volume, options, &error);

    if (!surface)
    {
        result.message = error;
        return result;
    }

    result.triangles = static_cast<int>(surface->GetNumberOfPolys());
    surface->GetBounds(result.bounds);

    vtkSmartPointer<vtkMassProperties> properties = vtkSmartPointer<vtkMassProperties>::New();
    properties->SetInputData(surface);
    properties->Update();
    result.volumeMilliliters = properties->GetVolume() / 1000.0;
    result.areaSquareCentimeters = properties->GetSurfaceArea() / 100.0;

    vtkSmartPointer<vtkFeatureEdges> openEdges = vtkSmartPointer<vtkFeatureEdges>::New();
    openEdges->SetInputData(surface);
    openEdges->BoundaryEdgesOn();
    openEdges->FeatureEdgesOff();
    openEdges->NonManifoldEdgesOff();
    openEdges->ManifoldEdgesOff();
    openEdges->Update();
    result.closed = openEdges->GetOutput()->GetNumberOfCells() == 0;

    vtkSmartPointer<vtkSTLWriter> writer = vtkSmartPointer<vtkSTLWriter>::New();
    writer->SetInputData(surface);
    writer->SetFileName(path.toLocal8Bit().constData());

    if (options.binary)
    {
        writer->SetFileTypeToBinary();
    }
    else
    {
        writer->SetFileTypeToASCII();
    }

    writer->Write();

    if (writer->GetErrorCode() != 0)
    {
        result.message = QObject::tr("The file %1 could not be written.").arg(path);
        return result;
    }

    if (!result.closed)
    {
        result.message = QObject::tr("The surface is not closed, so its volume is approximate.");
    }

    result.success = true;
    return result;
}

} // End namespace udg
