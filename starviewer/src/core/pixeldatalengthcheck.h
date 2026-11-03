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

#ifndef UDG_PIXELDATALENGTHCHECK_H
#define UDG_PIXELDATALENGTHCHECK_H

#include <QString>

class DcmDataset;

namespace udg {

/**
 * @brief Checks that uncompressed pixel data is as long as the image attributes declare.
 *
 * The readers accept a file with less pixel data, and the part with no data would be displayed as if it were part of the image.
 * Compressed pixel data and chroma subsampled images are not checked.
 */
class PixelDataLengthCheck {
public:
    /// Returns a description of the problem if the pixel data of the dataset is too short, or an empty string otherwise.
    static QString check(DcmDataset &dataset);

    /// Returns a description of the problem if the pixel data of the file is too short, or an empty string otherwise, also when the file
    /// cannot be read as DICOM, since the reader then reports that. Reads the file without the values of long elements, so it's quick.
    static QString checkFile(const QString &filename);
};

} // namespace udg

#endif // UDG_PIXELDATALENGTHCHECK_H
