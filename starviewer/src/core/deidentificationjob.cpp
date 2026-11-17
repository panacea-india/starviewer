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

#include "deidentificationjob.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QSet>

#include <dcmtk/dcmdata/dctk.h>

#include "dicompixelblackout.h"

namespace udg {

DeidentificationJob::Result DeidentificationJob::run(const QString &inputFolder, const QString &outputFolder, const DicomDeidentifier::Options &options, const QList<QRect> &blackout,
                                                     const std::function<bool(int, int)> &progress, const QString &salt)
{
    Result result;

    QFileInfo input(inputFolder);

    if (!input.isDir())
    {
        result.message = "The folder of the images does not exist.";
        return result;
    }

    if (outputFolder.trimmed().isEmpty())
    {
        result.message = "There is no folder for the new images.";
        return result;
    }

    QString inputPath = QDir::cleanPath(input.absoluteFilePath());
    QString outputPath = QDir::cleanPath(QFileInfo(outputFolder).absoluteFilePath());

    if (outputPath.compare(inputPath, Qt::CaseInsensitive) == 0 || (outputPath + "/").startsWith(inputPath + "/", Qt::CaseInsensitive))
    {
        result.message = "The folder for the new images cannot be the folder of the originals or be inside it.";
        return result;
    }

    if (!QDir().mkpath(outputPath))
    {
        result.message = "The folder for the new images could not be created.";
        return result;
    }

    QStringList files;
    QDirIterator it(inputPath, QDir::Files, QDirIterator::Subdirectories);

    while (it.hasNext())
    {
        files << it.next();
    }

    files.sort();
    DicomDeidentifier::Options usedOptions = options;
    usedOptions.pixelsCleaned = !blackout.isEmpty();
    DicomDeidentifier deidentifier(usedOptions, salt);
    QSet<QString> warningSet;
    int index = 0;

    for (int i = 0; i < files.size(); i++)
    {
        if (progress && !progress(i, files.size()))
        {
            result.message = "Stopped.";
            return result;
        }

        DcmFileFormat file;

        if (file.loadFile(files[i].toLocal8Bit().constData()).bad())
        {
            result.notDicom++;
            continue;
        }

        DcmDataset *dataset = file.getDataset();
        DicomDeidentifier::Report report = deidentifier.process(dataset);
        result.totals.removed += report.removed;
        result.totals.emptied += report.emptied;
        result.totals.replaced += report.replaced;
        result.totals.uidsReplaced += report.uidsReplaced;
        result.totals.privateRemoved += report.privateRemoved;
        result.totals.datesShifted += report.datesShifted;

        for (const QString &w : report.warnings)
        {
            warningSet.insert(w);
        }

        if (!blackout.isEmpty())
        {
            QString why;

            if (DicomPixelBlackout::apply(dataset, blackout, &why))
            {
                result.blanked++;
            }
            else
            {
                // An image whose text cannot be blanked is not written: the text would be shared
                result.failed++;
                result.errors << QString("%1: %2").arg(QFileInfo(files[i]).fileName(), why);
                continue;
            }
        }

        index++;
        QString name = QString("%1/IM%2.dcm").arg(outputPath).arg(index, 6, 10, QChar('0'));

        // The meta information is updated from the new SOP Instance UID
        OFCondition saved = file.saveFile(name.toLocal8Bit().constData(), EXS_Unknown, EET_ExplicitLength, EGL_recalcGL, EPD_noChange, 0, 0, EWM_updateMeta);

        if (saved.bad())
        {
            result.failed++;
            result.errors << QString("%1: %2").arg(QFileInfo(files[i]).fileName(), QString::fromLatin1(saved.text()));
            index--;
            continue;
        }

        result.processed++;
    }

    if (progress)
    {
        progress(files.size(), files.size());
    }

    result.warnings = warningSet.toList();
    result.warnings.sort();
    result.success = result.failed == 0;
    result.message = result.success ? "Done." : "Some images were not written.";
    return result;
}

} // End namespace udg
