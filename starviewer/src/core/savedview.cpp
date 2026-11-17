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

#include "savedview.h"

#include <algorithm>
#include <cmath>

#include <QJsonArray>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace udg {

namespace {

typedef std::array<double, 3> Vector;

Vector subtract(const Vector &a, const Vector &b)
{
    return {{a[0] - b[0], a[1] - b[1], a[2] - b[2]}};
}

Vector add(const Vector &a, const Vector &b)
{
    return {{a[0] + b[0], a[1] + b[1], a[2] + b[2]}};
}

Vector scale(const Vector &a, double factor)
{
    return {{a[0] * factor, a[1] * factor, a[2] * factor}};
}

double dot(const Vector &a, const Vector &b)
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

Vector cross(const Vector &a, const Vector &b)
{
    return {{a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]}};
}

double length(const Vector &a)
{
    return std::sqrt(dot(a, a));
}

Vector normalized(const Vector &a)
{
    double size = length(a);
    return size > 0.0 ? scale(a, 1.0 / size) : a;
}

QJsonArray toArray(const Vector &a)
{
    QJsonArray array;
    array << a[0] << a[1] << a[2];
    return array;
}

bool fromArray(const QJsonValue &value, Vector &result)
{
    QJsonArray array = value.toArray();

    if (array.size() != 3)
    {
        return false;
    }

    for (int i = 0; i < 3; i++)
    {
        if (!array.at(i).isDouble() || !std::isfinite(array.at(i).toDouble()))
        {
            return false;
        }

        result[i] = array.at(i).toDouble();
    }

    return true;
}

}

SavedView::SavedView()
    : position{{0.0, 0.0, 1.0}}, focalPoint{{0.0, 0.0, 0.0}}, viewUp{{0.0, 1.0, 0.0}}, viewAngle(30.0), parallelProjection(false), parallelScale(1.0)
{
}

QJsonObject SavedView::toJson() const
{
    QJsonObject object;
    object["name"] = name;
    object["position"] = toArray(position);
    object["focalPoint"] = toArray(focalPoint);
    object["viewUp"] = toArray(viewUp);
    object["viewAngle"] = viewAngle;
    object["parallelProjection"] = parallelProjection;
    object["parallelScale"] = parallelScale;
    return object;
}

SavedView SavedView::fromJson(const QJsonObject &object, bool *valid)
{
    SavedView view;
    bool ok = object["name"].isString() && fromArray(object["position"], view.position) && fromArray(object["focalPoint"], view.focalPoint)
              && fromArray(object["viewUp"], view.viewUp) && object["viewAngle"].isDouble() && object["parallelScale"].isDouble()
              && object["parallelProjection"].isBool();

    if (ok)
    {
        view.name = object["name"].toString();
        view.viewAngle = object["viewAngle"].toDouble();
        view.parallelScale = object["parallelScale"].toDouble();
        view.parallelProjection = object["parallelProjection"].toBool();
        ok = std::isfinite(view.viewAngle) && std::isfinite(view.parallelScale) && length(view.viewUp) > 0.0;
    }

    if (valid)
    {
        *valid = ok;
    }

    return ok ? view : SavedView();
}

SavedView SavedView::interpolate(const SavedView &first, const SavedView &second, double t)
{
    t = std::max(0.0, std::min(1.0, t));

    SavedView view = first;
    view.focalPoint = add(scale(first.focalPoint, 1.0 - t), scale(second.focalPoint, t));
    view.viewUp = normalized(add(scale(normalized(first.viewUp), 1.0 - t), scale(normalized(second.viewUp), t)));
    view.viewAngle = first.viewAngle * (1.0 - t) + second.viewAngle * t;
    view.parallelScale = first.parallelScale * (1.0 - t) + second.parallelScale * t;
    view.parallelProjection = t < 0.5 ? first.parallelProjection : second.parallelProjection;

    Vector a = subtract(first.position, first.focalPoint);
    Vector b = subtract(second.position, second.focalPoint);
    double distance = length(a) * (1.0 - t) + length(b) * t;
    Vector directionA = normalized(a);
    Vector directionB = normalized(b);
    double cosine = std::max(-1.0, std::min(1.0, dot(directionA, directionB)));
    double angle = std::acos(cosine);
    Vector direction;

    if (angle < 1e-6 || M_PI - angle < 1e-3)
    {
        // The same direction, or opposite ones, where an arc is not defined: a line is the best that can be done
        direction = normalized(add(scale(directionA, 1.0 - t), scale(directionB, t)));

        if (length(direction) == 0.0)
        {
            direction = directionA;
        }
    }
    else
    {
        direction = add(scale(directionA, std::sin((1.0 - t) * angle) / std::sin(angle)), scale(directionB, std::sin(t * angle) / std::sin(angle)));
    }

    view.position = add(view.focalPoint, scale(direction, distance));
    return view;
}

SavedView SavedView::orbited(double degrees) const
{
    SavedView view = *this;
    Vector axis = normalized(viewUp);
    Vector v = subtract(position, focalPoint);
    double angle = degrees * M_PI / 180.0;
    // Rodrigues' formula
    Vector rotated = add(add(scale(v, std::cos(angle)), scale(cross(axis, v), std::sin(angle))), scale(axis, dot(axis, v) * (1.0 - std::cos(angle))));
    view.position = add(focalPoint, rotated);
    return view;
}

QList<SavedView> SavedView::path(const QList<SavedView> &keys, int frames)
{
    QList<SavedView> views;

    if (keys.isEmpty() || frames < 1)
    {
        return views;
    }

    if (keys.size() == 1 || frames == 1)
    {
        for (int i = 0; i < frames; i++)
        {
            views << keys.first();
        }

        return views;
    }

    int segments = keys.size() - 1;

    for (int i = 0; i < frames; i++)
    {
        double parameter = static_cast<double>(i) / (frames - 1) * segments;
        int segment = std::min(static_cast<int>(parameter), segments - 1);
        views << interpolate(keys.at(segment), keys.at(segment + 1), parameter - segment);
    }

    return views;
}

QList<SavedView> SavedView::turntable(int frames) const
{
    QList<SavedView> views;

    for (int i = 0; i < frames; i++)
    {
        views << orbited(360.0 * i / frames);
    }

    return views;
}

} // End namespace udg
