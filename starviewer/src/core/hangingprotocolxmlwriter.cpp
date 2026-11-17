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

#include "hangingprotocolxmlwriter.h"

#include <QFile>
#include <QMap>
#include <QXmlStreamWriter>

#include "hangingprotocol.h"
#include "hangingprotocoldisplayset.h"
#include "hangingprotocolimageset.h"
#include "hangingprotocolimagesetrestriction.h"
#include "hangingprotocolimagesetrestrictionexpression.h"
#include "hangingprotocollayout.h"
#include "hangingprotocolmask.h"
#include "patientorientation.h"

namespace udg {

namespace {

// The shortest text that reads back as the same number
QString number(double value)
{
    for (int precision = 6; precision < 17; precision++)
    {
        QString text = QString::number(value, 'g', precision);

        if (text.toDouble() == value)
        {
            return text;
        }
    }

    return QString::number(value, 'g', 17);
}

QString typeName(HangingProtocolImageSet::Type type)
{
    switch (type)
    {
        case HangingProtocolImageSet::Type::Image:
            return "image";

        case HangingProtocolImageSet::Type::Fusion:
            return "fusion";

        case HangingProtocolImageSet::Type::Series:
        default:
            return "series";
    }
}

}

QString HangingProtocolXMLWriter::toXml(const HangingProtocol &hangingProtocol, QString *errorMessage)
{
    // The restrictions of the file are shared by all the image sets: the same number has to be the same restriction
    QMap<int, HangingProtocolImageSetRestriction> restrictions;

    for (HangingProtocolImageSet *imageSet : hangingProtocol.getImageSets())
    {
        for (const HangingProtocolImageSetRestrictionExpression &expression : imageSet->getRestrictionExpressions())
        {
            const QMap<int, HangingProtocolImageSetRestriction> &used = expression.getRestrictions();

            for (auto it = used.constBegin(); it != used.constEnd(); ++it)
            {
                if (restrictions.contains(it.key()) && !(restrictions[it.key()] == it.value()))
                {
                    if (errorMessage)
                    {
                        *errorMessage = QString("The restriction number %1 is used for two different restrictions.").arg(it.key());
                    }

                    return QString();
                }

                restrictions[it.key()] = it.value();
            }
        }
    }

    QString text;
    QXmlStreamWriter xml(&text);
    xml.setAutoFormatting(true);
    xml.setAutoFormattingIndent(4);
    xml.writeStartDocument();
    xml.writeStartElement("hangingProtocol");
    xml.writeAttribute("name", hangingProtocol.getName());
    xml.writeAttribute("version", "2");
    xml.writeTextElement("hangingProtocolName", hangingProtocol.getName());
    xml.writeTextElement("numberScreens", QString::number(hangingProtocol.getHangingProtocolLayout()->getNumberOfScreens()));

    for (const QString &protocol : hangingProtocol.getHangingProtocolMask()->getProtocolList())
    {
        xml.writeTextElement("protocol", protocol);
    }

    if (!hangingProtocol.getInstitutionsRegularExpression().pattern().isEmpty())
    {
        xml.writeTextElement("institutions", hangingProtocol.getInstitutionsRegularExpression().pattern());
    }

    xml.writeTextElement("priority", number(hangingProtocol.getPriority()));

    if (hangingProtocol.isStrict())
    {
        xml.writeTextElement("strictness", "yes");
    }

    if (hangingProtocol.getAllDifferent())
    {
        xml.writeTextElement("allDifferent", "yes");
    }

    if (!hangingProtocol.getIconType().isEmpty())
    {
        xml.writeTextElement("iconType", hangingProtocol.getIconType());
    }

    if (hangingProtocol.getNumberOfPriors() != 0)
    {
        xml.writeTextElement("numberOfPriors", QString::number(hangingProtocol.getNumberOfPriors()));
    }

    for (auto it = restrictions.constBegin(); it != restrictions.constEnd(); ++it)
    {
        xml.writeStartElement("restriction");
        xml.writeAttribute("identifier", QString::number(it.key()));
        xml.writeTextElement("selectorAttribute", it.value().getSelectorAttribute());
        xml.writeTextElement("selectorValue", it.value().getSelectorValue());
        xml.writeEndElement();
    }

    for (HangingProtocolImageSet *imageSet : hangingProtocol.getImageSets())
    {
        xml.writeStartElement("imageSet");
        xml.writeAttribute("identifier", QString::number(imageSet->getIdentifier()));
        xml.writeTextElement("type", typeName(imageSet->getType()));

        // A series or image set with an expression that is always true has none in the file; a fusion has two, always
        for (const HangingProtocolImageSetRestrictionExpression &expression : imageSet->getRestrictionExpressions())
        {
            QString expressionText = expression.getExpression();

            if (!expressionText.isEmpty() || imageSet->getType() == HangingProtocolImageSet::Type::Fusion)
            {
                xml.writeTextElement("restrictionExpression", expressionText);
            }
        }

        if (imageSet->getAbstractPriorValue() != 0)
        {
            xml.writeTextElement("abstractPriorValue", QString::number(imageSet->getAbstractPriorValue()));
        }

        if (imageSet->getImageNumberInStudyModality() != -1)
        {
            xml.writeTextElement("imageNumberInStudyModality", QString::number(imageSet->getImageNumberInStudyModality()));
        }

        xml.writeEndElement();
    }

    for (HangingProtocolDisplaySet *displaySet : hangingProtocol.getDisplaySets())
    {
        xml.writeStartElement("displaySet");
        xml.writeAttribute("identifier", QString::number(displaySet->getIdentifier()));

        if (displaySet->getImageSet())
        {
            xml.writeTextElement("imageSetNumber", QString::number(displaySet->getImageSet()->getIdentifier()));
        }

        if (!displaySet->getPosition().isEmpty())
        {
            xml.writeTextElement("position", displaySet->getPosition());
        }

        QString orientation = displaySet->getPatientOrientation().getDICOMFormattedPatientOrientation();

        if (!orientation.isEmpty())
        {
            xml.writeTextElement("patientOrientation", orientation);
        }

        if (!displaySet->getReconstruction().isEmpty())
        {
            xml.writeTextElement("reconstruction", displaySet->getReconstruction());
        }

        if (displaySet->getPhase() != -1)
        {
            xml.writeTextElement("phase", QString::number(displaySet->getPhase()));
        }

        if (displaySet->getSlice() != -1)
        {
            xml.writeTextElement("imageNumber", QString::number(displaySet->getSlice()));
        }

        if (!displaySet->getIconType().isEmpty())
        {
            xml.writeTextElement("iconType", displaySet->getIconType());
        }

        if (!displaySet->getAlignment().isEmpty())
        {
            xml.writeTextElement("alignment", displaySet->getAlignment());
        }

        if (!displaySet->getToolActivation().isEmpty())
        {
            xml.writeTextElement("toolActivation", displaySet->getToolActivation());
        }

        if (displaySet->getWindowWidth() != -1 || displaySet->getWindowCenter() != -1)
        {
            xml.writeStartElement("windowLevel");
            xml.writeTextElement("width", number(displaySet->getWindowWidth()));
            xml.writeTextElement("center", number(displaySet->getWindowCenter()));
            xml.writeEndElement();
        }

        xml.writeEndElement();
    }

    xml.writeEndElement();
    xml.writeEndDocument();

    return text;
}

bool HangingProtocolXMLWriter::writeFile(const HangingProtocol &hangingProtocol, const QString &path, QString *errorMessage)
{
    QString text = toXml(hangingProtocol, errorMessage);

    if (text.isEmpty())
    {
        return false;
    }

    QFile file(path);

    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
    {
        if (errorMessage)
        {
            *errorMessage = file.errorString();
        }

        return false;
    }

    QByteArray bytes = text.toUtf8();

    if (file.write(bytes) != bytes.size())
    {
        if (errorMessage)
        {
            *errorMessage = file.errorString();
        }

        return false;
    }

    return true;
}

} // End namespace udg
