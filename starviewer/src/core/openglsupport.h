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

#ifndef UDGOPENGLSUPPORT_H
#define UDGOPENGLSUPPORT_H

#include <QString>

namespace udg {

/**
    Tells whether this computer gives Starviewer the OpenGL version that the viewers need (VTK needs 3.2 or newer).
    Without it the viewers cannot start, and VTK ends the process, so the interface asks before it opens one.

    The check creates an OpenGL context once, on the first call, and keeps the answer. It must be called from the
    GUI thread, after the QApplication has been created.
 */
class OpenGLSupport {
public:
    /// Returns true if a context of the minimum version needed (see SystemRequirements) can be created and used.
    static bool isAvailable();

    /// Returns the version that the computer gave, like "3.3", or an empty string if there is no usable context.
    static QString getFoundVersion();

    /// Returns the minimum version the viewers need, like "3.2".
    static QString getRequiredVersion();

    /// Returns true if the version given by major and minor is the same or newer than the one required, written like "3.2".
    static bool isVersionAtLeast(int major, int minor, const QString &required);

private:
    static void check();
};

} // End namespace udg

#endif
