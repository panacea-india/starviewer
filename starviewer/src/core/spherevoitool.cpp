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

#include "spherevoitool.h"

#include <QSettings>

#include <vtkCommand.h>
#include <vtkImageData.h>

#include "image.h"
#include "logging.h"
#include "q2dviewer.h"
#include "qspherevoidialog.h"
#include "series.h"
#include "standarduptakevaluemeasurehandler.h"
#include "volume.h"

namespace udg {

SphereVoiTool::SphereVoiTool(QViewer *viewer, QObject *parent)
    : Tool(viewer, parent)
{
    m_toolName = "SphereVoiTool";
    m_2DViewer = Q2DViewer::castFromQViewer(viewer);
}

SphereVoiTool::~SphereVoiTool()
{
}

void SphereVoiTool::handleEvent(unsigned long eventId)
{
    if (eventId != vtkCommand::LeftButtonPressEvent || !m_2DViewer || !m_2DViewer->hasInput())
    {
        return;
    }

    QList<QSphereVoiDialog::Input> inputs;

    for (int i = 0; i < m_2DViewer->getNumberOfInputs(); i++)
    {
        Volume *volume = m_2DViewer->getInput(i);

        if (!volume || !volume->getVtkData() || volume->getImages().isEmpty())
        {
            continue;
        }

        Image *image = volume->getImages().first();
        Series *series = image->getParentSeries();
        QString name = series && !series->getDescription().trimmed().isEmpty() ? series->getDescription().trimmed() : QObject::tr("Volume %1").arg(i + 1);

        QSphereVoiDialog::Input input;

        // The point under the cursor in the coordinates of this volume, with the depth of the slice that is shown. If the cursor is not on this volume's image, it is left out
        if (!m_2DViewer->getCurrentCursorImageCoordinateOnInput(input.center, i))
        {
            continue;
        }

        input.name = name;
        input.values = volume->getVtkData();
        input.scale = 1.0;

        // A PET in SUV, if the study says how: the SUV is the stored value times a factor
        if (series && series->getModality() == "PT")
        {
            StandardUptakeValueMeasureHandler handler;
            handler.setImage(image);

            if (handler.canComputePreferredFormula())
            {
                double factor = handler.computePreferredFormula(1.0);

                if (factor > 0.0)
                {
                    input.scale = factor;
                    input.units = handler.getComputedFormulaLabel();
                }
            }
        }

        inputs << input;
    }

    if (inputs.isEmpty())
    {
        return;
    }

    QSettings settings;
    double radius = settings.value("SphereVoiTool/radius", 10.0).toDouble();

    QSphereVoiDialog dialog(inputs, radius, m_2DViewer);
    dialog.exec();
    settings.setValue("SphereVoiTool/radius", dialog.getRadius());
}

} // End namespace udg
