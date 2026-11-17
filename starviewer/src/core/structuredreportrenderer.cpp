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

#include "structuredreportrenderer.h"

#include <sstream>

#include <QObject>

#include <dcmtk/config/osconfig.h>
#include <dcmtk/dcmdata/dcfilefo.h>
#include <dcmtk/dcmsr/dsrdoc.h>

namespace udg {

QString StructuredReportRenderer::toHtml(const QString &path, QString *error)
{
    auto fail = [error](const QString &reason) {
        if (error)
        {
            *error = reason;
        }
        return QString();
    };

    DcmFileFormat file;
    OFCondition status = file.loadFile(path.toLocal8Bit().constData());

    if (status.bad())
    {
        return fail(QObject::tr("The file could not be read: %1").arg(status.text()));
    }

    DSRDocument document;
    status = document.read(*file.getDataset());

    if (status.bad())
    {
        return fail(QObject::tr("The report could not be read: %1").arg(status.text()));
    }

    std::ostringstream html;
    status = document.renderHTML(html, DSRTypes::HF_HTML32Compatibility | DSRTypes::HF_convertNonASCIICharacters | DSRTypes::HF_renderDcmtkFootnote);

    if (status.bad())
    {
        return fail(QObject::tr("The report could not be shown: %1").arg(status.text()));
    }

    // With HF_convertNonASCIICharacters the library writes only ASCII, so Latin 1 is a safe way to turn the bytes into text
    return QString::fromLatin1(html.str().c_str());
}

} // End namespace udg
