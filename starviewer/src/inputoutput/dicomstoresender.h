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

#ifndef UDGDICOMSTORESENDER_H
#define UDGDICOMSTORESENDER_H

#include <QString>
#include <QStringList>

#include "routingrules.h"

namespace udg {

/// Sends DICOM files to a PACS with C-STORE (DCMTK), for the routing: one association for all the files of a destination, with one presentation context for each type of image and transfer syntax of the
/// files. The files are sent as they are (no change of transfer syntax). It is not used for the sending of the viewer of studies, which has its own.
class DicomStoreSender {
public:
    /// Sends the files; sent is the number accepted by the PACS (a warning status counts as accepted). Returns false if there was no association (and then none was sent).
    static bool send(const RoutingRules::Destination &destination, const QStringList &files, int *sent, QStringList *errors);
};

} // End namespace udg

#endif
