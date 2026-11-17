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

#ifndef UDGENDOSCOPYTRANSFERFUNCTION_H
#define UDGENDOSCOPYTRANSFERFUNCTION_H

#include "transferfunction.h"

namespace udg {

/**
    The transfer functions to look at the inside of a cavity or a vessel from a camera that is in it (a virtual endoscopy): what fills the lumen has to be transparent and its wall opaque,
    so that the camera sees the wall as a surface. Values are in the units of the volume (HU for a CT).
 */
class EndoscopyTransferFunction {
public:
    /// For a lumen that is empty or full of air (the airways, the colon, the sinuses): everything below the threshold is transparent (air, from about -1000) and the tissue above is opaque.
    /// The opacity goes from 0 to 0.9 over 80 units from the threshold.
    static TransferFunction airFilledLumen(double thresholdHU = -400.0);

    /// For a lumen full of contrast (a vessel, the heart cavities): what is brighter than the threshold is transparent, the tissue below it (from -150) is the opaque wall, and the air outside
    /// the body is transparent. The opacity falls from 0.9 to 0 over the 80 units before the threshold.
    static TransferFunction contrastFilledLumen(double thresholdHU = 250.0);
};

} // End namespace udg

#endif
