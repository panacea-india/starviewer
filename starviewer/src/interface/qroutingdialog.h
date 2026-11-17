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

#ifndef UDGQROUTINGDIALOG_H
#define UDGQROUTINGDIALOG_H

#include <QDialog>

#include "routingjob.h"
#include "routingrules.h"

class QLineEdit;
class QPlainTextEdit;
class QPushButton;

namespace udg {

/// The dialog to send the DICOM files of a folder to PACS following routing rules: the rules are a JSON text that is checked and saved in the user data folder, "Show the plan" says what would go where
/// without sending anything, and "Send" sends after a confirmation (see RoutingJob).
class QRoutingDialog : public QDialog {
    Q_OBJECT

public:
    explicit QRoutingDialog(QWidget *parent = nullptr);

private slots:
    void browse();
    void check();
    void saveRules();
    void showPlan();
    void sendFiles();

private:
    bool readRules(RoutingRules *rules);
    QString rulesPath() const;

    QLineEdit *m_folder;
    QPlainTextEdit *m_rules;
    QPlainTextEdit *m_log;
    RoutingJob::Plan m_plan;
    RoutingRules m_plannedRules;
    bool m_hasPlan;
};

} // End namespace udg

#endif
