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

#include "structuredreportfillerstep.h"

#include "dicomtagreader.h"
#include "logging.h"
#include "patientfillerinput.h"
#include "series.h"
#include "structuredreport.h"
#include "study.h"

namespace udg {

StructuredReportFillerStep::StructuredReportFillerStep()
 : PatientFillerStep()
{
}

StructuredReportFillerStep::~StructuredReportFillerStep()
{
}

bool StructuredReportFillerStep::fillIndividually()
{
    const DICOMTagReader *dicomReader = m_input->getDICOMFile();

    if (!dicomReader || !StructuredReport::isStructuredReportClass(dicomReader->getValueAttributeAsQString(DICOMSOPClassUID)))
    {
        return true;
    }

    StructuredReport report = StructuredReport::fromDICOM(*dicomReader);
    Series *series = m_input->getCurrentSeries();
    Study *study = series ? series->getParentStudy() : nullptr;

    if (study && study->addStructuredReport(report))
    {
        INFO_LOG(QString("Structured Report %1 (%2)").arg(report.getSopInstanceUid()).arg(report.getTitle()));
    }

    return true;
}

} // End namespace udg
