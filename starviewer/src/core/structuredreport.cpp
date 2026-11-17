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

#include "structuredreport.h"

#include <QObject>
#include <QSet>

#include "dicomsequenceitem.h"
#include "dicomtagreader.h"

namespace udg {

StructuredReport::StructuredReport()
    : m_null(true)
{
}

bool StructuredReport::isStructuredReportClass(const QString &sopClassUid)
{
    static const QSet<QString> classes = {
        "1.2.840.10008.5.1.4.1.1.88.11",    // Basic Text SR
        "1.2.840.10008.5.1.4.1.1.88.22",    // Enhanced SR
        "1.2.840.10008.5.1.4.1.1.88.33",    // Comprehensive SR
        "1.2.840.10008.5.1.4.1.1.88.34",    // Comprehensive 3D SR
        "1.2.840.10008.5.1.4.1.1.88.40",    // Procedure Log
        "1.2.840.10008.5.1.4.1.1.88.50",    // Mammography CAD SR
        "1.2.840.10008.5.1.4.1.1.88.65",    // Chest CAD SR
        "1.2.840.10008.5.1.4.1.1.88.67",    // X-Ray Radiation Dose SR
        "1.2.840.10008.5.1.4.1.1.88.68",    // Radiopharmaceutical Radiation Dose SR
        "1.2.840.10008.5.1.4.1.1.88.69",    // Colon CAD SR
        "1.2.840.10008.5.1.4.1.1.88.70"     // Implantation Plan SR
    };

    return classes.contains(sopClassUid);
}

StructuredReport StructuredReport::fromDICOM(const DICOMTagReader &reader, QString *error)
{
    QString sopClass = reader.getValueAttributeAsQString(DICOMSOPClassUID);

    if (!isStructuredReportClass(sopClass))
    {
        if (error)
        {
            *error = QObject::tr("It is not a Structured Report.");
        }

        return StructuredReport();
    }

    StructuredReport report;
    report.m_sopClassUid = sopClass;
    report.m_sopInstanceUid = reader.getValueAttributeAsQString(DICOMSOPInstanceUID);
    report.m_completionFlag = reader.getValueAttributeAsQString(DICOMCompletionFlag).trimmed();
    report.m_verificationFlag = reader.getValueAttributeAsQString(DICOMVerificationFlag).trimmed();
    report.m_contentDate = reader.getValueAttributeAsQString(DICOMContentDate).trimmed();
    report.m_path = reader.getFileName();

    DICOMSequenceItem *title = reader.getFirstSequenceItem(DICOMConceptNameCodeSequence);

    if (title)
    {
        report.m_title = title->getValueAttributeAsQString(DICOMCodeMeaning).trimmed();
    }

    report.m_null = false;
    return report;
}

bool StructuredReport::isNull() const
{
    return m_null;
}

const QString& StructuredReport::getSopInstanceUid() const
{
    return m_sopInstanceUid;
}

const QString& StructuredReport::getSopClassUid() const
{
    return m_sopClassUid;
}

const QString& StructuredReport::getTitle() const
{
    return m_title;
}

const QString& StructuredReport::getCompletionFlag() const
{
    return m_completionFlag;
}

const QString& StructuredReport::getVerificationFlag() const
{
    return m_verificationFlag;
}

const QString& StructuredReport::getContentDate() const
{
    return m_contentDate;
}

const QString& StructuredReport::getPath() const
{
    return m_path;
}

bool StructuredReport::isFinal() const
{
    return m_completionFlag == "COMPLETE" && m_verificationFlag == "VERIFIED";
}

} // End namespace udg
