// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/gui/qml/QmlFrontend.h>
#include <ovito/gui/qml/mainwin/QmlViewportController.h>
#include <ovito/gui/qml/mainwin/QmlViewportLayout.h>
#include <ovito/gui/qml/viewport/QmlViewportMenu.h>
#include <ovito/gui/base/actions/ActionManager.h>
#include <ovito/gui/base/app/TaskProgressModel.h>
#include <ovito/gui/base/actions/Command.h>
#include <ovito/gui/base/app/GuiSettings.h>
#include <ovito/gui/base/app/GuiTaskScope.h>
#include <ovito/core/app/Application.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/core/dataset/io/FileImporter.h>
#include <ovito/core/dataset/io/FileSource.h>
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
        qmlRegisterUncreatableType<QmlViewportMenu>("Ovito.Qml", 1, 0, "ViewportMenu",
            QStringLiteral("There is one viewport context menu per workbench window."));
        qmlRegisterUncreatableType<Command>("Ovito.Qml", 1, 0, "Command",
            QStringLiteral("Commands are created by the frontend through the command manager."));
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

#ifdef Q_OS_WIN
    // Direct3D 11 - the backend Qt Quick picks by default on Windows - cannot use the shaders that OVITO's renderer
    // ships, because they are baked for HLSL shader model 6.0 and contain no 5.0 variant. Direct3D 12 is the backend
    // of the Windows platform matrix of this project anyway (see AGENTS.md), so select it before the window exists.
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Direct3D12);
#endif

    // Create the window that displays the workbench UI.
    auto* view = new QQuickView();
    // The user interface owns the window from here on; a failure while loading the shell deletes it again, which clears
    // this reference (it is a guarded pointer).
    _view = view;
    view->setResizeMode(QQuickView::SizeRootObjectToView);
    view->setColor(QColor(24, 24, 24));
    // The shell is not usable below this size: the command panel and a viewport have to fit next to each other.
    view->setMinimumSize(QSize(640, 400));

    // Create the input manager, the undo stack and the action manager. They are frontend-neutral and come from the
    // gui/base module; the window owns them because it is a QObject.
    initializeWorkbench(view);

    // Create the objects that the QML scene talks to: the shell state and commands, the controller that owns the
    // viewport items, the model that lays the panes of the viewport layout out, and the context menu of the viewports.
    _workbenchController = new QmlWorkbenchController(*this, view);
    _contextMenu = new QmlViewportMenu(*this, view);
    _qmlController = new QmlViewportController(*this, _contextMenu, view);
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
    view->rootContext()->setContextProperty(QStringLiteral("viewportMenu"), _contextMenu);

    // The running tasks and their progress are presented by the workbench's shared task progress model, which the
    // status line of the shell binds to.
    if(TaskProgressModel* model = taskProgressModel())
        view->rootContext()->setContextProperty(QStringLiteral("taskProgress"), static_cast<QObject*>(model));

    // The commands of the user interface are the same objects the classic frontend presents as QActions, so both
    // frontends share their state, their shortcuts and their handlers.
    if(ActionManager* manager = actionManager())
        view->rootContext()->setContextProperty(QStringLiteral("commandManager"), manager);

    // The commands of the shared command layer whose handler belongs to a frontend have to be given one here.
    connectFrontendCommands(view);

    // The settings the shell persists (color scheme, window state, file dialog behaviour) are the shared ones, so that
    // the Qt Quick workbench follows the same policy as the classic frontend instead of inventing its own.
    view->rootContext()->setContextProperty(QStringLiteral("guiSettings"), &GuiSettings::instance());

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

    // Open the window where the user left it the last time; a first launch uses the default size.
    if(!applyStoredWindowState())
        view->resize(1280, 800);
    // Remember the window state while the user changes it. Writing on every single resize event of a drag would be
    // pointless, so the state is written once the window has settled, and when the application quits.
    auto* saveTimer = new QTimer(view);
    saveTimer->setSingleShot(true);
    saveTimer->setInterval(500);
    QObject::connect(saveTimer, &QTimer::timeout, view, [this]() { saveWindowState(); });
    const auto scheduleSave = [saveTimer]() { saveTimer->start(); };
    QObject::connect(view, &QWindow::widthChanged, view, scheduleSave);
    QObject::connect(view, &QWindow::heightChanged, view, scheduleSave);
    QObject::connect(view, &QWindow::xChanged, view, scheduleSave);
    QObject::connect(view, &QWindow::yChanged, view, scheduleSave);
    QObject::connect(view, &QWindow::windowStateChanged, view, scheduleSave);
    QObject::connect(qApp, &QCoreApplication::aboutToQuit, view, [this]() { saveWindowState(); });

    // Open the window. One that was maximized when it was closed opens maximized again.
    if(GuiSettings::instance().isWorkbenchWindowMaximized())
        view->showMaximized();
    else
        view->show();
}

/******************************************************************************
* Restores the window state the user left behind.
******************************************************************************/
bool QmlMainWindowUI::applyStoredWindowState()
{
    if(_view == nullptr)
        return false;

    // A window that was maximized when it was closed opens maximized, and its remembered size is the size of its
    // restored state (below). A window that was never sized has nothing to restore.
    const QRect geometry = GuiSettings::instance().workbenchWindowGeometry();
    if(!geometry.isValid())
        return false;

    _view->resize(geometry.size());
    _view->setPosition(geometry.topLeft());
    return true;
}

/******************************************************************************
* Remembers the current window state.
******************************************************************************/
void QmlMainWindowUI::saveWindowState()
{
    if(_view == nullptr)
        return;

    GuiSettings& settings = GuiSettings::instance();
    const bool maximized = _view->windowStates().testFlag(Qt::WindowMaximized);
    settings.setWorkbenchWindowMaximized(maximized);

    // While the window is maximized its geometry is the screen, so only a normal window remembers its size - otherwise
    // the next launch would open maximized with the size of the screen and look the same after being restored.
    if(!maximized && _view->isVisible())
        settings.setWorkbenchWindowGeometry(_view->geometry());
}

/******************************************************************************
* Gives the frontend-dependent commands their handler.
******************************************************************************/
void QmlMainWindowUI::connectFrontendCommands(QQuickView* view)
{
    ActionManager* manager = actionManager();
    if(manager == nullptr)
        return;

    // Their shared, frontend-neutral part is the command itself: the title, the shortcut and the enabled state. The
    // desktop frontend opens a widget dialog for them (WidgetActionManager), this one opens a surface of the QML scene.
    if(Command* command = manager->findCommand(ACTION_FILE_IMPORT)) {
        QObject::connect(command, &Command::triggered, view, [this]() {
            _workbenchController->showImportDialog();
        });
    }
    if(Command* command = manager->findCommand(ACTION_HELP_ABOUT)) {
        QObject::connect(command, &Command::triggered, view, [this]() {
            _workbenchController->showAboutDialog();
        });
    }
    if(Command* command = manager->findCommand(ACTION_QUIT)) {
        QObject::connect(command, &Command::triggered, view, [this]() {
            // The same behaviour as the classic frontend's Quit: close the window, which ends the application when this
            // was the last user interface.
            shutdown();
            QCoreApplication::quit();
        });
    }
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
    // The progress itself is displayed from the workbench's task progress model; the controller only has to notice
    // that an operation it started is over (which can change the empty state of the scene).
    if(_workbenchController)
        _workbenchController->updateOperationState();
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
    // The notice that reports what the import made of the files names the file the user chose.
    const QString fileName = urlImporters.empty() ? QString() : urlImporters.front().first.fileName();

    // Remember the objects of the scene, so that a canceled import does not leave a partially loaded pipeline behind.
    std::vector<OORef<SceneNode>> previousNodes;
    if(scene) {
        previousNodes.reserve(scene->children().size());
        for(SceneNode* node : scene->children())
            previousNodes.push_back(node);
    }

    Future<OORef<Pipeline>> future = importer.importFileSet(scene, std::move(urlImporters), importMode, true, FileImporter::ImportAsTrajectory);

    OORef<Pipeline> pipeline;
    try {
        pipeline = future.blockForResult();
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

    // Tell the user what the import did with the files: which format the importer identified them as and how many source
    // frames the data source found. This is the only place in the frontends that reports the outcome of the format
    // autodetection, which is why it is worth reporting: a file whose content is claimed by another format than the user
    // expected (see defect F6 in docs/design/UI_PHASE1_SPIKE.md) otherwise just produces an empty scene.
    const QString formatName = importer.objectTitle();
    if(!formatName.isEmpty())
        _workbenchController->setImportNotice(tr("Imported \"%1\" as %2.").arg(fileName, formatName));

    // The frame list of the source is scanned lazily, i.e. after the import call returned (FileSourceImporter::importFileSet
    // only configures the FileSource), so the number of frames is reported as soon as the source knows it.
    if(OORef<FileSource> source = pipeline ? dynamic_object_cast<FileSource>(pipeline->source()) : nullptr) {
        _importNoticeFuture = source->requestFrameList(false).then(ObjectExecutor(this), [this, fileName, formatName](const QVector<FileSourceImporter::Frame>& frames) {
            // The frame list arrives in a task of its own; give that task the context of this user interface, so that
            // OVITO objects may be created and messages reported from here.
            GuiTaskScope taskScope(*this);
            if(_workbenchController == nullptr)
                return;
            if(frames.isEmpty() || formatName.isEmpty()) {
                // Nothing better to report than the format; the notice stands as it is.
                return;
            }
            const QString frameCountText = frames.size() == 1 ? tr("1 source frame") : tr("%1 source frames").arg(frames.size());
            _workbenchController->setImportNotice(tr("Imported \"%1\" as %2 (%3).").arg(fileName, formatName, frameCountText));
        });
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
