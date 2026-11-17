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

#include "openglsupport.h"

#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QSurfaceFormat>

#include "logging.h"
#include "systemrequirements.h"

namespace udg {

namespace {

bool checked = false;
bool available = false;
QString foundVersion;

}

bool OpenGLSupport::isAvailable()
{
    check();
    return available;
}

QString OpenGLSupport::getFoundVersion()
{
    check();
    return foundVersion;
}

QString OpenGLSupport::getRequiredVersion()
{
    return SystemRequirements().getMinimumGPUOpenGLVersion();
}

bool OpenGLSupport::isVersionAtLeast(int major, int minor, const QString &required)
{
    QStringList parts = required.split('.');
    int requiredMajor = parts.value(0).toInt();
    int requiredMinor = parts.value(1).toInt();
    return major > requiredMajor || (major == requiredMajor && minor >= requiredMinor);
}

void OpenGLSupport::check()
{
    if (checked)
    {
        return;
    }
    checked = true;

    // The format that VTK's widget asks for is the default one (see main.cpp), so this is the context the viewers will get.
    QOpenGLContext context;
    context.setFormat(QSurfaceFormat::defaultFormat());
    if (!context.create())
    {
        ERROR_LOG(QString("No OpenGL context could be created. OpenGL %1 or newer is needed for the viewers.").arg(getRequiredVersion()));
        return;
    }

    QOffscreenSurface surface;
    surface.setFormat(context.format());
    surface.create();
    if (!surface.isValid() || !context.makeCurrent(&surface))
    {
        ERROR_LOG(QString("The OpenGL context could not be used. OpenGL %1 or newer is needed for the viewers.").arg(getRequiredVersion()));
        return;
    }

    QString description = QString::fromLatin1(reinterpret_cast<const char*>(context.functions()->glGetString(GL_VERSION)));
    QString renderer = QString::fromLatin1(reinterpret_cast<const char*>(context.functions()->glGetString(GL_RENDERER)));
    QSurfaceFormat format = context.format();
    context.doneCurrent();

    foundVersion = QString("%1.%2").arg(format.majorVersion()).arg(format.minorVersion());
    available = isVersionAtLeast(format.majorVersion(), format.minorVersion(), getRequiredVersion());
    INFO_LOG(QString("OpenGL %1 (%2, %3). Version needed: %4. Available: %5")
             .arg(foundVersion).arg(description).arg(renderer).arg(getRequiredVersion()).arg(available ? "yes" : "no"));
    if (!available)
    {
        ERROR_LOG(QString("OpenGL %1 is older than the %2 needed for the viewers.").arg(foundVersion).arg(getRequiredVersion()));
    }
}

} // End namespace udg
