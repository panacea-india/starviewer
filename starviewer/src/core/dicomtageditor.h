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

#ifndef UDGDICOMTAGEDITOR_H
#define UDGDICOMTAGEDITOR_H

#include <memory>

#include <QList>
#include <QString>

class DcmFileFormat;
class DcmItem;

namespace udg {

/**
    Edits the attributes of a DICOM file: it lists them (also the ones inside sequences), changes the value of one, removes one or adds one, checking that the value is right for the type (VR) of the
    attribute (a date is a date, a UID has digits and dots, the length is not too long, and so on), and saves the result in another file (it never writes over the file that was opened).
    An attribute is named by its path: "0010,0010" at the top, or "0008,1140[0]/0008,1155" for an attribute inside the first item of a sequence. The attributes with the pixels and the binary values
    are shown but not edited. Changing the UIDs or the type of image can make the file useless for other programs: that is up to the user.
 */
class DicomTagEditor {
public:
    struct Entry {
        QString path;
        QString name;
        QString vr;
        QString value;
        int depth = 0;
        bool isSequence = false;
        bool isPrivate = false;
        bool editable = false;
    };

    DicomTagEditor();
    ~DicomTagEditor();

    bool load(const QString &path, QString *errorMessage = nullptr);
    bool isLoaded() const;
    bool isModified() const;

    /// All the attributes in order, the ones in sequences after the sequence and with their depth
    QList<Entry> entries() const;

    /// The value of the attribute at the path (empty if there is none), and whether it exists
    bool exists(const QString &path) const;
    QString value(const QString &path) const;

    /// Changes the value (several values are separated by a backslash). Only existing attributes that can be edited. False, with the reason, if the value is not valid for its VR.
    bool setValue(const QString &path, const QString &value, QString *errorMessage = nullptr);

    /// Removes an attribute (and what is inside it)
    bool remove(const QString &path, QString *errorMessage = nullptr);

    /// Adds an attribute at the top level or in an item of a sequence (the path of the item, such as "0008,1140[0]", is given as the parent; an empty parent is the top level). The VR is the one of the
    /// DICOM dictionary; an attribute that is not in it (a private one) is refused. The attribute must not exist already.
    bool add(const QString &parentPath, quint16 group, quint16 element, const QString &value, QString *errorMessage = nullptr);

    /// Saves in another file. It fails, and writes nothing, if the file is the one that was opened.
    bool save(const QString &path, QString *errorMessage = nullptr);

    /// Checks a value for a VR (such as "DA"): says what is wrong with it. Empty values are valid (the attribute has no value).
    static bool validate(const QString &vr, const QString &value, QString *errorMessage = nullptr);

    /// The path of the attribute for a group and an element, such as "0010,0010"
    static QString pathFor(quint16 group, quint16 element);

private:
    DcmItem* itemAt(const QString &parentPath, QString *errorMessage) const;

    std::unique_ptr<DcmFileFormat> m_file;
    QString m_loadedPath;
    bool m_modified;
};

} // End namespace udg

#endif
