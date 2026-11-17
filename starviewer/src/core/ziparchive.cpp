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

#include "ziparchive.h"

#include <algorithm>
#include <cstring>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QObject>
#include <QSet>

#include "itk_zlib.h"

namespace udg {

namespace {

const quint32 EndOfCentralDirectorySignature = 0x06054b50;
const quint32 CentralHeaderSignature = 0x02014b50;
const quint32 LocalHeaderSignature = 0x04034b50;

quint16 read16(const QByteArray &data, int offset)
{
    return static_cast<quint16>(static_cast<quint8>(data.at(offset)) | (static_cast<quint8>(data.at(offset + 1)) << 8));
}

quint32 read32(const QByteArray &data, int offset)
{
    return static_cast<quint32>(read16(data, offset)) | (static_cast<quint32>(read16(data, offset + 2)) << 16);
}

struct Entry {
    QString name;
    quint16 flags = 0;
    quint16 method = 0;
    quint32 crc = 0;
    quint32 compressedSize = 0;
    quint32 uncompressedSize = 0;
    quint32 localOffset = 0;
    quint32 externalAttributes = 0;
    bool isDirectory = false;
};

ZipArchive::Result fail(const QString &message)
{
    ZipArchive::Result result;
    result.message = message;
    return result;
}

void removeWritten(const QStringList &files)
{
    for (const QString &file : files)
    {
        QFile::remove(file);
    }
}

// Writes the data of one entry; returns an empty string if it went well, or the reason it did not
QString writeEntry(QFile &archive, const Entry &entry, quint64 dataStart, QFile &output, qint64 maxFileBytes, qint64 &totalBytes, qint64 maxTotalBytes)
{
    if (!archive.seek(static_cast<qint64>(dataStart)))
    {
        return QObject::tr("The file is cut short.");
    }

    quint32 crc = crc32(0L, Z_NULL, 0);
    quint64 written = 0;
    quint64 remaining = entry.compressedSize;
    const int chunk = 64 * 1024;
    QByteArray input(chunk, 0);
    QByteArray buffer(chunk, 0);

    auto store = [&](const char *data, int size) -> QString {
        written += static_cast<quint64>(size);

        // What the entry says it holds is a promise that is checked while writing: a file that holds more than that is a bomb
        if (written > entry.uncompressedSize || static_cast<qint64>(written) > maxFileBytes || totalBytes + static_cast<qint64>(size) > maxTotalBytes)
        {
            return QObject::tr("An entry holds more than its size, or the limit of size was passed.");
        }

        crc = crc32(crc, reinterpret_cast<const Bytef*>(data), static_cast<uInt>(size));
        totalBytes += size;

        if (output.write(data, size) != size)
        {
            return QObject::tr("The file could not be written (is the disk full?).");
        }

        return QString();
    };

    if (entry.method == 0)
    {
        if (entry.compressedSize != entry.uncompressedSize)
        {
            return QObject::tr("The sizes of a stored entry do not agree.");
        }

        while (remaining > 0)
        {
            qint64 toRead = static_cast<qint64>(std::min<quint64>(remaining, chunk));
            qint64 count = archive.read(input.data(), toRead);

            if (count != toRead)
            {
                return QObject::tr("The file is cut short.");
            }

            QString problem = store(input.constData(), static_cast<int>(count));

            if (!problem.isEmpty())
            {
                return problem;
            }

            remaining -= static_cast<quint64>(count);
        }
    }
    else
    {
        z_stream stream;
        std::memset(&stream, 0, sizeof(stream));

        // A negative window is a raw deflate stream, which is what a ZIP holds
        if (inflateInit2(&stream, -MAX_WBITS) != Z_OK)
        {
            return QObject::tr("The decompressor could not be started.");
        }

        int status = Z_OK;
        QString problem;

        while (status != Z_STREAM_END && problem.isEmpty())
        {
            if (stream.avail_in == 0)
            {
                if (remaining == 0)
                {
                    problem = QObject::tr("The compressed data of an entry ends too soon.");
                    break;
                }

                qint64 toRead = static_cast<qint64>(std::min<quint64>(remaining, chunk));
                qint64 count = archive.read(input.data(), toRead);

                if (count != toRead)
                {
                    problem = QObject::tr("The file is cut short.");
                    break;
                }

                remaining -= static_cast<quint64>(count);
                stream.next_in = reinterpret_cast<Bytef*>(input.data());
                stream.avail_in = static_cast<uInt>(count);
            }

            stream.next_out = reinterpret_cast<Bytef*>(buffer.data());
            stream.avail_out = chunk;
            status = inflate(&stream, Z_NO_FLUSH);

            if (status != Z_OK && status != Z_STREAM_END)
            {
                problem = QObject::tr("The compressed data of an entry is damaged.");
                break;
            }

            int produced = chunk - static_cast<int>(stream.avail_out);

            if (produced > 0)
            {
                problem = store(buffer.constData(), produced);
            }
        }

        inflateEnd(&stream);

        if (!problem.isEmpty())
        {
            return problem;
        }
    }

    if (written != entry.uncompressedSize)
    {
        return QObject::tr("An entry is not as large as it says.");
    }

    if (crc != entry.crc)
    {
        return QObject::tr("The check value (CRC) of an entry is wrong: the file is damaged.");
    }

    return QString();
}

}

QString ZipArchive::safeRelativePath(const QString &entryName)
{
    QString name = entryName;
    name.replace('\\', '/');

    if (name.isEmpty() || name.startsWith('/') || name.contains(':'))
    {
        return QString();
    }

    QStringList parts;
    QStringList pieces = name.split('/');

    for (int index = 0; index < pieces.size(); index++)
    {
        const QString &part = pieces.at(index);

        if (part.isEmpty())
        {
            // A trailing slash (a folder) is fine; "a//b" is not a path to guess at
            if (index == pieces.size() - 1 && index > 0)
            {
                continue;
            }

            return QString();
        }

        if (part == "." || part == ".." || part.endsWith(' ') || part.endsWith('.'))
        {
            return QString();
        }

        for (const QChar &character : part)
        {
            if (character.unicode() < 32)
            {
                return QString();
            }
        }

        parts << part;
    }

    return parts.join('/');
}

ZipArchive::Result ZipArchive::extract(const QString &archivePath, const QString &destinationFolder, const Limits &limits)
{
    QFile archive(archivePath);

    if (!archive.open(QIODevice::ReadOnly))
    {
        return fail(QObject::tr("The ZIP file could not be opened."));
    }

    qint64 size = archive.size();

    if (size < 22)
    {
        return fail(QObject::tr("It is not a ZIP file."));
    }

    // The end of central directory record is in the last 22 bytes plus a comment of up to 65,535 bytes
    qint64 tailSize = std::min<qint64>(size, 22 + 65535);
    archive.seek(size - tailSize);
    QByteArray tail = archive.read(tailSize);
    int endPosition = -1;

    for (int i = static_cast<int>(tail.size()) - 22; i >= 0; i--)
    {
        if (read32(tail, i) == EndOfCentralDirectorySignature)
        {
            endPosition = i;
            break;
        }
    }

    if (endPosition < 0)
    {
        return fail(QObject::tr("It is not a ZIP file."));
    }

    quint16 entriesInDirectory = read16(tail, endPosition + 10);
    quint32 directorySize = read32(tail, endPosition + 12);
    quint32 directoryOffset = read32(tail, endPosition + 16);

    if (entriesInDirectory == 0xFFFF || directorySize == 0xFFFFFFFF || directoryOffset == 0xFFFFFFFF)
    {
        return fail(QObject::tr("This ZIP file is of the ZIP64 kind (it is very large), which cannot be opened here. Unpack it with another program."));
    }

    if (static_cast<qint64>(directoryOffset) + directorySize > size || entriesInDirectory > limits.maxEntries)
    {
        return fail(QObject::tr("The ZIP file is damaged or holds too many files."));
    }

    archive.seek(directoryOffset);
    QByteArray directory = archive.read(directorySize);

    if (directory.size() != static_cast<int>(directorySize))
    {
        return fail(QObject::tr("The ZIP file is cut short."));
    }

    QList<Entry> entries;
    int position = 0;

    for (int i = 0; i < entriesInDirectory; i++)
    {
        if (position + 46 > directory.size() || read32(directory, position) != CentralHeaderSignature)
        {
            return fail(QObject::tr("The list of files of the ZIP file is damaged."));
        }

        Entry entry;
        entry.flags = read16(directory, position + 8);
        entry.method = read16(directory, position + 10);
        entry.crc = read32(directory, position + 16);
        entry.compressedSize = read32(directory, position + 20);
        entry.uncompressedSize = read32(directory, position + 24);
        int nameLength = read16(directory, position + 28);
        int extraLength = read16(directory, position + 30);
        int commentLength = read16(directory, position + 32);
        entry.externalAttributes = read32(directory, position + 38);
        entry.localOffset = read32(directory, position + 42);

        if (position + 46 + nameLength + extraLength + commentLength > directory.size())
        {
            return fail(QObject::tr("The list of files of the ZIP file is damaged."));
        }

        // The names are in UTF-8 if the flag says so, and in the old DOS character set otherwise: Latin 1 is what most of those are
        QByteArray rawName = directory.mid(position + 46, nameLength);
        entry.name = (entry.flags & 0x0800) ? QString::fromUtf8(rawName) : QString::fromLatin1(rawName);
        entry.isDirectory = entry.name.endsWith('/') || entry.name.endsWith('\\');
        entries << entry;
        position += 46 + nameLength + extraLength + commentLength;
    }

    // Everything is checked before anything is written
    QSet<QString> seen;
    qint64 declaredTotal = 0;

    for (const Entry &entry : entries)
    {
        if (entry.flags & 0x0001)
        {
            return fail(QObject::tr("The ZIP file is encrypted (with a password), which cannot be opened here."));
        }

        if (entry.compressedSize == 0xFFFFFFFF || entry.uncompressedSize == 0xFFFFFFFF || entry.localOffset == 0xFFFFFFFF)
        {
            return fail(QObject::tr("This ZIP file is of the ZIP64 kind (it is very large), which cannot be opened here. Unpack it with another program."));
        }

        if (!entry.isDirectory && entry.method != 0 && entry.method != 8)
        {
            return fail(QObject::tr("The ZIP file uses a kind of compression that cannot be opened here."));
        }

        QString relative = safeRelativePath(entry.name);

        if (relative.isEmpty())
        {
            return fail(QObject::tr("The ZIP file has a file name that is not safe (it would go outside the folder): \"%1\". Nothing was unpacked.").arg(entry.name.left(100)));
        }

        if (!entry.isDirectory)
        {
            if (seen.contains(relative.toLower()))
            {
                return fail(QObject::tr("The ZIP file has two files with the same name: \"%1\". Nothing was unpacked.").arg(relative.left(100)));
            }

            seen.insert(relative.toLower());

            if (static_cast<qint64>(entry.uncompressedSize) > limits.maxFileBytes)
            {
                return fail(QObject::tr("A file in the ZIP file is larger than the limit. Nothing was unpacked."));
            }

            declaredTotal += entry.uncompressedSize;

            if (declaredTotal > limits.maxTotalBytes)
            {
                return fail(QObject::tr("The ZIP file would unpack to more than the limit of size. Nothing was unpacked."));
            }
        }
    }

    QDir destination(destinationFolder);

    if (!destination.mkpath(".") )
    {
        return fail(QObject::tr("The folder for the files could not be created."));
    }

    QString root = QDir(destination.absolutePath()).canonicalPath();
    Result result;
    qint64 totalBytes = 0;

    for (const Entry &entry : entries)
    {
        QString relative = safeRelativePath(entry.name);
        QString path = QDir(root).absoluteFilePath(relative);

        // A second check, on the real path: a link that was in the folder before would take the file somewhere else
        QString parent = QFileInfo(path).absolutePath();
        QDir().mkpath(parent);
        QString canonicalParent = QDir(parent).canonicalPath();

        if (canonicalParent != root && !canonicalParent.startsWith(root + "/"))
        {
            removeWritten(result.files);
            return fail(QObject::tr("The ZIP file has a file name that is not safe. Nothing was unpacked."));
        }

        if (entry.isDirectory)
        {
            continue;
        }

        // Links are not unpacked: a DICOM study has none
        if (((entry.externalAttributes >> 16) & 0170000) == 0120000)
        {
            result.skippedEntries++;
            continue;
        }

        if (QFileInfo(path).isSymLink())
        {
            removeWritten(result.files);
            return fail(QObject::tr("The ZIP file has a file name that is not safe. Nothing was unpacked."));
        }

        // The local header: the name and the extra data there can be of another length than in the directory
        archive.seek(entry.localOffset);
        QByteArray local = archive.read(30);

        if (local.size() != 30 || read32(local, 0) != LocalHeaderSignature)
        {
            removeWritten(result.files);
            return fail(QObject::tr("The ZIP file is damaged."));
        }

        quint64 dataStart = static_cast<quint64>(entry.localOffset) + 30 + read16(local, 26) + read16(local, 28);

        if (dataStart + entry.compressedSize > static_cast<quint64>(size))
        {
            removeWritten(result.files);
            return fail(QObject::tr("The ZIP file is cut short."));
        }

        QFile output(path);

        if (!output.open(QIODevice::WriteOnly | QIODevice::Truncate))
        {
            removeWritten(result.files);
            return fail(QObject::tr("A file could not be written."));
        }

        result.files << path;
        QString problem = writeEntry(archive, entry, dataStart, output, limits.maxFileBytes, totalBytes, limits.maxTotalBytes);
        output.close();

        if (!problem.isEmpty())
        {
            removeWritten(result.files);
            return fail(problem);
        }
    }

    result.success = true;
    result.bytes = totalBytes;

    if (result.skippedEntries > 0)
    {
        result.message = QObject::tr("%1 links in the ZIP file were not unpacked.").arg(result.skippedEntries);
    }

    return result;
}

} // End namespace udg
