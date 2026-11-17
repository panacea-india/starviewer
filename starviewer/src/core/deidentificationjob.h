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

#ifndef UDGDEIDENTIFICATIONJOB_H
#define UDGDEIDENTIFICATIONJOB_H

#include <functional>

#include <QList>
#include <QRect>
#include <QString>
#include <QStringList>

#include "dicomdeidentifier.h"

namespace udg {

/**
    De-identifies all the DICOM files of a folder (and the folders in it) into another folder, with the same options and the same UIDs for all of them, so that the studies stay together. The new files are
    named IM000001.dcm and so on, in one folder, because the names and the folders of the original ones can have the identity of the patient. Files that are not DICOM are left out and counted.
    Nothing is written in the folder of the originals, and it refuses to write in a folder that is the one of the originals or is inside it.
 */
class DeidentificationJob {
public:
    struct Result {
        bool success = false;
        QString message;
        int processed = 0;
        int failed = 0;
        int notDicom = 0;
        int blanked = 0;
        QStringList warnings;
        QStringList errors;
        DicomDeidentifier::Report totals;
    };

    /// Runs the job. The rectangles are blanked in all the images (an empty list leaves the pixels as they are). The progress function is called with the number of files done and the total; if it
    /// returns false the job stops (the files that were written stay).
    static Result run(const QString &inputFolder, const QString &outputFolder, const DicomDeidentifier::Options &options, const QList<QRect> &blackout = QList<QRect>(),
                      const std::function<bool(int, int)> &progress = nullptr, const QString &salt = QString());
};

} // End namespace udg

#endif
