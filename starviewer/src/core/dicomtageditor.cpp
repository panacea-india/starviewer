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

#include "dicomtageditor.h"

#include <QDate>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStringList>

#include <dcmtk/dcmdata/dctk.h>

namespace udg {

namespace {

// "0008,1140[0]/0008,1155" into steps: group, element and the item (-1 for the last one, which has none)
struct Step {
    quint16 group = 0;
    quint16 element = 0;
    int item = -1;
};

bool parsePath(const QString &path, QList<Step> *steps)
{
    static const QRegularExpression part("^([0-9A-Fa-f]{4}),([0-9A-Fa-f]{4})(?:\\[(\\d+)\\])?$");

    if (path.isEmpty())
    {
        return true;
    }

    for (const QString &text : path.split('/'))
    {
        QRegularExpressionMatch m = part.match(text.trimmed());

        if (!m.hasMatch())
        {
            return false;
        }

        Step s;
        s.group = m.captured(1).toUShort(nullptr, 16);
        s.element = m.captured(2).toUShort(nullptr, 16);
        s.item = m.captured(3).isEmpty() ? -1 : m.captured(3).toInt();
        *steps << s;
    }

    return true;
}

QString vrName(DcmEVR vr)
{
    return QString::fromLatin1(DcmVR(vr).getValidVRName());
}

bool fail(QString *errorMessage, const QString &reason)
{
    if (errorMessage)
    {
        *errorMessage = reason;
    }

    return false;
}

bool isBinary(const QString &vr)
{
    return vr == "OB" || vr == "OW" || vr == "OF" || vr == "OD" || vr == "OL" || vr == "OV" || vr == "UN" || vr == "AT" || vr == "SQ";
}

// Resolves the item that a path of steps ends in (every step has an item number), from the dataset
DcmItem* walk(DcmItem *root, const QList<Step> &steps, QString *errorMessage)
{
    DcmItem *current = root;

    for (const Step &s : steps)
    {
        DcmSequenceOfItems *sequence = nullptr;

        if (current->findAndGetSequence(DcmTagKey(s.group, s.element), sequence).bad() || !sequence)
        {
            fail(errorMessage, QString("There is no sequence (%1,%2).").arg(s.group, 4, 16, QChar('0')).arg(s.element, 4, 16, QChar('0')));
            return nullptr;
        }

        if (s.item < 0 || static_cast<unsigned long>(s.item) >= sequence->card())
        {
            fail(errorMessage, "That item of the sequence does not exist.");
            return nullptr;
        }

        current = sequence->getItem(static_cast<unsigned long>(s.item));
    }

    return current;
}

void collect(DcmItem *item, const QString &prefix, int depth, QList<DicomTagEditor::Entry> &out)
{
    for (unsigned long i = 0; i < item->card(); i++)
    {
        DcmElement *element = item->getElement(i);
        DicomTagEditor::Entry entry;
        quint16 g = element->getGTag();
        quint16 e = element->getETag();
        entry.path = prefix + DicomTagEditor::pathFor(g, e);
        entry.depth = depth;
        entry.vr = vrName(element->getVR());
        entry.isPrivate = g % 2 == 1;
        entry.isSequence = !element->isLeaf();
        DcmTag tag(g, e);
        entry.name = entry.isPrivate ? QString("Private attribute") : QString::fromLatin1(tag.getTagName());

        if (entry.isSequence)
        {
            DcmSequenceOfItems *sequence = dynamic_cast<DcmSequenceOfItems*>(element);
            unsigned long count = sequence ? sequence->card() : 0;
            entry.value = QString("(sequence with %1 items)").arg(count);
            out << entry;

            for (unsigned long k = 0; sequence && k < count; k++)
            {
                collect(sequence->getItem(k), QString("%1[%2]/").arg(entry.path).arg(k), depth + 1, out);
            }

            continue;
        }

        if (isBinary(entry.vr) || element->getLength() > 4096)
        {
            entry.value = QString("(%1 bytes)").arg(element->getLength());
            entry.editable = false;
        }
        else
        {
            OFString all;
            element->getOFStringArray(all);
            entry.value = QString::fromLatin1(all.c_str());
            entry.editable = true;
        }

        out << entry;
    }
}

}

DicomTagEditor::DicomTagEditor()
    : m_modified(false)
{
}

DicomTagEditor::~DicomTagEditor()
{
}

QString DicomTagEditor::pathFor(quint16 group, quint16 element)
{
    return QString("%1,%2").arg(group, 4, 16, QChar('0')).arg(element, 4, 16, QChar('0'));
}

bool DicomTagEditor::load(const QString &path, QString *errorMessage)
{
    std::unique_ptr<DcmFileFormat> file(new DcmFileFormat());
    OFCondition condition = file->loadFile(path.toLocal8Bit().constData());

    if (condition.bad())
    {
        return fail(errorMessage, QString("The file could not be read as DICOM: %1").arg(QString::fromLatin1(condition.text())));
    }

    // The pixels are read now, so that the file can be saved
    file->getDataset()->loadAllDataIntoMemory();
    m_file = std::move(file);
    m_loadedPath = QFileInfo(path).absoluteFilePath();
    m_modified = false;
    return true;
}

bool DicomTagEditor::isLoaded() const
{
    return m_file != nullptr;
}

bool DicomTagEditor::isModified() const
{
    return m_modified;
}

QList<DicomTagEditor::Entry> DicomTagEditor::entries() const
{
    QList<Entry> list;

    if (m_file)
    {
        collect(m_file->getDataset(), QString(), 0, list);
    }

    return list;
}

DcmItem* DicomTagEditor::itemAt(const QString &parentPath, QString *errorMessage) const
{
    if (!m_file)
    {
        fail(errorMessage, "There is no file.");
        return nullptr;
    }

    QList<Step> steps;

    if (!parsePath(parentPath, &steps))
    {
        fail(errorMessage, "The path is not valid.");
        return nullptr;
    }

    return walk(m_file->getDataset(), steps, errorMessage);
}

bool DicomTagEditor::exists(const QString &path) const
{
    QList<Step> steps;

    if (!m_file || path.isEmpty() || !parsePath(path, &steps))
    {
        return false;
    }

    Step last = steps.takeLast();
    DcmItem *item = walk(m_file->getDataset(), steps, nullptr);
    return item && item->tagExists(DcmTagKey(last.group, last.element));
}

QString DicomTagEditor::value(const QString &path) const
{
    QList<Step> steps;

    if (!m_file || path.isEmpty() || !parsePath(path, &steps))
    {
        return QString();
    }

    Step last = steps.takeLast();
    DcmItem *item = walk(m_file->getDataset(), steps, nullptr);
    OFString s;

    if (item && item->findAndGetOFStringArray(DcmTagKey(last.group, last.element), s).good())
    {
        return QString::fromLatin1(s.c_str());
    }

    return QString();
}

bool DicomTagEditor::validate(const QString &vr, const QString &value, QString *errorMessage)
{
    if (value.isEmpty())
    {
        return true;
    }

    QStringList values = value.split('\\');

    auto check = [&](const QRegularExpression &regex, int maxLength, const QString &what) {
        for (const QString &v : values)
        {
            if (v.length() > maxLength)
            {
                return fail(errorMessage, QString("A value of type %1 has at most %2 characters.").arg(vr).arg(maxLength));
            }

            if (!regex.match(v).hasMatch())
            {
                return fail(errorMessage, QString("\"%1\" is not %2.").arg(v, what));
            }
        }

        return true;
    };

    if (vr == "DA")
    {
        for (const QString &v : values)
        {
            if (v.length() != 8 || !QDate::fromString(v, "yyyyMMdd").isValid())
            {
                return fail(errorMessage, QString("\"%1\" is not a date (YYYYMMDD).").arg(v));
            }
        }

        return true;
    }

    if (vr == "TM")
    {
        return check(QRegularExpression("^([01]\\d|2[0-3])([0-5]\\d([0-5]\\d(\\.\\d{1,6})?)?)?$"), 14, "a time (HHMMSS)");
    }

    if (vr == "DT")
    {
        return check(QRegularExpression("^\\d{4}(\\d{2}(\\d{2}(\\d{2}(\\d{2}(\\d{2}(\\.\\d{1,6})?)?)?)?)?)?([+-]\\d{4})?$"), 26, "a date and time (YYYYMMDDHHMMSS)");
    }

    if (vr == "UI")
    {
        for (const QString &v : values)
        {
            if (v.length() > 64 || !QRegularExpression("^(0|[1-9]\\d*)(\\.(0|[1-9]\\d*))*$").match(v).hasMatch())
            {
                return fail(errorMessage, QString("\"%1\" is not a UID (numbers separated by dots, with no zero at the start of a number, at most 64 characters).").arg(v));
            }
        }

        return true;
    }

    if (vr == "CS")
    {
        return check(QRegularExpression("^[A-Z0-9 _]*$"), 16, "a code string (capital letters, digits, space and underscore)");
    }

    if (vr == "AE" || vr == "SH")
    {
        return check(QRegularExpression("^[^\\x00-\\x1F]*$"), 16, "a text");
    }

    if (vr == "LO")
    {
        return check(QRegularExpression("^[^\\x00-\\x1F]*$"), 64, "a text");
    }

    if (vr == "PN")
    {
        for (const QString &v : values)
        {
            QStringList groups = v.split('=');

            if (groups.size() > 3)
            {
                return fail(errorMessage, "A person name has at most three groups separated by =.");
            }

            for (const QString &g : groups)
            {
                if (g.length() > 64 || g.split('^').size() > 5)
                {
                    return fail(errorMessage, "A person name has at most 5 components separated by ^ and 64 characters in each group.");
                }
            }
        }

        return true;
    }

    if (vr == "AS")
    {
        return check(QRegularExpression("^\\d{3}[DWMY]$"), 4, "an age (three digits and D, W, M or Y: 045Y)");
    }

    if (vr == "IS")
    {
        return check(QRegularExpression("^[+-]?\\d+$"), 12, "an integer");
    }

    if (vr == "DS")
    {
        return check(QRegularExpression("^\\s*[+-]?(\\d+\\.?\\d*|\\.\\d+)([eE][+-]?\\d+)?\\s*$"), 16, "a decimal number");
    }

    if (vr == "US" || vr == "UL" || vr == "UV")
    {
        for (const QString &v : values)
        {
            bool ok = false;
            qulonglong n = v.trimmed().toULongLong(&ok);

            if (!ok || (vr == "US" && n > 65535ULL) || (vr == "UL" && n > 4294967295ULL))
            {
                return fail(errorMessage, QString("\"%1\" is not a whole number in the range of %2.").arg(v, vr));
            }
        }

        return true;
    }

    if (vr == "SS" || vr == "SL" || vr == "SV")
    {
        for (const QString &v : values)
        {
            bool ok = false;
            qlonglong n = v.trimmed().toLongLong(&ok);

            if (!ok || (vr == "SS" && (n < -32768 || n > 32767)) || (vr == "SL" && (n < -2147483648LL || n > 2147483647LL)))
            {
                return fail(errorMessage, QString("\"%1\" is not a whole number in the range of %2.").arg(v, vr));
            }
        }

        return true;
    }

    if (vr == "FL" || vr == "FD")
    {
        for (const QString &v : values)
        {
            bool ok = false;
            v.trimmed().toDouble(&ok);

            if (!ok)
            {
                return fail(errorMessage, QString("\"%1\" is not a number.").arg(v));
            }
        }

        return true;
    }

    if (vr == "LT" || vr == "ST" || vr == "UT" || vr == "UR" || vr == "UC")
    {
        int maxLength = vr == "ST" ? 1024 : vr == "LT" ? 10240 : 1000000;

        if (value.length() > maxLength)
        {
            return fail(errorMessage, QString("A value of type %1 has at most %2 characters.").arg(vr).arg(maxLength));
        }

        return true;
    }

    return fail(errorMessage, QString("The values of type %1 cannot be edited here.").arg(vr));
}

bool DicomTagEditor::setValue(const QString &path, const QString &newValue, QString *errorMessage)
{
    QList<Step> steps;

    if (!m_file || path.isEmpty() || !parsePath(path, &steps))
    {
        return fail(errorMessage, "The path is not valid or there is no file.");
    }

    Step last = steps.takeLast();
    DcmItem *item = walk(m_file->getDataset(), steps, errorMessage);

    if (!item)
    {
        return false;
    }

    DcmElement *element = nullptr;

    if (item->findAndGetElement(DcmTagKey(last.group, last.element), element).bad() || !element)
    {
        return fail(errorMessage, "That attribute does not exist.");
    }

    QString vr = vrName(element->getVR());

    if (!element->isLeaf() || isBinary(vr))
    {
        return fail(errorMessage, QString("The values of type %1 cannot be edited here.").arg(vr));
    }

    if (!validate(vr, newValue, errorMessage))
    {
        return false;
    }

    OFCondition condition = element->putString(newValue.toLatin1().constData());

    if (condition.bad())
    {
        return fail(errorMessage, QString::fromLatin1(condition.text()));
    }

    m_modified = true;
    return true;
}

bool DicomTagEditor::remove(const QString &path, QString *errorMessage)
{
    QList<Step> steps;

    if (!m_file || path.isEmpty() || !parsePath(path, &steps))
    {
        return fail(errorMessage, "The path is not valid or there is no file.");
    }

    Step last = steps.takeLast();
    DcmItem *item = walk(m_file->getDataset(), steps, errorMessage);

    if (!item)
    {
        return false;
    }

    if (item->findAndDeleteElement(DcmTagKey(last.group, last.element)).bad())
    {
        return fail(errorMessage, "That attribute does not exist.");
    }

    m_modified = true;
    return true;
}

bool DicomTagEditor::add(const QString &parentPath, quint16 group, quint16 element, const QString &newValue, QString *errorMessage)
{
    DcmItem *item = itemAt(parentPath, errorMessage);

    if (!item)
    {
        return false;
    }

    DcmTag tag(group, element);

    if (tag.error().bad() || tag.getEVR() == EVR_UNKNOWN || tag.getEVR() == EVR_UN || group % 2 == 1)
    {
        return fail(errorMessage, "That attribute is not in the DICOM dictionary, so its type is not known.");
    }

    if (item->tagExists(tag))
    {
        return fail(errorMessage, "That attribute already exists: change its value.");
    }

    QString vr = vrName(tag.getEVR());

    if (isBinary(vr))
    {
        return fail(errorMessage, QString("The values of type %1 cannot be added here.").arg(vr));
    }

    if (!validate(vr, newValue, errorMessage))
    {
        return false;
    }

    OFCondition condition = item->putAndInsertString(tag, newValue.toLatin1().constData());

    if (condition.bad())
    {
        return fail(errorMessage, QString::fromLatin1(condition.text()));
    }

    m_modified = true;
    return true;
}

bool DicomTagEditor::save(const QString &path, QString *errorMessage)
{
    if (!m_file)
    {
        return fail(errorMessage, "There is no file.");
    }

    if (QFileInfo(path).absoluteFilePath().compare(m_loadedPath, Qt::CaseInsensitive) == 0)
    {
        return fail(errorMessage, "The file that was opened is not written over: save in another file.");
    }

    OFCondition condition = m_file->saveFile(path.toLocal8Bit().constData(), EXS_Unknown, EET_ExplicitLength, EGL_recalcGL, EPD_noChange, 0, 0, EWM_updateMeta);

    if (condition.bad())
    {
        return fail(errorMessage, QString::fromLatin1(condition.text()));
    }

    return true;
}

} // End namespace udg
