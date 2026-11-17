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

#ifndef UDGQDEIDENTIFYDIALOG_H
#define UDGQDEIDENTIFYDIALOG_H

#include <QDialog>

class QCheckBox;
class QLineEdit;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;

namespace udg {

/// The dialog to de-identify the DICOM files of a folder into another one (see DeidentificationJob): the folders, the options of the profile, the name and ID that the patient gets, and the rectangles to blank
/// in the pixels (for text that is burned into the images). It writes a summary with the warnings of what it could not check.
class QDeidentifyDialog : public QDialog {
    Q_OBJECT

public:
    explicit QDeidentifyDialog(QWidget *parent = nullptr);

private slots:
    void browseInput();
    void browseOutput();
    void run();

private:
    QLineEdit *m_input;
    QLineEdit *m_output;
    QCheckBox *m_retainDates;
    QCheckBox *m_modifiedDates;
    QCheckBox *m_characteristics;
    QCheckBox *m_device;
    QCheckBox *m_uids;
    QCheckBox *m_descriptors;
    QLineEdit *m_name;
    QLineEdit *m_patientId;
    QLineEdit *m_blackout;
    QProgressBar *m_progress;
    QPlainTextEdit *m_log;
    QPushButton *m_run;
    bool m_running;
    bool m_stop;
};

} // End namespace udg

#endif
