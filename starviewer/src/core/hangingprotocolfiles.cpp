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

#include "hangingprotocolfiles.h"

#include <algorithm>
#include <memory>

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>

#include "hangingprotocol.h"
#include "hangingprotocolmask.h"
#include "hangingprotocolxmlreader.h"
#include "hangingprotocolxmlwriter.h"

namespace udg {

namespace {

std::unique_ptr<HangingProtocol> readProtocol(const QString &path)
{
    // The reader shows a message if it cannot open the file: look first
    QFile file(path);

    if (!file.open(QIODevice::ReadOnly))
    {
        return nullptr;
    }

    file.close();
    HangingProtocolXMLReader reader;
    return std::unique_ptr<HangingProtocol>(reader.readFile(path));
}

void addFrom(const QString &directory, bool isUser, QList<HangingProtocolFiles::Entry> &entries)
{
    if (directory.isEmpty() || !QFileInfo(directory).isDir())
    {
        return;
    }

    QDirIterator it(directory, QStringList() << "*.xml", QDir::Files, QDirIterator::Subdirectories);

    while (it.hasNext())
    {
        QString path = it.next();
        std::unique_ptr<HangingProtocol> protocol = readProtocol(path);

        if (!protocol)
        {
            continue;
        }

        HangingProtocolFiles::Entry entry;
        entry.path = QFileInfo(path).absoluteFilePath();
        entry.name = protocol->getName();
        entry.modalities = protocol->getHangingProtocolMask()->getProtocolList();
        entry.priority = protocol->getPriority();
        entry.isUser = isUser;
        entries << entry;
    }
}

bool isInside(const QString &path, const QString &directory)
{
    QString base = QDir::cleanPath(QFileInfo(directory).absoluteFilePath());
    QString file = QDir::cleanPath(QFileInfo(path).absoluteFilePath());

    return file.startsWith(base + "/", Qt::CaseInsensitive);
}

}

QList<HangingProtocolFiles::Entry> HangingProtocolFiles::list(const QStringList &builtInDirectories, const QString &userDirectory)
{
    QList<Entry> entries;

    for (const QString &directory : builtInDirectories)
    {
        addFrom(directory, false, entries);
    }

    addFrom(userDirectory, true, entries);

    std::stable_sort(entries.begin(), entries.end(), [](const Entry &a, const Entry &b) {
        if (a.isUser != b.isUser)
        {
            return !a.isUser;
        }

        return QString::compare(a.name, b.name, Qt::CaseInsensitive) < 0;
    });

    return entries;
}

QString HangingProtocolFiles::fileNameFor(const QString &name)
{
    QString result;

    for (const QChar &c : name.trimmed())
    {
        bool keep = (c.unicode() < 128 && c.isLetterOrNumber()) || c == '-' || c == '_';
        result += keep ? c : QChar('_');
    }

    // If only separators are left, nothing of the name was kept
    QString meaningful = result;
    meaningful.remove('_').remove('-');
    return meaningful.isEmpty() ? QString("protocol") : result;
}

QString HangingProtocolFiles::copyToUser(const QString &sourcePath, const QString &userDirectory, const QString &newName, QString *errorMessage)
{
    auto fail = [errorMessage](const QString &reason) {
        if (errorMessage)
        {
            *errorMessage = reason;
        }

        return QString();
    };

    if (newName.trimmed().isEmpty())
    {
        return fail("The new protocol needs a name.");
    }

    if (userDirectory.isEmpty())
    {
        return fail("There is no folder for the protocols of the user.");
    }

    std::unique_ptr<HangingProtocol> protocol = readProtocol(sourcePath);

    if (!protocol)
    {
        return fail("The protocol could not be read.");
    }

    if (!QDir().mkpath(userDirectory))
    {
        return fail("The folder of the protocols of the user could not be created.");
    }

    QString path = QDir(userDirectory).absoluteFilePath(fileNameFor(newName) + ".xml");

    if (QFile::exists(path))
    {
        return fail("There is already a protocol file with that name.");
    }

    protocol->setName(newName.trimmed());
    QString reason;

    if (!HangingProtocolXMLWriter::writeFile(*protocol, path, &reason))
    {
        QFile::remove(path);
        return fail(reason);
    }

    return path;
}

bool HangingProtocolFiles::removeUserFile(const QString &path, const QString &userDirectory, QString *errorMessage)
{
    if (userDirectory.isEmpty() || !isInside(path, userDirectory) || QFileInfo(path).suffix().toLower() != "xml")
    {
        if (errorMessage)
        {
            *errorMessage = "Only the protocol files of the user can be deleted.";
        }

        return false;
    }

    if (!QFile::remove(path))
    {
        if (errorMessage)
        {
            *errorMessage = "The file could not be deleted.";
        }

        return false;
    }

    return true;
}

bool HangingProtocolFiles::isValidPosition(const QString &position)
{
    QStringList parts = position.split("\\");

    if (parts.size() != 4)
    {
        return false;
    }

    double values[4];

    for (int i = 0; i < 4; i++)
    {
        bool ok = false;
        values[i] = parts[i].trimmed().toDouble(&ok);

        if (!ok || values[i] < 0.0 || values[i] > 1.0)
        {
            return false;
        }
    }

    return values[0] < values[2] && values[1] > values[3];
}

} // End namespace udg
