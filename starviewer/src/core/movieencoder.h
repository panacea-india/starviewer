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

#ifndef UDGMOVIEENCODER_H
#define UDGMOVIEENCODER_H

#include <QString>
#include <QStringList>

namespace udg {

/**
    Makes a video file from a folder of numbered pictures by running ffmpeg, when the computer has it. Starviewer ships no encoder (see ROADMAP, decisions), so
    without ffmpeg the movie is left as the pictures, which any editor can join. The pictures are always the result; the video is a convenience.
 */
class MovieEncoder {
public:
    /// The path of the ffmpeg program: the one named in the environment variable FFMPEG_PATH, or the one found in the PATH. Empty if there is none.
    static QString findFfmpeg();

    /// The name of picture number `index` (from 0) of a movie in a folder: "frame_000123.png". The numbers have 6 digits, so that the order of the names is the order of the movie.
    static QString frameFileName(int index);

    /// The arguments for ffmpeg that make an H.264 video (MP4) of the pictures of the folder, at the given frames per second. The size is made even, as H.264 needs.
    static QStringList ffmpegArguments(const QString &folder, int framesPerSecond, const QString &outputFile);

    /// Runs ffmpeg (found by findFfmpeg) with the arguments above, and waits for it. Returns an empty string if the video was made, or the reason it was not.
    static QString encode(const QString &folder, int framesPerSecond, const QString &outputFile);
};

} // End namespace udg

#endif
