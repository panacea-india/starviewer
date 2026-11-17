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

#include "dicompdfreport.h"

#include <QBuffer>
#include <QDateTime>
#include <QFileInfo>
#include <QPageSize>
#include <QPdfWriter>
#include <QTextDocument>

#include <dcmtk/dcmdata/dctk.h>

namespace udg {

namespace {

bool fail(QString *errorMessage, const QString &reason)
{
    if (errorMessage)
    {
        *errorMessage = reason;
    }

    return false;
}

QString html(const QString &text)
{
    return text.toHtmlEscaped().replace("\n", "<br>");
}

QString value(DcmItem *item, const DcmTagKey &key)
{
    OFString s;
    item->findAndGetOFString(key, s);
    return QString::fromLatin1(s.c_str());
}

}

bool DicomPdfReport::hasText(const Content &content)
{
    return !content.title.trimmed().isEmpty() || !content.findings.trimmed().isEmpty() || !content.impression.trimmed().isEmpty();
}

QByteArray DicomPdfReport::renderPdf(const Content &content, const QString &patientLine)
{
    if (!hasText(content))
    {
        return QByteArray();
    }

    QString page = "<div style='font-family: Arial; font-size: 11pt;'>";
    page += "<p style='color:#a00000;'><b>Draft - not for diagnostic use</b></p>";

    if (!content.title.trimmed().isEmpty())
    {
        page += "<h2>" + html(content.title.trimmed()) + "</h2>";
    }

    if (!patientLine.trimmed().isEmpty())
    {
        page += "<p style='color:#444444;'>" + html(patientLine.trimmed()) + "</p><hr>";
    }

    if (!content.findings.trimmed().isEmpty())
    {
        page += "<h3>Findings</h3><p>" + html(content.findings.trimmed()) + "</p>";
    }

    if (!content.impression.trimmed().isEmpty())
    {
        page += "<h3>Impression</h3><p>" + html(content.impression.trimmed()) + "</p>";
    }

    page += "<hr><p style='color:#444444;'>";

    if (!content.author.trimmed().isEmpty())
    {
        page += "Written by " + html(content.author.trimmed()) + ", ";
    }

    page += QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm") + "</p></div>";

    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    QPdfWriter writer(&buffer);
    writer.setPageSize(QPageSize(QPageSize::A4));
    writer.setPageMargins(QMarginsF(20, 20, 20, 20), QPageLayout::Millimeter);
    writer.setTitle(content.title.trimmed());
    writer.setCreator("Starviewer");

    QTextDocument document;
    document.setHtml(page);
    document.print(&writer);
    buffer.close();
    return bytes;
}

bool DicomPdfReport::write(const QString &sourcePath, const Content &content, const QString &outputPath, QString *errorMessage)
{
    if (!hasText(content))
    {
        return fail(errorMessage, "The report is empty.");
    }

    if (outputPath.trimmed().isEmpty() || QFileInfo(outputPath).absoluteFilePath().compare(QFileInfo(sourcePath).absoluteFilePath(), Qt::CaseInsensitive) == 0)
    {
        return fail(errorMessage, "The report needs its own file: it is not written over the image.");
    }

    DcmFileFormat source;

    if (source.loadFile(sourcePath.toLocal8Bit().constData()).bad())
    {
        return fail(errorMessage, "The image the report is about could not be read as DICOM.");
    }

    DcmDataset *from = source.getDataset();
    QString studyUid = value(from, DCM_StudyInstanceUID);

    if (studyUid.isEmpty())
    {
        return fail(errorMessage, "The image has no study.");
    }

    QString patientLine = QString("%1   ID %2   Study %3 %4").arg(value(from, DCM_PatientName).replace('^', ' '), value(from, DCM_PatientID), value(from, DCM_StudyDate), value(from, DCM_StudyDescription)).trimmed();
    QByteArray pdf = renderPdf(content, patientLine);

    if (pdf.isEmpty())
    {
        return fail(errorMessage, "The PDF could not be made.");
    }

    DcmFileFormat file;
    DcmDataset *ds = file.getDataset();

    // The patient and the study of the image
    const DcmTagKey copied[] = {DCM_PatientName, DCM_PatientID, DCM_PatientBirthDate, DCM_PatientSex, DCM_StudyInstanceUID, DCM_StudyDate, DCM_StudyTime, DCM_AccessionNumber,
                                DCM_ReferringPhysicianName, DCM_StudyID, DCM_StudyDescription, DCM_SpecificCharacterSet};

    for (const DcmTagKey &key : copied)
    {
        OFString s;

        if (from->findAndGetOFStringArray(key, s).good())
        {
            ds->putAndInsertString(DcmTag(key), s.c_str());
        }
        else if (key != DCM_SpecificCharacterSet && key != DCM_StudyDescription)
        {
            ds->putAndInsertString(DcmTag(key), "");
        }
    }

    char uid[100];
    ds->putAndInsertString(DCM_SOPClassUID, UID_EncapsulatedPDFStorage);
    ds->putAndInsertString(DCM_SOPInstanceUID, dcmGenerateUniqueIdentifier(uid, SITE_INSTANCE_UID_ROOT));
    ds->putAndInsertString(DCM_SeriesInstanceUID, dcmGenerateUniqueIdentifier(uid, SITE_SERIES_UID_ROOT));
    ds->putAndInsertString(DCM_Modality, "DOC");
    ds->putAndInsertString(DCM_ConversionType, "WSD");
    ds->putAndInsertString(DCM_SeriesNumber, "9000");
    ds->putAndInsertString(DCM_InstanceNumber, "1");
    ds->putAndInsertString(DCM_Manufacturer, "Starviewer");
    ds->putAndInsertString(DCM_SeriesDescription, "Report");
    ds->putAndInsertString(DCM_BurnedInAnnotation, "YES");
    ds->putAndInsertString(DCM_DocumentTitle, content.title.trimmed().left(64).toLatin1().constData());
    ds->putAndInsertString(DCM_MIMETypeOfEncapsulatedDocument, "application/pdf");

    QDateTime now = QDateTime::currentDateTime();
    ds->putAndInsertString(DCM_SeriesDate, now.toString("yyyyMMdd").toLatin1().constData());
    ds->putAndInsertString(DCM_SeriesTime, now.toString("HHmmss").toLatin1().constData());
    ds->putAndInsertString(DCM_ContentDate, now.toString("yyyyMMdd").toLatin1().constData());
    ds->putAndInsertString(DCM_ContentTime, now.toString("HHmmss").toLatin1().constData());
    ds->putAndInsertString(DCM_InstanceCreationDate, now.toString("yyyyMMdd").toLatin1().constData());
    ds->putAndInsertString(DCM_InstanceCreationTime, now.toString("HHmmss").toLatin1().constData());

    // The document: an even number of bytes, padded with a zero
    if (pdf.size() % 2 != 0)
    {
        pdf.append('\0');
    }

    if (ds->putAndInsertUint8Array(DCM_EncapsulatedDocument, reinterpret_cast<const Uint8*>(pdf.constData()), static_cast<unsigned long>(pdf.size())).bad())
    {
        return fail(errorMessage, "The PDF could not be put in the object.");
    }

    OFCondition saved = file.saveFile(outputPath.toLocal8Bit().constData(), EXS_LittleEndianExplicit, EET_ExplicitLength, EGL_recalcGL, EPD_noChange, 0, 0, EWM_updateMeta);

    if (saved.bad())
    {
        return fail(errorMessage, QString::fromLatin1(saved.text()));
    }

    return true;
}

} // End namespace udg
