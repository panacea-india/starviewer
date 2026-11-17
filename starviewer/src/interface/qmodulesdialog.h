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

#ifndef UDGQMODULESDIALOG_H
#define UDGQMODULESDIALOG_H

#include <QDialog>

#include "externalmodule.h"

class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;

namespace udg {

/// Lists the modules (separate programs, see doc/extension-boundary.md) and runs one on a folder of images, writing in another folder. It says what the module declares (research only, network) before
/// it runs, shows the progress and the log, and lists the outputs with a button to open their folder.
class QModulesDialog : public QDialog {
    Q_OBJECT

public:
    explicit QModulesDialog(QWidget *parent = nullptr);

private slots:
    void showSelected();
    void browseInput();
    void browseOutput();
    void run();
    void openOutput();

private:
    QStringList moduleDirectories() const;

    QList<ModuleManifest> m_modules;
    QComboBox *m_choice;
    QLabel *m_description;
    QLineEdit *m_input;
    QLineEdit *m_output;
    QPlainTextEdit *m_parameters;
    QProgressBar *m_progress;
    QPlainTextEdit *m_log;
    QPushButton *m_run;
    QPushButton *m_open;
    bool m_running;
    bool m_stop;
};

} // End namespace udg

#endif
