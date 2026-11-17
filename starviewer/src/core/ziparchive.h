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

#ifndef UDGZIPARCHIVE_H
#define UDGZIPARCHIVE_H

#include <QString>
#include <QStringList>

namespace udg {

/**
    Unpacks a ZIP file into a folder, for the studies that come as a ZIP (a CD export, a mail from another hospital). The file is not trusted: every path is checked so that nothing
    is written outside the folder (the "zip slip"), the sizes that the file declares are held to the real ones, and the total size, the size of one file and the number of files
    are limited, so a small file cannot fill the disk. The CRC of each file is checked. It reads stored and deflated files (what ZIP programs make), and it does not read encrypted
    files or ZIP64 archives (larger than 4 GiB or with more than 65,534 files), which it says.
 */
class ZipArchive {
public:
    struct Limits {
        qint64 maxTotalBytes = 20LL * 1024 * 1024 * 1024;
        qint64 maxFileBytes = 4LL * 1024 * 1024 * 1024;
        int maxEntries = 100000;
    };

    struct Result {
        bool success = false;
        /// The reason that it failed, or a note (such as the entries that were skipped), for the person to read
        QString message;
        /// The files that were written, with their full paths
        QStringList files;
        qint64 bytes = 0;
        int skippedEntries = 0;
    };

    /// Unpacks the archive into the folder, which is created. If it fails, the files that were already written are removed.
    static Result extract(const QString &archivePath, const QString &destinationFolder, const Limits &limits = Limits());

    /// The path of an entry as it goes under the folder (with / as the separator and no empty parts), or an empty string if the name is not safe: it is empty, absolute, has a drive or
    /// a stream (a ":"), a ".." part, a control character, or ends the way that Windows changes (a space or a dot at the end of a part).
    static QString safeRelativePath(const QString &entryName);

    /// Removes the folders of the root that were last changed more than the given hours ago (the unpacked studies of earlier sessions: they are patient data). Returns how many were removed.
    static int removeOldExtractions(const QString &rootFolder, int maxAgeHours);
};

} // End namespace udg

#endif
