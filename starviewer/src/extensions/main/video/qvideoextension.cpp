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

#include "qvideoextension.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSlider>
#include <QSplitter>
#include <QVBoxLayout>
#include <QVideoWidget>

#include "dicomtagreader.h"
#include "logging.h"
#include "patient.h"
#include "study.h"
#include "videoobject.h"

namespace udg {

QVideoExtension::QVideoExtension(QWidget *parent)
    : QWidget(parent)
{
    QVBoxLayout *layout = new QVBoxLayout(this);

    m_problemLabel = new QLabel(this);
    m_problemLabel->setWordWrap(true);
    m_problemLabel->setStyleSheet("background-color: #ffe08a; color: black; padding: 4px;");
    m_problemLabel->hide();
    layout->addWidget(m_problemLabel);

    QSplitter *splitter = new QSplitter(Qt::Horizontal, this);
    m_listWidget = new QListWidget(splitter);

    QWidget *player = new QWidget(splitter);
    QVBoxLayout *playerLayout = new QVBoxLayout(player);
    m_videoWidget = new QVideoWidget(player);
    m_videoWidget->setMinimumSize(320, 240);
    playerLayout->addWidget(m_videoWidget, 1);

    QHBoxLayout *controls = new QHBoxLayout();
    m_playButton = new QPushButton(tr("Play"), player);
    m_positionSlider = new QSlider(Qt::Horizontal, player);
    controls->addWidget(m_playButton);
    controls->addWidget(m_positionSlider, 1);
    playerLayout->addLayout(controls);

    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({220, 800});
    layout->addWidget(splitter, 1);

    m_player = new QMediaPlayer(this);
    m_player->setVideoOutput(m_videoWidget);

    connect(m_listWidget, &QListWidget::currentItemChanged, [this](QListWidgetItem *current, QListWidgetItem*) { loadVideo(current); });
    connect(m_playButton, &QPushButton::clicked, this, &QVideoExtension::togglePlay);
    connect(m_player, &QMediaPlayer::stateChanged, this, &QVideoExtension::updatePlayButton);
    connect(m_player, QOverload<QMediaPlayer::Error>::of(&QMediaPlayer::error), this, &QVideoExtension::showPlayerError);
    connect(m_player, &QMediaPlayer::mediaStatusChanged, this, &QVideoExtension::showMediaStatus);
    connect(m_player, &QMediaPlayer::durationChanged, [this](qint64 duration) { m_positionSlider->setRange(0, static_cast<int>(duration)); });
    connect(m_player, &QMediaPlayer::positionChanged, [this](qint64 position) {
        if (!m_positionSlider->isSliderDown())
        {
            m_positionSlider->setValue(static_cast<int>(position));
        }
    });
    connect(m_positionSlider, &QSlider::sliderMoved, [this](int position) { m_player->setPosition(position); });
}

QVideoExtension::~QVideoExtension()
{
    m_player->stop();
    m_player->setMedia(QMediaContent());
}

void QVideoExtension::setPatient(Patient *patient)
{
    m_listWidget->clear();
    m_problemLabel->hide();

    if (!patient)
    {
        DEBUG_LOG("Received a null patient");
        return;
    }

    int count = 0;

    for (Study *study : patient->getStudies())
    {
        for (const VideoObject &video : study->getVideos())
        {
            count++;
            QString name = video.getDescription().isEmpty() ? tr("Video %1").arg(count) : video.getDescription();
            QString details = tr("%1, %2 x %3, %4 s").arg(video.getFormatName()).arg(video.getColumns()).arg(video.getRows())
                              .arg(video.getDurationInSeconds(), 0, 'f', 1);
            QListWidgetItem *item = new QListWidgetItem(name + "\n" + details, m_listWidget);
            item->setData(Qt::UserRole, video.getPath());
        }
    }

    if (count > 0)
    {
        m_listWidget->setCurrentRow(0);
    }
}

void QVideoExtension::showProblem(const QString &text)
{
    m_problemLabel->setText(text);
    m_problemLabel->show();
}

void QVideoExtension::loadVideo(QListWidgetItem *item)
{
    m_player->stop();
    m_problemLabel->hide();

    if (!item)
    {
        return;
    }

    // The same file is read again to get the bitstream: the list keeps only the path
    DICOMTagReader reader(item->data(Qt::UserRole).toString());
    VideoObject video = VideoObject::fromDICOM(reader);

    if (video.isNull())
    {
        showProblem(tr("This file is not a video."));
        return;
    }

    m_currentFormat = video.getFormatName();

    if (!m_temporaryDir.isValid())
    {
        showProblem(tr("A temporary folder for the video could not be made."));
        return;
    }

    QString file = m_temporaryDir.filePath(video.getSopInstanceUid() + VideoObject::getFileExtension(video.getTransferSyntaxUid()));
    QString error;

    if (!video.extractTo(file, &error))
    {
        ERROR_LOG(QString("The video %1 could not be prepared: %2").arg(video.getPath(), error));
        showProblem(tr("This video cannot be shown: %1").arg(error));
        return;
    }

    m_player->setMedia(QUrl::fromLocalFile(file));

    // Without a media service no video can be played: say it now, not when the user presses Play
    if (!m_player->isAvailable())
    {
        showProblem(tr("This computer cannot play this %1 video: it has no media player service. The decoder for the format is part of the operating system or "
                       "a package to install (on Windows N editions, the Media Feature Pack).").arg(m_currentFormat));
    }
}

void QVideoExtension::togglePlay()
{
    if (m_player->state() == QMediaPlayer::PlayingState)
    {
        m_player->pause();
    }
    else
    {
        m_player->play();
    }
}

void QVideoExtension::updatePlayButton()
{
    m_playButton->setText(m_player->state() == QMediaPlayer::PlayingState ? tr("Pause") : tr("Play"));
}

void QVideoExtension::showPlayerError()
{
    ERROR_LOG(QString("The video player reported: %1").arg(m_player->errorString()));
    showProblem(tr("This computer cannot play this %1 video: %2. The decoder for the format is part of the operating system or a package "
                   "to install (on Windows N editions, the Media Feature Pack).").arg(m_currentFormat, m_player->errorString()));
}

void QVideoExtension::showMediaStatus(QMediaPlayer::MediaStatus status)
{
    if (status == QMediaPlayer::InvalidMedia)
    {
        showPlayerError();
    }
}

} // namespace udg
