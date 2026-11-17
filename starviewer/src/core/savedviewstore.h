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

#ifndef UDGSAVEDVIEWSTORE_H
#define UDGSAVEDVIEWSTORE_H

#include <QList>
#include <QString>

#include "savedview.h"

namespace udg {

/// Keeps the saved views of a volume in a JSON file, so that they are there the next time it is opened. A file that cannot be read gives no views.
class SavedViewStore {
public:
    /// The views of the file, in order; the ones with fields that are missing or wrong are left out. A file that does not exist has no views.
    static QList<SavedView> load(const QString &filePath);
    /// Writes the views, replacing the file (the directory is created if needed). Returns false if it could not be written.
    static bool save(const QString &filePath, const QList<SavedView> &views);
    /// The name of the file for a series of images, which does not hold anything of the patient: the hash of its UID.
    static QString fileNameForSeries(const QString &seriesInstanceUid);
};

} // End namespace udg

#endif
