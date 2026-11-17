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

#ifndef UDGTIMEINTENSITYCURVETOOL_H
#define UDGTIMEINTENSITYCURVETOOL_H

#include "tool.h"

namespace udg {

class Q2DViewer;

/**
    Click a point of an image of a series with several phases (a dynamic study) to see how the values of the pixels around it change from phase to phase: a curve with a table,
    a radius to change and a button to save it as CSV. See TimeIntensityCurve.
 */
class TimeIntensityCurveTool : public Tool {
    Q_OBJECT

public:
    explicit TimeIntensityCurveTool(QViewer *viewer, QObject *parent = nullptr);
    ~TimeIntensityCurveTool() override;

    void handleEvent(unsigned long eventId) override;

private:
    void showCurve();

private:
    Q2DViewer *m_2DViewer;
};

} // End namespace udg

#endif
