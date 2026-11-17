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

#include "endoscopytransferfunction.h"

namespace udg {

TransferFunction EndoscopyTransferFunction::airFilledLumen(double thresholdHU)
{
    TransferFunction function;
    function.setName("Endoscopy, air");

    // Wall: pink, like a mucous surface
    function.set(thresholdHU - 600.0, 0.85, 0.55, 0.50, 0.0);
    function.set(thresholdHU, 0.85, 0.55, 0.50, 0.0);
    function.set(thresholdHU + 80.0, 0.90, 0.60, 0.55, 0.9);
    function.set(thresholdHU + 1000.0, 1.0, 0.95, 0.90, 0.9);
    function.set(thresholdHU + 4000.0, 1.0, 1.0, 1.0, 0.9);
    return function;
}

TransferFunction EndoscopyTransferFunction::contrastFilledLumen(double thresholdHU)
{
    TransferFunction function;
    function.setName("Endoscopy, contrast");

    // Air outside the body is transparent; the tissue is the wall; the contrast is transparent
    function.set(-1024.0, 0.85, 0.35, 0.30, 0.0);
    function.set(-150.0, 0.85, 0.35, 0.30, 0.0);
    function.set(-100.0, 0.90, 0.45, 0.40, 0.9);
    function.set(thresholdHU - 80.0, 0.95, 0.60, 0.50, 0.9);
    function.set(thresholdHU, 0.95, 0.60, 0.50, 0.0);
    function.set(thresholdHU + 4000.0, 0.95, 0.60, 0.50, 0.0);
    return function;
}

} // End namespace udg
