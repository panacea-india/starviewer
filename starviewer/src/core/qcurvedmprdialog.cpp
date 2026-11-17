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

#include "qcurvedmprdialog.h"

#include <algorithm>

#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QVBoxLayout>

#include <vtkImageData.h>

namespace udg {

QImage QCurvedMprDialog::toGrayImage(vtkImageData *image, double window, double level)
{
    int dimensions[3];
    image->GetDimensions(dimensions);

    double lower = level - window / 2.0;
    double upper = level + window / 2.0;

    if (window <= 0.0)
    {
        double range[2];
        image->GetScalarRange(range);
        lower = range[0];
        upper = range[1];
    }

    double span = std::max(upper - lower, 1e-9);
    QImage gray(dimensions[0], dimensions[1], QImage::Format_Grayscale8);

    for (int row = 0; row < dimensions[1]; row++)
    {
        uchar *line = gray.scanLine(row);

        for (int column = 0; column < dimensions[0]; column++)
        {
            double value = (image->GetScalarComponentAsDouble(column, row, 0, 0) - lower) / span;
            line[column] = static_cast<uchar>(std::max(0.0, std::min(1.0, value)) * 255.0 + 0.5);
        }
    }

    return gray;
}

QCurvedMprDialog::QCurvedMprDialog(const CurvedReformation::Result &result, double window, double level, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Curved MPR"));

    QVBoxLayout *layout = new QVBoxLayout(this);

    QLabel *summary = new QLabel(tr("Path length: %1 mm. The path is in the middle of each image and goes from the top down. The two views are a quarter of a turn "
                                    "apart. The distance across an image is true only in its own plane.").arg(result.length, 0, 'f', 1), this);
    summary->setWordWrap(true);
    layout->addWidget(summary);

    QHBoxLayout *images = new QHBoxLayout();
    QVBoxLayout *first = new QVBoxLayout();
    first->addWidget(new QLabel(tr("View 1"), this));
    first->addWidget(createImageLabel(result.normalImage, window, level));
    QVBoxLayout *second = new QVBoxLayout();
    second->addWidget(new QLabel(tr("View 2"), this));
    second->addWidget(createImageLabel(result.binormalImage, window, level));
    images->addLayout(first);
    images->addLayout(second);
    layout->addLayout(images);

    QPushButton *close = new QPushButton(tr("Close"), this);
    connect(close, &QPushButton::clicked, this, &QDialog::accept);
    layout->addWidget(close, 0, Qt::AlignRight);
}

QLabel* QCurvedMprDialog::createImageLabel(vtkImageData *image, double window, double level)
{
    QLabel *label = new QLabel(this);
    label->setObjectName("curvedImage");
    QImage gray = toGrayImage(image, window, level);
    // Each row is a step along the path and each column a step across: the image is shown with the same size for both, at a scale that makes it easy to see
    double scale = std::max(1.0, std::min(4.0, 600.0 / std::max(gray.width(), gray.height())));
    label->setPixmap(QPixmap::fromImage(gray.scaled(static_cast<int>(gray.width() * scale), static_cast<int>(gray.height() * scale * image->GetSpacing()[1] / image->GetSpacing()[0]),
                                                    Qt::IgnoreAspectRatio, Qt::FastTransformation)));
    return label;
}

} // End namespace udg
