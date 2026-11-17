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

#ifndef UDGHANGINGPROTOCOLFILES_H
#define UDGHANGINGPROTOCOLFILES_H

#include <QList>
#include <QString>
#include <QStringList>

namespace udg {

/**
    The XML files of the hanging protocols: the ones that come with the program (not changed) and the ones of the user (which are copied from them or written by hand and are the ones that can be
    changed or deleted). The program reads both at start-up.
 */
class HangingProtocolFiles {
public:
    struct Entry {
        QString path;
        QString name;
        QStringList modalities;
        double priority = 0.0;
        bool isUser = false;
    };

    /// The protocols of the XML files that are in the directories (and the ones below them), the built-in ones first and then the ones of the user, each group ordered by name. The files that cannot be read are left out.
    static QList<Entry> list(const QStringList &builtInDirectories, const QString &userDirectory);

    /// A name for a file from the name of a protocol: letters, digits, "-" and "_" are kept and the rest become "_". "protocol" if nothing is left.
    static QString fileNameFor(const QString &name);

    /// Makes a copy of the protocol of the file in the user directory (it is created if needed) with the new name, in a file named from it. It does not replace a file: it fails if
    /// there is one. Returns the path of the new file, or an empty string and the reason.
    static QString copyToUser(const QString &sourcePath, const QString &userDirectory, const QString &newName, QString *errorMessage = nullptr);

    /// Deletes the file if it is an XML file inside the user directory. Returns false and the reason if not.
    static bool removeUserFile(const QString &path, const QString &userDirectory, QString *errorMessage = nullptr);

    /// True if the position is "x1\y1\x2\y2": four numbers from 0 to 1, with x1 < x2 and y1 > y2 (the top left corner and the bottom right one, the vertical axis going up).
    static bool isValidPosition(const QString &position);
};

} // End namespace udg

#endif
