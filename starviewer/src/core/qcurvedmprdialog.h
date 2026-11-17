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

#ifndef UDGQCURVEDMPRDIALOG_H
#define UDGQCURVEDMPRDIALOG_H

#include <QDialog>

#include "curvedreformation.h"

class QLabel;

namespace udg {

/**
    Shows the two straightened views of a curved reformation side by side, with the window and level of the viewer it was made from, and the length of the path.
    The rows of the images are along the path, from its first point at the top, and the path is in the middle column.
 */
class QCurvedMprDialog : public QDialog {
    Q_OBJECT

public:
    /// Creates the dialog for a successful result. The window and level are the ones used to turn the values into grays. If the window is not positive, the range of each image is used.
    QCurvedMprDialog(const CurvedReformation::Result &result, double window, double level, QWidget *parent = nullptr);

    /// Returns the 8 bit gray image that is shown for an image of the result: the values in the window around the level go from black to white.
    static QImage toGrayImage(vtkImageData *image, double window, double level);

private:
    QLabel *createImageLabel(vtkImageData *image, double window, double level);
};

} // End namespace udg

#endif
