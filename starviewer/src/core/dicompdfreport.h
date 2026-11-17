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

#ifndef UDGDICOMPDFREPORT_H
#define UDGDICOMPDFREPORT_H

#include <QByteArray>
#include <QString>

namespace udg {

/**
    A simple report saved as a DICOM object: the text of the report (a title, the findings, the impression and who wrote it) is made into a PDF page or pages (A4), and the PDF is put in an
    Encapsulated PDF Storage object (modality DOC) of the same patient and study as the image the report is about, in a new series. The object says "BurnedInAnnotation YES" (the PDF is the text)
    and the PDF has the line "Draft - not for diagnostic use", because the program is not for diagnosis. It does not send the object anywhere: it writes a file.
 */
class DicomPdfReport {
public:
    struct Content {
        QString title;
        QString findings;
        QString impression;
        QString author;
    };

    /// The PDF of the report, with the patient line (name, ID, study date and description) on top. Empty if there is nothing to write.
    static QByteArray renderPdf(const Content &content, const QString &patientLine);

    /// Writes the DICOM object with the report, for the patient and the study of the DICOM file at sourcePath, in outputPath. It does not replace the source. False and the reason otherwise.
    static bool write(const QString &sourcePath, const Content &content, const QString &outputPath, QString *errorMessage = nullptr);

    /// True if the report has something to write (a title, the findings or the impression)
    static bool hasText(const Content &content);
};

} // End namespace udg

#endif
