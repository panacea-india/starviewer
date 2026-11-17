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

#ifndef UDGCURVEDMPRTOOL_H
#define UDGCURVEDMPRTOOL_H

#include <QVector>

#include "tool.h"

namespace udg {

class Q2DViewer;

/**
    A tool for the curved MPR: the user clicks points along a winding structure (on the same slice or on different ones), and a double click or the Enter key
    straightens the volume along the curve through them (see CurvedReformation) and shows the result in a dialog. Escape forgets the points.
 */
class CurvedMprTool : public Tool {
    Q_OBJECT

public:
    explicit CurvedMprTool(QViewer *viewer, QObject *parent = nullptr);
    ~CurvedMprTool() override;

    void handleEvent(unsigned long eventId) override;

private slots:
    void reset();

private:
    void addPoint();
    void finish();

private:
    Q2DViewer *m_2DViewer;
    /// x, y, z of each point of the path
    QVector<double> m_points;
};

} // End namespace udg

#endif
