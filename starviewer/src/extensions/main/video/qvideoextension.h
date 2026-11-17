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

#ifndef UDG_QVIDEOEXTENSION_H
#define UDG_QVIDEOEXTENSION_H

#include <QWidget>

#include <QMediaPlayer>
#include <QTemporaryDir>

class QLabel;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QSlider;
class QVideoWidget;

namespace udg {

class Patient;

/**
 * @brief QVideoExtension lists the DICOM videos of a Patient and plays the one that is chosen.
 *
 * The bitstream is written to a temporary file and played by Qt Multimedia, so the decoding is done by the operating system. If it has no decoder
 * for the video, the extension says which format needs one instead of showing nothing.
 */
class QVideoExtension : public QWidget {

    Q_OBJECT

public:
    explicit QVideoExtension(QWidget *parent = nullptr);
    ~QVideoExtension() override;

    /// Sets the given patient to the extension. All the videos of its studies are listed, and the first one is ready to play.
    void setPatient(Patient *patient);

private slots:
    /// Writes the video of the given item to a file and loads it in the player.
    void loadVideo(QListWidgetItem *item);

    void togglePlay();
    void updatePlayButton();
    void showPlayerError();
    void showMediaStatus(QMediaPlayer::MediaStatus status);

private:
    void showProblem(const QString &text);

private:
    QListWidget *m_listWidget;
    QVideoWidget *m_videoWidget;
    QMediaPlayer *m_player;
    QPushButton *m_playButton;
    QSlider *m_positionSlider;
    QLabel *m_problemLabel;
    QTemporaryDir m_temporaryDir;
    QString m_currentFormat;
};

} // namespace udg

#endif // UDG_QVIDEOEXTENSION_H
