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

#include "dicomstoresender.h"

#include <QFileInfo>
#include <QMap>
#include <QPair>

#include <dcmtk/dcmdata/dctk.h>
#include <dcmtk/dcmnet/scu.h>

namespace udg {

bool DicomStoreSender::send(const RoutingRules::Destination &destination, const QStringList &files, int *sent, QStringList *errors)
{
    *sent = 0;

    // The class and the transfer syntax of each file
    struct Item {
        QString file;
        QString sopClass;
        QString syntax;
    };

    QList<Item> items;
    QMap<QPair<QString, QString>, int> contexts;

    for (const QString &file : files)
    {
        DcmFileFormat format;

        if (format.loadFileUntilTag(file.toLocal8Bit().constData(), EXS_Unknown, EGL_noChange, DCM_MaxReadLength, ERM_autoDetect, DCM_PixelData).bad())
        {
            if (errors)
            {
                *errors << QString("%1: it could not be read as DICOM.").arg(QFileInfo(file).fileName());
            }

            continue;
        }

        OFString sop, syntax;
        format.getMetaInfo()->findAndGetOFString(DCM_MediaStorageSOPClassUID, sop);

        if (sop.empty())
        {
            format.getDataset()->findAndGetOFString(DCM_SOPClassUID, sop);
        }

        format.getMetaInfo()->findAndGetOFString(DCM_TransferSyntaxUID, syntax);

        if (syntax.empty())
        {
            syntax = UID_LittleEndianExplicitTransferSyntax;
        }

        Item item{file, QString::fromLatin1(sop.c_str()), QString::fromLatin1(syntax.c_str())};
        items << item;
        contexts[qMakePair(item.sopClass, item.syntax)] = 0;
    }

    if (items.isEmpty())
    {
        return true;
    }

    DcmSCU scu;
    scu.setAETitle(destination.callingAet.toLatin1().constData());
    scu.setPeerAETitle(destination.calledAet.toLatin1().constData());
    scu.setPeerHostName(destination.host.toLatin1().constData());
    scu.setPeerPort(static_cast<Uint16>(destination.port));
    scu.setDIMSETimeout(30);
    scu.setACSETimeout(15);

    for (auto it = contexts.constBegin(); it != contexts.constEnd(); ++it)
    {
        OFList<OFString> syntaxes;
        syntaxes.push_back(it.key().second.toLatin1().constData());
        scu.addPresentationContext(it.key().first.toLatin1().constData(), syntaxes);
    }

    OFCondition condition = scu.initNetwork();

    if (condition.good())
    {
        condition = scu.negotiateAssociation();
    }

    if (condition.bad())
    {
        if (errors)
        {
            *errors << QString("The connection to %1:%2 (AE title %3) failed: %4").arg(destination.host).arg(destination.port).arg(destination.calledAet, QString::fromLatin1(condition.text()));
        }

        return false;
    }

    for (const Item &item : items)
    {
        T_ASC_PresentationContextID id = scu.findPresentationContextID(item.sopClass.toLatin1().constData(), item.syntax.toLatin1().constData());

        if (id == 0)
        {
            if (errors)
            {
                *errors << QString("%1: the PACS did not accept this type of image (%2).").arg(QFileInfo(item.file).fileName(), item.sopClass);
            }

            continue;
        }

        Uint16 status = 0;
        OFCondition sentCondition = scu.sendSTORERequest(id, item.file.toLocal8Bit().constData(), nullptr, status);

        // Success is 0000; the warnings (B000, B006, B007) mean that the image was stored with a change
        if (sentCondition.good() && (status == 0 || (status & 0xF000) == 0xB000))
        {
            (*sent)++;
        }
        else if (errors)
        {
            *errors << QString("%1: the PACS answered with status %2 (%3).").arg(QFileInfo(item.file).fileName()).arg(status, 4, 16, QChar('0')).arg(QString::fromLatin1(sentCondition.text()));
        }
    }

    scu.releaseAssociation();
    return true;
}

} // End namespace udg
