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

#include "routingjob.h"

#include <QDir>
#include <QDirIterator>

#include <dcmtk/dcmdata/dctk.h>

namespace udg {

QMap<QString, QString> RoutingJob::readAttributes(const QString &file, bool *isDicom)
{
    QMap<QString, QString> attributes;
    DcmFileFormat format;

    if (format.loadFile(file.toLocal8Bit().constData()).bad())
    {
        if (isDicom)
        {
            *isDicom = false;
        }

        return attributes;
    }

    if (isDicom)
    {
        *isDicom = true;
    }

    static const QList<QPair<const char*, DcmTagKey>> tags = {
        {"Modality", DCM_Modality}, {"StudyDescription", DCM_StudyDescription}, {"SeriesDescription", DCM_SeriesDescription}, {"BodyPartExamined", DCM_BodyPartExamined},
        {"InstitutionName", DCM_InstitutionName}, {"StationName", DCM_StationName}, {"Manufacturer", DCM_Manufacturer}, {"ProtocolName", DCM_ProtocolName}, {"PatientSex", DCM_PatientSex},
        {"ImageType", DCM_ImageType}, {"SOPClassUID", DCM_SOPClassUID}, {"StudyDate", DCM_StudyDate}, {"ReferringPhysicianName", DCM_ReferringPhysicianName}};

    for (const auto &pair : tags)
    {
        OFString value;

        if (format.getDataset()->findAndGetOFStringArray(pair.second, value).good())
        {
            attributes[pair.first] = QString::fromLatin1(value.c_str());
        }
    }

    return attributes;
}

RoutingJob::Plan RoutingJob::plan(const QString &folder, const RoutingRules &rules)
{
    Plan plan;
    QStringList files;
    QDirIterator it(folder, QDir::Files, QDirIterator::Subdirectories);

    while (it.hasNext())
    {
        files << it.next();
    }

    files.sort();

    for (const QString &file : files)
    {
        bool dicom = false;
        QMap<QString, QString> attributes = readAttributes(file, &dicom);

        if (!dicom)
        {
            plan.notDicom++;
            continue;
        }

        PlanEntry entry;
        entry.file = file;
        QList<RoutingRules::Destination> destinations = rules.destinationsFor(attributes, &entry.rules);

        if (destinations.isEmpty())
        {
            plan.unrouted << file;
            continue;
        }

        for (const RoutingRules::Destination &d : destinations)
        {
            entry.destinations << d.name;
            plan.perDestination[d.name]++;
        }

        plan.routed << entry;
    }

    return plan;
}

RoutingJob::SendResult RoutingJob::send(const Plan &plan, const RoutingRules &rules, const Sender &sender)
{
    SendResult result;

    if (!sender)
    {
        result.message = "There is no way to send.";
        return result;
    }

    QMap<QString, QStringList> byDestination;

    for (const PlanEntry &entry : plan.routed)
    {
        for (const QString &name : entry.destinations)
        {
            byDestination[name] << entry.file;
        }
    }

    bool allConnected = true;

    for (auto it = byDestination.constBegin(); it != byDestination.constEnd(); ++it)
    {
        const RoutingRules::Destination *destination = rules.findDestination(it.key());

        if (!destination)
        {
            result.failed += it.value().size();
            result.errors << QString("%1: the destination does not exist.").arg(it.key());
            continue;
        }

        int sent = 0;
        QStringList errors;
        bool connected = sender(*destination, it.value(), &sent, &errors);
        result.sent += sent;
        result.failed += it.value().size() - sent;

        for (const QString &error : errors)
        {
            result.errors << QString("%1: %2").arg(it.key(), error);
        }

        if (!connected)
        {
            allConnected = false;
        }
    }

    result.success = result.failed == 0 && allConnected;
    result.message = result.success ? "Done." : "Some images were not sent.";
    return result;
}

} // End namespace udg
