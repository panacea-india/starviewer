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

#include "shortcutconfiguration.h"

#include "settings.h"
#include "settingsregistry.h"
#include "shortcutmanager.h"

namespace udg {

namespace {

const QString Prefix("Shortcuts/");

}

QStringList ShortcutConfiguration::keys()
{
    QStringList result = SettingsRegistry::instance()->getKeysWithPrefix(Prefix);
    result.sort();
    return result;
}

QString ShortcutConfiguration::displayName(const QString &key)
{
    QString name = key.startsWith(Prefix) ? key.mid(Prefix.size()) : key;
    QString text;

    for (int i = 0; i < name.size(); i++)
    {
        // A capital letter after a small one or a digit starts a new word: "SlicingMouseTool" is "Slicing mouse tool"
        if (i > 0 && name.at(i).isUpper() && (name.at(i - 1).isLower() || name.at(i - 1).isDigit()))
        {
            text += ' ';
            text += name.at(i).toLower();
        }
        else
        {
            text += name.at(i);
        }
    }

    return text;
}

QList<QKeySequence> ShortcutConfiguration::current(const QString &key)
{
    return ShortcutManager::getShortcuts(key);
}

QList<QKeySequence> ShortcutConfiguration::defaults(const QString &key)
{
    QList<QKeySequence> sequences;

    for (const QVariant &value : SettingsRegistry::instance()->getDefaultValue(key).toList())
    {
        sequences << QKeySequence(value.toString());
    }

    return sequences;
}

void ShortcutConfiguration::set(const QString &key, const QList<QKeySequence> &sequences)
{
    QStringList texts;

    for (const QKeySequence &sequence : sequences)
    {
        if (!sequence.isEmpty())
        {
            texts << sequence.toString(QKeySequence::PortableText);
        }
    }

    Settings settings;
    settings.setValue(key, texts.isEmpty() ? QStringList(QString("")) : texts);
}

void ShortcutConfiguration::reset(const QString &key)
{
    Settings settings;
    settings.remove(key);
}

QMap<QString, QStringList> ShortcutConfiguration::conflicts(const QMap<QString, QList<QKeySequence>> &shortcuts)
{
    QMap<QString, QStringList> users;

    for (auto it = shortcuts.constBegin(); it != shortcuts.constEnd(); ++it)
    {
        for (const QKeySequence &sequence : it.value())
        {
            if (!sequence.isEmpty())
            {
                QString text = sequence.toString(QKeySequence::PortableText);

                if (!users[text].contains(it.key()))
                {
                    users[text] << it.key();
                }
            }
        }
    }

    QMap<QString, QStringList> result;

    for (auto it = users.constBegin(); it != users.constEnd(); ++it)
    {
        if (it.value().size() > 1)
        {
            result.insert(it.key(), it.value());
        }
    }

    return result;
}

} // End namespace udg
