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

#ifndef UDG_QSREXTENSION_H
#define UDG_QSREXTENSION_H

#include <QWidget>

#include "structuredreport.h"

class QListWidget;
class QListWidgetItem;
class QTextBrowser;
class QLabel;

namespace udg {

class Patient;

/**
 * @brief QSrExtension lists the Structured Reports of a Patient and shows the one that is chosen, rendered as text.
 */
class QSrExtension : public QWidget {

    Q_OBJECT

public:
    explicit QSrExtension(QWidget *parent = nullptr);
    ~QSrExtension() override;

    /// Sets the given patient to the extension. All the Structured Reports of its studies are listed, and the first one is shown.
    void setPatient(Patient *patient);

private slots:
    /// Shows the report of the given item.
    void showReport(QListWidgetItem *item);

private:
    QListWidget *m_listWidget;
    QTextBrowser *m_textBrowser;
    QLabel *m_noticeLabel;
};

} // namespace udg

#endif // UDG_QSREXTENSION_H
