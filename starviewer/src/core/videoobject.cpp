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

#include "videoobject.h"

#include <QFile>
#include <QObject>

#include <dcmtk/config/osconfig.h>
#include <dcmtk/dcmdata/dcdatset.h>
#include <dcmtk/dcmdata/dcdeftag.h>
#include <dcmtk/dcmdata/dcfilefo.h>
#include <dcmtk/dcmdata/dcpixel.h>
#include <dcmtk/dcmdata/dcpixseq.h>
#include <dcmtk/dcmdata/dcpxitem.h>

#include "dicomtagreader.h"

namespace udg {

namespace {

const QString VideoSyntaxPrefix = "1.2.840.10008.1.2.4.";

// The number that ends the transfer syntax, if it is a video one (100 to 108), or -1
int videoSyntaxNumber(const QString &transferSyntaxUid)
{
    if (!transferSyntaxUid.startsWith(VideoSyntaxPrefix))
    {
        return -1;
    }

    bool ok = false;
    int number = transferSyntaxUid.mid(VideoSyntaxPrefix.size()).toInt(&ok);
    return ok && number >= 100 && number <= 108 ? number : -1;
}

}

VideoObject::VideoObject()
    : m_rows(0), m_columns(0), m_numberOfFrames(0), m_frameTimeMilliseconds(0.0), m_null(true)
{
}

bool VideoObject::isVideoTransferSyntax(const QString &transferSyntaxUid)
{
    return videoSyntaxNumber(transferSyntaxUid) >= 0;
}

QString VideoObject::getFileExtension(const QString &transferSyntaxUid)
{
    int number = videoSyntaxNumber(transferSyntaxUid);

    if (number < 0)
    {
        return QString();
    }

    return number <= 101 ? ".mpg" : ".mp4";
}

QString VideoObject::getFormatName(const QString &transferSyntaxUid)
{
    int number = videoSyntaxNumber(transferSyntaxUid);

    if (number < 0)
    {
        return QString();
    }

    if (number <= 101)
    {
        return "MPEG-2";
    }

    return number <= 106 ? "MPEG-4 AVC/H.264" : "HEVC/H.265";
}

VideoObject VideoObject::fromDICOM(const DICOMTagReader &reader, QString *error)
{
    QString transferSyntax = reader.getValueAttributeAsQString(DICOMTransferSyntaxUID).trimmed();

    if (!isVideoTransferSyntax(transferSyntax) || !reader.tagExists(DICOMPixelData))
    {
        if (error)
        {
            *error = QObject::tr("It is not a DICOM video.");
        }

        return VideoObject();
    }

    VideoObject video;
    video.m_sopInstanceUid = reader.getValueAttributeAsQString(DICOMSOPInstanceUID);
    video.m_transferSyntaxUid = transferSyntax;
    video.m_description = reader.getValueAttributeAsQString(DICOMSeriesDescription).trimmed();
    video.m_path = reader.getFileName();
    video.m_rows = reader.getValueAttributeAsQString(DICOMRows).toInt();
    video.m_columns = reader.getValueAttributeAsQString(DICOMColumns).toInt();
    video.m_numberOfFrames = reader.getValueAttributeAsQString(DICOMNumberOfFrames).toInt();

    // The frame time is in milliseconds. If it is missing, the cine rate (frames per second) tells it.
    video.m_frameTimeMilliseconds = reader.getValueAttributeAsQString(DICOMFrameTime).toDouble();

    if (video.m_frameTimeMilliseconds <= 0.0)
    {
        double rate = reader.getValueAttributeAsQString(DICOMCineRate).toDouble();
        video.m_frameTimeMilliseconds = rate > 0.0 ? 1000.0 / rate : 0.0;
    }

    video.m_null = false;
    return video;
}

bool VideoObject::extractTo(const QString &outputPath, QString *error) const
{
    auto fail = [error](const QString &reason) {
        if (error)
        {
            *error = reason;
        }
        return false;
    };

    if (m_null)
    {
        return fail(QObject::tr("It is not a DICOM video."));
    }

    DcmFileFormat file;
    OFCondition status = file.loadFile(m_path.toLocal8Bit().constData());

    if (status.bad())
    {
        return fail(QObject::tr("The file could not be read: %1").arg(status.text()));
    }

    DcmElement *element = nullptr;

    if (file.getDataset()->findAndGetElement(DCM_PixelData, element).bad() || !element)
    {
        return fail(QObject::tr("The file has no pixel data."));
    }

    DcmPixelData *pixelData = dynamic_cast<DcmPixelData*>(element);
    DcmPixelSequence *sequence = nullptr;
    E_TransferSyntax syntax = EXS_Unknown;
    const DcmRepresentationParameter *parameter = nullptr;

    if (!pixelData)
    {
        return fail(QObject::tr("The pixel data cannot be read."));
    }

    pixelData->getOriginalRepresentationKey(syntax, parameter);

    if (pixelData->getEncapsulatedRepresentation(syntax, parameter, sequence).bad() || !sequence)
    {
        return fail(QObject::tr("The video is not stored as a bitstream."));
    }

    QFile output(outputPath);

    if (!output.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        return fail(QObject::tr("The file %1 could not be written.").arg(outputPath));
    }

    // The first item is the basic offset table. The bitstream is in the fragments that follow.
    qint64 written = 0;

    for (unsigned long i = 1; i < sequence->card(); i++)
    {
        DcmPixelItem *item = nullptr;
        Uint8 *data = nullptr;

        if (sequence->getItem(item, i).bad() || !item || item->getUint8Array(data).bad() || !data)
        {
            output.close();
            output.remove();
            return fail(QObject::tr("A fragment of the video could not be read."));
        }

        qint64 length = static_cast<qint64>(item->getLength());

        if (output.write(reinterpret_cast<const char*>(data), length) != length)
        {
            output.close();
            output.remove();
            return fail(QObject::tr("The file %1 could not be written.").arg(outputPath));
        }

        written += length;
    }

    output.close();

    if (written == 0)
    {
        output.remove();
        return fail(QObject::tr("The video has no data."));
    }

    return true;
}

bool VideoObject::isNull() const
{
    return m_null;
}

const QString& VideoObject::getSopInstanceUid() const
{
    return m_sopInstanceUid;
}

const QString& VideoObject::getTransferSyntaxUid() const
{
    return m_transferSyntaxUid;
}

QString VideoObject::getFormatName() const
{
    return getFormatName(m_transferSyntaxUid);
}

const QString& VideoObject::getDescription() const
{
    return m_description;
}

int VideoObject::getRows() const
{
    return m_rows;
}

int VideoObject::getColumns() const
{
    return m_columns;
}

int VideoObject::getNumberOfFrames() const
{
    return m_numberOfFrames;
}

double VideoObject::getDurationInSeconds() const
{
    return m_numberOfFrames * m_frameTimeMilliseconds / 1000.0;
}

const QString& VideoObject::getPath() const
{
    return m_path;
}

} // End namespace udg
