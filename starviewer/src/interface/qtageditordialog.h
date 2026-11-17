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

#ifndef UDGQTAGEDITORDIALOG_H
#define UDGQTAGEDITORDIALOG_H

#include <QDialog>

#include "dicomtageditor.h"

class QLabel;
class QPushButton;
class QTableWidget;

namespace udg {

/// The dialog to look at and change the attributes of a DICOM file (see DicomTagEditor): open a file, double click a value to change it, remove or add attributes, and save in another file.
class QTagEditorDialog : public QDialog {
    Q_OBJECT

public:
    explicit QTagEditorDialog(QWidget *parent = nullptr);

private slots:
    void open();
    void edit(int row);
    void removeSelected();
    void add();
    void save();
    void updateButtons();

private:
    void fill();

    DicomTagEditor m_editor;
    QList<DicomTagEditor::Entry> m_entries;
    QTableWidget *m_table;
    QLabel *m_file;
    QPushButton *m_remove;
    QPushButton *m_add;
    QPushButton *m_save;
};

} // End namespace udg

#endif
