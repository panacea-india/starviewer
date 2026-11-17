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

#include "dicompixelblackout.h"

#include <algorithm>

#include <dcmtk/dcmdata/dctk.h>

namespace udg {

bool DicomPixelBlackout::apply(DcmDataset *dataset, const QList<QRect> &rectangles, QString *errorMessage)
{
    auto fail = [errorMessage](const QString &reason) {
        if (errorMessage)
        {
            *errorMessage = reason;
        }

        return false;
    };

    if (!dataset)
    {
        return fail("There is no image.");
    }

    if (rectangles.isEmpty())
    {
        return true;
    }

    DcmElement *pixelData = nullptr;

    if (dataset->findAndGetElement(DCM_PixelData, pixelData).bad() || !pixelData)
    {
        return fail("The object has no pixels.");
    }

    if (dataset->getOriginalXfer() != EXS_Unknown && DcmXfer(dataset->getOriginalXfer()).isEncapsulated())
    {
        return fail("The pixels are compressed: decompress the image first.");
    }

    Uint16 rows = 0, columns = 0, bitsAllocated = 0, bitsStored = 0, samples = 1, planar = 0;
    Sint32 frames = 1;
    dataset->findAndGetUint16(DCM_Rows, rows);
    dataset->findAndGetUint16(DCM_Columns, columns);
    dataset->findAndGetUint16(DCM_BitsAllocated, bitsAllocated);
    dataset->findAndGetUint16(DCM_BitsStored, bitsStored);
    dataset->findAndGetUint16(DCM_SamplesPerPixel, samples);
    dataset->findAndGetUint16(DCM_PlanarConfiguration, planar);
    dataset->findAndGetSint32(DCM_NumberOfFrames, frames);
    frames = std::max<Sint32>(frames, 1);

    if (rows == 0 || columns == 0 || (bitsAllocated != 8 && bitsAllocated != 16) || (samples != 1 && samples != 3))
    {
        return fail("The pixels have a format that is not handled (8 or 16 bits, one or three values per pixel).");
    }

    int bytesPerSample = bitsAllocated / 8;
    qint64 frameBytes = static_cast<qint64>(rows) * columns * samples * bytesPerSample;
    Uint8 *bytes = nullptr;

    if (pixelData->getUint8Array(bytes).bad() || !bytes || static_cast<qint64>(pixelData->getLength()) < frameBytes * frames)
    {
        return fail("The pixels are shorter than the image says.");
    }

    // The value that looks black: 0, or the highest one for the images where the high values are dark
    OFString photometric;
    dataset->findAndGetOFString(DCM_PhotometricInterpretation, photometric);
    unsigned int blackValue = std::string(photometric.c_str()) == "MONOCHROME1" ? ((1u << std::min<int>(bitsStored ? bitsStored : bitsAllocated, 16)) - 1u) : 0u;

    for (int f = 0; f < frames; f++)
    {
        Uint8 *frame = bytes + static_cast<qint64>(f) * frameBytes;

        for (const QRect &r : rectangles)
        {
            QRect area = r.normalized().intersected(QRect(0, 0, columns, rows));

            for (int y = area.top(); y <= area.bottom(); y++)
            {
                for (int x = area.left(); x <= area.right(); x++)
                {
                    for (int s = 0; s < samples; s++)
                    {
                        qint64 sampleIndex = samples == 1 ? static_cast<qint64>(y) * columns + x
                                                          : (planar == 1 ? static_cast<qint64>(s) * rows * columns + static_cast<qint64>(y) * columns + x
                                                                         : (static_cast<qint64>(y) * columns + x) * samples + s);
                        Uint8 *p = frame + sampleIndex * bytesPerSample;

                        if (bytesPerSample == 1)
                        {
                            *p = static_cast<Uint8>(blackValue & 0xFF);
                        }
                        else
                        {
                            p[0] = static_cast<Uint8>(blackValue & 0xFF);
                            p[1] = static_cast<Uint8>((blackValue >> 8) & 0xFF);
                        }
                    }
                }
            }
        }
    }

    return true;
}

} // End namespace udg
