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

#ifndef UDGREGISTEREDVOLUMES_H
#define UDGREGISTEREDVOLUMES_H

#include <QString>

namespace udg {

class Volume;

/**
    Knows which volumes were made by a registration (a copy of a volume put on the grid of another one), and what to say about each. A viewer asks it
    whether an input is registered, so that nobody takes the result of a registration for the original images. The note lives with the volume, not with
    the viewer, because the viewers of a layout are replaced when it changes.
 */
class RegisteredVolumes {
public:
    /// Says that the volume is the result of a registration, with a note like "rotation 2.1 deg, translation 7.3 mm". It is forgotten when the volume is deleted.
    static void add(Volume *volume, const QString &note);

    /// Returns the note of the volume, or an empty string if it is not a registered one.
    static QString getNote(const Volume *volume);

    /// Returns true if the volume is a registered one.
    static bool isRegistered(const Volume *volume);
};

} // End namespace udg

#endif
