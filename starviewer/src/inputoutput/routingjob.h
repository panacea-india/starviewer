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

#ifndef UDGROUTINGJOB_H
#define UDGROUTINGJOB_H

#include <functional>

#include <QMap>
#include <QString>
#include <QStringList>

#include "routingrules.h"

namespace udg {

/**
    Applies RoutingRules to the DICOM files of a folder: first a plan (which files go to which destination and why, which go nowhere and which are not DICOM), then, if the user wants it, the sending.
    The sending is done by a function that is given (DicomStoreSender is the one that sends with DICOM C-STORE), so that the plan and the grouping are tested without a network. A file that matches rules
    with the same destination is sent once to it. Nothing is sent by the plan.
 */
class RoutingJob {
public:
    struct PlanEntry {
        QString file;
        QStringList rules;
        QStringList destinations;
    };

    struct Plan {
        QList<PlanEntry> routed;
        QStringList unrouted;
        int notDicom = 0;
        /// The number of files for each destination
        QMap<QString, int> perDestination;
    };

    struct SendResult {
        bool success = false;
        QString message;
        int sent = 0;
        int failed = 0;
        QStringList errors;
    };

    /// What a sender does for a destination: sends the files and says how many were sent, with the reasons for the ones that were not. False for a failure of all of them (the connection).
    typedef std::function<bool(const RoutingRules::Destination&, const QStringList &files, int *sent, QStringList *errors)> Sender;

    /// The attributes of the file that the rules can use (empty if it is not DICOM)
    static QMap<QString, QString> readAttributes(const QString &file, bool *isDicom = nullptr);

    static Plan plan(const QString &folder, const RoutingRules &rules);

    /// Sends what the plan says, a destination at a time
    static SendResult send(const Plan &plan, const RoutingRules &rules, const Sender &sender);
};

} // End namespace udg

#endif
