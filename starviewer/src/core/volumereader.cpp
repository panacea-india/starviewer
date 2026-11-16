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

#include "volumereader.h"

#include "image.h"
#include "logging.h"
#include "pixeldatalengthcheck.h"
#include "postprocessor.h"
#include "starviewerapplication.h"
#include "volume.h"
#include "volumepixeldatareader.h"
#include "volumepixeldatareaderfactory.h"

#include <QMessageBox>
#include <QtConcurrentMap>

namespace udg {

namespace {

// Returns the frame number of the given image.
int getFrameNumber(const Image *image)
{
    return image->getFrameNumber();
}

}

VolumeReader::VolumeReader(QObject *parent)
    : QObject(parent), m_volumePixelDataReader(0), m_abortRequested(false)
{
     m_lastError = VolumePixelDataReader::NoError;
}

VolumeReader::~VolumeReader()
{
    if (m_volumePixelDataReader)
    {
        delete m_volumePixelDataReader;
    }
}

void VolumeReader::executePixelDataReader(Volume *volume)
{
    if (!volume)
    {
        DEBUG_LOG("The given volume is NULL! Returning.");
        return;
    }

    m_lastError = VolumePixelDataReader::NoError;

    if (volume->isPixelDataLoaded())
    {
        emit progress(100);
        return;
    }

    // Obtenim els arxius que hem de llegir
    QStringList fileList = this->getFilesToRead(volume);
    if (!fileList.isEmpty())
    {
        if (m_abortRequested)
        {
            m_lastError = VolumePixelDataReader::ReadAborted;
            return;
        }

        // Posem a punt el reader i llegim les dades
        this->setUpReader(volume);

        // Set the frame numbers to the pixel data reader (needed for multiframe files)
        QList<int> frameNumbers = QtConcurrent::blockingMapped(volume->getImages(), getFrameNumber);
        m_volumePixelDataReader->setFrameNumbers(frameNumbers);

        if (m_abortRequested)
        {
            m_lastError = VolumePixelDataReader::ReadAborted;
        }
        else if (!m_volumePixelDataReader->checksPixelDataLength() && hasShortPixelData(fileList))
        {
            m_lastError = VolumePixelDataReader::CannotReadFile;
            volume->convertToNeutralVolume();
            this->logWarningLastError(fileList);
        }
        else
        {
            m_lastError = m_volumePixelDataReader->read(fileList);
            if (m_lastError == VolumePixelDataReader::NoError)
            {
                // Tot ha anat ok, assignem les dades al volum
                volume->setPixelData(m_volumePixelDataReader->getVolumePixelData());
                runPostprocessors(volume);
                fixSpacingIssues(volume);
            }
            else
            {
                volume->convertToNeutralVolume();
                this->logWarningLastError(fileList);
            }
        }
    }
}

void VolumeReader::read(Volume *volume)
{
    this->executePixelDataReader(volume);
    this->showMessageBoxWithLastError();
}

bool VolumeReader::readWithoutShowingError(Volume *volume)
{
    this->executePixelDataReader(volume);

    return m_lastError == VolumePixelDataReader::NoError;
}

void VolumeReader::requestAbort()
{
    if (m_volumePixelDataReader)
    {
        m_volumePixelDataReader->requestAbort();
    }

    m_abortRequested = true;
}

void VolumeReader::showMessageBoxWithLastError() const
{
    if (m_lastError == VolumePixelDataReader::NoError)
    {
        return;
    }

    QString messageBoxTitle = "";
    switch (m_lastError)
    {
        case VolumePixelDataReader::OutOfMemory:
            messageBoxTitle = tr("Out of memory");
            break;

        case VolumePixelDataReader::MissingFile:
            messageBoxTitle = tr("Missing Files");
            break;

        case VolumePixelDataReader::CannotReadFile:
            messageBoxTitle = tr("Cannot Read Files");
            break;

        case VolumePixelDataReader::UnknownError:
            messageBoxTitle = tr("Unknown Error");
            break;
    }

    QMessageBox::warning(0, messageBoxTitle, this->getLastErrorMessageToUser());
}

QString VolumeReader::getLastErrorMessageToUser() const
{
    switch (m_lastError)
    {
        case VolumePixelDataReader::OutOfMemory:
            return tr("There's not enough memory to load the series you requested. Try to close all the open %1 windows and restart "
                      "the application and try again. If the problem persists, adding more RAM memory or switching to a 64-bit operating "
                      "system may solve the problem.").arg(ApplicationNameString);
        case VolumePixelDataReader::MissingFile:
            return tr("%1 could not find the corresponding files for this series. Maybe they had been removed or are corrupted.").arg(ApplicationNameString);
        case VolumePixelDataReader::CannotReadFile:
            return tr("%1 could not read the requested files for this series.").arg(ApplicationNameString);
        case VolumePixelDataReader::ReadAborted:
            return tr("Loading of data has been aborted.");
        case VolumePixelDataReader::UnknownError:
            return tr("%1 found an unexpected error reading this series. No series data has been loaded.").arg(ApplicationNameString);
        case VolumePixelDataReader::NoError:
            return "";
    }
    return "";
}

void VolumeReader::logWarningLastError(const QStringList &fileList) const
{
    switch (m_lastError)
    {
        case VolumePixelDataReader::OutOfMemory:
            WARN_LOG("The files cannot be loaded because they do not fit in memory\n" + fileList.join("\n"));
            break;

        case VolumePixelDataReader::MissingFile:
            WARN_LOG("The files cannot be loaded because files are missing");
            break;

        case VolumePixelDataReader::CannotReadFile:
            WARN_LOG("The following files cannot be read (unknown reason)\n" + fileList.join("\n"));
            break;

        case VolumePixelDataReader::UnknownError:
            WARN_LOG("The files cannot be loaded because of an unknown error");
            break;
    }
}

void VolumeReader::fixSpacingIssues(Volume *volume)
{
    if (!volume)
    {
        DEBUG_LOG("The volume is null!");
        return;
    }

    if (volume->getNumberOfPhases() > 1 && volume->getNumberOfSlicesPerPhase() > 1)
    {
        double zSpacing = qAbs(Image::distance(volume->getImage(0)) - Image::distance(volume->getImage(1)));
        DEBUG_LOG(QString("Fixing the z-spacing for a volume with phases. z-spacing read (badly computed): %1  - New correctly computed z-spacing: %2")
            .arg(volume->getSpacing()[2]).arg(zSpacing));
        
        volume->getSpacing()[2] = zSpacing;
    }
}

QStringList VolumeReader::getFilesToRead(Volume *volume) const
{
    QStringList fileList;
    foreach (Image *image, volume->getImages())
    {
        // Evitem afegir més vegades l'arxiu si aquest és multiframe
        if (!fileList.contains(image->getPath()))
        {
            fileList << image->getPath();
        }
    }

    return fileList;
}

bool VolumeReader::hasShortPixelData(const QStringList &fileList) const
{
    for (const QString &filename : fileList)
    {
        QString problem = PixelDataLengthCheck::checkFile(filename);

        if (!problem.isEmpty())
        {
            ERROR_LOG(problem);
            return true;
        }
    }

    return false;
}

void VolumeReader::setUpReader(Volume *volume)
{
    // Eliminem un lector anterior si l'havia
    if (m_volumePixelDataReader)
    {
        delete m_volumePixelDataReader;
    }

    VolumePixelDataReaderFactory readerFactory;
    readerFactory.setVolume(volume);
    m_volumePixelDataReader = readerFactory.getReader();
    m_postprocessorsQueue = readerFactory.getPostprocessors();

    // Connectem les senyals de notificació de progrés
    connect(m_volumePixelDataReader, SIGNAL(progress(int)), SIGNAL(progress(int)));
}

void VolumeReader::runPostprocessors(Volume *volume)
{
    while (!m_postprocessorsQueue.isEmpty())
    {
        m_postprocessorsQueue.dequeue()->postprocess(volume);
    }
}

} // End namespace udg
