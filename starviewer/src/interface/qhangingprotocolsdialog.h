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

#ifndef UDGQHANGINGPROTOCOLSDIALOG_H
#define UDGQHANGINGPROTOCOLSDIALOG_H

#include <QDialog>

#include "hangingprotocolfiles.h"

class QLabel;
class QPushButton;
class QTableWidget;

namespace udg {

/// Lists the hanging protocols (the ones that come with the program and the ones of the user) and lets the user copy one to make a protocol of their own, change the name, priority,
/// modalities and the places of the viewers of their protocols, delete them and open their folder. The protocols are read when the program starts.
class QHangingProtocolsDialog : public QDialog {
    Q_OBJECT

public:
    explicit QHangingProtocolsDialog(QWidget *parent = nullptr);

private slots:
    void copySelected();
    void editSelected();
    void deleteSelected();
    void openFolder();
    void updateButtons();

private:
    void reload();
    const HangingProtocolFiles::Entry* selectedEntry() const;

private:
    QStringList m_builtInDirectories;
    QString m_userDirectory;
    QList<HangingProtocolFiles::Entry> m_entries;

    QTableWidget *m_table;
    QPushButton *m_copy;
    QPushButton *m_edit;
    QPushButton *m_delete;
};

} // End namespace udg

#endif
