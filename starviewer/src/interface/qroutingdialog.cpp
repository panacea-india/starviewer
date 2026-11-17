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

#include "qroutingdialog.h"

#include <QApplication>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include "coresettings.h"
#include "dicomstoresender.h"
#include "logging.h"
#include "starviewerapplication.h"

namespace udg {

namespace {

const char *Template = R"({
  "destinations": [
    {"name": "archive", "host": "127.0.0.1", "port": 104, "calledAet": "ARCHIVE", "callingAet": "STARVIEWER"}
  ],
  "rules": [
    {
      "name": "all CT to the archive",
      "enabled": true,
      "match": "all",
      "destination": "archive",
      "conditions": [
        {"attribute": "Modality", "operator": "equals", "value": "CT"}
      ]
    }
  ]
})";

}

QRoutingDialog::QRoutingDialog(QWidget *parent)
    : QDialog(parent), m_hasPlan(false)
{
    setWindowTitle(tr("Route images"));
    resize(720, 700);

    m_folder = new QLineEdit(this);
    QPushButton *browse = new QPushButton(tr("Browse..."), this);
    connect(browse, &QPushButton::clicked, this, &QRoutingDialog::browse);
    QHBoxLayout *folderRow = new QHBoxLayout();
    folderRow->addWidget(m_folder);
    folderRow->addWidget(browse);

    m_rules = new QPlainTextEdit(this);
    m_rules->setFont(QFont("Consolas", 9));
    QFile file(rulesPath());

    if (file.open(QIODevice::ReadOnly))
    {
        m_rules->setPlainText(QString::fromUtf8(file.readAll()));
    }
    else
    {
        m_rules->setPlainText(Template);
    }

    QLabel *help = new QLabel(tr("Attributes: %1. Operators: equals, notEquals, contains, notContains, startsWith, endsWith, matches (a regular expression). "
                                 "\"match\" is \"all\" or \"any\". Destinations are PACS (host, port, AE titles).").arg(RoutingRules::attributeNames().join(", ")), this);
    help->setWordWrap(true);

    QPushButton *check = new QPushButton(tr("Check the rules"), this);
    QPushButton *save = new QPushButton(tr("Save the rules"), this);
    QPushButton *plan = new QPushButton(tr("Show the plan"), this);
    QPushButton *send = new QPushButton(tr("Send..."), this);
    connect(check, &QPushButton::clicked, this, &QRoutingDialog::check);
    connect(save, &QPushButton::clicked, this, &QRoutingDialog::saveRules);
    connect(plan, &QPushButton::clicked, this, &QRoutingDialog::showPlan);
    connect(send, &QPushButton::clicked, this, &QRoutingDialog::sendFiles);
    QHBoxLayout *buttonsRow = new QHBoxLayout();
    buttonsRow->addWidget(check);
    buttonsRow->addWidget(save);
    buttonsRow->addStretch();
    buttonsRow->addWidget(plan);
    buttonsRow->addWidget(send);

    m_log = new QPlainTextEdit(this);
    m_log->setReadOnly(true);
    m_log->setMaximumHeight(200);
    m_log->setPlainText(tr("Nothing is sent until you press Send and confirm. The images that match no rule are not sent. The images are sent as they are: if they have the identity of the patient, "
                           "de-identify them first."));

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(tr("Folder of the images:"), this));
    layout->addLayout(folderRow);
    layout->addWidget(new QLabel(tr("Rules:"), this));
    layout->addWidget(m_rules, 1);
    layout->addWidget(help);
    layout->addLayout(buttonsRow);
    layout->addWidget(m_log);
    layout->addWidget(buttons);
}

QString QRoutingDialog::rulesPath() const
{
    return UserDataRootPath + "routing.json";
}

void QRoutingDialog::browse()
{
    QString folder = QFileDialog::getExistingDirectory(this, tr("Folder of the images"), m_folder->text());

    if (!folder.isEmpty())
    {
        m_folder->setText(folder);
    }
}

bool QRoutingDialog::readRules(RoutingRules *rules)
{
    QJsonParseError error;
    QJsonDocument document = QJsonDocument::fromJson(m_rules->toPlainText().toUtf8(), &error);

    if (error.error != QJsonParseError::NoError || !document.isObject())
    {
        m_log->setPlainText(tr("The rules are not valid JSON: %1 (at character %2).").arg(error.errorString()).arg(error.offset));
        return false;
    }

    QStringList problems;

    if (!rules->fromJson(document.object(), &problems))
    {
        m_log->setPlainText(tr("The rules have problems:") + "\n - " + problems.join("\n - "));
        return false;
    }

    return true;
}

void QRoutingDialog::check()
{
    RoutingRules rules;

    if (readRules(&rules))
    {
        m_log->setPlainText(tr("The rules are valid: %1 rules and %2 destinations.").arg(rules.rules.size()).arg(rules.destinations.size()));
    }
}

void QRoutingDialog::saveRules()
{
    RoutingRules rules;

    if (!readRules(&rules))
    {
        return;
    }

    QDir().mkpath(QFileInfo(rulesPath()).absolutePath());
    QFile file(rulesPath());

    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate) || file.write(m_rules->toPlainText().toUtf8()) < 0)
    {
        QMessageBox::warning(this, ApplicationNameString, tr("The rules could not be saved."));
        return;
    }

    m_log->setPlainText(tr("The rules were saved in %1.").arg(QDir::toNativeSeparators(rulesPath())));
}

void QRoutingDialog::showPlan()
{
    RoutingRules rules;

    if (!readRules(&rules))
    {
        return;
    }

    if (!QFileInfo(m_folder->text().trimmed()).isDir())
    {
        m_log->setPlainText(tr("The folder of the images does not exist."));
        return;
    }

    QApplication::setOverrideCursor(Qt::WaitCursor);
    m_plan = RoutingJob::plan(m_folder->text().trimmed(), rules);
    QApplication::restoreOverrideCursor();
    m_plannedRules = rules;
    m_hasPlan = true;

    QString text = tr("Plan (nothing was sent):") + "\n";
    text += tr("Images to send: %1, not sent because no rule matches: %2, files that are not DICOM: %3").arg(m_plan.routed.size()).arg(m_plan.unrouted.size()).arg(m_plan.notDicom) + "\n";

    for (auto it = m_plan.perDestination.constBegin(); it != m_plan.perDestination.constEnd(); ++it)
    {
        const RoutingRules::Destination *d = rules.findDestination(it.key());
        text += tr(" - %1 (%2:%3, %4): %5 images").arg(it.key(), d ? d->host : QString(), d ? QString::number(d->port) : QString(), d ? d->calledAet : QString()).arg(it.value()) + "\n";
    }

    m_log->setPlainText(text);
}

void QRoutingDialog::sendFiles()
{
    showPlan();

    if (!m_hasPlan || m_plan.routed.isEmpty())
    {
        return;
    }

    QString summary;

    for (auto it = m_plan.perDestination.constBegin(); it != m_plan.perDestination.constEnd(); ++it)
    {
        summary += tr("%1 images to %2\n").arg(it.value()).arg(it.key());
    }

    if (QMessageBox::question(this, tr("Send"), tr("Send these images?\n\n%1\nThey are sent as they are, with the identity they have.").arg(summary), QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
    {
        return;
    }

    QApplication::setOverrideCursor(Qt::WaitCursor);
    RoutingJob::SendResult result = RoutingJob::send(m_plan, m_plannedRules, &DicomStoreSender::send);
    QApplication::restoreOverrideCursor();

    QString text = result.message + "\n" + tr("Sent: %1, failed: %2").arg(result.sent).arg(result.failed) + "\n";

    if (!result.errors.isEmpty())
    {
        text += "\n" + tr("Problems:") + "\n - " + result.errors.join("\n - ");
    }

    m_log->setPlainText(text);
    INFO_LOG(QString("Routing: %1 images sent, %2 failed").arg(result.sent).arg(result.failed));
}

} // End namespace udg
