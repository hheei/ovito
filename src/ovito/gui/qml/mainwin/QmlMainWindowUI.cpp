// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/gui/qml/QmlFrontend.h>
#include <ovito/gui/qml/mainwin/QmlViewportController.h>
#include <ovito/gui/base/actions/ActionManager.h>
#include <ovito/core/app/undo/UndoStack.h>
#include <ovito/core/app/Application.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/core/dataset/io/FileImporter.h>
#include <ovito/core/viewport/Viewport.h>
#include <ovito/core/viewport/ViewportConfiguration.h>
#include "QmlMainWindowUI.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(QmlMainWindowUI);

/******************************************************************************
* Constructor.
******************************************************************************/
QmlMainWindowUI::QmlMainWindowUI()
{
}

/******************************************************************************
* Destructor.
******************************************************************************/
QmlMainWindowUI::~QmlMainWindowUI()
{
    delete _qmlController;
    delete _view;
}

/******************************************************************************
* Creates the Qt Quick window and loads the workbench UI.
******************************************************************************/
void QmlMainWindowUI::initializeWindow()
{
    OVITO_ASSERT(!_view);

    // Create the input manager, the undo stack and the action manager. These objects are frontend-neutral
    // and are provided by the gui/base module, so the QML frontend does not have to duplicate them.
    setViewportInputManager(new ViewportInputManager(nullptr, *this));
    setUndoStack(new UndoStack(*this));
    setActionManager(new ActionManager(nullptr, *this));

    // Clear the undo stack whenever a new dataset is loaded.
    QObject::connect(&datasetContainer(), &DataSetContainer::dataSetChanged, undoStack(), &UndoStack::clear);

    initializeDataset();

    // Create the object that the QML scene talks to.
    _qmlController = new QmlViewportController(*this);

    // Create the window and load the QML workbench UI.
    auto* view = new QQuickView();
    view->setTitle(tr("OVITO"));
    view->setResizeMode(QQuickView::SizeRootObjectToView);
    view->setColor(QColor(24, 24, 24));

    // Make the C++ side of the frontend available to QML.
    view->rootContext()->setContextProperty(QStringLiteral("viewportController"), _qmlController);

    view->setSource(QUrl(QStringLiteral("qrc:/ovito/gui/qml/WorkbenchWindow.qml")));
    if(view->status() == QQuickView::Error) {
        for(const QQmlError& error : view->errors())
            qWarning() << "QML error:" << error.toString();
        delete view;
        throw Exception(tr("Failed to load the QML workbench user interface."));
    }

    view->resize(1280, 800);
    view->show();

    _view = view;
}

/******************************************************************************
* Creates the default dataset if no dataset has been loaded yet.
******************************************************************************/
void QmlMainWindowUI::initializeDataset()
{
    if(datasetContainer().currentSet() == nullptr)
        datasetContainer().setCurrentSet(OORef<DataSet>::create());
}

/******************************************************************************
* Imports a simulation data file into the current scene.
******************************************************************************/
void QmlMainWindowUI::importFile(const QUrl& url)
{
    OORef<DataSet> dataset = datasetContainer().currentSet();
    OORef<Scene> scene = datasetContainer().activeScene();
    if(!dataset || !scene)
        throw Exception(tr("Cannot import because there is no active scene."));

    handleExceptions([&] {
        OORef<FileImporter> importer = FileImporter::autodetectFileFormat(url).blockForResult();
        if(!importer)
            throw Exception(tr("Could not auto-detect the format of the file %1.").arg(url.fileName()));

        std::vector<std::pair<QUrl, OORef<FileImporter>>> importers;
        importers.emplace_back(url, std::move(importer));

        Future<OORef<Pipeline>> future = importers.front().second->importFileSet(scene, std::move(importers),
            FileImporter::ResetScene, true, FileImporter::ImportAsTrajectory);
        (void)future.blockForResult();

        dataset->setFilePath({});

        // Fit the new data into the viewports.
        zoomToSceneExtentsWhenReady();

        showStatusBarMessage(tr("Imported %1").arg(url.fileName()));
    });
}

/******************************************************************************
* Displays a message string in the window's status bar.
******************************************************************************/
void QmlMainWindowUI::showStatusBarMessage(const QString& message, int timeout)
{
    Q_UNUSED(timeout);
    if(_statusMessage != message) {
        _statusMessage = message;
        if(_qmlController)
            Q_EMIT _qmlController->statusMessageChanged();
    }
}

/******************************************************************************
* Hides any messages currently displayed in the status bar.
******************************************************************************/
void QmlMainWindowUI::clearStatusBarMessage()
{
    showStatusBarMessage({});
}

/******************************************************************************
* Gives the active viewport the input focus.
******************************************************************************/
void QmlMainWindowUI::setViewportInputFocus()
{
    if(_qmlController) {
        if(ViewportConfiguration* viewportConfig = datasetContainer().activeViewportConfig())
            _qmlController->setViewportInputFocus(viewportConfig->activeViewport());
    }
}

/******************************************************************************
* Cancels all running tasks associated with this user interface and closes the user interface.
******************************************************************************/
bool QmlMainWindowUI::shutdown()
{
    if(_view)
        _view->hide();
    return UserInterface::shutdown();
}

/******************************************************************************
* Displays the error message(s) stored in the Exception object to the user.
******************************************************************************/
void QmlMainWindowUI::reportError(const Exception& ex, bool blocking)
{
    Q_UNUSED(blocking);

    // The prototype has no error dialog yet. Print the error message to the console and to the status line.
    qWarning().noquote() << ex.message();
    showStatusBarMessage(ex.message().split(QLatin1Char('\n')).first());
}

/******************************************************************************
* Displays a message box to the user.
******************************************************************************/
UserInterface::MessageBoxButton QmlMainWindowUI::showMessageBox(MessageBoxIcon icon, const QString& title, const QString& text, int buttons, MessageBoxButton defaultButton, const QString& detailedText)
{
    Q_UNUSED(icon);
    Q_UNUSED(buttons);
    Q_UNUSED(detailedText);

    // The prototype has no message box yet. Log the message instead of blocking the application.
    qWarning().noquote() << title << ":" << text;
    return defaultButton;
}

}   // End of namespace
