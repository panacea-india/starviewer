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

#include "qdeidentifydialog.h"

#include <QApplication>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>

#include "deidentificationjob.h"
#include "logging.h"
#include "starviewerapplication.h"

namespace udg {

namespace {

// "10,10,200,30; 0,0,50,20" into rectangles; false if something is not a rectangle
bool parseRectangles(const QString &text, QList<QRect> *rectangles, QString *problem)
{
    for (const QString &part : text.split(';', QString::SkipEmptyParts))
    {
        QStringList numbers = part.split(',');
        bool ok = numbers.size() == 4;
        int values[4] = {0, 0, 0, 0};

        for (int i = 0; ok && i < 4; i++)
        {
            values[i] = numbers[i].trimmed().toInt(&ok);
        }

        if (!ok || values[0] < 0 || values[1] < 0 || values[2] <= 0 || values[3] <= 0)
        {
            *problem = QObject::tr("\"%1\" is not a rectangle: it is x, y, width and height in pixels, with the position from the top left corner, such as 0,0,200,40.").arg(part.trimmed());
            return false;
        }

        *rectangles << QRect(values[0], values[1], values[2], values[3]);
    }

    return true;
}

}

QDeidentifyDialog::QDeidentifyDialog(QWidget *parent)
    : QDialog(parent), m_running(false), m_stop(false)
{
    setWindowTitle(tr("De-identify images"));
    resize(640, 640);

    m_input = new QLineEdit(this);
    m_output = new QLineEdit(this);
    QPushButton *browseInput = new QPushButton(tr("Browse..."), this);
    QPushButton *browseOutput = new QPushButton(tr("Browse..."), this);
    connect(browseInput, &QPushButton::clicked, this, &QDeidentifyDialog::browseInput);
    connect(browseOutput, &QPushButton::clicked, this, &QDeidentifyDialog::browseOutput);

    QHBoxLayout *inputRow = new QHBoxLayout();
    inputRow->addWidget(m_input);
    inputRow->addWidget(browseInput);
    QHBoxLayout *outputRow = new QHBoxLayout();
    outputRow->addWidget(m_output);
    outputRow->addWidget(browseOutput);

    QFormLayout *folders = new QFormLayout();
    folders->addRow(tr("Folder of the images:"), inputRow);
    folders->addRow(tr("Folder for the new images:"), outputRow);

    m_retainDates = new QCheckBox(tr("Keep the dates and times"), this);
    m_modifiedDates = new QCheckBox(tr("Move the dates by a number of days that is the same for each patient (the intervals are kept)"), this);
    m_characteristics = new QCheckBox(tr("Keep the sex, age, size and weight of the patient"), this);
    m_device = new QCheckBox(tr("Keep the identity of the device (serial numbers, station name)"), this);
    m_uids = new QCheckBox(tr("Keep the UIDs (the images can then be matched with the originals)"), this);
    m_descriptors = new QCheckBox(tr("Keep the descriptions of the study and the series that do not have the name or the ID of the patient"), this);
    connect(m_retainDates, &QCheckBox::toggled, this, [this](bool on) { if (on) { m_modifiedDates->setChecked(false); } });
    connect(m_modifiedDates, &QCheckBox::toggled, this, [this](bool on) { if (on) { m_retainDates->setChecked(false); } });

    QGroupBox *options = new QGroupBox(tr("What to keep (the standard profile removes all of these)"), this);
    QVBoxLayout *optionsLayout = new QVBoxLayout(options);
    optionsLayout->addWidget(m_retainDates);
    optionsLayout->addWidget(m_modifiedDates);
    optionsLayout->addWidget(m_characteristics);
    optionsLayout->addWidget(m_device);
    optionsLayout->addWidget(m_uids);
    optionsLayout->addWidget(m_descriptors);

    m_name = new QLineEdit("ANONYMOUS", this);
    m_patientId = new QLineEdit(this);
    m_patientId->setPlaceholderText(tr("a code made from the original ID"));
    m_blackout = new QLineEdit(this);
    m_blackout->setPlaceholderText(tr("such as 0,0,300,30; 0,450,300,30"));
    m_blackout->setToolTip(tr("Rectangles to blank in all the images: x, y, width and height in pixels, from the top left corner, separated by semicolons"));

    QFormLayout *values = new QFormLayout();
    values->addRow(tr("Name of the patient:"), m_name);
    values->addRow(tr("ID of the patient:"), m_patientId);
    values->addRow(tr("Blank in the pixels:"), m_blackout);

    m_progress = new QProgressBar(this);
    m_progress->setValue(0);
    m_log = new QPlainTextEdit(this);
    m_log->setReadOnly(true);
    m_log->setPlainText(tr("The standard profile removes the identifying attributes, replaces the UIDs and removes the private attributes. It does not look at the pixels: text in the images (ultrasound, "
                           "secondary captures, some angiograms) has to be blanked with the rectangles, and the images have to be looked at before they are shared. "
                           "There is no list to give the identity back."));

    m_run = new QPushButton(tr("De-identify"), this);
    connect(m_run, &QPushButton::clicked, this, &QDeidentifyDialog::run);
    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    buttons->addButton(m_run, QDialogButtonBox::ActionRole);
    connect(buttons, &QDialogButtonBox::rejected, this, [this]() { m_stop = true; reject(); });

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addLayout(folders);
    layout->addWidget(options);
    layout->addLayout(values);
    layout->addWidget(m_progress);
    layout->addWidget(m_log, 1);
    layout->addWidget(buttons);
}

void QDeidentifyDialog::browseInput()
{
    QString folder = QFileDialog::getExistingDirectory(this, tr("Folder of the images"), m_input->text());

    if (!folder.isEmpty())
    {
        m_input->setText(folder);
    }
}

void QDeidentifyDialog::browseOutput()
{
    QString folder = QFileDialog::getExistingDirectory(this, tr("Folder for the new images"), m_output->text());

    if (!folder.isEmpty())
    {
        m_output->setText(folder);
    }
}

void QDeidentifyDialog::run()
{
    if (m_running)
    {
        m_stop = true;
        return;
    }

    QList<QRect> rectangles;
    QString problem;

    if (!parseRectangles(m_blackout->text(), &rectangles, &problem))
    {
        QMessageBox::warning(this, ApplicationNameString, problem);
        return;
    }

    DicomDeidentifier::Options options;
    options.retainDates = m_retainDates->isChecked();
    options.retainModifiedDates = m_modifiedDates->isChecked();
    options.retainPatientCharacteristics = m_characteristics->isChecked();
    options.retainDeviceIdentity = m_device->isChecked();
    options.retainUids = m_uids->isChecked();
    options.cleanDescriptors = m_descriptors->isChecked();
    options.patientName = m_name->text().trimmed().isEmpty() ? QString("ANONYMOUS") : m_name->text().trimmed();
    options.patientId = m_patientId->text().trimmed();

    m_running = true;
    m_stop = false;
    m_run->setText(tr("Stop"));
    m_log->setPlainText(tr("Working..."));

    DeidentificationJob::Result result = DeidentificationJob::run(m_input->text().trimmed(), m_output->text().trimmed(), options, rectangles, [this](int done, int total) {
        m_progress->setMaximum(qMax(total, 1));
        m_progress->setValue(done);
        QApplication::processEvents();
        return !m_stop;
    });

    m_running = false;
    m_run->setText(tr("De-identify"));

    QString text = result.message + "\n\n";
    text += tr("Images written: %1\nFailed: %2\nNot DICOM (left out): %3\n").arg(result.processed).arg(result.failed).arg(result.notDicom);
    text += tr("Attributes removed: %1, emptied: %2, dates replaced: %3, dates moved: %4, UIDs replaced: %5, private attributes removed: %6\n")
                .arg(result.totals.removed).arg(result.totals.emptied).arg(result.totals.replaced).arg(result.totals.datesShifted).arg(result.totals.uidsReplaced).arg(result.totals.privateRemoved);

    if (!rectangles.isEmpty())
    {
        text += tr("Images with the rectangles blanked: %1\n").arg(result.blanked);
    }

    if (!result.warnings.isEmpty())
    {
        text += "\n" + tr("Look at these before sharing the images:") + "\n - " + result.warnings.join("\n - ") + "\n";
    }

    if (!result.errors.isEmpty())
    {
        text += "\n" + tr("Not written:") + "\n - " + result.errors.join("\n - ") + "\n";
    }

    m_log->setPlainText(text);
    INFO_LOG(QString("De-identification: %1 images written, %2 failed, %3 not DICOM").arg(result.processed).arg(result.failed).arg(result.notDicom));
}

} // End namespace udg
