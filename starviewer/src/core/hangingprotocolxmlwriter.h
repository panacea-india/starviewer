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

#ifndef UDGHANGINGPROTOCOLXMLWRITER_H
#define UDGHANGINGPROTOCOLXMLWRITER_H

#include <QString>

namespace udg {

class HangingProtocol;

/**
    Writes a hanging protocol as the XML (version 2) that HangingProtocolXMLReader reads, so that what is read and written again is the same protocol. Only what is not the default
    is written. If two image sets use the same restriction number for different restrictions, the protocol cannot be written as a file (the numbers of a file are shared).
 */
class HangingProtocolXMLWriter {
public:
    /// The XML text of the protocol, or an empty string (and the reason in errorMessage if it is given) if it cannot be written.
    static QString toXml(const HangingProtocol &hangingProtocol, QString *errorMessage = nullptr);

    /// Writes the XML to the file (replacing it) and returns true if all was written.
    static bool writeFile(const HangingProtocol &hangingProtocol, const QString &path, QString *errorMessage = nullptr);
};

} // End namespace udg

#endif
