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

#ifndef UDGIMAGEFILTERS_H
#define UDGIMAGEFILTERS_H

#include <vtkSmartPointer.h>

class vtkImageData;

namespace udg {

/**
    Smoothing and sharpening of the images of a volume, slice by slice (in the plane of the images), for looking at them more easily. Each filter gives a new volume of the same
    kind and size: the values are kept in the range of their type. A filtered image is not to be measured in HU or SUV: the viewer marks it.
 */
class ImageFilters {
public:
    /// Blurs each slice with a Gaussian of the given standard deviation in mm (it is turned into pixels with the spacing of each axis). Null if there is no volume, more than one component or a sigma that is not positive.
    static vtkSmartPointer<vtkImageData> gaussianSmooth(vtkImageData *volume, double sigmaMillimeters);

    /// Sharpens with an unsharp mask: image + amount * (image - blurred image), the blur being the one above. An amount of 1 is a strong sharpening. Null for the cases above, or a negative amount.
    static vtkSmartPointer<vtkImageData> unsharpMask(vtkImageData *volume, double sigmaMillimeters, double amount);
};

} // End namespace udg

#endif
