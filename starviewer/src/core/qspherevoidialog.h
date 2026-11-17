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

#ifndef UDGQSPHEREVOIDIALOG_H
#define UDGQSPHEREVOIDIALOG_H

#include <QDialog>
#include <QList>
#include <QString>

class QDoubleSpinBox;
class QLabel;
class QTableWidget;

class vtkImageData;

namespace udg {

/**
    Shows the statistics of a sphere of a volume around a point (a VOI), for each of the volumes that a viewer shows, and recomputes them when the radius or the threshold
    changes. A PET has its values also as SUV when the study says how. The numbers come from VolumeRoiStatistics.
 */
class QSphereVoiDialog : public QDialog {
    Q_OBJECT

public:
    /// A volume to take the statistics of
    struct Input {
        QString name;
        vtkImageData *values;
        /// What the stored values are multiplied by to be in the units that are shown, and the name of the units ("" for the stored ones, "SUVbw" for a PET)
        double scale;
        QString units;
        /// The point at the center of the sphere, in the coordinates of this volume, in mm
        double center[3];
    };

    /// Creates the dialog for the inputs, with the radius in mm to start with.
    QSphereVoiDialog(const QList<Input> &inputs, double radius, QWidget *parent = nullptr);

    /// Returns the radius that is chosen, in mm.
    double getRadius() const;

private slots:
    /// Computes the statistics again with the radius and the threshold of the dialog.
    void update();

    /// Puts the text of the table in the clipboard.
    void copyToClipboard();

private:
    QList<Input> m_inputs;
    QDoubleSpinBox *m_radiusSpinBox;
    QDoubleSpinBox *m_thresholdSpinBox;
    QTableWidget *m_table;
    QLabel *m_note;
};

} // End namespace udg

#endif
