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

#include "qspherevoidialog.h"

#include <QApplication>
#include <QClipboard>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

#include <vtkImageData.h>

#include "volumeroistatistics.h"

namespace udg {

namespace {

const QStringList Rows = {QT_TRANSLATE_NOOP("QSphereVoiDialog", "Volume (mL)"), QT_TRANSLATE_NOOP("QSphereVoiDialog", "Mean"),
                          QT_TRANSLATE_NOOP("QSphereVoiDialog", "Maximum"), QT_TRANSLATE_NOOP("QSphereVoiDialog", "Peak (1 mL)"),
                          QT_TRANSLATE_NOOP("QSphereVoiDialog", "Minimum"), QT_TRANSLATE_NOOP("QSphereVoiDialog", "Deviation")};

}

QSphereVoiDialog::QSphereVoiDialog(const QList<Input> &inputs, double radius, QWidget *parent)
    : QDialog(parent), m_inputs(inputs)
{
    setWindowTitle(tr("Sphere VOI"));

    QVBoxLayout *layout = new QVBoxLayout(this);
    QFormLayout *form = new QFormLayout();

    m_radiusSpinBox = new QDoubleSpinBox(this);
    m_radiusSpinBox->setObjectName("radiusSpinBox");
    m_radiusSpinBox->setRange(1.0, 200.0);
    m_radiusSpinBox->setDecimals(1);
    m_radiusSpinBox->setSuffix(tr(" mm"));
    m_radiusSpinBox->setValue(radius);
    form->addRow(tr("Radius:"), m_radiusSpinBox);

    m_thresholdSpinBox = new QDoubleSpinBox(this);
    m_thresholdSpinBox->setObjectName("thresholdSpinBox");
    m_thresholdSpinBox->setRange(0.0, 100.0);
    m_thresholdSpinBox->setDecimals(0);
    m_thresholdSpinBox->setSuffix(tr(" % of the maximum"));
    m_thresholdSpinBox->setSpecialValueText(tr("Off"));
    m_thresholdSpinBox->setToolTip(tr("Only the voxels with at least this fraction of the maximum of the sphere are used, which gives the metabolic volume of a lesion in it"));
    form->addRow(tr("Threshold:"), m_thresholdSpinBox);
    layout->addLayout(form);

    m_table = new QTableWidget(Rows.size(), m_inputs.size(), this);
    m_table->setObjectName("statisticsTable");
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    QStringList headers;

    for (const Input &input : m_inputs)
    {
        headers << (input.units.isEmpty() ? input.name : input.name + " (" + input.units + ")");
    }

    m_table->setHorizontalHeaderLabels(headers);
    QStringList rowLabels;

    for (const QString &row : Rows)
    {
        rowLabels << tr(row.toUtf8().constData());
    }

    m_table->setVerticalHeaderLabels(rowLabels);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->setMinimumWidth(420);
    m_table->setMinimumHeight(40 + 30 * Rows.size());
    layout->addWidget(m_table);

    m_note = new QLabel(this);
    m_note->setWordWrap(true);
    layout->addWidget(m_note);

    QHBoxLayout *buttons = new QHBoxLayout();
    QPushButton *copy = new QPushButton(tr("Copy"), this);
    QPushButton *close = new QPushButton(tr("Close"), this);
    close->setDefault(true);
    buttons->addWidget(copy);
    buttons->addStretch();
    buttons->addWidget(close);
    layout->addLayout(buttons);

    connect(m_radiusSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &QSphereVoiDialog::update);
    connect(m_thresholdSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &QSphereVoiDialog::update);
    connect(copy, &QPushButton::clicked, this, &QSphereVoiDialog::copyToClipboard);
    connect(close, &QPushButton::clicked, this, &QDialog::accept);

    update();
}

double QSphereVoiDialog::getRadius() const
{
    return m_radiusSpinBox->value();
}

void QSphereVoiDialog::update()
{
    QApplication::setOverrideCursor(Qt::WaitCursor);
    QStringList notes;

    for (int column = 0; column < m_inputs.size(); column++)
    {
        const Input &input = m_inputs.at(column);
        vtkSmartPointer<vtkImageData> mask = VolumeRoiStatistics::createSphereMask(input.values, input.center, m_radiusSpinBox->value());

        if (m_thresholdSpinBox->value() > 0.0)
        {
            double maximum = VolumeRoiStatistics::compute(input.values, mask, input.scale).maximum;
            mask = VolumeRoiStatistics::createThresholdMask(input.values, mask, input.scale, maximum * m_thresholdSpinBox->value() / 100.0);
        }

        VolumeRoiStatistics::Statistics statistics = VolumeRoiStatistics::compute(input.values, mask, input.scale, true);

        for (int row = 0; row < Rows.size(); row++)
        {
            QString text = "-";

            if (statistics.voxels > 0)
            {
                switch (row)
                {
                    case 0: text = QString::number(statistics.volumeMilliliters, 'f', 2); break;
                    case 1: text = QString::number(statistics.mean, 'f', 2); break;
                    case 2: text = QString::number(statistics.maximum, 'f', 2); break;
                    case 3: text = QString::number(statistics.peak, 'f', 2); break;
                    case 4: text = QString::number(statistics.minimum, 'f', 2); break;
                    case 5: text = QString::number(statistics.standardDeviation, 'f', 2); break;
                }
            }

            m_table->setItem(row, column, new QTableWidgetItem(text));
        }

        if (statistics.voxels == 0)
        {
            notes << tr("%1: the sphere has no voxel of the volume.").arg(input.name);
        }
    }

    m_note->setText(notes.join("\n"));
    QApplication::restoreOverrideCursor();
}

void QSphereVoiDialog::copyToClipboard()
{
    QStringList lines;
    lines << tr("Sphere VOI: radius %1 mm").arg(m_radiusSpinBox->value(), 0, 'f', 1);

    for (int column = 0; column < m_table->columnCount(); column++)
    {
        lines << m_table->horizontalHeaderItem(column)->text()
                 + tr(" (center %1, %2, %3 mm)").arg(m_inputs.at(column).center[0], 0, 'f', 1).arg(m_inputs.at(column).center[1], 0, 'f', 1)
                       .arg(m_inputs.at(column).center[2], 0, 'f', 1);

        for (int row = 0; row < m_table->rowCount(); row++)
        {
            lines << QString("  %1: %2").arg(m_table->verticalHeaderItem(row)->text(), m_table->item(row, column)->text());
        }
    }

    QApplication::clipboard()->setText(lines.join("\n"));
}

} // End namespace udg
