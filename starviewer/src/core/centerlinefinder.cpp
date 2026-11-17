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

#include "centerlinefinder.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <limits>
#include <queue>
#include <vector>

#include <vtkImageData.h>

#include "logging.h"

namespace udg {

namespace {

struct Grid {
    int nx, ny, nz;
    int extent[6];
    double spacing[3];
    double origin[3];

    qint64 index(int x, int y, int z) const { return (static_cast<qint64>(z) * ny + y) * nx + x; }
    bool contains(int x, int y, int z) const { return x >= 0 && y >= 0 && z >= 0 && x < nx && y < ny && z < nz; }
};

}

CenterlineFinder::Result CenterlineFinder::findAutomatically(vtkImageData *data, const double start[3], const double end[3], double tolerance)
{
    Result best;
    best.message = "The points are outside the volume.";


    bool first = true;
    bool found = false;
    Result chosen;

    // The range is taken from the value around each of the two points, in turn: a point at the very end of the structure or at its border can have mostly what is next to it around
    for (int attempt = 0; attempt < 2; attempt++)
    {
        const double *seed = attempt == 0 ? start : end;
        double lower, upper;

        if (!rangeAround(data, seed, tolerance, &lower, &upper))
        {
            continue;
        }

        Result result = find(data, start, end, lower, upper);
        INFO_LOG(QString("Centerline attempt %1: range %2 to %3, success %4 (%5), %6 voxels in the structure, %7 mm").arg(attempt).arg(lower).arg(upper).arg(result.success).arg(result.message).arg(result.structureVoxels).arg(result.lengthMm));

        if (result.success)
        {
            // Both ranges can give a path (one of them through the tissue around the structure): the smaller structure is the one that was meant
            if (!found || result.structureVoxels < chosen.structureVoxels)
            {
                chosen = result;
                found = true;
            }
        }
        else if (first)
        {
            best = result;
            first = false;
        }
    }

    return found ? chosen : best;
}

bool CenterlineFinder::rangeAround(vtkImageData *data, const double point[3], double tolerance, double *lower, double *upper)
{
    if (!data || data->GetNumberOfScalarComponents() != 1 || !(tolerance >= 0.0))
    {
        return false;
    }

    int extent[6];
    data->GetExtent(extent);
    double origin[3];
    double spacing[3];
    data->GetOrigin(origin);
    data->GetSpacing(spacing);

    int index[3];

    for (int k = 0; k < 3; k++)
    {
        index[k] = static_cast<int>(std::lround((point[k] - origin[k]) / spacing[k]));

        if (index[k] < extent[2 * k] || index[k] > extent[2 * k + 1])
        {
            return false;
        }
    }

    std::vector<double> values;

    for (int z = std::max(index[2] - 1, extent[4]); z <= std::min(index[2] + 1, extent[5]); z++)
    {
        for (int y = std::max(index[1] - 1, extent[2]); y <= std::min(index[1] + 1, extent[3]); y++)
        {
            for (int x = std::max(index[0] - 1, extent[0]); x <= std::min(index[0] + 1, extent[1]); x++)
            {
                values.push_back(data->GetScalarComponentAsDouble(x, y, z, 0));
            }
        }
    }

    // The median, which is not moved by a few voxels of what is next to the structure (a point at its end) or by noise
    std::nth_element(values.begin(), values.begin() + values.size() / 2, values.end());
    double median = values[values.size() / 2];
    *lower = median - tolerance;
    *upper = median + tolerance;
    return true;
}

CenterlineFinder::Result CenterlineFinder::find(vtkImageData *data, const double start[3], const double end[3], double lower, double upper)
{
    Result result;

    if (!data || data->GetNumberOfScalarComponents() != 1)
    {
        result.message = "There is no volume with one value per voxel.";
        return result;
    }

    if (!(lower <= upper))
    {
        result.message = "The range of values is not valid.";
        return result;
    }

    Grid g;
    data->GetExtent(g.extent);
    data->GetSpacing(g.spacing);
    data->GetOrigin(g.origin);
    g.nx = g.extent[1] - g.extent[0] + 1;
    g.ny = g.extent[3] - g.extent[2] + 1;
    g.nz = g.extent[5] - g.extent[4] + 1;

    if (g.nx < 1 || g.ny < 1 || g.nz < 1 || g.spacing[0] <= 0.0 || g.spacing[1] <= 0.0 || g.spacing[2] <= 0.0)
    {
        result.message = "The volume is empty.";
        return result;
    }

    qint64 total = static_cast<qint64>(g.nx) * g.ny * g.nz;

    if (total > 400000000LL)
    {
        result.message = "The volume is too big for this.";
        return result;
    }

    QVector<unsigned char> mask(static_cast<int>(total), 0);

    for (int z = 0; z < g.nz; z++)
    {
        for (int y = 0; y < g.ny; y++)
        {
            for (int x = 0; x < g.nx; x++)
            {
                double v = data->GetScalarComponentAsDouble(x + g.extent[0], y + g.extent[2], z + g.extent[4], 0);
                mask[g.index(x, y, z)] = (v >= lower && v <= upper) ? 1 : 0;
            }
        }
    }

    // The voxel of a point, moved to the nearest voxel of the structure within 3 voxels
    auto voxelOf = [&](const double p[3], int *v) {
        // The index in the extent of the nearest voxel of the grid
        int center[3];

        for (int k = 0; k < 3; k++)
        {
            center[k] = static_cast<int>(std::lround((p[k] - g.origin[k]) / g.spacing[k]));
        }

        double best = std::numeric_limits<double>::max();
        bool found = false;

        for (int dz = -3; dz <= 3; dz++)
        {
            for (int dy = -3; dy <= 3; dy++)
            {
                for (int dx = -3; dx <= 3; dx++)
                {
                    int x = center[0] + dx - g.extent[0];
                    int y = center[1] + dy - g.extent[2];
                    int z = center[2] + dz - g.extent[4];

                    if (g.contains(x, y, z) && mask[g.index(x, y, z)])
                    {
                        double d = dx * dx + dy * dy + dz * dz;

                        if (d < best)
                        {
                            best = d;
                            v[0] = x;
                            v[1] = y;
                            v[2] = z;
                            found = true;
                        }
                    }
                }
            }
        }

        return found;
    };

    int s[3];
    int e[3];

    if (!voxelOf(start, s))
    {
        result.message = "The start point is not in the structure (its values are not in the range).";
        return result;
    }

    if (!voxelOf(end, e))
    {
        result.message = "The end point is not in the structure (its values are not in the range).";
        return result;
    }

    // The structure connected to the start (26 neighbours), and the distance to the border in voxels (steps through the structure from a voxel that is not in it, 6 neighbours)
    QVector<unsigned char> connected(static_cast<int>(total), 0);
    QVector<int> queue;
    qint64 startIndex = g.index(s[0], s[1], s[2]);
    connected[startIndex] = 1;
    queue << static_cast<int>(startIndex);
    qint64 count = 0;

    for (int head = 0; head < queue.size(); head++)
    {
        int i = queue[head];
        int x = i % g.nx;
        int y = (i / g.nx) % g.ny;
        int z = i / (g.nx * g.ny);
        count++;

        for (int dz = -1; dz <= 1; dz++)
        {
            for (int dy = -1; dy <= 1; dy++)
            {
                for (int dx = -1; dx <= 1; dx++)
                {
                    int nx = x + dx;
                    int ny = y + dy;
                    int nz = z + dz;

                    if (g.contains(nx, ny, nz))
                    {
                        qint64 n = g.index(nx, ny, nz);

                        if (mask[n] && !connected[n])
                        {
                            connected[n] = 1;
                            queue << static_cast<int>(n);
                        }
                    }
                }
            }
        }
    }

    result.structureVoxels = count;

    if (!connected[g.index(e[0], e[1], e[2])])
    {
        result.message = "The end point is not connected to the start point through voxels in the range.";
        return result;
    }

    // Distance to the border in mm: a search from the voxels that touch what is not in the structure (or the volume border), with the steps of the 26 neighbours in mm
    QVector<float> distance(static_cast<int>(total), std::numeric_limits<float>::max());
    typedef std::pair<float, int> DistanceEntry;
    std::priority_queue<DistanceEntry, std::vector<DistanceEntry>, std::greater<DistanceEntry>> border;

    for (int i : queue)
    {
        int x = i % g.nx;
        int y = (i / g.nx) % g.ny;
        int z = i / (g.nx * g.ny);
        static const int d6[6][3] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};

        for (const int *dd : d6)
        {
            int nx = x + dd[0];
            int ny = y + dd[1];
            int nz = z + dd[2];

            if (!g.contains(nx, ny, nz) || !mask[g.index(nx, ny, nz)])
            {
                // The border is half a voxel away in that direction
                float h = static_cast<float>(0.5 * (dd[0] != 0 ? g.spacing[0] : dd[1] != 0 ? g.spacing[1] : g.spacing[2]));

                if (h < distance[i])
                {
                    distance[i] = h;
                    border.push(DistanceEntry(h, i));
                }
            }
        }
    }

    float maxDistance = static_cast<float>(0.5 * std::min(g.spacing[0], std::min(g.spacing[1], g.spacing[2])));

    while (!border.empty())
    {
        DistanceEntry top = border.top();
        border.pop();
        int i = top.second;

        if (top.first > distance[i])
        {
            continue;
        }

        maxDistance = std::max(maxDistance, top.first);
        int x = i % g.nx;
        int y = (i / g.nx) % g.ny;
        int z = i / (g.nx * g.ny);

        for (int dz = -1; dz <= 1; dz++)
        {
            for (int dy = -1; dy <= 1; dy++)
            {
                for (int dx = -1; dx <= 1; dx++)
                {
                    if (dx == 0 && dy == 0 && dz == 0)
                    {
                        continue;
                    }

                    int nx = x + dx;
                    int ny = y + dy;
                    int nz = z + dz;

                    if (g.contains(nx, ny, nz))
                    {
                        int n = static_cast<int>(g.index(nx, ny, nz));

                        if (connected[n])
                        {
                            float next = top.first + static_cast<float>(std::sqrt(std::pow(dx * g.spacing[0], 2) + std::pow(dy * g.spacing[1], 2) + std::pow(dz * g.spacing[2], 2)));

                            if (next < distance[n])
                            {
                                distance[n] = next;
                                border.push(DistanceEntry(next, n));
                            }
                        }
                    }
                }
            }
        }
    }

    // The cheapest path (Dijkstra, 26 neighbours): the length of the step times a factor that is 1 in the middle of the structure and grows toward the border
    typedef std::pair<double, int> Entry;
    std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> open;
    QVector<double> cost(static_cast<int>(total), std::numeric_limits<double>::max());
    QVector<int> from(static_cast<int>(total), -1);
    int target = static_cast<int>(g.index(e[0], e[1], e[2]));
    cost[startIndex] = 0.0;
    open.push(Entry(0.0, static_cast<int>(startIndex)));

    while (!open.empty())
    {
        Entry top = open.top();
        open.pop();
        int i = top.second;

        if (top.first > cost[i])
        {
            continue;
        }

        if (i == target)
        {
            break;
        }

        int x = i % g.nx;
        int y = (i / g.nx) % g.ny;
        int z = i / (g.nx * g.ny);

        for (int dz = -1; dz <= 1; dz++)
        {
            for (int dy = -1; dy <= 1; dy++)
            {
                for (int dx = -1; dx <= 1; dx++)
                {
                    if (dx == 0 && dy == 0 && dz == 0)
                    {
                        continue;
                    }

                    int nx = x + dx;
                    int ny = y + dy;
                    int nz = z + dz;

                    if (!g.contains(nx, ny, nz))
                    {
                        continue;
                    }

                    int n = static_cast<int>(g.index(nx, ny, nz));

                    if (!connected[n])
                    {
                        continue;
                    }

                    double step = std::sqrt(std::pow(dx * g.spacing[0], 2) + std::pow(dy * g.spacing[1], 2) + std::pow(dz * g.spacing[2], 2));
                    double middle = 1.0 - static_cast<double>(distance[n]) / maxDistance;
                    double next = cost[i] + step * (1.0 + 150.0 * middle * middle);

                    if (next < cost[n])
                    {
                        cost[n] = next;
                        from[n] = i;
                        open.push(Entry(next, n));
                    }
                }
            }
        }
    }

    if (from[target] < 0 && target != static_cast<int>(startIndex))
    {
        result.message = "No path was found.";
        return result;
    }

    // From the end back to the start, in mm, and then turned to go from the start to the end
    QVector<std::array<double, 3>> raw;

    for (int i = target; i >= 0; i = from[i])
    {
        int x = i % g.nx;
        int y = (i / g.nx) % g.ny;
        int z = i / (g.nx * g.ny);
        raw.append({{g.origin[0] + (x + g.extent[0]) * g.spacing[0], g.origin[1] + (y + g.extent[2]) * g.spacing[1], g.origin[2] + (z + g.extent[4]) * g.spacing[2]}});

        if (i == startIndex)
        {
            break;
        }
    }

    std::reverse(raw.begin(), raw.end());
    int n = raw.size();

    if (n < 2)
    {
        result.message = "The start and the end are the same voxel.";
        return result;
    }

    // A moving average over 5 points takes out the steps of the voxels (the ends stay where they are)
    QVector<std::array<double, 3>> smooth = raw;

    for (int i = 1; i + 1 < n; i++)
    {
        int from_ = std::max(0, i - 2);
        int to = std::min(n - 1, i + 2);
        int half = std::min(i - from_, to - i);

        for (int k = 0; k < 3; k++)
        {
            double sum = 0.0;

            for (int j = i - half; j <= i + half; j++)
            {
                sum += raw[j][k];
            }

            smooth[i][k] = sum / (2 * half + 1);
        }
    }

    // About one point every 2 mm (and always the last one)
    QVector<double> points;
    double accumulated = 0.0;
    double length = 0.0;
    points << smooth[0][0] << smooth[0][1] << smooth[0][2];

    for (int i = 1; i < n; i++)
    {
        double d = std::sqrt(std::pow(smooth[i][0] - smooth[i - 1][0], 2) + std::pow(smooth[i][1] - smooth[i - 1][1], 2) + std::pow(smooth[i][2] - smooth[i - 1][2], 2));
        accumulated += d;
        length += d;

        if (accumulated >= 2.0 || i == n - 1)
        {
            points << smooth[i][0] << smooth[i][1] << smooth[i][2];
            accumulated = 0.0;
        }
    }

    result.success = true;
    result.points = points;
    result.lengthMm = length;
    return result;
}

} // End namespace udg
