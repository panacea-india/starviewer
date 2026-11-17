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

#ifndef UDGKEYOBJECTSELECTION_H
#define UDGKEYOBJECTSELECTION_H

#include <QList>
#include <QSet>
#include <QString>

namespace udg {

class DICOMTagReader;

/**
    A Key Object Selection Document (DICOM KO): a list of images that someone chose, with the reason, such as "Of Interest" or "For Teaching".
    The same document type also says that images are rejected, for example for quality reasons, and that must be shown, not hidden.

    This class reads the document and answers questions about it. It knows nothing about the interface.
    Only the images (IMAGE content items) are read. A reference to some frames of a multiframe image applies to the whole image here.
 */
class KeyObjectSelection {
public:
    /// What the document says about the images it refers to.
    enum class Kind {
        /// The images were chosen (Of Interest, For Teaching...).
        Selection,
        /// The images are rejected (for quality or patient safety reasons, wrong worklist entry, retention policy expired).
        Rejection,
        /// The document lists the content of a study or acquisition, not a choice (Manifest, Complete Study Content...).
        Manifest
    };

    struct Reference {
        QString sopClassUid;
        QString sopInstanceUid;
        /// The frames that are referenced, starting at 1. Empty if the whole instance is.
        QList<int> frames;
    };

    /// Creates a null object.
    KeyObjectSelection();

    /// Reads the Key Object Selection Document of the given reader. If the file is not one, or refers to no image, it returns a null object and,
    /// if error is not null, says why in it.
    static KeyObjectSelection fromDICOM(const DICOMTagReader &reader, QString *error = nullptr);

    bool isNull() const;

    const QString& getSopInstanceUid() const;
    /// Returns the value of the code of the document title, like "113000".
    const QString& getTitleCodeValue() const;
    /// Returns the text of the document title, like "Of Interest".
    const QString& getTitleMeaning() const;
    /// Returns the description that the author wrote, if any.
    const QString& getNote() const;
    Kind getKind() const;
    const QList<Reference>& getReferences() const;

    /// Returns true if the document refers to the instance with the given SOP Instance UID.
    bool references(const QString &sopInstanceUid) const;

    /// Returns the references to instances that are not in the given set of SOP Instance UIDs.
    QList<Reference> getUnresolvedReferences(const QSet<QString> &availableSopInstanceUids) const;

    /// Returns the text to show on an image that the document refers to, like "KEY IMAGE: Of Interest" or "REJECTED: Rejected for Quality Reasons".
    /// It is empty for a manifest, which does not choose images.
    QString getDisplayText() const;

    /// Returns the kind that a document title has. Unknown titles are selections, because the document still says that the images were chosen.
    static Kind getKindOfTitle(const QString &codeValue, const QString &codingSchemeDesignator);

private:
    QString m_sopInstanceUid;
    QString m_titleCodeValue;
    QString m_titleMeaning;
    QString m_note;
    Kind m_kind;
    QList<Reference> m_references;
    bool m_null;
};

} // End namespace udg

#endif
