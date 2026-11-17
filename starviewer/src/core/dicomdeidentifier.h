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

#ifndef UDGDICOMDEIDENTIFIER_H
#define UDGDICOMDEIDENTIFIER_H

#include <QHash>
#include <QRect>
#include <QString>
#include <QStringList>

class DcmDataset;
class DcmItem;

namespace udg {

/**
    De-identification of DICOM objects following the Basic Application Level Confidentiality Profile of DICOM (PS 3.15, annex E), with some of its options:
    - the attributes that identify the patient, the institution or the people are removed or emptied, the other dates and times and the descriptions of the study and the series are removed too;
    - every UID that is not a standard one (SOP classes, transfer syntaxes) is replaced by a new one, the same for the same original in the whole run, so that the studies keep their structure;
    - the private attributes are removed, in the sequences too;
    - the object is marked as de-identified (Patient Identity Removed and De-identification Method).
    The options keep what the Basic Profile removes: the dates and times (Retain Longitudinal Temporal Information, Full Dates), the dates moved by a number of days that is the same for each patient
    (Modified Dates), the sex, age, size and weight (Retain Patient Characteristics), the device identity, the UIDs, and the descriptions that do not have the name or the ID of the patient (Clean Descriptors).
    The pixels are not looked at: see DicomPixelBlackout, and the warnings that process() gives for the objects that may have text in them.
    It does not keep a table that gives back the original identity.
 */
class DicomDeidentifier {
public:
    struct Options {
        bool retainDates = false;
        bool retainModifiedDates = false;
        bool retainPatientCharacteristics = false;
        bool retainDeviceIdentity = false;
        bool retainUids = false;
        bool cleanDescriptors = false;
        /// Says in the object that the pixels were cleaned (set by whoever blanked the text in them, as DeidentificationJob does)
        bool pixelsCleaned = false;
        /// The name given to the patient, and the ID (an empty one is a code made from the original ID, the same for the same original)
        QString patientName = "ANONYMOUS";
        QString patientId;
    };

    struct Report {
        int removed = 0;
        int emptied = 0;
        int replaced = 0;
        int uidsReplaced = 0;
        int privateRemoved = 0;
        int datesShifted = 0;
        QStringList warnings;
    };

    /// The salt makes the new UIDs and codes different from the ones of another run; with the same salt a run gives the same ones (useful for tests). An empty salt is a random one.
    explicit DicomDeidentifier(const Options &options = Options(), const QString &salt = QString());

    /// De-identifies the dataset in place (all its sequences too).
    Report process(DcmDataset *dataset);

    /// The new UID for an original one in this run (made when it is asked for the first time)
    QString mappedUid(const QString &original);

    /// The number of days that the dates of a patient are moved (a number from -365 to -30), the same for the same original ID with the same salt
    int dateShiftDaysFor(const QString &originalPatientId) const;

    /// The attributes of the Basic Profile that are removed or emptied as (group, element): the list the engine works with, to see it or to test it
    static QList<QPair<quint16, quint16>> removedAttributes();

private:
    void processItem(DcmItem *item, Report &report, int shiftDays, const QString &originalName, const QString &originalId, bool topLevel);
    QString hashHex(const QString &text, int digits) const;

    Options m_options;
    QString m_salt;
    QHash<QString, QString> m_uidMap;
};

} // End namespace udg

#endif
