// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/gui/qml/QmlFrontend.h>
#include <ovito/gui/qml/mainwin/QmlViewportController.h>
#include <ovito/gui/qml/mainwin/QmlViewportLayout.h>
#include <ovito/gui/base/actions/ActionManager.h>
#include <ovito/core/app/Application.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/core/dataset/io/FileImporter.h>
#include <ovito/core/dataset/scene/Scene.h>
#include <ovito/core/dataset/scene/SceneNode.h>
#include <ovito/core/viewport/Viewport.h>
#include <ovito/core/viewport/ViewportConfiguration.h>
#include <QtCore/QTimer>
#include <QtQml/qqml.h>
#include <algorithm>
#include "QmlMainWindowUI.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(QmlMainWindowUI);

/******************************************************************************
* Registers the C++ classes of the workbench shell with the QML type system.
******************************************************************************/
static void registerQmlTypes()
{
    // The classes are not creatable from QML: the workbench shell gets its panes, handles and controllers from the
    // frontend. Registering them nevertheless makes the properties of the shell's QML components statically typed, so
    // that a typo in a property name is an error instead of a silently broken binding.
    static const bool registered = []() {
        qmlRegisterUncreatableType<QmlViewportPane>("Ovito.Qml", 1, 0, "ViewportPane",
            QStringLiteral("Panes are created by the viewport layout model."));
        qmlRegisterUncreatableType<QmlViewportSplitter>("Ovito.Qml", 1, 0, "ViewportSplitter",
            QStringLiteral("Handles are created by the viewport layout model."));
        qmlRegisterUncreatableType<QmlViewportLayout>("Ovito.Qml", 1, 0, "ViewportLayout",
            QStringLiteral("There is one viewport layout per workbench window."));
        qmlRegisterUncreatableType<QmlViewportController>("Ovito.Qml", 1, 0, "ViewportController",
            QStringLiteral("There is one viewport controller per workbench window."));
        qmlRegisterUncreatableType<QmlWorkbenchController>("Ovito.Qml", 1, 0, "WorkbenchController",
            QStringLiteral("There is one workbench controller per workbench window."));
        return true;
    }();
    Q_UNUSED(registered);
}

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
    // The controllers are child objects of the window, so deleting the window deletes them as well.
    delete _view;
}

/******************************************************************************
* Creates the Qt Quick window and loads the workbench UI.
******************************************************************************/
void QmlMainWindowUI::initializeWindow()
{
    OVITO_ASSERT(!_view);

    // Create the window that displays the workbench UI.
    auto* view = new QQuickView();
    view->setResizeMode(QQuickView::SizeRootObjectToView);
    view->setColor(QColor(24, 24, 24));
    // The shell is not usable below this size: the command panel and a viewport have to fit next to each other.
    view->setMinimumSize(QSize(640, 400));

    // Create the input manager, the undo stack and the action manager. They are frontend-neutral and come from the
    // gui/base module; the window owns them because it is a QObject.
    initializeWorkbench(view);

    // Create the objects that the QML scene talks to: the shell state and commands, the controller that owns the
    // viewport items, and the model that lays the panes of the viewport layout out.
    _workbenchController = new QmlWorkbenchController(*this, view);
    _qmlController = new QmlViewportController(*this, view);
    _viewportLayout = new QmlViewportLayout(*this, view);

    // The window title follows the data set, i.e. the session file it was loaded from.
    QObject::connect(_workbenchController, &QmlWorkbenchController::windowTitleChanged, view, [this, view]() {
        view->setTitle(_workbenchController->windowTitle());
    });
    view->setTitle(_workbenchController->windowTitle());

    // Make the C++ side of the frontend available to QML. The types have to be registered before the QML file that uses
    // them is loaded.
    registerQmlTypes();
    view->rootContext()->setContextProperty(QStringLiteral("workbenchController"), _workbenchController);
    view->rootContext()->setContextProperty(QStringLiteral("viewportController"), _qmlController);
    view->rootContext()->setContextProperty(QStringLiteral("viewportLayout"), _viewportLayout);

    // Create a default dataset if no dataset has been loaded yet, so that the workbench has viewports to display.
    initializeDataset();

    // Load the workbench UI.
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
* Creates the action manager of this workbench.
******************************************************************************/
ActionManager* QmlMainWindowUI::createActionManager(QObject* parent)
{
    return new ActionManager(parent, *this);
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
* Displays a message string in the window's status bar.
******************************************************************************/
void QmlMainWindowUI::showStatusBarMessage(const QString& message, int timeout)
{
    Q_UNUSED(timeout);
    if(_workbenchController)
        _workbenchController->setStatusMessage(message);
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
* Forwards the changed progress state of the running tasks to the QML scene.
******************************************************************************/
void QmlMainWindowUI::progressTasksChanged()
{
    if(_workbenchController)
        _workbenchController->updateTaskState();
}

/******************************************************************************
* Shows the file selection dialog of the QML scene.
******************************************************************************/
void QmlMainWindowUI::openImportDialog(const QString& directoryPath)
{
    if(_workbenchController == nullptr)
        throw Exception(tr("Cannot select files to import: the workbench window is not available."));

    // The dialog itself belongs to the QML scene; it only needs to know where to start looking for files.
    _workbenchController->requestImportDialog(directoryPath);
}

/******************************************************************************
* Runs the import operation while keeping track of the task, so that the user can cancel it.
******************************************************************************/
void QmlMainWindowUI::runFileImport(FileImporter& importer, Scene* scene, std::vector<std::pair<QUrl, OORef<FileImporter>>> urlImporters, FileImporter::ImportMode importMode)
{
    // Remember the objects of the scene, so that a canceled import does not leave a partially loaded pipeline behind.
    std::vector<OORef<SceneNode>> previousNodes;
    if(scene) {
        previousNodes.reserve(scene->children().size());
        for(SceneNode* node : scene->children())
            previousNodes.push_back(node);
    }

    Future<OORef<Pipeline>> future = importer.importFileSet(scene, std::move(urlImporters), importMode, true, FileImporter::ImportAsTrajectory);

    try {
        (void)future.blockForResult();
    }
    catch(const OperationCanceled&) {
        // The importer adds the new pipeline to the scene before it loads the data, so a canceled import leaves a node
        // whose data source was never filled. Remove it, so that the user does not have to clean up a data set that was
        // never imported. (Whatever the import mode replaced before this point cannot be restored; that is the order of
        // events the shared import code prescribes.) A *failed* import keeps its pipeline, because the error is shown
        // there and the user can retry it.
        if(scene) {
            for(SceneNode* node : scene->children()) {
                if(std::find_if(previousNodes.begin(), previousNodes.end(), [node](const OORef<SceneNode>& previous) {
                        return previous.get() == node;
                    }) == previousNodes.end()) {
                    node->requestObjectDeletion();
                }
            }
        }
        throw;
    }
    catch(...) {
        throw;
    }
}

/******************************************************************************
* Shows the error message in a message dialog, and its first line in the status bar.
******************************************************************************/
void QmlMainWindowUI::displayErrorMessage(const Exception& ex, bool blocking)
{
    showStatusBarMessage(ex.message().split(QLatin1Char('\n')).first());

    // The desktop frontend queues non-blocking errors and shows them when the event loop is idle again, so that a
    // series of errors does not block the caller. Do the same here.
    if(blocking) {
        showErrorMessage(ex);
    }
    else if(_workbenchController) {
        QPointer<QmlWorkbenchController> controller = _workbenchController;
        QTimer::singleShot(0, controller, [this, controller, ex]() {
            if(controller)
                showErrorMessage(ex);
        });
    }
}

/******************************************************************************
* Shows the message of an exception in a message dialog of the QML scene.
******************************************************************************/
void QmlMainWindowUI::showErrorMessage(const Exception& ex)
{
    if(_workbenchController == nullptr)
        return;

    // If the exception is associated with additional message strings, show them in the details section of the dialog.
    QString detailText;
    for(int i = 1; i < ex.messages().size(); i++)
        detailText += ex.messages()[i] + QStringLiteral("\n");
    if(!ex.traceback().isEmpty()) {
        if(!detailText.isEmpty())
            detailText += QChar('\n');
        detailText += ex.traceback();
    }

    _workbenchController->presentMessageBox(MessageBoxIcon::CriticalIcon,
        tr("Error - %1").arg(Application::applicationName()), ex.message(),
        MessageBoxButton::Ok, MessageBoxButton::Ok, detailText);
}

/******************************************************************************
* Displays a message box to the user.
******************************************************************************/
UserInterface::MessageBoxButton QmlMainWindowUI::showMessageBox(MessageBoxIcon icon, const QString& title, const QString& text, int buttons, MessageBoxButton defaultButton, const QString& detailedText)
{
    if(_workbenchController == nullptr) {
        // A frontend without a window cannot ask the user. Report the message and continue with the default answer.
        qWarning().noquote() << title << ":" << text;
        return defaultButton;
    }
    return _workbenchController->presentMessageBox(icon, title, text, buttons, defaultButton, detailedText);
}

}   // End of namespace
