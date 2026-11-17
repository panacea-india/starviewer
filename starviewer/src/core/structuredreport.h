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

#ifndef UDGSTRUCTUREDREPORT_H
#define UDGSTRUCTUREDREPORT_H

#include <QString>

namespace udg {

class DICOMTagReader;

/**
    A DICOM Structured Report (SR), as listed in a study: which document it is and where its file is. It is shown by the SR extension, which
    renders the file. Key Object Selection Documents are SR too, but they have their own class (KeyObjectSelection) and are not listed here.
 */
class StructuredReport {
public:
    /// Creates a null object.
    StructuredReport();

    /// Returns true if the SOP class is one of a Structured Report that can be shown (not a Key Object Selection Document).
    static bool isStructuredReportClass(const QString &sopClassUid);

    /// Reads the identification of the document of the given reader. If the file is not a Structured Report, it returns a null object and,
    /// if error is not null, says why.
    static StructuredReport fromDICOM(const DICOMTagReader &reader, QString *error = nullptr);

    bool isNull() const;

    const QString& getSopInstanceUid() const;
    const QString& getSopClassUid() const;
    /// Returns the title of the document, like "Radiology Report", or an empty string if it has none.
    const QString& getTitle() const;
    /// Returns the completion flag: COMPLETE or PARTIAL.
    const QString& getCompletionFlag() const;
    /// Returns the verification flag: VERIFIED or UNVERIFIED.
    const QString& getVerificationFlag() const;
    const QString& getContentDate() const;
    /// Returns the path of the file of the document.
    const QString& getPath() const;

    /// Returns true if the document is complete and verified. A partial or unverified report is not final, and the viewer says so.
    bool isFinal() const;

private:
    QString m_sopInstanceUid;
    QString m_sopClassUid;
    QString m_title;
    QString m_completionFlag;
    QString m_verificationFlag;
    QString m_contentDate;
    QString m_path;
    bool m_null;
};

} // End namespace udg

#endif
