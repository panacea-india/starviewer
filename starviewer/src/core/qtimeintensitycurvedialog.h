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

#ifndef UDGQTIMEINTENSITYCURVEDIALOG_H
#define UDGQTIMEINTENSITYCURVEDIALOG_H

#include <QDialog>

#include <vtkSmartPointer.h>

#include "timeintensitycurve.h"

class QDoubleSpinBox;
class QLabel;
class QTableWidget;
class vtkImageData;

namespace udg {

/// The plot of a TimeIntensityCurve (mean with the standard deviation as a band, the peak marked), the table of its values, the radius of the disc to change and a button to save the CSV.
class QCurvePlot : public QWidget {
    Q_OBJECT

public:
    explicit QCurvePlot(QWidget *parent = nullptr);

    void setCurve(const TimeIntensityCurve &curve);
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    TimeIntensityCurve m_curve;
};

class QTimeIntensityCurveDialog : public QDialog {
    Q_OBJECT

public:
    /// The data is the volume with all the phases (see TimeIntensityCurve); the center is given in pixel indexes. It is not kept after the dialog is closed.
    QTimeIntensityCurveDialog(vtkImageData *data, int numberOfPhases, int slice, double centerX, double centerY, double spacingX, double spacingY, QWidget *parent = nullptr);

private slots:
    void recompute();
    void saveCsv();

private:
    vtkSmartPointer<vtkImageData> m_data;
    int m_numberOfPhases;
    int m_slice;
    double m_centerX;
    double m_centerY;
    double m_spacingX;
    double m_spacingY;
    TimeIntensityCurve m_curve;

    QDoubleSpinBox *m_radius;
    QCurvePlot *m_plot;
    QTableWidget *m_table;
    QLabel *m_summary;
};

} // End namespace udg

#endif
