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

#ifndef UDGSURFACEEXPORTER_H
#define UDGSURFACEEXPORTER_H

#include <QString>

#include <vtkSmartPointer.h>

class vtkImageData;
class vtkPolyData;

namespace udg {

/**
    Makes a surface of the voxels of a volume that are above a threshold (a bone, a contrast-filled vessel, a lesion) and writes it as an STL file, for 3D printing
    or for other programs. It knows nothing about the interface.

    The coordinates are those of the volume: millimeters, with its origin and spacing. An STL file does not say its unit, and programs usually take it as
    millimeters, which is what these are. The surface is closed even where the object touches the edge of the volume, so that it has a volume.
 */
class SurfaceExporter {
public:
    struct Options {
        Options();

        /// The surface goes where the value of the volume is the threshold (for CT, in the units of the volume: HU plus the offset that it has)
        double threshold;
        /// Keep only the largest connected piece, which removes the small ones that noise makes
        bool largestPieceOnly;
        /// Iterations of smoothing that keep the shape and the volume (windowed sinc), from 0 to 50. Zero leaves the voxels' staircase as it is.
        int smoothingIterations;
        /// Write the file in binary, which is about five times smaller than text
        bool binary;
    };

    struct Result {
        Result();

        bool success;
        /// What went wrong, if it did not succeed. Also set, as a warning, when it succeeded but the surface is open.
        QString message;
        int triangles;
        /// The volume inside the surface, in milliliters (1000 mm3 each), and its area in cm2
        double volumeMilliliters;
        double areaSquareCentimeters;
        /// The bounds in mm: xmin, xmax, ymin, ymax, zmin, zmax
        double bounds[6];
        /// False if the surface has edges that belong to one triangle only, so its volume is not exact
        bool closed;
    };

    /// Makes the surface of the volume at the threshold. It returns null and, if error is not null, says why, if there is no surface.
    static vtkSmartPointer<vtkPolyData> createSurface(vtkImageData *volume, const Options &options, QString *error = nullptr);

    /// Makes the surface and writes it to the given file.
    static Result exportStl(vtkImageData *volume, const QString &path, const Options &options);
};

} // End namespace udg

#endif
