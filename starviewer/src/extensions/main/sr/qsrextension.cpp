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

#include "qsrextension.h"

#include <algorithm>

#include <QLabel>
#include <QListWidget>
#include <QSplitter>
#include <QTextBrowser>
#include <QVBoxLayout>

#include "logging.h"
#include "patient.h"
#include "structuredreportrenderer.h"
#include "study.h"

namespace udg {

QSrExtension::QSrExtension(QWidget *parent)
    : QWidget(parent)
{
    QVBoxLayout *layout = new QVBoxLayout(this);

    m_noticeLabel = new QLabel(this);
    m_noticeLabel->setWordWrap(true);
    m_noticeLabel->setStyleSheet("background-color: #ffe08a; color: black; padding: 4px;");
    m_noticeLabel->hide();
    layout->addWidget(m_noticeLabel);

    QSplitter *splitter = new QSplitter(Qt::Horizontal, this);
    m_listWidget = new QListWidget(splitter);
    m_textBrowser = new QTextBrowser(splitter);
    // The text of a report is not trusted: no link is followed
    m_textBrowser->setOpenLinks(false);
    m_textBrowser->setOpenExternalLinks(false);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({220, 800});
    layout->addWidget(splitter, 1);

    connect(m_listWidget, &QListWidget::currentItemChanged, [this](QListWidgetItem *current, QListWidgetItem*) { showReport(current); });
}

QSrExtension::~QSrExtension()
{
}

void QSrExtension::setPatient(Patient *patient)
{
    m_listWidget->clear();
    m_textBrowser->clear();
    m_noticeLabel->hide();

    if (!patient)
    {
        DEBUG_LOG("Received a null patient");
        return;
    }

    QList<StructuredReport> reports;

    for (Study *study : patient->getStudies())
    {
        reports << study->getStructuredReports();
    }

    // The reports that are final come first, so that a draft is not the first thing that is shown
    std::stable_sort(reports.begin(), reports.end(), [](const StructuredReport &a, const StructuredReport &b) { return a.isFinal() && !b.isFinal(); });

    int count = 0;

    {
        for (const StructuredReport &report : reports)
        {
            count++;
            QString title = report.getTitle().isEmpty() ? tr("Report %1").arg(count) : report.getTitle();
            QString text = report.isFinal() ? title : tr("%1 (%2, %3)").arg(title, report.getCompletionFlag().toLower(), report.getVerificationFlag().toLower());
            QListWidgetItem *item = new QListWidgetItem(text, m_listWidget);
            item->setData(Qt::UserRole, report.getPath());
            item->setData(Qt::UserRole + 1, report.isFinal());
            item->setData(Qt::UserRole + 2, QStringList{report.getCompletionFlag(), report.getVerificationFlag()});
        }
    }

    if (count > 0)
    {
        m_listWidget->setCurrentRow(0);
    }
}

void QSrExtension::showReport(QListWidgetItem *item)
{
    if (!item)
    {
        return;
    }

    QString error;
    QString html = StructuredReportRenderer::toHtml(item->data(Qt::UserRole).toString(), &error);

    if (html.isEmpty())
    {
        ERROR_LOG(QString("The Structured Report %1 could not be shown: %2").arg(item->data(Qt::UserRole).toString(), error));
        m_textBrowser->setPlainText(tr("This report cannot be shown: %1").arg(error));
    }
    else
    {
        m_textBrowser->setHtml(html);
    }

    // A report that is partial or not verified is not final, and a reader has to know
    if (item->data(Qt::UserRole + 1).toBool())
    {
        m_noticeLabel->hide();
    }
    else
    {
        QStringList flags = item->data(Qt::UserRole + 2).toStringList();
        m_noticeLabel->setText(tr("This report is not final: completion %1, verification %2.").arg(flags.value(0), flags.value(1)));
        m_noticeLabel->show();
    }
}

} // namespace udg
