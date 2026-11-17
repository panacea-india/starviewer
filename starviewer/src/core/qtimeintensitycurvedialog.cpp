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

#include "qtimeintensitycurvedialog.h"

#include <algorithm>

#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

#include <vtkImageData.h>

#include "logging.h"
#include "starviewerapplication.h"

namespace udg {

QCurvePlot::QCurvePlot(QWidget *parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void QCurvePlot::setCurve(const TimeIntensityCurve &curve)
{
    m_curve = curve;
    update();
}

QSize QCurvePlot::minimumSizeHint() const
{
    return QSize(360, 220);
}

void QCurvePlot::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), palette().base());

    if (m_curve.isEmpty())
    {
        painter.drawText(rect(), Qt::AlignCenter, tr("Nothing to show"));
        return;
    }

    int n = m_curve.mean.size();
    double low = m_curve.mean[0] - m_curve.standardDeviation[0];
    double high = m_curve.mean[0] + m_curve.standardDeviation[0];

    for (int i = 1; i < n; i++)
    {
        low = std::min(low, m_curve.mean[i] - m_curve.standardDeviation[i]);
        high = std::max(high, m_curve.mean[i] + m_curve.standardDeviation[i]);
    }

    if (high - low < 1e-9)
    {
        low -= 1.0;
        high += 1.0;
    }

    QRectF area(52, 10, width() - 64, height() - 36);
    auto toPoint = [&](int i, double value) {
        double x = n > 1 ? area.left() + area.width() * i / (n - 1) : area.center().x();
        double y = area.bottom() - area.height() * (value - low) / (high - low);
        return QPointF(x, y);
    };

    painter.setPen(palette().color(QPalette::Mid));
    painter.drawRect(area);
    painter.setPen(palette().color(QPalette::Text));
    painter.drawText(QRectF(0, area.top() - 8, 48, 16), Qt::AlignRight | Qt::AlignVCenter, QString::number(high, 'g', 5));
    painter.drawText(QRectF(0, area.bottom() - 8, 48, 16), Qt::AlignRight | Qt::AlignVCenter, QString::number(low, 'g', 5));
    painter.drawText(QRectF(area.left() - 10, area.bottom() + 2, 20, 16), Qt::AlignCenter, "1");
    painter.drawText(QRectF(area.right() - 20, area.bottom() + 2, 40, 16), Qt::AlignCenter, tr("%1").arg(n));

    // The band of the standard deviation
    QPolygonF band;

    for (int i = 0; i < n; i++)
    {
        band << toPoint(i, m_curve.mean[i] + m_curve.standardDeviation[i]);
    }

    for (int i = n - 1; i >= 0; i--)
    {
        band << toPoint(i, m_curve.mean[i] - m_curve.standardDeviation[i]);
    }

    QColor line(30, 100, 200);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(line.red(), line.green(), line.blue(), 50));
    painter.drawPolygon(band);

    QPolygonF mean;

    for (int i = 0; i < n; i++)
    {
        mean << toPoint(i, m_curve.mean[i]);
    }

    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(line, 2));
    painter.drawPolyline(mean);

    painter.setBrush(line);

    for (const QPointF &point : mean)
    {
        painter.drawEllipse(point, 3, 3);
    }

    painter.setBrush(QColor(210, 60, 40));
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(mean[m_curve.peakPhase()], 6, 6);
}

QTimeIntensityCurveDialog::QTimeIntensityCurveDialog(vtkImageData *data, int numberOfPhases, int slice, double centerX, double centerY, double spacingX, double spacingY,
                                                     QWidget *parent)
    : QDialog(parent), m_data(data), m_numberOfPhases(numberOfPhases), m_slice(slice), m_centerX(centerX), m_centerY(centerY), m_spacingX(spacingX), m_spacingY(spacingY)
{
    setWindowTitle(tr("Time-intensity curve"));
    resize(560, 620);

    m_radius = new QDoubleSpinBox(this);
    m_radius->setRange(0.0, 100.0);
    m_radius->setDecimals(1);
    m_radius->setSingleStep(1.0);
    m_radius->setSuffix(tr(" mm"));
    m_radius->setValue(5.0);
    connect(m_radius, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &QTimeIntensityCurveDialog::recompute);

    m_summary = new QLabel(this);
    m_summary->setWordWrap(true);
    m_plot = new QCurvePlot(this);

    m_table = new QTableWidget(0, 3, this);
    m_table->setHorizontalHeaderLabels(QStringList() << tr("Phase") << tr("Mean") << tr("Standard deviation"));
    m_table->verticalHeader()->hide();
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    QPushButton *save = new QPushButton(tr("Save as CSV..."), this);
    connect(save, &QPushButton::clicked, this, &QTimeIntensityCurveDialog::saveCsv);
    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    buttons->addButton(save, QDialogButtonBox::ActionRole);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    QFormLayout *form = new QFormLayout();
    form->addRow(tr("Radius of the region:"), m_radius);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(m_plot, 2);
    layout->addWidget(m_summary);
    layout->addWidget(m_table, 1);
    layout->addWidget(buttons);

    recompute();
}

void QTimeIntensityCurveDialog::recompute()
{
    m_curve = TimeIntensityCurve::compute(m_data, m_numberOfPhases, m_slice, m_centerX, m_centerY, m_radius->value(), m_spacingX, m_spacingY);
    m_plot->setCurve(m_curve);
    m_table->setRowCount(m_curve.mean.size());

    for (int i = 0; i < m_curve.mean.size(); i++)
    {
        m_table->setItem(i, 0, new QTableWidgetItem(QString::number(i + 1)));
        m_table->setItem(i, 1, new QTableWidgetItem(QString::number(m_curve.mean[i], 'f', 2)));
        m_table->setItem(i, 2, new QTableWidgetItem(QString::number(m_curve.standardDeviation[i], 'f', 2)));
    }

    if (m_curve.isEmpty())
    {
        m_summary->setText(tr("The region is outside the image."));
        return;
    }

    int peak = m_curve.peakPhase();
    int half = m_curve.firstPhaseReaching(0.5);
    m_summary->setText(tr("%1 pixels. Peak: %2 at phase %3. Half of the rise is reached at phase %4. The values are those of the pixels (not converted).")
                           .arg(m_curve.pixels).arg(m_curve.mean[peak], 0, 'f', 2).arg(peak + 1).arg(half >= 0 ? QString::number(half + 1) : tr("(none: the curve is flat)")));
}

void QTimeIntensityCurveDialog::saveCsv()
{
    if (m_curve.isEmpty())
    {
        return;
    }

    QString path = QFileDialog::getSaveFileName(this, tr("Save the curve"), QString(), tr("CSV files (*.csv)"));

    if (path.isEmpty())
    {
        return;
    }

    QFile file(path);

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text) || file.write(m_curve.toCsv().toUtf8()) < 0)
    {
        QMessageBox::warning(this, ApplicationNameString, tr("The file could not be written."));
        return;
    }

    INFO_LOG(QString("Saved a time-intensity curve to %1").arg(path));
}

} // End namespace udg
