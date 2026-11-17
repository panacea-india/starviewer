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

#include "flythroughpath.h"

#include <array>
#include <cmath>

namespace udg {

namespace {

typedef std::array<double, 3> Vec;

Vec sub(const Vec &a, const Vec &b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }
Vec add(const Vec &a, const Vec &b) { return {a[0] + b[0], a[1] + b[1], a[2] + b[2]}; }
Vec scale(const Vec &a, double k) { return {a[0] * k, a[1] * k, a[2] * k}; }
double dot(const Vec &a, const Vec &b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
double norm(const Vec &a) { return std::sqrt(dot(a, a)); }
Vec unit(const Vec &a) { double n = norm(a); return n > 0.0 ? scale(a, 1.0 / n) : Vec{0.0, 0.0, 0.0}; }

QVector<Vec> distinctPoints(const QVector<double> &points)
{
    QVector<Vec> result;

    for (int i = 0; i + 2 < points.size(); i += 3)
    {
        Vec p = {points[i], points[i + 1], points[i + 2]};

        if (!std::isfinite(p[0]) || !std::isfinite(p[1]) || !std::isfinite(p[2]))
        {
            return QVector<Vec>();
        }

        if (result.isEmpty() || norm(sub(p, result.last())) > 1e-6)
        {
            result << p;
        }
    }

    return result;
}

// Catmull-Rom through the points, sampled densely
QVector<Vec> denseCurve(const QVector<Vec> &p)
{
    const int samples = 24;
    QVector<Vec> curve;

    for (int i = 0; i + 1 < p.size(); i++)
    {
        const Vec &p0 = p[qMax(i - 1, 0)];
        const Vec &p1 = p[i];
        const Vec &p2 = p[i + 1];
        const Vec &p3 = p[qMin(i + 2, p.size() - 1)];

        for (int s = 0; s < samples; s++)
        {
            double t = static_cast<double>(s) / samples;
            double t2 = t * t;
            double t3 = t2 * t;
            Vec point;

            for (int k = 0; k < 3; k++)
            {
                point[k] = 0.5 * ((2.0 * p1[k]) + (-p0[k] + p2[k]) * t + (2.0 * p0[k] - 5.0 * p1[k] + 4.0 * p2[k] - p3[k]) * t2 + (-p0[k] + 3.0 * p1[k] - 3.0 * p2[k] + p3[k]) * t3);
            }

            curve << point;
        }
    }

    curve << p.last();
    return curve;
}

QVector<double> cumulative(const QVector<Vec> &curve)
{
    QVector<double> s(curve.size(), 0.0);

    for (int i = 1; i < curve.size(); i++)
    {
        s[i] = s[i - 1] + norm(sub(curve[i], curve[i - 1]));
    }

    return s;
}

QVector<double> g_lastPath;

}

double FlythroughPath::length(const QVector<double> &points)
{
    QVector<Vec> p = distinctPoints(points);
    return p.size() < 2 ? 0.0 : cumulative(denseCurve(p)).last();
}

QList<SavedView> FlythroughPath::views(const QVector<double> &points, int frames, double lookAheadMillimeters, double viewAngleDegrees)
{
    QList<SavedView> result;
    QVector<Vec> p = distinctPoints(points);

    if (p.size() < 2 || frames < 2 || !(lookAheadMillimeters > 0.0) || !std::isfinite(lookAheadMillimeters) || !(viewAngleDegrees > 0.0) || !std::isfinite(viewAngleDegrees))
    {
        return result;
    }

    QVector<Vec> curve = denseCurve(p);
    QVector<double> s = cumulative(curve);
    double total = s.last();

    // Position and direction at the arc length d (called with growing d)
    int index = 0;
    auto at = [&](double d, Vec *direction) {
        while (index + 2 < curve.size() && s[index + 1] < d)
        {
            index++;
        }

        double span = s[index + 1] - s[index];
        double t = span > 0.0 ? qBound(0.0, (d - s[index]) / span, 1.0) : 0.0;
        *direction = unit(sub(curve[index + 1], curve[index]));
        return add(curve[index], scale(sub(curve[index + 1], curve[index]), t));
    };

    Vec up = {0.0, 0.0, 1.0};
    bool first = true;

    for (int f = 0; f < frames; f++)
    {
        Vec direction;
        Vec position = at(total * f / (frames - 1), &direction);

        if (first)
        {
            if (std::abs(dot(up, direction)) > 0.99)
            {
                up = {0.0, 1.0, 0.0};
            }

            first = false;
        }

        // The up direction that keeps the most of the previous one and is perpendicular to the new direction
        up = unit(sub(up, scale(direction, dot(up, direction))));

        SavedView view;
        view.name = QString("Flight %1").arg(f + 1);
        view.position = position;
        view.focalPoint = add(position, scale(direction, lookAheadMillimeters));
        view.viewUp = up;
        view.viewAngle = viewAngleDegrees;
        view.parallelProjection = false;
        result << view;
    }

    return result;
}

void FlythroughPath::setLastPath(const QVector<double> &points)
{
    g_lastPath = points;
}

QVector<double> FlythroughPath::lastPath()
{
    return g_lastPath;
}

} // End namespace udg
