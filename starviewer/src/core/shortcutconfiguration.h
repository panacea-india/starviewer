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

#ifndef UDGSHORTCUTCONFIGURATION_H
#define UDGSHORTCUTCONFIGURATION_H

#include <QKeySequence>
#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>

namespace udg {

/**
    What the user can change about the keyboard shortcuts: the list of the shortcuts, their current and default keys, saving and going back to the default, and finding the keys that
    two actions share. The shortcuts are kept as settings ("Shortcuts/..."), as the application reads them.
 */
class ShortcutConfiguration {
public:
    /// The keys of all the shortcut settings, in alphabetical order.
    static QStringList keys();

    /// The text for a key of a setting: "Shortcuts/SlicingMouseTool" is "Slicing mouse tool".
    static QString displayName(const QString &key);

    /// The keys that the shortcut has now, and the ones it has by default.
    static QList<QKeySequence> current(const QString &key);
    static QList<QKeySequence> defaults(const QString &key);

    /// Saves the keys of the shortcut (an empty list is a shortcut with no key). Keys that are the same as the default are saved as such.
    static void set(const QString &key, const QList<QKeySequence> &sequences);
    /// Goes back to the default keys.
    static void reset(const QString &key);

    /// The shortcuts that have the same key: the key (as text) and the names of the actions that use it, only for the keys that more than one action uses.
    static QMap<QString, QStringList> conflicts(const QMap<QString, QList<QKeySequence>> &shortcuts);
};

} // End namespace udg

#endif
