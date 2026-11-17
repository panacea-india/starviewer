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

#ifndef UDGVIDEOOBJECT_H
#define UDGVIDEOOBJECT_H

#include <QString>

namespace udg {

class DICOMTagReader;

/**
    A DICOM video: a multiframe VL image whose pixel data is not pixels but an MPEG-2, MPEG-4 AVC/H.264 or HEVC/H.265 bitstream in one fragment.
    This class knows which transfer syntaxes are video, reads what describes the video, and writes the bitstream to a file that a media player can open.
    It does not decode anything: the decoding is left to the operating system (see the video extension), so that no codec is shipped with Starviewer.
 */
class VideoObject {
public:
    /// Creates a null object.
    VideoObject();

    /// Returns true if the transfer syntax is one of the DICOM video ones (MPEG-2, MPEG-4 AVC/H.264 and HEVC/H.265, 1.2.840.10008.1.2.4.100 to .108).
    static bool isVideoTransferSyntax(const QString &transferSyntaxUid);

    /// Returns the extension, with the dot, of the file that holds the bitstream of the given transfer syntax: .mpg for MPEG-2 and .mp4 for the rest.
    static QString getFileExtension(const QString &transferSyntaxUid);

    /// Returns the name of the format of the given video transfer syntax, like "MPEG-4 AVC/H.264", or an empty string if it is not one.
    static QString getFormatName(const QString &transferSyntaxUid);

    /// Reads what describes the video of the given reader. If the file is not a video, it returns a null object and, if error is not null, says why.
    static VideoObject fromDICOM(const DICOMTagReader &reader, QString *error = nullptr);

    /// Writes the bitstream of the video to the given file. It returns false and, if error is not null, says why, if it could not.
    bool extractTo(const QString &outputPath, QString *error = nullptr) const;

    bool isNull() const;

    const QString& getSopInstanceUid() const;
    const QString& getTransferSyntaxUid() const;
    /// Returns the name of the format of the video.
    QString getFormatName() const;
    /// Returns the description of the series, or an empty string.
    const QString& getDescription() const;
    int getRows() const;
    int getColumns() const;
    int getNumberOfFrames() const;
    /// Returns the duration in seconds, or 0 if the file does not say.
    double getDurationInSeconds() const;
    /// Returns the path of the file of the video.
    const QString& getPath() const;

private:
    QString m_sopInstanceUid;
    QString m_transferSyntaxUid;
    QString m_description;
    QString m_path;
    int m_rows;
    int m_columns;
    int m_numberOfFrames;
    double m_frameTimeMilliseconds;
    bool m_null;
};

} // End namespace udg

#endif
