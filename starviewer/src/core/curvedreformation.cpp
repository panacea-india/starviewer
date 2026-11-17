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

#include "curvedreformation.h"

#include <algorithm>
#include <cmath>

#include <QObject>

#include <vtkImageData.h>

namespace udg {

namespace {

struct Vec {
    double x;
    double y;
    double z;
};

Vec operator+(const Vec &a, const Vec &b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
Vec operator-(const Vec &a, const Vec &b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
Vec operator*(const Vec &a, double s) { return {a.x * s, a.y * s, a.z * s}; }
double dot(const Vec &a, const Vec &b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
double norm(const Vec &a) { return std::sqrt(dot(a, a)); }
Vec cross(const Vec &a, const Vec &b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }

Vec normalized(const Vec &a)
{
    double length = norm(a);
    return length > 0.0 ? a * (1.0 / length) : a;
}

// The point of the centripetal Catmull-Rom spline through p1 and p2 (p0 before them and p3 after) at t from 0 to 1
Vec catmullRom(const Vec &p0, const Vec &p1, const Vec &p2, const Vec &p3, double t)
{
    const double alpha = 0.5;
    auto knot = [alpha](const Vec &a, const Vec &b, double previous) {
        double distance = norm(b - a);
        return previous + std::pow(std::max(distance, 1e-9), alpha);
    };

    double t0 = 0.0;
    double t1 = knot(p0, p1, t0);
    double t2 = knot(p1, p2, t1);
    double t3 = knot(p2, p3, t2);
    double u = t1 + (t2 - t1) * t;

    Vec a1 = p0 * ((t1 - u) / (t1 - t0)) + p1 * ((u - t0) / (t1 - t0));
    Vec a2 = p1 * ((t2 - u) / (t2 - t1)) + p2 * ((u - t1) / (t2 - t1));
    Vec a3 = p2 * ((t3 - u) / (t3 - t2)) + p3 * ((u - t2) / (t3 - t2));
    Vec b1 = a1 * ((t2 - u) / (t2 - t0)) + a2 * ((u - t0) / (t2 - t0));
    Vec b2 = a2 * ((t3 - u) / (t3 - t1)) + a3 * ((u - t1) / (t3 - t1));
    return b1 * ((t2 - u) / (t2 - t1)) + b2 * ((u - t1) / (t2 - t1));
}

// A fine polyline of the curve through the points
QVector<Vec> densePath(const double *points, int numberOfPoints)
{
    QVector<Vec> control;

    for (int i = 0; i < numberOfPoints; i++)
    {
        control << Vec{points[3 * i], points[3 * i + 1], points[3 * i + 2]};
    }

    QVector<Vec> dense;
    const int subdivisions = 64;

    for (int i = 0; i + 1 < control.size(); i++)
    {
        // The ends are extended in a straight line, so that the curve does not bend there
        Vec p0 = i == 0 ? control[0] * 2.0 - control[1] : control[i - 1];
        Vec p3 = i + 2 >= control.size() ? control[i + 1] * 2.0 - control[i] : control[i + 2];

        for (int s = 0; s < subdivisions; s++)
        {
            dense << catmullRom(p0, control[i], control[i + 1], p3, static_cast<double>(s) / subdivisions);
        }
    }

    dense << control.last();
    return dense;
}

double sample(vtkImageData *volume, const Vec &point, const double origin[3], const double spacing[3], const int dimensions[3], double background)
{
    double fx = (point.x - origin[0]) / spacing[0];
    double fy = (point.y - origin[1]) / spacing[1];
    double fz = (point.z - origin[2]) / spacing[2];

    if (fx < 0.0 || fy < 0.0 || fz < 0.0 || fx > dimensions[0] - 1 || fy > dimensions[1] - 1 || fz > dimensions[2] - 1)
    {
        return background;
    }

    int i = std::min(static_cast<int>(fx), dimensions[0] - 2 < 0 ? 0 : dimensions[0] - 2);
    int j = std::min(static_cast<int>(fy), dimensions[1] - 2 < 0 ? 0 : dimensions[1] - 2);
    int k = std::min(static_cast<int>(fz), dimensions[2] - 2 < 0 ? 0 : dimensions[2] - 2);
    double ax = fx - i;
    double ay = fy - j;
    double az = fz - k;
    int i1 = std::min(i + 1, dimensions[0] - 1);
    int j1 = std::min(j + 1, dimensions[1] - 1);
    int k1 = std::min(k + 1, dimensions[2] - 1);

    auto value = [&](int a, int b, int c) { return volume->GetScalarComponentAsDouble(a, b, c, 0); };
    double c00 = value(i, j, k) * (1 - ax) + value(i1, j, k) * ax;
    double c10 = value(i, j1, k) * (1 - ax) + value(i1, j1, k) * ax;
    double c01 = value(i, j, k1) * (1 - ax) + value(i1, j, k1) * ax;
    double c11 = value(i, j1, k1) * (1 - ax) + value(i1, j1, k1) * ax;
    double c0 = c00 * (1 - ay) + c10 * ay;
    double c1 = c01 * (1 - ay) + c11 * ay;
    return c0 * (1 - az) + c1 * az;
}

vtkSmartPointer<vtkImageData> createImage(int columns, int rows, double columnSpacing, double step, double width)
{
    vtkSmartPointer<vtkImageData> image = vtkSmartPointer<vtkImageData>::New();
    image->SetDimensions(columns, rows, 1);
    image->SetSpacing(columnSpacing, step, 1.0);
    image->SetOrigin(-width / 2.0, 0.0, 0.0);
    image->AllocateScalars(VTK_FLOAT, 1);
    return image;
}

}

CurvedReformation::Options::Options()
    : step(0.0), width(40.0), columnSpacing(0.0)
{
    reference[0] = 0.0;
    reference[1] = 0.0;
    reference[2] = 1.0;
}

CurvedReformation::Result::Result()
    : success(false), length(0.0), rows(0), columns(0)
{
}

double CurvedReformation::curveLength(const double *points, int numberOfPoints)
{
    if (!points || numberOfPoints < 2)
    {
        return 0.0;
    }

    QVector<Vec> dense = densePath(points, numberOfPoints);
    double length = 0.0;

    for (int i = 1; i < dense.size(); i++)
    {
        length += norm(dense[i] - dense[i - 1]);
    }

    return length;
}

CurvedReformation::Result CurvedReformation::straighten(vtkImageData *volume, const double *points, int numberOfPoints, const Options &options)
{
    Result result;

    if (!volume || volume->GetNumberOfScalarComponents() != 1)
    {
        result.message = QObject::tr("The volume must have a single component.");
        return result;
    }

    if (!points || numberOfPoints < 2)
    {
        result.message = QObject::tr("The path needs at least two points.");
        return result;
    }

    double origin[3];
    double spacing[3];
    int dimensions[3];
    volume->GetOrigin(origin);
    volume->GetSpacing(spacing);
    volume->GetDimensions(dimensions);

    double step = options.step > 0.0 ? options.step : std::min(spacing[0], std::min(spacing[1], spacing[2]));
    double columnSpacing = options.columnSpacing > 0.0 ? options.columnSpacing : step;
    double width = std::max(options.width, columnSpacing);

    QVector<Vec> dense = densePath(points, numberOfPoints);
    QVector<double> cumulative(dense.size(), 0.0);

    for (int i = 1; i < dense.size(); i++)
    {
        cumulative[i] = cumulative[i - 1] + norm(dense[i] - dense[i - 1]);
    }

    double length = cumulative.last();

    if (length < step)
    {
        result.message = QObject::tr("The path is shorter than one step of %1 mm.").arg(step);
        return result;
    }

    int rows = static_cast<int>(std::floor(length / step)) + 1;
    int columns = static_cast<int>(std::floor(width / columnSpacing)) + 1;

    result.normalImage = createImage(columns, rows, columnSpacing, step, width);
    result.binormalImage = createImage(columns, rows, columnSpacing, step, width);
    float *normalPixels = static_cast<float*>(result.normalImage->GetScalarPointer());
    float *binormalPixels = static_cast<float*>(result.binormalImage->GetScalarPointer());

    double range[2];
    volume->GetScalarRange(range);
    double background = range[0];

    Vec reference = normalized(Vec{options.reference[0], options.reference[1], options.reference[2]});

    if (norm(reference) == 0.0)
    {
        reference = Vec{0.0, 0.0, 1.0};
    }

    int segment = 0;

    for (int row = 0; row < rows; row++)
    {
        double distance = std::min(row * step, length);

        while (segment + 2 < dense.size() && cumulative[segment + 1] < distance)
        {
            segment++;
        }

        double span = cumulative[segment + 1] - cumulative[segment];
        double fraction = span > 0.0 ? (distance - cumulative[segment]) / span : 0.0;
        Vec center = dense[segment] + (dense[segment + 1] - dense[segment]) * fraction;

        // The tangent over a short stretch of the curve, so that it is the direction of the path and not of a tiny piece of it
        int before = std::max(0, segment - 4);
        int after = std::min(static_cast<int>(dense.size()) - 1, segment + 5);
        Vec tangent = normalized(dense[after] - dense[before]);

        Vec up = reference;

        if (std::abs(dot(up, tangent)) > 0.99)
        {
            up = std::abs(tangent.y) < 0.9 ? Vec{0.0, 1.0, 0.0} : Vec{1.0, 0.0, 0.0};
        }

        Vec normal = normalized(up - tangent * dot(up, tangent));
        Vec binormal = normalized(cross(tangent, normal));

        result.centers << center.x << center.y << center.z;
        result.tangents << tangent.x << tangent.y << tangent.z;

        for (int column = 0; column < columns; column++)
        {
            double offset = -width / 2.0 + column * columnSpacing;
            normalPixels[row * columns + column] = static_cast<float>(sample(volume, center + normal * offset, origin, spacing, dimensions, background));
            binormalPixels[row * columns + column] = static_cast<float>(sample(volume, center + binormal * offset, origin, spacing, dimensions, background));
        }
    }

    result.length = length;
    result.rows = rows;
    result.columns = columns;
    result.success = true;
    return result;
}

} // End namespace udg
