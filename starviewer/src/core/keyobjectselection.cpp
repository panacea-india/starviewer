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

#include "keyobjectselection.h"

#include <QObject>

#include "dicomsequenceattribute.h"
#include "dicomsequenceitem.h"
#include "dicomtagreader.h"

namespace udg {

namespace {

const char* const KeyObjectSelectionDocumentStorageUid = "1.2.840.10008.5.1.4.1.1.88.59";
const char* const DcmCodingScheme = "DCM";
// Code of the TEXT item that holds the description of the key objects, "Key Object Description"
const char* const KeyObjectDescriptionCode = "113012";

QList<int> parseFrames(const QString &value)
{
    QList<int> frames;

    for (const QString &part : value.split('\\', QString::SkipEmptyParts))
    {
        bool ok = false;
        int frame = part.trimmed().toInt(&ok);

        if (ok && frame > 0)
        {
            frames << frame;
        }
    }

    return frames;
}

}

KeyObjectSelection::KeyObjectSelection()
    : m_kind(Kind::Selection), m_null(true)
{
}

KeyObjectSelection KeyObjectSelection::fromDICOM(const DICOMTagReader &reader, QString *error)
{
    auto fail = [error](const QString &reason) {
        if (error)
        {
            *error = reason;
        }
        return KeyObjectSelection();
    };

    if (reader.getValueAttributeAsQString(DICOMSOPClassUID) != KeyObjectSelectionDocumentStorageUid)
    {
        return fail(QObject::tr("It is not a Key Object Selection Document."));
    }

    KeyObjectSelection selection;
    selection.m_sopInstanceUid = reader.getValueAttributeAsQString(DICOMSOPInstanceUID);

    // The document title
    DICOMSequenceItem *title = reader.getFirstSequenceItem(DICOMConceptNameCodeSequence);

    if (title)
    {
        selection.m_titleCodeValue = title->getValueAttributeAsQString(DICOMCodeValue);
        selection.m_titleMeaning = title->getValueAttributeAsQString(DICOMCodeMeaning);
        selection.m_kind = getKindOfTitle(selection.m_titleCodeValue, title->getValueAttributeAsQString(DICOMCodingSchemeDesignator));
    }

    // The items of the content: the images, and the description
    DICOMSequenceAttribute *content = reader.getSequenceAttribute(DICOMContentSequence);

    if (content)
    {
        for (DICOMSequenceItem *item : content->getItems())
        {
            QString valueType = item->getValueAttributeAsQString(DICOMValueType);

            if (valueType == "IMAGE")
            {
                DICOMSequenceAttribute *referenced = item->getSequenceAttribute(DICOMReferencedSOPSequence);

                if (!referenced)
                {
                    continue;
                }

                for (DICOMSequenceItem *referencedItem : referenced->getItems())
                {
                    Reference reference;
                    reference.sopClassUid = referencedItem->getValueAttributeAsQString(DICOMReferencedSOPClassUID);
                    reference.sopInstanceUid = referencedItem->getValueAttributeAsQString(DICOMReferencedSOPInstanceUID);
                    reference.frames = parseFrames(referencedItem->getValueAttributeAsQString(DICOMReferencedFrameNumber));

                    if (!reference.sopInstanceUid.isEmpty())
                    {
                        selection.m_references << reference;
                    }
                }
            }
            else if (valueType == "TEXT" && selection.m_note.isEmpty())
            {
                DICOMSequenceAttribute *concept = item->getSequenceAttribute(DICOMConceptNameCodeSequence);

                if (concept && !concept->getItems().isEmpty()
                    && concept->getItems().first()->getValueAttributeAsQString(DICOMCodeValue) == KeyObjectDescriptionCode)
                {
                    selection.m_note = item->getValueAttributeAsQString(DICOMTextValue);
                }
            }
        }
    }

    if (selection.m_references.isEmpty())
    {
        return fail(QObject::tr("The Key Object Selection Document does not refer to any image."));
    }

    selection.m_null = false;
    return selection;
}

KeyObjectSelection::Kind KeyObjectSelection::getKindOfTitle(const QString &codeValue, const QString &codingSchemeDesignator)
{
    if (codingSchemeDesignator == DcmCodingScheme)
    {
        // Rejected for Quality Reasons, Rejected for Patient Safety Reasons, Incorrect Modality Worklist Entry, Data Retention Policy Expired
        if (codeValue == "113001" || codeValue == "113037" || codeValue == "113038" || codeValue == "113039")
        {
            return Kind::Rejection;
        }

        // Manifest, Signed Manifest, Complete Study Content, Signed Complete Study Content, Complete Acquisition Content, Signed Complete Acquisition Content
        if (codeValue == "113030" || codeValue == "113031" || codeValue == "113032" || codeValue == "113033" || codeValue == "113034"
            || codeValue == "113035")
        {
            return Kind::Manifest;
        }
    }

    return Kind::Selection;
}

bool KeyObjectSelection::isNull() const
{
    return m_null;
}

const QString& KeyObjectSelection::getSopInstanceUid() const
{
    return m_sopInstanceUid;
}

const QString& KeyObjectSelection::getTitleCodeValue() const
{
    return m_titleCodeValue;
}

const QString& KeyObjectSelection::getTitleMeaning() const
{
    return m_titleMeaning;
}

const QString& KeyObjectSelection::getNote() const
{
    return m_note;
}

KeyObjectSelection::Kind KeyObjectSelection::getKind() const
{
    return m_kind;
}

const QList<KeyObjectSelection::Reference>& KeyObjectSelection::getReferences() const
{
    return m_references;
}

bool KeyObjectSelection::references(const QString &sopInstanceUid) const
{
    for (const Reference &reference : m_references)
    {
        if (reference.sopInstanceUid == sopInstanceUid)
        {
            return true;
        }
    }

    return false;
}

QList<KeyObjectSelection::Reference> KeyObjectSelection::getUnresolvedReferences(const QSet<QString> &availableSopInstanceUids) const
{
    QList<Reference> unresolved;

    for (const Reference &reference : m_references)
    {
        if (!availableSopInstanceUids.contains(reference.sopInstanceUid))
        {
            unresolved << reference;
        }
    }

    return unresolved;
}

QString KeyObjectSelection::getDisplayText() const
{
    QString meaning = m_titleMeaning.isEmpty() ? m_titleCodeValue : m_titleMeaning;

    switch (m_kind)
    {
        case Kind::Selection:
            return meaning.isEmpty() ? QObject::tr("KEY IMAGE") : QObject::tr("KEY IMAGE") + ": " + meaning;

        case Kind::Rejection:
            return meaning.isEmpty() ? QObject::tr("REJECTED") : QObject::tr("REJECTED") + ": " + meaning;

        case Kind::Manifest:
            break;
    }

    return QString();
}

} // End namespace udg
