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

#ifndef UDGQSHORTCUTSDIALOG_H
#define UDGQSHORTCUTSDIALOG_H

#include <QDialog>
#include <QStringList>

class QKeySequenceEdit;
class QLabel;
class QTableWidget;

namespace udg {

/// The dialog where the user changes the keyboard shortcuts: a table with the action, the key to press, a button to go back to the default, and a warning for the keys that two actions share.
class QShortcutsDialog : public QDialog {
    Q_OBJECT

public:
    explicit QShortcutsDialog(QWidget *parent = nullptr);

private slots:
    void updateConflicts();
    void apply();

private:
    QTableWidget *m_table;
    QLabel *m_conflicts;
    QStringList m_keys;
    QList<QKeySequenceEdit*> m_editors;
};

} // End namespace udg

#endif
