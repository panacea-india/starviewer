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

#ifndef UDGSTRUCTUREDREPORTRENDERER_H
#define UDGSTRUCTUREDREPORTRENDERER_H

#include <QString>

namespace udg {

/**
    Renders a DICOM Structured Report file as HTML, with the DCMTK library. The HTML is plain (HTML 3.2, no scripts and no style sheets), the text
    of the report is escaped by the library, and links to other content are not followed by the interface that shows it.
 */
class StructuredReportRenderer {
public:
    /// Returns the HTML of the report in the given file. If it cannot be read or rendered, it returns an empty string and, if error is not null,
    /// says why.
    static QString toHtml(const QString &path, QString *error = nullptr);
};

} // End namespace udg

#endif
