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

#ifndef UDGDICOMPIXELBLACKOUT_H
#define UDGDICOMPIXELBLACKOUT_H

#include <QList>
#include <QRect>
#include <QString>

class DcmDataset;

namespace udg {

/**
    Blanks rectangles of the pixels of an image (in all its frames), for the text that is burned into the pixels (the name of the patient on an ultrasound or a secondary capture, for example).
    It works on images with the pixels not compressed (8 or 16 bits, one or three values per pixel); for the other ones it says so, and leaves the image as it is, because the pixels would have
    to be decoded and coded again. The rectangles are in pixels, from the top left corner, and the parts that are outside the image are left out.
 */
class DicomPixelBlackout {
public:
    /// Gives true if the rectangles were blanked (or there were none). Otherwise false and the reason in errorMessage, with the image unchanged.
    static bool apply(DcmDataset *dataset, const QList<QRect> &rectangles, QString *errorMessage = nullptr);
};

} // End namespace udg

#endif
