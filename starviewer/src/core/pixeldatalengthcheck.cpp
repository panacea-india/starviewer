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

#include "pixeldatalengthcheck.h"

#include <dcdeftag.h>   // DCM_PixelData
#include <dcfilefo.h>   // DcmFileFormat
#include <dcxfer.h>     // DcmXfer

namespace udg {

namespace {

// Elements longer than this are not loaded by checkFile(), which only needs the length of the pixel data.
const Uint32 MaximumLoadedElementLength = 4096;

}

QString PixelDataLengthCheck::check(DcmDataset &dataset)
{
    DcmElement *pixelData = nullptr;

    if (DcmXfer(dataset.getOriginalXfer()).isEncapsulated() || dataset.findAndGetElement(DCM_PixelData, pixelData).bad())
    {
        return QString();
    }

    OFString photometricInterpretation;
    dataset.findAndGetOFString(DCM_PhotometricInterpretation, photometricInterpretation);

    // Chroma subsampling makes these smaller than rows x columns x samples
    if (photometricInterpretation == "YBR_FULL_422" || photometricInterpretation == "YBR_PARTIAL_422" || photometricInterpretation == "YBR_PARTIAL_420")
    {
        return QString();
    }

    Uint16 rows = 0, columns = 0, bitsAllocated = 0, samplesPerPixel = 1;
    Sint32 numberOfFrames = 1;
    dataset.findAndGetUint16(DCM_Rows, rows);
    dataset.findAndGetUint16(DCM_Columns, columns);
    dataset.findAndGetUint16(DCM_BitsAllocated, bitsAllocated);

    if (dataset.findAndGetUint16(DCM_SamplesPerPixel, samplesPerPixel).bad())
    {
        samplesPerPixel = 1;
    }

    if (dataset.findAndGetSint32(DCM_NumberOfFrames, numberOfFrames).bad() || numberOfFrames < 1)
    {
        numberOfFrames = 1;
    }

    quint64 expectedBits = static_cast<quint64>(rows) * columns * samplesPerPixel * numberOfFrames * bitsAllocated;
    quint64 expectedLength = (expectedBits + 7) / 8;

    if (pixelData->getLength() < expectedLength)
    {
        return QString("Pixel data has %1 bytes, but the image attributes declare %2.").arg(pixelData->getLength()).arg(expectedLength);
    }

    return QString();
}

QString PixelDataLengthCheck::checkFile(const QString &filename)
{
    DcmFileFormat file;

    if (file.loadFile(qPrintable(filename), EXS_Unknown, EGL_noChange, MaximumLoadedElementLength).bad())
    {
        return QString();
    }

    QString problem = check(*file.getDataset());
    return problem.isEmpty() ? problem : filename + ": " + problem;
}

} // namespace udg
