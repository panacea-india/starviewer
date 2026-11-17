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

#include "qmodulesdialog.h"

#include <QApplication>
#include <QComboBox>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>

#include "logging.h"
#include "starviewerapplication.h"

namespace udg {

QModulesDialog::QModulesDialog(QWidget *parent)
    : QDialog(parent), m_running(false), m_stop(false)
{
    setWindowTitle(tr("Modules"));
    resize(680, 640);

    QStringList problems;
    m_modules = ExternalModules::discover(moduleDirectories(), &problems);

    m_choice = new QComboBox(this);

    for (const ModuleManifest &m : m_modules)
    {
        m_choice->addItem(QString("%1 %2").arg(m.name, m.version));
    }

    m_description = new QLabel(this);
    m_description->setWordWrap(true);
    connect(m_choice, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &QModulesDialog::showSelected);

    m_input = new QLineEdit(this);
    m_output = new QLineEdit(this);
    QPushButton *browseInput = new QPushButton(tr("Browse..."), this);
    QPushButton *browseOutput = new QPushButton(tr("Browse..."), this);
    connect(browseInput, &QPushButton::clicked, this, &QModulesDialog::browseInput);
    connect(browseOutput, &QPushButton::clicked, this, &QModulesDialog::browseOutput);
    QHBoxLayout *inputRow = new QHBoxLayout();
    inputRow->addWidget(m_input);
    inputRow->addWidget(browseInput);
    QHBoxLayout *outputRow = new QHBoxLayout();
    outputRow->addWidget(m_output);
    outputRow->addWidget(browseOutput);

    m_parameters = new QPlainTextEdit(this);
    m_parameters->setMaximumHeight(70);
    m_parameters->setPlaceholderText(tr("Parameters for the module as JSON, such as {\"threshold\": 0.5} (optional)"));

    QFormLayout *form = new QFormLayout();
    form->addRow(tr("Module:"), m_choice);
    form->addRow(tr("Folder of the images:"), inputRow);
    form->addRow(tr("Folder for the results:"), outputRow);
    form->addRow(tr("Parameters:"), m_parameters);

    m_progress = new QProgressBar(this);
    m_log = new QPlainTextEdit(this);
    m_log->setReadOnly(true);
    m_run = new QPushButton(tr("Run"), this);
    m_open = new QPushButton(tr("Open the results folder"), this);
    m_open->setEnabled(false);
    connect(m_run, &QPushButton::clicked, this, &QModulesDialog::run);
    connect(m_open, &QPushButton::clicked, this, &QModulesDialog::openOutput);

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    buttons->addButton(m_open, QDialogButtonBox::ActionRole);
    buttons->addButton(m_run, QDialogButtonBox::ActionRole);
    connect(buttons, &QDialogButtonBox::rejected, this, [this]() { m_stop = true; reject(); });

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(m_description);
    layout->addWidget(m_progress);
    layout->addWidget(m_log, 1);
    layout->addWidget(buttons);

    if (m_modules.isEmpty())
    {
        m_run->setEnabled(false);
        m_log->setPlainText(tr("There are no modules. A module is a folder with a module.json and a program, in a subfolder of:\n%1\n\nSee doc/extension-boundary.md.").arg(moduleDirectories().join("\n")));
    }
    else
    {
        showSelected();
    }

    if (!problems.isEmpty())
    {
        m_log->appendPlainText("\n" + tr("Modules that could not be used:") + "\n - " + problems.join("\n - "));
    }
}

QStringList QModulesDialog::moduleDirectories() const
{
    QStringList list;
    list << sourcePath() + "/modules" << sourcePath() + "/modules/examples" << installationPath() + "/modules" << UserDataRootPath + "modules";
    list.removeDuplicates();
    return list;
}

void QModulesDialog::showSelected()
{
    int index = m_choice->currentIndex();

    if (index < 0 || index >= m_modules.size())
    {
        return;
    }

    const ModuleManifest &m = m_modules[index];
    QString text = m.description + "\n";
    text += m.researchOnly ? tr("RESEARCH ONLY: not for clinical use; look at what it gives before using it.") : tr("Not marked as research only by its author.");
    text += "\n" + tr("Network: %1").arg(m.network == "none" ? tr("none") : m.network);
    m_description->setText(text);
}

void QModulesDialog::browseInput()
{
    QString folder = QFileDialog::getExistingDirectory(this, tr("Folder of the images"), m_input->text());

    if (!folder.isEmpty())
    {
        m_input->setText(folder);
    }
}

void QModulesDialog::browseOutput()
{
    QString folder = QFileDialog::getExistingDirectory(this, tr("Folder for the results"), m_output->text());

    if (!folder.isEmpty())
    {
        m_output->setText(folder);
    }
}

void QModulesDialog::run()
{
    if (m_running)
    {
        m_stop = true;
        return;
    }

    int index = m_choice->currentIndex();

    if (index < 0 || index >= m_modules.size())
    {
        return;
    }

    QJsonObject parameters;
    QString parameterText = m_parameters->toPlainText().trimmed();

    if (!parameterText.isEmpty())
    {
        QJsonParseError error;
        QJsonDocument document = QJsonDocument::fromJson(parameterText.toUtf8(), &error);

        if (error.error != QJsonParseError::NoError || !document.isObject())
        {
            QMessageBox::warning(this, ApplicationNameString, tr("The parameters are not a JSON object: %1").arg(error.errorString()));
            return;
        }

        parameters = document.object();
    }

    const ModuleManifest &module = m_modules[index];
    QString network = module.network == "none" ? tr("It says that it does not use the network.") : tr("It says that it uses the network: %1").arg(module.network);

    if (QMessageBox::question(this, tr("Run the module"), tr("Run \"%1\" on the images of this folder?\n\n%2\n\n%3\n\nThe module is a program of its own: it can read the folder you chose and write in the results folder.")
                                                              .arg(module.name, module.researchOnly ? tr("It is for research only.") : QString(), network),
                              QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
    {
        return;
    }

    m_running = true;
    m_stop = false;
    m_run->setText(tr("Cancel"));
    m_open->setEnabled(false);
    m_progress->setValue(0);
    m_log->setPlainText(tr("Running..."));

    ModuleRunner::Result result = ModuleRunner::run(module, m_input->text().trimmed(), m_output->text().trimmed(), parameters, [this](double value, const QString &message) {
        m_progress->setMaximum(1000);
        m_progress->setValue(static_cast<int>(value * 1000));
        m_progress->setFormat(message.isEmpty() ? QString("%p%") : message + " (%p%)");
        QApplication::processEvents();
        return !m_stop;
    });

    m_running = false;
    m_run->setText(tr("Run"));

    QString text = (result.success ? tr("Done: %1").arg(result.message) : tr("It failed: %1").arg(result.message)) + "\n";

    if (!result.log.isEmpty())
    {
        text += "\n" + tr("Log of the module:") + "\n - " + result.log.join("\n - ") + "\n";
    }

    if (!result.outputs.isEmpty())
    {
        text += "\n" + tr("Results:") + "\n";

        for (const ExternalModules::Message::Output &output : result.outputs)
        {
            text += QString(" - %1%2\n").arg(QDir::toNativeSeparators(output.path), output.description.isEmpty() ? QString() : " (" + output.description + ")");
        }
    }

    m_log->setPlainText(text);
    m_open->setEnabled(result.success);
    INFO_LOG(QString("Module %1 ended: %2").arg(module.name, result.success ? QString("ok") : result.message));
}

void QModulesDialog::openOutput()
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(m_output->text().trimmed()));
}

} // End namespace udg
