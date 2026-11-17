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

#include "dicomdeidentifier.h"

#include <algorithm>

#include <QCryptographicHash>
#include <QDate>
#include <QPair>
#include <QRegExp>
#include <QSet>
#include <QUuid>

#include <dcmtk/dcmdata/dctk.h>

namespace udg {

namespace {

typedef QPair<quint16, quint16> TagPair;

// The attributes that identify the patient, the institution or the people: removed
const quint16 RemovedTable[][2] = {
    {0x0008, 0x0080}, {0x0008, 0x0081}, {0x0008, 0x0092}, {0x0008, 0x0094}, {0x0008, 0x1010}, {0x0008, 0x1040}, {0x0008, 0x1048}, {0x0008, 0x1049}, {0x0008, 0x1050},
    {0x0008, 0x1052}, {0x0008, 0x1060}, {0x0008, 0x1062}, {0x0008, 0x1070}, {0x0008, 0x1072}, {0x0008, 0x1080}, {0x0008, 0x1084}, {0x0008, 0x2111}, {0x0008, 0x0096},
    {0x0010, 0x0032}, {0x0010, 0x1000}, {0x0010, 0x1001}, {0x0010, 0x1002}, {0x0010, 0x1005}, {0x0010, 0x1040}, {0x0010, 0x1060}, {0x0010, 0x1080}, {0x0010, 0x1081},
    {0x0010, 0x1090}, {0x0010, 0x2000}, {0x0010, 0x2110}, {0x0010, 0x2150}, {0x0010, 0x2152}, {0x0010, 0x2154}, {0x0010, 0x2160}, {0x0010, 0x2180}, {0x0010, 0x21A0},
    {0x0010, 0x21B0}, {0x0010, 0x21C0}, {0x0010, 0x21D0}, {0x0010, 0x21F0}, {0x0010, 0x2203}, {0x0010, 0x4000}, {0x0010, 0x0050}, {0x0010, 0x0101}, {0x0010, 0x0102},
    {0x0018, 0x1030}, {0x0018, 0x1400}, {0x0018, 0x4000}, {0x0018, 0x9424}, {0x0020, 0x4000}, {0x0032, 0x1030}, {0x0032, 0x1032}, {0x0032, 0x1033}, {0x0032, 0x1060},
    {0x0032, 0x1064}, {0x0032, 0x4000}, {0x0038, 0x0010}, {0x0038, 0x0011}, {0x0038, 0x0020}, {0x0038, 0x0021}, {0x0038, 0x0040}, {0x0038, 0x0050}, {0x0038, 0x0300},
    {0x0038, 0x0400}, {0x0038, 0x0500}, {0x0038, 0x4000}, {0x0040, 0x0006}, {0x0040, 0x0007}, {0x0040, 0x000B}, {0x0040, 0x0010}, {0x0040, 0x0011}, {0x0040, 0x0012},
    {0x0040, 0x0241}, {0x0040, 0x0242}, {0x0040, 0x0243}, {0x0040, 0x0244}, {0x0040, 0x0245}, {0x0040, 0x0250}, {0x0040, 0x0251}, {0x0040, 0x0253}, {0x0040, 0x0254},
    {0x0040, 0x0275}, {0x0040, 0x0280}, {0x0040, 0x1001}, {0x0040, 0x1002}, {0x0040, 0x1004}, {0x0040, 0x1005}, {0x0040, 0x1010}, {0x0040, 0x1011}, {0x0040, 0x1102},
    {0x0040, 0x1103}, {0x0040, 0x2008}, {0x0040, 0x2009}, {0x0040, 0x2010}, {0x0040, 0x2016}, {0x0040, 0x2017}, {0x0040, 0x2400}, {0x0040, 0x3001}, {0x0040, 0xA075},
    {0x0040, 0xA073}, {0x0040, 0xA123}, {0x0040, 0xA730}, {0x0070, 0x0084}, {0x0070, 0x0086}, {0x0088, 0x0200}, {0x0088, 0x0904}, {0x0088, 0x0906}, {0x0088, 0x0910},
    {0x0088, 0x0912}, {0x3006, 0x00A6}, {0x3008, 0x0105}, {0x4008, 0x0114}, {0x4008, 0x0115}, {0x4008, 0x0300}, {0x4008, 0x0119}, {0x300E, 0x0008}
};

// Emptied (they are required to be there, with no value)
const quint16 EmptiedTable[][2] = {
    {0x0008, 0x0050}, // Accession Number
    {0x0008, 0x0090}, // Referring Physician's Name
    {0x0010, 0x0030}, // Patient's Birth Date
    {0x0010, 0x0040}, // Patient's Sex (kept with the characteristics option)
    {0x0020, 0x0010}  // Study ID
};

// Device identity, kept with the option: Device Serial Number, Device UID is a UID, Plate ID, Cassette ID, Gantry ID, Detector ID, Generator ID
const quint16 DeviceTable[][2] = {{0x0018, 0x1000}, {0x0018, 0x1004}, {0x0018, 0x1007}, {0x0018, 0x1008}, {0x0018, 0x700A}, {0x0018, 0x1005}, {0x0008, 0x1010}};

// Patient characteristics, kept with the option: age, size, weight, smoking status, pregnancy status, sex
const quint16 CharacteristicsTable[][2] = {{0x0010, 0x1010}, {0x0010, 0x1020}, {0x0010, 0x1030}, {0x0010, 0x21A0}, {0x0010, 0x21C0}, {0x0010, 0x0040}, {0x0010, 0x1022}, {0x0010, 0x1021}};

// Study and series descriptions and other descriptions: removed unless the option to clean them is on
const quint16 DescriptorTable[][2] = {{0x0008, 0x1030}, {0x0008, 0x103E}, {0x0008, 0x1090}, {0x0018, 0x1030}, {0x0008, 0x2111}};

template <int N>
bool inTable(const quint16 (&table)[N][2], quint16 g, quint16 e)
{
    for (int i = 0; i < N; i++)
    {
        if (table[i][0] == g && table[i][1] == e)
        {
            return true;
        }
    }

    return false;
}

// UIDs that are not replaced: the classes and the syntaxes
bool isFixedUid(const DcmElement *element, const QString &value)
{
    quint16 g = element->getGTag();
    quint16 e = element->getETag();

    if ((g == 0x0008 && (e == 0x0016 || e == 0x1150)) || (g == 0x0002 && (e == 0x0002 || e == 0x0010 || e == 0x0012)) || (g == 0x0004 && e == 0x1510))
    {
        return true;
    }

    // The UIDs that the standard defines
    return value.startsWith("1.2.840.10008.");
}

}

DicomDeidentifier::DicomDeidentifier()
    : m_salt(QUuid::createUuid().toString())
{
}

DicomDeidentifier::DicomDeidentifier(const Options &options, const QString &salt)
    : m_options(options), m_salt(salt.isEmpty() ? QUuid::createUuid().toString() : salt)
{
}

QString DicomDeidentifier::hashHex(const QString &text, int digits) const
{
    return QString(QCryptographicHash::hash((m_salt + "/" + text).toUtf8(), QCryptographicHash::Sha256).toHex()).left(digits);
}

QString DicomDeidentifier::mappedUid(const QString &original)
{
    auto it = m_uidMap.constFind(original);

    if (it != m_uidMap.constEnd())
    {
        return it.value();
    }

    // 2.25 and a number of 128 bits made from the original and the salt
    QByteArray digest = QCryptographicHash::hash((m_salt + "/uid/" + original).toUtf8(), QCryptographicHash::Sha256).left(16);
    digest[0] = digest[0] & 0x3F;
    QString decimal;
    QByteArray number = digest;

    // Hexadecimal to decimal by long division
    while (!number.isEmpty() && number != QByteArray(number.size(), '\0'))
    {
        int remainder = 0;

        for (int i = 0; i < number.size(); i++)
        {
            int value = remainder * 256 + static_cast<unsigned char>(number[i]);
            number[i] = static_cast<char>(value / 10);
            remainder = value % 10;
        }

        decimal.prepend(QChar('0' + remainder));
    }

    QString uid = "2.25." + (decimal.isEmpty() ? QString("0") : decimal);
    m_uidMap.insert(original, uid);
    return uid;
}

int DicomDeidentifier::dateShiftDaysFor(const QString &originalPatientId) const
{
    bool ok = false;
    quint32 value = hashHex("shift/" + originalPatientId, 8).toUInt(&ok, 16);
    return -(30 + static_cast<int>(value % 336));
}

QList<QPair<quint16, quint16>> DicomDeidentifier::removedAttributes()
{
    QList<TagPair> list;

    for (const auto &t : RemovedTable)
    {
        list << TagPair(t[0], t[1]);
    }

    for (const auto &t : EmptiedTable)
    {
        list << TagPair(t[0], t[1]);
    }

    return list;
}

DicomDeidentifier::Report DicomDeidentifier::process(DcmDataset *dataset)
{
    Report report;
    OFString original;
    dataset->findAndGetOFString(DCM_PatientID, original);
    QString originalId = QString::fromLatin1(original.c_str());
    dataset->findAndGetOFString(DCM_PatientName, original);
    QString originalName = QString::fromLatin1(original.c_str());

    int shift = m_options.retainModifiedDates ? dateShiftDaysFor(originalId) : 0;

    // Warnings about the pixels, before anything changes
    OFString burned;
    dataset->findAndGetOFString(DCM_BurnedInAnnotation, burned);

    if (QString::fromLatin1(burned.c_str()).trimmed().toUpper() == "YES")
    {
        report.warnings << "Burned In Annotation is YES: the pixels have text, which may be the identity of the patient.";
    }

    OFString modality;
    dataset->findAndGetOFString(DCM_Modality, modality);
    QString m = QString::fromLatin1(modality.c_str()).trimmed().toUpper();

    if ((m == "US" || m == "SC" || m == "OT" || m == "ES" || m == "XA" || m == "RF" || m == "CR" || m == "DX") && burned.empty())
    {
        report.warnings << QString("%1 images often have text in the pixels, and this one does not say whether it has: look at it before sharing.").arg(m);
    }

    processItem(dataset, report, shift, originalName, originalId, true);

    // The object says that it is de-identified, and how
    QString patientId = m_options.patientId.isEmpty() ? "ANON-" + hashHex("id/" + originalId, 10).toUpper() : m_options.patientId;
    dataset->putAndInsertString(DCM_PatientName, m_options.patientName.toLatin1().constData());
    dataset->putAndInsertString(DCM_PatientID, patientId.toLatin1().constData());
    dataset->putAndInsertString(DCM_PatientIdentityRemoved, "YES");
    // The method: the profile as text, and each option as a code of the DICOM context group 7050
    struct Code {
        const char *value;
        const char *meaning;
    };

    QList<Code> codes;
    codes << Code{"113100", "Basic Application Confidentiality Profile"};

    if (m_options.pixelsCleaned)
    {
        codes << Code{"113101", "Clean Pixel Data Option"};
    }

    if (m_options.cleanDescriptors)
    {
        codes << Code{"113105", "Clean Descriptors Option"};
    }

    if (m_options.retainDates)
    {
        codes << Code{"113106", "Retain Longitudinal Temporal Information Full Dates Option"};
    }

    if (m_options.retainModifiedDates)
    {
        codes << Code{"113107", "Retain Longitudinal Temporal Information Modified Dates Option"};
    }

    if (m_options.retainPatientCharacteristics)
    {
        codes << Code{"113108", "Retain Patient Characteristics Option"};
    }

    if (m_options.retainDeviceIdentity)
    {
        codes << Code{"113109", "Retain Device Identity Option"};
    }

    if (m_options.retainUids)
    {
        codes << Code{"113110", "Retain UIDs Option"};
    }

    dataset->putAndInsertString(DCM_DeidentificationMethod, "Starviewer: Basic Application Level Confidentiality Profile");
    dataset->findAndDeleteElement(DCM_DeidentificationMethodCodeSequence);

    for (const Code &code : codes)
    {
        DcmItem *item = nullptr;

        if (dataset->findOrCreateSequenceItem(DCM_DeidentificationMethodCodeSequence, item, -2).good() && item)
        {
            item->putAndInsertString(DCM_CodeValue, code.value);
            item->putAndInsertString(DCM_CodingSchemeDesignator, "DCM");
            item->putAndInsertString(DCM_CodeMeaning, code.meaning);
        }
    }

    return report;
}

void DicomDeidentifier::processItem(DcmItem *item, Report &report, int shiftDays, const QString &originalName, const QString &originalId, bool topLevel)
{
    Q_UNUSED(topLevel);
    QList<DcmTagKey> toRemove;
    QList<DcmTagKey> toEmpty;
    QList<QPair<DcmTagKey, QString>> toSet;
    QStringList nameTokens = originalName.split(QRegExp("[\\^ ,]"), QString::SkipEmptyParts);

    for (unsigned long i = 0; i < item->card(); i++)
    {
        DcmElement *element = item->getElement(i);

        if (!element)
        {
            continue;
        }

        quint16 g = element->getGTag();
        quint16 e = element->getETag();
        DcmTagKey key(g, e);
        DcmEVR vr = element->getVR();

        // Private attributes, whatever they hold
        if (g % 2 == 1)
        {
            toRemove << key;
            report.privateRemoved++;
            continue;
        }

        // Sequences: their items are processed too (the sequence itself is removed if the table says so)
        if (element->isLeaf() == OFFalse)
        {
            DcmSequenceOfItems *sequence = dynamic_cast<DcmSequenceOfItems*>(element);

            if (sequence && !inTable(RemovedTable, g, e))
            {
                for (unsigned long k = 0; k < sequence->card(); k++)
                {
                    DcmItem *nested = sequence->getItem(k);

                    if (nested)
                    {
                        processItem(nested, report, shiftDays, originalName, originalId, false);
                    }
                }
            }
        }

        bool isUid = vr == EVR_UI;
        bool descriptor = inTable(DescriptorTable, g, e);

        if (isUid)
        {
            OFString value;
            element->getOFString(value, 0);
            QString v = QString::fromLatin1(value.c_str());

            if (!m_options.retainUids && !isFixedUid(element, v) && !v.isEmpty())
            {
                toSet.append(qMakePair(key, mappedUid(v)));
                report.uidsReplaced++;
            }

            continue;
        }

        if (g == 0x0010 && e == 0x0010)
        {
            continue; // set at the end
        }

        if (g == 0x0010 && e == 0x0020)
        {
            continue;
        }

        if (inTable(CharacteristicsTable, g, e) && m_options.retainPatientCharacteristics)
        {
            continue;
        }

        if (inTable(DeviceTable, g, e) && m_options.retainDeviceIdentity)
        {
            continue;
        }

        // Without the options, the characteristics of the patient and the identity of the device are removed (the sex is emptied)
        if (inTable(CharacteristicsTable, g, e) && !(g == 0x0010 && e == 0x0040))
        {
            toRemove << key;
            report.removed++;
            continue;
        }

        if (inTable(DeviceTable, g, e))
        {
            toRemove << key;
            report.removed++;
            continue;
        }

        if (descriptor && m_options.cleanDescriptors)
        {
            // The description stays if it has nothing of the name or the ID of the patient
            OFString value;
            element->getOFString(value, 0);
            QString v = QString::fromLatin1(value.c_str()).toUpper();
            bool leaks = !originalId.isEmpty() && originalId.length() >= 3 && v.contains(originalId.toUpper());

            for (const QString &token : nameTokens)
            {
                if (token.length() >= 3 && v.contains(token.toUpper()))
                {
                    leaks = true;
                }
            }

            if (!leaks)
            {
                continue;
            }

            toEmpty << key;
            report.emptied++;
            continue;
        }

        if (inTable(RemovedTable, g, e) || descriptor)
        {
            toRemove << key;
            report.removed++;
            continue;
        }

        if (inTable(EmptiedTable, g, e))
        {
            toEmpty << key;
            report.emptied++;
            continue;
        }

        // Any other name of a person (in the sequences of reports and plans, for example) is emptied
        if (vr == EVR_PN)
        {
            toEmpty << key;
            report.emptied++;
            continue;
        }

        // Dates and times
        if (vr == EVR_DA || vr == EVR_TM || vr == EVR_DT)
        {
            if (m_options.retainDates)
            {
                continue;
            }

            OFString value;
            element->getOFString(value, 0);
            QString v = QString::fromLatin1(value.c_str());

            if (m_options.retainModifiedDates && vr != EVR_TM && v.length() >= 8)
            {
                QDate date = QDate::fromString(v.left(8), "yyyyMMdd");

                if (date.isValid())
                {
                    toSet.append(qMakePair(key, date.addDays(shiftDays).toString("yyyyMMdd") + v.mid(8)));
                    report.datesShifted++;
                    continue;
                }
            }

            if (m_options.retainModifiedDates && vr == EVR_TM)
            {
                continue; // the time of the day does not say who it is; kept with the shifted dates
            }

            toSet.append(qMakePair(key, vr == EVR_DA ? QString("19000101") : vr == EVR_TM ? QString("000000") : QString("19000101000000")));
            report.replaced++;
        }
    }

    for (const DcmTagKey &key : toRemove)
    {
        item->findAndDeleteElement(key);
    }

    for (const DcmTagKey &key : toEmpty)
    {
        item->putAndInsertString(DcmTag(key), "");
    }

    for (const auto &pair : toSet)
    {
        item->putAndInsertString(DcmTag(pair.first), pair.second.toLatin1().constData());
    }
}

} // End namespace udg
