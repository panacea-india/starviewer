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

#include "rtregionstatistics.h"

#include <algorithm>
#include <cmath>

#include <QMap>
#include <QVector>

namespace udg {

namespace {

typedef std::array<double, 3> Vec;

// The Newell vector: perpendicular to the polygon, with a length of twice its area
Vec newell(const QList<Vec> &p)
{
    Vec n = {0.0, 0.0, 0.0};

    for (int i = 0; i < p.size(); i++)
    {
        const Vec &a = p[i];
        const Vec &b = p[(i + 1) % p.size()];
        n[0] += (a[1] - b[1]) * (a[2] + b[2]);
        n[1] += (a[2] - b[2]) * (a[0] + b[0]);
        n[2] += (a[0] - b[0]) * (a[1] + b[1]);
    }

    return n;
}

}

bool RtRegionStatistics::contains(const RtStructureSet::Contour &contour, const std::array<double, 3> &point)
{
    // The contour and the point in the plane of the two axes that are not the main one of the normal
    Vec n = newell(contour.points);
    int drop = std::abs(n[0]) >= std::abs(n[1]) && std::abs(n[0]) >= std::abs(n[2]) ? 0 : std::abs(n[1]) >= std::abs(n[2]) ? 1 : 2;
    int a = drop == 0 ? 1 : 0;
    int b = drop == 2 ? 1 : 2;
    bool inside = false;
    int count = contour.points.size();

    for (int i = 0, j = count - 1; i < count; j = i++)
    {
        const Vec &p = contour.points[i];
        const Vec &q = contour.points[j];

        if ((p[b] > point[b]) != (q[b] > point[b]) && point[a] < (q[a] - p[a]) * (point[b] - p[b]) / (q[b] - p[b]) + p[a])
        {
            inside = !inside;
        }
    }

    return inside;
}

double RtRegionStatistics::area(const QList<std::array<double, 3>> &points)
{
    if (points.size() < 3)
    {
        return 0.0;
    }

    Vec n = newell(points);
    return 0.5 * std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
}

RtRegionStatistics::Result RtRegionStatistics::compute(const RtStructureSet::Roi &roi)
{
    Result result;
    // The area of each plane, by its position along the normal in units of 0.01 mm
    QMap<qint64, double> areas;

    // The closed contours of each plane, to tell which are inside others (holes)
    struct Item {
        const RtStructureSet::Contour *contour;
        double area;
        qint64 plane;
    };

    QList<Item> items;

    for (const RtStructureSet::Contour &contour : roi.contours)
    {
        if (!contour.closed || contour.points.size() < 3)
        {
            continue;
        }

        Vec n = newell(contour.points);
        double length = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);

        if (length <= 0.0)
        {
            continue;
        }

        // The normal with a fixed sign, so that the same plane has the same position whatever the direction of the contour
        for (double &c : n)
        {
            c /= length;
        }

        if (n[2] < 0.0 || (n[2] == 0.0 && (n[1] < 0.0 || (n[1] == 0.0 && n[0] < 0.0))))
        {
            for (double &c : n)
            {
                c = -c;
            }
        }

        double position = n[0] * contour.points[0][0] + n[1] * contour.points[0][1] + n[2] * contour.points[0][2];
        Item item = {&contour, 0.5 * length, static_cast<qint64>(std::llround(position * 100.0))};
        items.append(item);
        result.contours++;
    }

    // A contour inside an odd number of others of its plane is a hole: its area is taken away
    for (int i = 0; i < items.size(); i++)
    {
        int depth = 0;

        for (int j = 0; j < items.size(); j++)
        {
            if (i != j && items[i].plane == items[j].plane && items[j].area > items[i].area && contains(*items[j].contour, items[i].contour->points[0]))
            {
                depth++;
            }
        }

        areas[items[i].plane] += (depth % 2 == 0 ? 1.0 : -1.0) * items[i].area;
    }

    result.planes = areas.size();

    if (result.planes < 2)
    {
        return result;
    }

    QVector<double> gaps;
    QList<qint64> keys = areas.keys();

    for (int i = 1; i < keys.size(); i++)
    {
        gaps << (keys[i] - keys[i - 1]) / 100.0;
    }

    std::sort(gaps.begin(), gaps.end());
    result.planeSpacingMm = gaps[gaps.size() / 2];

    double sum = 0.0;

    for (double a : areas)
    {
        sum += a;
    }

    result.volumeMl = sum * result.planeSpacingMm / 1000.0;
    return result;
}

} // End namespace udg
