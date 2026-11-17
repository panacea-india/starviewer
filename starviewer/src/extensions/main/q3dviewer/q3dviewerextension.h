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

#ifndef UDGQ3DVIEWEREXTENSION_H
#define UDGQ3DVIEWEREXTENSION_H

#include "ui_q3dviewerextensionbase.h"

#include <QDir>
#include <QMap>

#include "savedview.h"
#include "transferfunction.h"

class QAction;
class QMenu;
class QToolButton;
class QModelIndex;
class QStandardItemModel;

namespace udg {

class Q3DViewer;
class Volume;
class ToolManager;

/**
 * ...
*/
class Q3DViewerExtension : public QWidget, private ::Ui::Q3DViewerExtensionBase {
Q_OBJECT
public:
    Q3DViewerExtension(QWidget *parent = 0);
    virtual ~Q3DViewerExtension();

    /// Li donem l'input a processar
    void setInput(Volume *input);

public slots:
    /// Asks for a threshold and a file, and writes the surface of the volume at that threshold as an STL file.
    void exportStl();

    /// Asks for a name and saves the camera of the 3D view with it, for this series of images.
    void saveCurrentView();
    /// Puts the camera of the 3D view where the saved view (by position in the list) has it.
    void applyView(int index);
    /// Asks which saved view to forget.
    void deleteSavedView();
    /// Makes a movie of the 3D view: pictures (and a video, if ffmpeg is on the computer) of the camera turning around the volume or going through the saved views.
    void exportMovie();
    /// Takes out of the 3D view everything at or above a value that is not the largest piece (the table, the loose spots). The loaded volume is not changed.
    void sculptKeepLargestPiece();
    /// Shows the volume as it was loaded, before any sculpting.
    void sculptRestore();

public slots:
    /// Aplica una funció de transferència predefinida a la visualització.
    void applyPresetClut(const QString &clutName);

    /// Aplica la funció de transferència passada a la visualització.
    void applyClut(const TransferFunction &clut, bool preset = false);

    void setScalarRange(double min, double max);

    void updateUiForBlendMode(int blendModeIndex);

    /// Actualitza la vista d'acord amb tots els paràmetres actuals de la interfície.
    void updateView(bool fast = true);

private:
    /// Posa a punt les tools que es poden fer servir en l'extensió
    void initializeTools();

    /// Obté la llista de funcions de transferència predefinides.
    void loadClutPresets();

    /// Carrega els estils de rendering predefinits.
    void loadRenderingStyles();

    /// Estableix les connexions de signals i slots
    void createConnections();

    /// Fa que es cridi updateView() quan canvia qualsevol element de la interfície.
    void enableAutoUpdate();
    /// Fa que no es cridi updateView() quan canvia qualsevol element de la interfície.
    void disableAutoUpdate();

private slots:
    void render();
    void loadClut();
    void saveClut();
    void switchEditor();
    void applyEditorClut();
    void toggleClutEditor();
    void hideClutEditor();
    /// Canvia l'estat del botó del custom style depenent de si s'està mostrant o no l'editor en aquell moment.
    void setCustomStyleButtonStateBySplitter();
    void changeViewerTransferFunction();
    void applyRenderingStyle(const QModelIndex &index);
    void showScreenshotsExporterDialog();
    void fillViewsMenu();
    void fillSculptMenu();

private:
    QString savedViewsFilePath() const;
    void loadSavedViews();
    void storeSavedViews();
    SavedView currentView() const;
    void showView(const SavedView &view);
    void showVolume(Volume *volume);

private:
    /// El volum d'input
    Volume *m_input;

    /// The volume as it was loaded, and the volume that has parts taken out of it (if any) that is shown in its place
    Volume *m_originalInput;
    Volume *m_sculptedInput;

    QMenu *m_viewsMenu;
    QMenu *m_sculptMenu;
    QList<SavedView> m_savedViews;

    /// Gestor de tools
    ToolManager *m_toolManager;

    /// Directori de funcions de transferència predefinides.
    QDir m_clutsDir;
    /// Mapa entre noms de funcions de transferència i el nom de fitxer corresponent.
    QMap<QString, QString> m_clutNameToFileName;

    /// Última clut aplicada.
    TransferFunction m_currentClut;

    /// Serà cert abans d'entrar el primer input.
    bool m_firstInput;

    /// Model que guarda els estils de rendering.
    QStandardItemModel *m_renderingStyleModel;

    /// Temporitzador que serveix per fer renderings amb qualitat després d'un rendering ràpid.
    QTimer *m_timer;
};

} // end namespace udg

#endif
