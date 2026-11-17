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

#include "q3dviewerextension.h"
#include <QApplication>
#include <QDir>
#include <QToolButton>
#include <QMessageBox>
#include <QInputDialog>
#include <QBoxLayout>
#include "surfaceexporter.h"
#include <algorithm>
#include "movieencoder.h"
#include "flythroughpath.h"
#include "endoscopytransferfunction.h"
#include "savedviewstore.h"
#include "volumesculptor.h"
#include "series.h"
#include "logging.h"
#include <QDateTime>
#include <QMenu>
#include <QProgressDialog>
#include <vtkCamera.h>
#include <vtkPNGWriter.h>
#include <vtkRenderer.h>
#include <vtkWindowToImageFilter.h>
#include "volume.h"
#include "toolmanager.h"
#include "transferfunctionio.h"
#include "renderingstyle.h"
#include "starviewerapplication.h"
#include "q3dviewerextensionsettings.h"
#include "screenshottool.h"
#include "toolproxy.h"
#include "qexportertool.h"
// Qt
#include <QAction>
#include <QFileDialog>
#include <QStandardItemModel>
#include <QTimer>
// Vtk
#include <vtkImageData.h>
// Actualització ràpida
#include <vtkRenderWindow.h>
// Actualització ràpida
#include <vtkRenderWindowInteractor.h>

namespace udg {

namespace {

// Returns the box layout that holds the widget, looking inside the layouts of the layout
QBoxLayout* findBoxLayoutOf(QLayout *layout, QWidget *widget)
{
    if (!layout)
    {
        return nullptr;
    }

    QBoxLayout *box = qobject_cast<QBoxLayout*>(layout);

    if (box && box->indexOf(widget) >= 0)
    {
        return box;
    }

    for (int i = 0; i < layout->count(); i++)
    {
        QBoxLayout *found = findBoxLayoutOf(layout->itemAt(i)->layout(), widget);

        if (found)
        {
            return found;
        }
    }

    return nullptr;
}

}

Q3DViewerExtension::Q3DViewerExtension(QWidget *parent)
 : QWidget(parent)
{
    setupUi(this);
    m_input = nullptr;
    m_originalInput = nullptr;
    m_sculptedInput = nullptr;
    m_viewsMenu = nullptr;
    m_sculptMenu = nullptr;
    Q3DViewerExtensionSettings().init();

    // Creem el temporitzador (s'ha de fer abans del createConnections())
    m_timer = new QTimer(this);
    m_timer->setSingleShot(true);
    /// \todo Poso 1000 ms arbitràriament.
    m_timer->setInterval(1000);

    initializeTools();
    loadClutPresets();
    loadRenderingStyles();
    createConnections();
    updateUiForBlendMode(m_blendModeComboBox->currentIndex());

    m_firstInput = true;

    hideClutEditor();
    m_screenshotsExporterToolButton->setToolTip(tr("Export viewer image to DICOM and send it to a PACS server"));
    m_customStyleToolButton->setToolTip(tr("Show/Hide advanced colour options"));
}

Q3DViewerExtension::~Q3DViewerExtension()
{
    // El que aquí volem fer és forçar a eliminar primer totes les tools abans de que s'esborri el viewer
    // TODO potser caldria refactoritzar el nom d'aquest mètode o crear-ne un per aquesta tasca
    m_toolManager->disableAllToolsTemporarily();
}

void Q3DViewerExtension::initializeTools()
{
    m_toolManager = new ToolManager(this);
    // Obtenim les accions de cada tool que volem
    m_zoomToolButton->setDefaultAction(m_toolManager->registerTool("ZoomTool"));
    m_rotate3DToolButton->setDefaultAction(m_toolManager->registerTool("Rotate3DTool"));
    m_windowLevelToolButton->setDefaultAction(m_toolManager->registerTool("WindowLevelTool"));
    m_toolManager->registerTool("TranslateTool");
    m_clippingBoxToolButton->setDefaultAction(m_toolManager->registerTool("ClippingPlanesTool"));
    m_toolManager->registerTool("ScreenShotTool");
    m_screenShotToolButton->setToolTip(m_toolManager->getRegisteredToolAction("ScreenShotTool")->toolTip());

    // Action tools
    m_axialOrientationButton->setDefaultAction(m_toolManager->registerActionTool("AxialViewActionTool"));
    m_sagitalOrientationButton->setDefaultAction(m_toolManager->registerActionTool("SagitalViewActionTool"));
    m_coronalOrientationButton->setDefaultAction(m_toolManager->registerActionTool("CoronalViewActionTool"));

    // Activem les tools que volem tenir per defecte, això és com si clickéssim a cadascun dels ToolButton
    QStringList defaultTools;
    defaultTools << "ZoomTool" << "TranslateTool" << "Rotate3DTool" << "ScreenShotTool";
    m_toolManager->triggerTools(defaultTools);

    // Registrem al manager les tools que van amb el viewer principal
    // A button to export the surface of the volume, next to the one of the screen shot
    QToolButton *stlButton = new QToolButton(this);
    stlButton->setText(tr("STL"));
    stlButton->setToolTip(tr("Export the surface of the volume as an STL file, for 3D printing"));
    stlButton->setStatusTip(stlButton->toolTip());
    stlButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    connect(stlButton, &QToolButton::clicked, this, &Q3DViewerExtension::exportStl);
    QBoxLayout *buttonsLayout = findBoxLayoutOf(m_screenShotToolButton->parentWidget()->layout(), m_screenShotToolButton);

    // Buttons for saved views, sculpting and the movie, next to the one of the STL
    QToolButton *viewsButton = new QToolButton(this);
    viewsButton->setText(tr("Views"));
    viewsButton->setToolTip(tr("Save the point of view of the 3D image with a name, and come back to it later"));
    viewsButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    viewsButton->setPopupMode(QToolButton::InstantPopup);
    m_viewsMenu = new QMenu(viewsButton);
    viewsButton->setMenu(m_viewsMenu);
    connect(m_viewsMenu, &QMenu::aboutToShow, this, &Q3DViewerExtension::fillViewsMenu);

    QToolButton *sculptButton = new QToolButton(this);
    sculptButton->setText(tr("Sculpt"));
    sculptButton->setToolTip(tr("Take parts out of the 3D image, such as the table, without changing the volume that was loaded"));
    sculptButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    sculptButton->setPopupMode(QToolButton::InstantPopup);
    m_sculptMenu = new QMenu(sculptButton);
    sculptButton->setMenu(m_sculptMenu);
    connect(m_sculptMenu, &QMenu::aboutToShow, this, &Q3DViewerExtension::fillSculptMenu);

    QToolButton *movieButton = new QToolButton(this);
    movieButton->setText(tr("Movie"));
    movieButton->setToolTip(tr("Make a movie of the 3D image turning around, or going through the saved views"));
    movieButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    connect(movieButton, &QToolButton::clicked, this, &Q3DViewerExtension::exportMovie);

    if (buttonsLayout)
    {
        int next = buttonsLayout->indexOf(m_screenShotToolButton) + 1;
        buttonsLayout->insertWidget(next, stlButton);
        buttonsLayout->insertWidget(next + 1, viewsButton);
        buttonsLayout->insertWidget(next + 2, sculptButton);
        buttonsLayout->insertWidget(next + 3, movieButton);
    }

    m_toolManager->setupRegisteredTools(m_3DView);
    m_toolManager->enableRegisteredActionTools(m_3DView);

    QStringList rightButtonExclusiveTools;
    rightButtonExclusiveTools << "Rotate3DTool" << "WindowLevelTool";
    m_toolManager->addExclusiveToolsGroup("RightButtonGroup", rightButtonExclusiveTools);

    // Fem que quan es clicki al botó es faci un screen shot
    ScreenShotTool *screenShotTool = dynamic_cast<ScreenShotTool*>(m_3DView->getToolProxy()->getTool("ScreenShotTool"));
    connect(m_screenShotToolButton, SIGNAL(clicked()), screenShotTool, SLOT(singleCapture()));
}

void Q3DViewerExtension::loadClutPresets()
{
    DEBUG_LOG("loadClutPresets()");

    disconnect(m_clutPresetsComboBox, SIGNAL(currentIndexChanged(const QString&)), this, SLOT(applyPresetClut(const QString&)));

    m_clutPresetsComboBox->clear();

    QString path = QDir::toNativeSeparators(UserDataRootPath + "cluts/");
    m_clutsDir.setPath(path);

    if (!m_clutsDir.exists())
    {
        m_clutsDir.mkpath(path);
    }
    if (!m_clutsDir.isReadable())
    {
        // No es pot accedir al directori
        return;
    }

    QStringList nameFilters;
    nameFilters << "*.tf";

    QStringList clutList = m_clutsDir.entryList(nameFilters);

    foreach (const QString &clutName, clutList)
    {
        TransferFunction *transferFunction = TransferFunctionIO::fromFile(m_clutsDir.absoluteFilePath(clutName));
        if (transferFunction)
        {
            m_clutNameToFileName[transferFunction->name()] = clutName;
            delete transferFunction;
        }
    }

    m_clutPresetsComboBox->addItems(m_clutNameToFileName.keys());
    m_clutPresetsComboBox->setCurrentIndex(-1);

    connect(m_clutPresetsComboBox, SIGNAL(currentIndexChanged(const QString&)), this, SLOT(applyPresetClut(const QString&)));
}

void Q3DViewerExtension::loadRenderingStyles()
{
    m_renderingStyleModel = new QStandardItemModel(this);

    // Per la primera versió creem tot això a pèl.
    // Per cada estil creem un item i li assignem unes dades que seran les que es faran servir a l'hora d'aplicar-lo.
    QStandardItem *item;
    RenderingStyle renderingStyle;
    TransferFunction *transferFunction;

    // The two styles for the flight inside a cavity or a vessel: the lumen is transparent and its wall is the surface
    for (int kind = 0; kind < 2; kind++)
    {
        item = new QStandardItem(kind == 0 ? tr("Inside: air") : tr("Inside: contrast"));
        item->setToolTip(kind == 0 ? tr("For looking from inside an airway, the colon or a sinus: the air is transparent and the tissue is the wall")
                                   : tr("For looking from inside a vessel or a heart cavity full of contrast: the contrast is transparent and the tissue is the wall"));
        renderingStyle.setMethod(RenderingStyle::RayCasting);
        renderingStyle.setShading(true);
        renderingStyle.setAmbientCoefficient(0.3);
        renderingStyle.setDiffuseCoefficient(0.7);
        renderingStyle.setSpecularCoefficient(0.2);
        renderingStyle.setSpecularPower(16.0);
        renderingStyle.setTransferFunction(kind == 0 ? EndoscopyTransferFunction::airFilledLumen() : EndoscopyTransferFunction::contrastFilledLumen());
        item->setData(renderingStyle.toVariant());
        m_renderingStyleModel->appendRow(item);
    }

    item = new QStandardItem(QIcon(":/extensions/Q3DViewerExtension/renderingstyles/spine2.png"), tr("Spine"));
    renderingStyle.setMethod(RenderingStyle::RayCasting);
    renderingStyle.setShading(true);
    renderingStyle.setAmbientCoefficient(0.1);
    renderingStyle.setDiffuseCoefficient(0.7);
    renderingStyle.setSpecularCoefficient(1.0);
    renderingStyle.setSpecularPower(64.0);
    transferFunction = TransferFunctionIO::fromXmlFile(":/extensions/Q3DViewerExtension/renderingstyles/spine2.xml");
    renderingStyle.setTransferFunction(*transferFunction);
    delete transferFunction;
    item->setData(renderingStyle.toVariant());
    m_renderingStyleModel->appendRow(item);

    item = new QStandardItem(QIcon(":/extensions/Q3DViewerExtension/renderingstyles/thorax1.png"), tr("Thorax"));
    renderingStyle.setMethod(RenderingStyle::RayCasting);
    renderingStyle.setShading(true);
    renderingStyle.setAmbientCoefficient(0.1);
    renderingStyle.setDiffuseCoefficient(0.7);
    renderingStyle.setSpecularCoefficient(1.0);
    renderingStyle.setSpecularPower(64.0);
    transferFunction = TransferFunctionIO::fromXmlFile(":/extensions/Q3DViewerExtension/renderingstyles/thorax1.xml");
    renderingStyle.setTransferFunction(*transferFunction);
    delete transferFunction;
    item->setData(renderingStyle.toVariant());
    m_renderingStyleModel->appendRow(item);

    item = new QStandardItem(QIcon(":/extensions/Q3DViewerExtension/renderingstyles/pelvis2.png"), tr("Pelvis"));
    renderingStyle.setMethod(RenderingStyle::RayCasting);
    renderingStyle.setShading(true);
    renderingStyle.setAmbientCoefficient(0.1);
    renderingStyle.setDiffuseCoefficient(0.7);
    renderingStyle.setSpecularCoefficient(1.0);
    renderingStyle.setSpecularPower(64.0);
    transferFunction = TransferFunctionIO::fromXmlFile(":/extensions/Q3DViewerExtension/renderingstyles/pelvis2.xml");
    renderingStyle.setTransferFunction(*transferFunction);
    delete transferFunction;
    item->setData(renderingStyle.toVariant());
    m_renderingStyleModel->appendRow(item);

    item = new QStandardItem(QIcon(":/extensions/Q3DViewerExtension/renderingstyles/cow1.png"), tr("Circle of Willis"));
    renderingStyle.setMethod(RenderingStyle::RayCasting);
    renderingStyle.setShading(true);
    renderingStyle.setAmbientCoefficient(0.1);
    renderingStyle.setDiffuseCoefficient(0.7);
    renderingStyle.setSpecularCoefficient(0.0);
    transferFunction = TransferFunctionIO::fromXmlFile(":/extensions/Q3DViewerExtension/renderingstyles/cow1.xml");
    renderingStyle.setTransferFunction(*transferFunction);
    delete transferFunction;
    item->setData(renderingStyle.toVariant());
    m_renderingStyleModel->appendRow(item);

    item = new QStandardItem(QIcon(":/extensions/Q3DViewerExtension/renderingstyles/carotids2.png"), tr("Carotids"));
    renderingStyle.setMethod(RenderingStyle::RayCasting);
    renderingStyle.setShading(true);
    renderingStyle.setAmbientCoefficient(0.1);
    renderingStyle.setDiffuseCoefficient(0.7);
    renderingStyle.setSpecularCoefficient(1.0);
    renderingStyle.setSpecularPower(64.0);
    transferFunction = TransferFunctionIO::fromXmlFile(":/extensions/Q3DViewerExtension/renderingstyles/carotids2.xml");
    renderingStyle.setTransferFunction(*transferFunction);
    delete transferFunction;
    item->setData(renderingStyle.toVariant());
    m_renderingStyleModel->appendRow(item);

    item = new QStandardItem(QIcon(":/extensions/Q3DViewerExtension/renderingstyles/bones4.png"), tr("Bones"));
    renderingStyle.setMethod(RenderingStyle::RayCasting);
    renderingStyle.setShading(true);
    renderingStyle.setAmbientCoefficient(0.1);
    renderingStyle.setDiffuseCoefficient(0.7);
    renderingStyle.setSpecularCoefficient(1.0);
    renderingStyle.setSpecularPower(64.0);
    transferFunction = TransferFunctionIO::fromXmlFile(":/extensions/Q3DViewerExtension/renderingstyles/bones4.xml");
    renderingStyle.setTransferFunction(*transferFunction);
    delete transferFunction;
    item->setData(renderingStyle.toVariant());
    m_renderingStyleModel->appendRow(item);

    item = new QStandardItem(QIcon(":/extensions/Q3DViewerExtension/renderingstyles/bonehires.png"), tr("Bones 2"));
    renderingStyle.setMethod(RenderingStyle::RayCasting);
    renderingStyle.setShading(true);
    renderingStyle.setAmbientCoefficient(0.1);
    renderingStyle.setDiffuseCoefficient(0.7);
    renderingStyle.setSpecularCoefficient(0.0);
    transferFunction = TransferFunctionIO::fromXmlFile(":/extensions/Q3DViewerExtension/renderingstyles/bonehires.xml");
    renderingStyle.setTransferFunction(*transferFunction);
    delete transferFunction;
    item->setData(renderingStyle.toVariant());
    m_renderingStyleModel->appendRow(item);

    item = new QStandardItem(QIcon(":/extensions/Q3DViewerExtension/renderingstyles/abdomenbones.png"), tr("Abdomen bones"));
    renderingStyle.setMethod(RenderingStyle::RayCasting);
    renderingStyle.setShading(true);
    renderingStyle.setAmbientCoefficient(0.1);
    renderingStyle.setDiffuseCoefficient(0.7);
    renderingStyle.setSpecularCoefficient(1.0);
    renderingStyle.setSpecularPower(64.0);
    transferFunction = TransferFunctionIO::fromXmlFile(":/extensions/Q3DViewerExtension/renderingstyles/abdomenbones.xml");
    renderingStyle.setTransferFunction(*transferFunction);
    delete transferFunction;
    item->setData(renderingStyle.toVariant());
    m_renderingStyleModel->appendRow(item);

    item = new QStandardItem(QIcon(":/extensions/Q3DViewerExtension/renderingstyles/abdomenrunoff1.png"), tr("Abdomen run-off"));
    renderingStyle.setMethod(RenderingStyle::RayCasting);
    renderingStyle.setShading(true);
    renderingStyle.setAmbientCoefficient(0.1);
    renderingStyle.setDiffuseCoefficient(0.7);
    renderingStyle.setSpecularCoefficient(1.0);
    renderingStyle.setSpecularPower(64.0);
    transferFunction = TransferFunctionIO::fromXmlFile(":/extensions/Q3DViewerExtension/renderingstyles/abdomenrunoff1.xml");
    renderingStyle.setTransferFunction(*transferFunction);
    delete transferFunction;
    item->setData(renderingStyle.toVariant());
    m_renderingStyleModel->appendRow(item);

    item = new QStandardItem(QIcon(":/extensions/Q3DViewerExtension/renderingstyles/abdomenslab.png"), tr("Abdomen slab"));
    renderingStyle.setMethod(RenderingStyle::RayCasting);
    renderingStyle.setShading(true);
    renderingStyle.setAmbientCoefficient(0.1);
    renderingStyle.setDiffuseCoefficient(0.7);
    renderingStyle.setSpecularCoefficient(1.0);
    renderingStyle.setSpecularPower(64.0);
    transferFunction = TransferFunctionIO::fromXmlFile(":/extensions/Q3DViewerExtension/renderingstyles/abdomenslab.xml");
    renderingStyle.setTransferFunction(*transferFunction);
    delete transferFunction;
    item->setData(renderingStyle.toVariant());
    m_renderingStyleModel->appendRow(item);

    m_renderingStyleListView->setModel(m_renderingStyleModel);
}

void Q3DViewerExtension::createConnections()
{
    // Actualització del mètode de rendering
    connect(m_blendModeComboBox, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &Q3DViewerExtension::updateUiForBlendMode);

    enableAutoUpdate();

    // Clut editor
    connect(m_loadClutPushButton, SIGNAL(clicked()), SLOT(loadClut()));
    connect(m_saveClutPushButton, SIGNAL(clicked()), SLOT(saveClut()));
    connect(m_switchEditorPushButton, SIGNAL(clicked()), SLOT(switchEditor()));
    connect(m_applyPushButton, SIGNAL(clicked()), SLOT(applyEditorClut()));

    connect(m_customStyleToolButton, SIGNAL(clicked()), SLOT(toggleClutEditor()));

    connect(m_3DView, SIGNAL(transferFunctionChanged()), SLOT(changeViewerTransferFunction()));
    connect(m_3DView, &Q3DViewer::volumeChanged, [this](Volume *volume) {
        double range[2];
        volume->getScalarRange(range);
        this->setScalarRange(range[0], range[1]);
    });

    // Rendering styles
    connect(m_renderingStyleListView, SIGNAL(activated(const QModelIndex&)), SLOT(applyRenderingStyle(const QModelIndex&)));

    connect(m_editorSplitter, SIGNAL(splitterMoved(int, int)), SLOT(setCustomStyleButtonStateBySplitter()));

    // Temporitzador
    connect(m_timer, SIGNAL(timeout()), SLOT(render()));

    // Per mostrar exportació
    connect(m_screenshotsExporterToolButton, SIGNAL(clicked()), SLOT(showScreenshotsExporterDialog()));
}

void Q3DViewerExtension::exportStl()
{
    if (!m_input || !m_input->getVtkData())
    {
        QMessageBox::information(this, ApplicationNameString, tr("There is no volume to export."));
        return;
    }

    double range[2];
    m_input->getVtkData()->GetScalarRange(range);

    bool accepted = false;
    double threshold = QInputDialog::getDouble(this, tr("Export a surface as STL"),
                                               tr("The surface goes where the value of the volume is (the values go from %1 to %2):").arg(range[0]).arg(range[1]),
                                               range[0] + 0.35 * (range[1] - range[0]), range[0], range[1], 1, &accepted);

    if (!accepted)
    {
        return;
    }

    QString path = QFileDialog::getSaveFileName(this, tr("Export a surface as STL"), QString(), tr("STL files (*.stl)"));

    if (path.isEmpty())
    {
        return;
    }

    if (!path.endsWith(".stl", Qt::CaseInsensitive))
    {
        path += ".stl";
    }

    SurfaceExporter::Options options;
    options.threshold = threshold;
    // The small pieces that noise makes are of no use in a model, and a little smoothing takes the staircase of the voxels away
    options.largestPieceOnly = true;
    options.smoothingIterations = 10;

    QApplication::setOverrideCursor(Qt::WaitCursor);
    SurfaceExporter::Result result = SurfaceExporter::exportStl(m_input->getVtkData(), path, options);
    QApplication::restoreOverrideCursor();

    if (!result.success)
    {
        QMessageBox::warning(this, ApplicationNameString, tr("The surface could not be exported: %1").arg(result.message));
        return;
    }

    INFO_LOG(QString("Exported a surface to %1: %2 triangles, %3 mL, closed: %4").arg(path).arg(result.triangles).arg(result.volumeMilliliters).arg(result.closed));

    QString text = tr("The surface was written to %1.\n\n%2 triangles, a volume of %3 mL and an area of %4 cm2, in millimeters.\n\n"
                      "Only the largest piece was kept, and the surface was smoothed. A model of a patient is personal data: keep the file as such.")
                   .arg(QDir::toNativeSeparators(path)).arg(result.triangles).arg(result.volumeMilliliters, 0, 'f', 1).arg(result.areaSquareCentimeters, 0, 'f', 1);

    if (!result.closed)
    {
        text += "\n\n" + result.message;
    }

    QMessageBox::information(this, ApplicationNameString, text);
}

void Q3DViewerExtension::setInput(Volume *input)
{
    // A new volume is shown as it was loaded: what was sculpted belongs to the one before
    delete m_sculptedInput;
    m_sculptedInput = nullptr;
    m_originalInput = input;
    showVolume(input);
    loadSavedViews();
}

void Q3DViewerExtension::showVolume(Volume *volume)
{
    // Another volume of the same grid (a sculpted one, or the original again) is seen from where the camera was, not from the default view
    bool keepCamera = m_input && volume && m_input != volume && m_input->getVtkData() && volume->getVtkData()
                      && std::equal(m_input->getVtkData()->GetDimensions(), m_input->getVtkData()->GetDimensions() + 3, volume->getVtkData()->GetDimensions());
    SavedView camera;

    if (keepCamera)
    {
        camera = currentView();
    }

    m_input = volume;
    m_3DView->setInput(m_input);

    applyClut(m_currentClut);

    if (keepCamera)
    {
        showView(camera);
    }

    this->render();
}

QString Q3DViewerExtension::savedViewsFilePath() const
{
    if (!m_originalInput || !m_originalInput->getSeries())
    {
        return QString();
    }

    return UserDataRootPath + "views/" + SavedViewStore::fileNameForSeries(m_originalInput->getSeries()->getInstanceUID());
}

void Q3DViewerExtension::loadSavedViews()
{
    QString path = savedViewsFilePath();
    m_savedViews = path.isEmpty() ? QList<SavedView>() : SavedViewStore::load(path);
}

void Q3DViewerExtension::storeSavedViews()
{
    QString path = savedViewsFilePath();

    if (path.isEmpty() || !SavedViewStore::save(path, m_savedViews))
    {
        QMessageBox::warning(this, ApplicationNameString, tr("The views could not be saved."));
    }
}

SavedView Q3DViewerExtension::currentView() const
{
    vtkCamera *camera = m_3DView->getRenderer()->GetActiveCamera();
    SavedView view;

    for (int i = 0; i < 3; i++)
    {
        view.position[i] = camera->GetPosition()[i];
        view.focalPoint[i] = camera->GetFocalPoint()[i];
        view.viewUp[i] = camera->GetViewUp()[i];
    }

    view.viewAngle = camera->GetViewAngle();
    view.parallelProjection = camera->GetParallelProjection() != 0;
    view.parallelScale = camera->GetParallelScale();
    return view;
}

void Q3DViewerExtension::showView(const SavedView &view)
{
    vtkCamera *camera = m_3DView->getRenderer()->GetActiveCamera();
    camera->SetPosition(view.position.data());
    camera->SetFocalPoint(view.focalPoint.data());
    camera->SetViewUp(view.viewUp.data());
    camera->SetViewAngle(view.viewAngle);
    camera->SetParallelProjection(view.parallelProjection ? 1 : 0);
    camera->SetParallelScale(view.parallelScale);
    m_3DView->getRenderer()->ResetCameraClippingRange();
    m_3DView->render();
}

void Q3DViewerExtension::fillViewsMenu()
{
    m_viewsMenu->clear();
    m_viewsMenu->addAction(tr("Save this view..."), this, &Q3DViewerExtension::saveCurrentView);
    m_viewsMenu->addAction(tr("Fly along the last curved MPR path"), this, &Q3DViewerExtension::flyAlongPath);

    if (!m_savedViews.isEmpty())
    {
        m_viewsMenu->addSeparator();

        for (int i = 0; i < m_savedViews.size(); i++)
        {
            m_viewsMenu->addAction(m_savedViews.at(i).name, this, [this, i]() { applyView(i); });
        }

        m_viewsMenu->addSeparator();
        m_viewsMenu->addAction(tr("Forget a view..."), this, &Q3DViewerExtension::deleteSavedView);
    }
}

void Q3DViewerExtension::saveCurrentView()
{
    if (!m_input)
    {
        return;
    }

    bool accepted = false;
    QString name = QInputDialog::getText(this, tr("Save this view"), tr("Name of the view:"), QLineEdit::Normal, tr("View %1").arg(m_savedViews.size() + 1), &accepted).trimmed();

    if (!accepted || name.isEmpty())
    {
        return;
    }

    SavedView view = currentView();
    view.name = name;
    m_savedViews << view;
    storeSavedViews();
}

void Q3DViewerExtension::applyView(int index)
{
    if (index >= 0 && index < m_savedViews.size())
    {
        showView(m_savedViews.at(index));
    }
}

void Q3DViewerExtension::deleteSavedView()
{
    QStringList names;

    foreach (const SavedView &view, m_savedViews)
    {
        names << view.name;
    }

    bool accepted = false;
    QString chosen = QInputDialog::getItem(this, tr("Forget a view"), tr("View to forget:"), names, 0, false, &accepted);

    if (accepted)
    {
        m_savedViews.removeAt(names.indexOf(chosen));
        storeSavedViews();
    }
}

void Q3DViewerExtension::fillSculptMenu()
{
    m_sculptMenu->clear();
    m_sculptMenu->addAction(tr("Keep the largest piece..."), this, &Q3DViewerExtension::sculptKeepLargestPiece);
    QAction *restore = m_sculptMenu->addAction(tr("Show the volume as it was loaded"), this, &Q3DViewerExtension::sculptRestore);
    restore->setEnabled(m_sculptedInput != nullptr);
}

void Q3DViewerExtension::sculptKeepLargestPiece()
{
    if (!m_originalInput || !m_originalInput->getVtkData())
    {
        return;
    }

    double range[2];
    m_originalInput->getVtkData()->GetScalarRange(range);

    bool accepted = false;
    double threshold = QInputDialog::getDouble(this, tr("Keep the largest piece"),
                                               tr("Everything at or above this value (the values go from %1 to %2) that is not part of the largest piece is taken out of the 3D image. "
                                                  "The volume that was loaded is not changed.").arg(range[0]).arg(range[1]),
                                               range[0] + 0.35 * (range[1] - range[0]), range[0], range[1], 1, &accepted);

    if (!accepted)
    {
        return;
    }

    QApplication::setOverrideCursor(Qt::WaitCursor);
    VolumeSculptor::Result result = VolumeSculptor::keepLargestPiece(m_originalInput->getVtkData(), threshold);
    QApplication::restoreOverrideCursor();

    if (!result.image)
    {
        QMessageBox::information(this, ApplicationNameString, tr("Nothing in the volume reaches that value, or the volume cannot be sculpted."));
        return;
    }

    Volume *sculpted = new Volume(this);
    sculpted->setImages(m_originalInput->getImages());
    sculpted->setData(result.image);

    Volume *previous = m_sculptedInput;
    m_sculptedInput = sculpted;
    showVolume(sculpted);
    delete previous;

    INFO_LOG(QString("Sculpted the 3D view: kept the largest piece above %1 (%2 voxels kept, %3 taken out)").arg(threshold).arg(result.keptVoxels).arg(result.removedVoxels));
    QMessageBox::information(this, ApplicationNameString, tr("%1 voxels were taken out of the 3D image and %2 were kept. This is a view only: the images of the study are not changed, "
                                                             "and a measurement or an export should not be made from it.").arg(result.removedVoxels).arg(result.keptVoxels));
}

void Q3DViewerExtension::sculptRestore()
{
    if (!m_sculptedInput)
    {
        return;
    }

    Volume *previous = m_sculptedInput;
    m_sculptedInput = nullptr;
    showVolume(m_originalInput);
    delete previous;
}

void Q3DViewerExtension::exportMovie()
{
    if (!m_input)
    {
        QMessageBox::information(this, ApplicationNameString, tr("There is no volume to make a movie of."));
        return;
    }

    QStringList kinds;
    kinds << tr("Turn once around the volume");

    if (m_savedViews.size() >= 2)
    {
        kinds << tr("Go through the saved views, in order");
    }

    QString flightKind = tr("Fly along the last curved MPR path");

    if (FlythroughPath::length(FlythroughPath::lastPath()) > 0.0)
    {
        kinds << flightKind;
    }

    bool accepted = false;
    QString kind = QInputDialog::getItem(this, tr("Movie"), tr("The camera will:"), kinds, 0, false, &accepted);

    if (!accepted)
    {
        return;
    }

    int frames = QInputDialog::getInt(this, tr("Movie"), tr("Number of pictures (25 make a second of video):"), 100, 2, 2000, 1, &accepted);

    if (!accepted)
    {
        return;
    }

    QString parent = QFileDialog::getExistingDirectory(this, tr("Folder for the movie"));

    if (parent.isEmpty())
    {
        return;
    }

    QDir folder(parent);
    QString subfolder = "movie_" + QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");

    if (!folder.mkpath(subfolder))
    {
        QMessageBox::warning(this, ApplicationNameString, tr("The folder for the pictures could not be created."));
        return;
    }

    folder.cd(subfolder);

    SavedView start = currentView();
    QList<SavedView> views = kind == kinds.first() ? start.turntable(frames) : kind == flightKind ? FlythroughPath::views(FlythroughPath::lastPath(), frames, 5.0, 90.0) : SavedView::path(m_savedViews, frames);

    QProgressDialog progress(tr("Making the pictures..."), tr("Cancel"), 0, views.size(), this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(0);

    // A picture of the full quality for each frame
    updateView(false);
    bool failed = false;
    int written = 0;

    for (int i = 0; i < views.size() && !progress.wasCanceled(); i++)
    {
        progress.setValue(i);
        showView(views.at(i));

        vtkSmartPointer<vtkWindowToImageFilter> grabber = vtkSmartPointer<vtkWindowToImageFilter>::New();
        grabber->SetInput(m_3DView->getRenderWindow());
        grabber->ReadFrontBufferOff();
        grabber->Update();

        vtkSmartPointer<vtkPNGWriter> writer = vtkSmartPointer<vtkPNGWriter>::New();
        writer->SetFileName(qPrintable(folder.filePath(MovieEncoder::frameFileName(i))));
        writer->SetInputConnection(grabber->GetOutputPort());
        writer->Write();

        if (writer->GetErrorCode() != 0)
        {
            failed = true;
            break;
        }

        written++;
    }

    progress.setValue(views.size());
    showView(start);

    if (failed)
    {
        QMessageBox::warning(this, ApplicationNameString, tr("A picture could not be written to %1.").arg(QDir::toNativeSeparators(folder.absolutePath())));
        return;
    }

    QString text = tr("%1 pictures were written to %2.").arg(written).arg(QDir::toNativeSeparators(folder.absolutePath()));

    if (written == views.size() && !MovieEncoder::findFfmpeg().isEmpty())
    {
        QString problem = MovieEncoder::encode(folder.absolutePath(), 25, folder.filePath("movie.mp4"));
        text += problem.isEmpty() ? "\n\n" + tr("The video movie.mp4 was made in the same folder.") : "\n\n" + problem;
    }
    else if (written == views.size())
    {
        text += "\n\n" + tr("ffmpeg was not found on this computer, so there is no video: any video editor can join the pictures, which are in order.");
    }
    else
    {
        text += "\n\n" + tr("The movie was cancelled before the last picture.");
    }

    text += "\n\n" + tr("The pictures show the patient: keep them as such.");
    QMessageBox::information(this, ApplicationNameString, text);
}

void Q3DViewerExtension::flyAlongPath()
{
    QVector<double> path = FlythroughPath::lastPath();

    if (!m_input || FlythroughPath::length(path) <= 0.0)
    {
        QMessageBox::information(this, ApplicationNameString, tr("There is no path yet. In the 2D viewer, choose the Curved MPR tool, click points along a vessel or a cavity and press Enter: "
                                                                  "then come back here. The path is in the coordinates of the volume that was open in the 2D viewer."));
        return;
    }

    // About 25 millimeters of path a second
    int frames = qBound(40, static_cast<int>(FlythroughPath::length(path) / 1.0), 600);
    QList<SavedView> views = FlythroughPath::views(path, frames, 5.0, 90.0);
    SavedView start = currentView();

    for (const SavedView &view : views)
    {
        showView(view);
        QApplication::processEvents();
    }

    showView(start);
}

void Q3DViewerExtension::setScalarRange(double min, double max)
{
    int minimum = qRound(min);
    int maximum = qRound(max);
    m_gradientEditor->setMinimum(minimum);
    m_gradientEditor->setMaximum(maximum);

    // Workaround for #2824: the editor by values doesn't support such small ranges
    if (static_cast<int>(min) == static_cast<int>(max))
    {
        m_editorsStackedWidget->setCurrentIndex(0);
        m_switchEditorPushButton->setEnabled(false);
    }
    else
    {
        m_switchEditorPushButton->setEnabled(true);
        m_editorByValues->setMinimum(minimum);
        m_editorByValues->setMaximum(maximum);
    }

    m_isoValueSpinBox->setMinimum(minimum);
    m_isoValueSpinBox->setMaximum(maximum);

    if (m_firstInput)
    {
        m_currentClut.set(min, Qt::black, 0.0);
        m_currentClut.set(max, Qt::white, 1.0);
        m_firstInput = false;
    }
}

void Q3DViewerExtension::applyPresetClut(const QString &clutName)
{
    DEBUG_LOG("applyPresetClut()");

    const QString &fileName = m_clutNameToFileName[clutName];
    TransferFunction *transferFunction = TransferFunctionIO::fromFile(m_clutsDir.absoluteFilePath(QDir::toNativeSeparators(fileName)));
    if (transferFunction)
    {
        //m_currentClut = *transferFunction;
        applyClut(*transferFunction, true);
        this->render();
        //m_3DView->setTransferFunction(transferFunction);
        delete transferFunction;
    }
    //this->render();
}

void Q3DViewerExtension::applyClut(const TransferFunction &clut, bool preset)
{
    DEBUG_LOG("applyClut()");

    m_currentClut = clut;
    if (!preset)
    {
        // Cal fer el disconnect per evitar un bucle infinit
        disconnect(m_clutPresetsComboBox, SIGNAL(currentIndexChanged(const QString&)), this, SLOT(applyPresetClut(const QString&)));
        m_clutPresetsComboBox->setCurrentIndex(-1);
        connect(m_clutPresetsComboBox, SIGNAL(currentIndexChanged(const QString&)), this, SLOT(applyPresetClut(const QString&)));
    }
    m_gradientEditor->setTransferFunction(m_currentClut);
    m_editorByValues->setTransferFunction(m_currentClut);
    m_3DView->setTransferFunction(m_currentClut);
//     this->render();
}

void Q3DViewerExtension::changeViewerTransferFunction()
{
    // Actualitzem l'editor de cluts quan es canvia per la funció pel w/l del visor
    m_gradientEditor->setTransferFunction(m_3DView->getTransferFunction());
    m_editorByValues->setTransferFunction(m_3DView->getTransferFunction());
}

void Q3DViewerExtension::render()
{
    this->setCursor(QCursor(Qt::WaitCursor));
    m_3DView->applyCurrentRenderingMethod();
    this->unsetCursor();
}

void Q3DViewerExtension::loadClut()
{
    Settings settings;

    QString customClutsDirPath = settings.getValue(Q3DViewerExtensionSettings::CustomClutsDirPath).toString();

    QString transferFunctionFileName =
            QFileDialog::getOpenFileName(this, tr("Load CLUT"),
                                         customClutsDirPath, tr("Transfer function files (*.tf);;All files (*)"));

    if (!transferFunctionFileName.isNull())
    {
        TransferFunction *transferFunction = TransferFunctionIO::fromFile(transferFunctionFileName);
        QTransferFunctionEditor *currentEditor = qobject_cast<QTransferFunctionEditor*>(m_editorsStackedWidget->currentWidget());
        currentEditor->setTransferFunction(*transferFunction);
        delete transferFunction;

        QFileInfo transferFunctionFileInfo(transferFunctionFileName);
        settings.setValue(Q3DViewerExtensionSettings::CustomClutsDirPath, transferFunctionFileInfo.absolutePath());
    }
}

void Q3DViewerExtension::saveClut()
{
    Settings settings;

    QString customClutsDirPath = settings.getValue(Q3DViewerExtensionSettings::CustomClutsDirPath).toString();

    QFileDialog saveDialog(this, tr("Save CLUT"), customClutsDirPath, tr("Transfer function files (*.tf);;All files (*)"));
    saveDialog.setAcceptMode(QFileDialog::AcceptSave);
    saveDialog.setDefaultSuffix("tf");

    if (saveDialog.exec() == QDialog::Accepted)
    {
        QString transferFunctionFileName = saveDialog.selectedFiles().constFirst();
        QTransferFunctionEditor *currentEditor = qobject_cast<QTransferFunctionEditor*>(m_editorsStackedWidget->currentWidget());
        TransferFunctionIO::toFile(transferFunctionFileName, currentEditor->getTransferFunction());

        QFileInfo transferFunctionFileInfo(transferFunctionFileName);
        settings.setValue(Q3DViewerExtensionSettings::CustomClutsDirPath, transferFunctionFileInfo.absolutePath());
    }
}

void Q3DViewerExtension::switchEditor()
{
    QTransferFunctionEditor *currentEditor = qobject_cast<QTransferFunctionEditor*>(m_editorsStackedWidget->currentWidget());
    const TransferFunction &currentTransferFunction = currentEditor->getTransferFunction();
    m_editorsStackedWidget->setCurrentIndex(1 - m_editorsStackedWidget->currentIndex());
    currentEditor = qobject_cast<QTransferFunctionEditor*>(m_editorsStackedWidget->currentWidget());
    currentEditor->setTransferFunction(currentTransferFunction);
}

void Q3DViewerExtension::applyEditorClut()
{
    QTransferFunctionEditor *currentEditor = qobject_cast<QTransferFunctionEditor*>(m_editorsStackedWidget->currentWidget());
    applyClut(currentEditor->getTransferFunction());
    this->render();
}

void Q3DViewerExtension::toggleClutEditor()
{
    if (m_editorSplitter->sizes().at(1) == 0)
    {
        // Show
        m_editorSplitter->setSizes(QList<int>() << 1 << 1);
        m_gradientEditor->setTransferFunction(m_currentClut);
        m_editorByValues->setTransferFunction(m_currentClut);
    }
    else
    {
        // Hide
        hideClutEditor();
    }
}

void Q3DViewerExtension::hideClutEditor()
{
    m_editorSplitter->setSizes(QList<int>() << 1 << 0);
}

void Q3DViewerExtension::setCustomStyleButtonStateBySplitter()
{
    m_customStyleToolButton->setChecked(m_editorSplitter->sizes().at(1) != 0);
}

void Q3DViewerExtension::applyRenderingStyle(const QModelIndex &index)
{
    disableAutoUpdate();

    QStandardItem *item = m_renderingStyleModel->itemFromIndex(index);
    RenderingStyle renderingStyle = RenderingStyle::fromVariant(item->data());

    switch (renderingStyle.getMethod())
    {
        case RenderingStyle::RayCasting:
            m_blendModeComboBox->setCurrentIndex(0);
            applyClut(renderingStyle.getTransferFunction());
            m_shadingGroupBox->setChecked(renderingStyle.getShading());

            if (renderingStyle.getShading())
            {
                m_ambientCoefficientDoubleSpinBox->setValue(renderingStyle.getAmbientCoefficient());
                m_diffuseCoefficientDoubleSpinBox->setValue(renderingStyle.getDiffuseCoefficient());
                m_specularCoefficientDoubleSpinBox->setValue(renderingStyle.getSpecularCoefficient());
                m_specularPowerDoubleSpinBox->setValue(renderingStyle.getSpecularPower());
            }

            break;

        case RenderingStyle::MIP:
            m_blendModeComboBox->setCurrentIndex(1);
            break;

        case RenderingStyle::IsoSurface:
            m_blendModeComboBox->setCurrentIndex(5);
            m_isoValueSpinBox->setValue(static_cast<int>(qRound(renderingStyle.getIsoValue())));
            break;
    }

    updateView(false);
    enableAutoUpdate();
}

void Q3DViewerExtension::showScreenshotsExporterDialog()
{
    QExporterTool exporter(m_3DView);
    exporter.exec();
}

void Q3DViewerExtension::updateUiForBlendMode(int blendModeIndex)
{
    auto blendMode = Q3DViewer::BlendMode(blendModeIndex);

    m_shadingGroupBox->setVisible(blendMode == Q3DViewer::BlendMode::Composite);
    m_rayCastingOptionsWidget->setVisible(blendMode != Q3DViewer::BlendMode::Isosurface);
    m_isosurfaceOptionsWidget->setVisible(blendMode == Q3DViewer::BlendMode::Isosurface);
    m_transferFunctionsWidget->setVisible(blendMode != Q3DViewer::BlendMode::Isosurface);
    m_noTransferFunctionsWidget->setVisible(m_transferFunctionsWidget->isHidden());
}

void Q3DViewerExtension::updateView(bool fast)
{
    m_timer->stop();

    this->setCursor(QCursor(Qt::WaitCursor));

    m_3DView->setBlendMode(Q3DViewer::BlendMode(m_blendModeComboBox->currentIndex()));
    m_3DView->setRenderMode(Q3DViewer::RenderMode(m_renderModeComboBox->currentIndex()));
    m_3DView->setIndependentComponents(!m_colorImageCheckBox->isChecked());
    m_3DView->setShading(m_shadingGroupBox->isChecked());
    m_3DView->setAmbientCoefficient(m_ambientCoefficientDoubleSpinBox->value());
    m_3DView->setDiffuseCoefficient(m_diffuseCoefficientDoubleSpinBox->value());
    m_3DView->setSpecularCoefficient(m_specularCoefficientDoubleSpinBox->value());
    m_3DView->setSpecularPower(m_specularPowerDoubleSpinBox->value());
    m_3DView->setIsoValue(m_isoValueSpinBox->value());

    if (fast)
    {
        /// \todo Ara ho fem abans del render. Caldria comprovar en sistemes lents si és millor abans o després.
        m_timer->start();
        m_3DView->getRenderWindow()->SetDesiredUpdateRate(m_3DView->getInteractor()->GetDesiredUpdateRate());
    }

    m_3DView->applyCurrentRenderingMethod();

    if (fast)
    {
        m_3DView->getRenderWindow()->SetDesiredUpdateRate(m_3DView->getInteractor()->GetStillUpdateRate());
    }

    this->unsetCursor();
}

void Q3DViewerExtension::enableAutoUpdate()
{
    connect(m_blendModeComboBox, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &Q3DViewerExtension::updateView);
    connect(m_renderModeComboBox, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &Q3DViewerExtension::updateView);
    connect(m_colorImageCheckBox, &QCheckBox::toggled, this, &Q3DViewerExtension::updateView);

    // Shading
    connect(m_shadingGroupBox, SIGNAL(toggled(bool)), this, SLOT(updateView()));
    connect(m_ambientCoefficientDoubleSpinBox, SIGNAL(valueChanged(double)), this, SLOT(updateView()));
    connect(m_diffuseCoefficientDoubleSpinBox, SIGNAL(valueChanged(double)), this, SLOT(updateView()));
    connect(m_specularCoefficientDoubleSpinBox, SIGNAL(valueChanged(double)), this, SLOT(updateView()));
    connect(m_specularPowerDoubleSpinBox, SIGNAL(valueChanged(double)), this, SLOT(updateView()));

    // Isosuperfícies
    connect(m_isoValueSpinBox, SIGNAL(valueChanged(int)), this, SLOT(updateView()));
}

void Q3DViewerExtension::disableAutoUpdate()
{
    disconnect(m_blendModeComboBox, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &Q3DViewerExtension::updateView);
    disconnect(m_renderModeComboBox, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &Q3DViewerExtension::updateView);
    disconnect(m_colorImageCheckBox, &QCheckBox::toggled, this, &Q3DViewerExtension::updateView);

    // Shading
    disconnect(m_shadingGroupBox, SIGNAL(toggled(bool)), this, SLOT(updateView()));
    disconnect(m_ambientCoefficientDoubleSpinBox, SIGNAL(valueChanged(double)), this, SLOT(updateView()));
    disconnect(m_diffuseCoefficientDoubleSpinBox, SIGNAL(valueChanged(double)), this, SLOT(updateView()));
    disconnect(m_specularCoefficientDoubleSpinBox, SIGNAL(valueChanged(double)), this, SLOT(updateView()));
    disconnect(m_specularPowerDoubleSpinBox, SIGNAL(valueChanged(double)), this, SLOT(updateView()));

    // Isosuperfícies
    disconnect(m_isoValueSpinBox, SIGNAL(valueChanged(int)), this, SLOT(updateView()));
}

}
