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

#include "movieencoder.h"

#include <QDir>
#include <QFileInfo>
#include <QObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QStandardPaths>

namespace udg {

QString MovieEncoder::findFfmpeg()
{
    QString fromEnvironment = QProcessEnvironment::systemEnvironment().value("FFMPEG_PATH");

    if (!fromEnvironment.isEmpty() && QFileInfo(fromEnvironment).isExecutable())
    {
        return fromEnvironment;
    }

    return QStandardPaths::findExecutable("ffmpeg");
}

QString MovieEncoder::frameFileName(int index)
{
    return QString("frame_%1.png").arg(index, 6, 10, QChar('0'));
}

QStringList MovieEncoder::ffmpegArguments(const QString &folder, int framesPerSecond, const QString &outputFile)
{
    return QStringList() << "-y" << "-framerate" << QString::number(framesPerSecond) << "-i" << QDir(folder).filePath("frame_%06d.png")
                         << "-vf" << "pad=ceil(iw/2)*2:ceil(ih/2)*2" << "-c:v" << "libx264" << "-pix_fmt" << "yuv420p" << outputFile;
}

QString MovieEncoder::encode(const QString &folder, int framesPerSecond, const QString &outputFile)
{
    QString ffmpeg = findFfmpeg();

    if (ffmpeg.isEmpty())
    {
        return QObject::tr("ffmpeg was not found on this computer, so the movie was left as pictures.");
    }

    QProcess process;
    process.start(ffmpeg, ffmpegArguments(folder, framesPerSecond, outputFile));

    if (!process.waitForStarted(10000) || !process.waitForFinished(10 * 60 * 1000))
    {
        return QObject::tr("ffmpeg could not be run.");
    }

    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0)
    {
        return QObject::tr("ffmpeg failed: %1").arg(QString::fromLocal8Bit(process.readAllStandardError()).right(400));
    }

    return QString();
}

} // End namespace udg
