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

#ifndef UDGSPHEREVOITOOL_H
#define UDGSPHEREVOITOOL_H

#include "tool.h"

namespace udg {

class Q2DViewer;

/**
    A tool that takes the statistics of a sphere around the point where the user clicks: the volume, mean, maximum, peak, minimum and deviation of each volume that the viewer
    shows, in a dialog where the radius and a threshold can be changed. The values of a PET are given as SUV when the study has what it takes. The sphere is of the volume,
    in 3D, not of the slice.
 */
class SphereVoiTool : public Tool {
    Q_OBJECT

public:
    explicit SphereVoiTool(QViewer *viewer, QObject *parent = nullptr);
    ~SphereVoiTool() override;

    void handleEvent(unsigned long eventId) override;

private:
    Q2DViewer *m_2DViewer;
};

} // End namespace udg

#endif
