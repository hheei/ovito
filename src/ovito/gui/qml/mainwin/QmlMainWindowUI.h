// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/qml/QmlFrontend.h>
#include <ovito/gui/qml/mainwin/QmlViewportController.h>
#include <ovito/gui/qml/mainwin/QmlViewportLayout.h>
#include <ovito/gui/qml/mainwin/QmlWorkbenchController.h>
#include <ovito/gui/qml/viewport/QmlViewportMenu.h>
#include <ovito/gui/base/app/WorkbenchUI.h>

namespace Ovito {

/**
 * \brief The user interface implementation of the Qt Quick / QML frontend.
 *
 * Each instance owns one Qt Quick window showing the workbench UI. This class plays the role that MainWindowUI plays
 * for the classic QtWidgets frontend; the parts of the user interface contract that do not depend on the presentation
 * are provided by the WorkbenchUI base class (see gui/base).
 *
 * Note that UserInterface is not a QObject, so the parts of this class that need to be reachable from QML are provided
 * by the separate QmlWorkbenchController (the shell: status line, task progress, message dialogs, import commands),
 * QmlViewportController (the viewport items and the undo stack) and QmlViewportLayout (the pane layout) objects.
 */
class OVITO_GUIQML_EXPORT QmlMainWindowUI : public WorkbenchUI
{
    OVITO_CLASS(QmlMainWindowUI)

public:

    /// Constructor.
    QmlMainWindowUI();

    /// Destructor.
    ~QmlMainWindowUI();

    /// Creates the Qt Quick window and loads the workbench UI.
    void initializeWindow();

    /// Checks whether the scene graph of the workbench window could create a graphics device and reports the
    /// consequence when it could not: Qt's offscreen platform plugin, for instance, provides no QRhi, which would
    /// leave the user with empty viewport panes and no explanation. Does nothing once the answer is known.
    void checkGraphicsDevice();

    /// Tells the user that the viewports cannot be rendered, naming the platform plugin that provides no graphics
    /// device and the way to run the frontend without a display server.
    void reportMissingGraphicsDevice();

    /// Returns the Qt Quick window displaying the workbench.
    QQuickView* view() const { return _view; }

    /// Returns the object that exposes the shell of the frontend to the QML scene.
    QmlWorkbenchController* workbenchController() const { return _workbenchController; }

    /// Returns the object that exposes the viewports of the frontend to the QML scene.
    QmlViewportController* qmlController() const { return _qmlController; }

    /// Returns the model that lays the viewport panes of the workbench out.
    QmlViewportLayout* viewportLayout() const { return _viewportLayout; }

    /// Returns the model of the viewport context menu.
    QmlViewportMenu* viewportMenu() const { return _contextMenu; }

    /// Reports whether the workbench window has a graphics device to render its viewports with. Returns no value while
    /// the scene graph of the window has not been initialized yet (see checkGraphicsDevice()).
    std::optional<bool> hasGraphicsDevice() const { return _graphicsDevice; }

    /// Restores the window size, the window position and the maximized state the user left behind, in so far as they were
    /// remembered (see GuiSettings). Called while the window is created; it can be called again whenever the window has
    /// to return to the remembered state.
    /// \return True if a remembered window state was applied; false leaves the window as it is.
    bool applyStoredWindowState();

    /// Displays a message string in the window's status bar.
    void showStatusBarMessage(const QString& message, int timeout = 0) override;

    /// Hides any messages currently displayed in the status bar.
    void clearStatusBarMessage() override;

    /// Gives the active viewport the input focus.
    void setViewportInputFocus() override;

    /// Cancels all running tasks associated with this user interface and closes the user interface.
    bool shutdown() override;

    /// Displays a message box to the user.
    MessageBoxButton showMessageBox(MessageBoxIcon icon, const QString& title, const QString& text, int buttons, MessageBoxButton defaultButton = NoButton, const QString& detailedText = {}) override;

protected:

    // Implementation of the presentation hooks of WorkbenchUI (see gui/base/app/WorkbenchUI.h).

    /// Creates the action manager of this workbench.
    virtual ActionManager* createActionManager(QObject* parent) override;

    /// Shows the error message in a message dialog, and its first line in the status bar of the workbench window.
    /// The message has already been written to the terminal by WorkbenchUI::reportError().
    virtual void displayErrorMessage(const Exception& ex, bool blocking) override;

    /// Shows the file selection dialog of the QML scene.
    virtual void openImportDialog(const QString& directoryPath) override;

    /// Runs the import operation while keeping track of the task, so that the user can cancel it.
    virtual void runFileImport(FileImporter& importer, Scene* scene, std::vector<std::pair<QUrl, OORef<FileImporter>>> urlImporters, FileImporter::ImportMode importMode) override;

    /// Forwards the changed progress state of the running tasks to the QML scene.
    virtual void progressTasksChanged() override;

private:

    /// Creates the default dataset if no dataset has been loaded yet.
    void initializeDataset();

    /// Shows the message of an exception in a message dialog of the QML scene.
    void showErrorMessage(const Exception& ex);

    /// Remembers the current window state, so that the next launch looks like this one.
    void saveWindowState();

    /// Gives the commands whose handlers belong to a frontend (Open, Save, About, Quit) the handlers of this one.
    void connectFrontendCommands(QQuickView* view);

    /// The Qt Quick window displaying the workbench UI.
    QPointer<QQuickView> _view;

    /// The object exposing the shell of this frontend to the QML scene.
    QPointer<QmlWorkbenchController> _workbenchController;

    /// The object exposing the viewports of this frontend to the QML scene.
    QPointer<QmlViewportController> _qmlController;

    /// The layout of the viewport panes displayed by this window.
    QPointer<QmlViewportLayout> _viewportLayout;

    /// The context menu of the viewports of this window.
    QPointer<QmlViewportMenu> _contextMenu;

    /// Keeps the continuation alive that completes the import notice with the number of source frames of the imported
    /// file. A future nobody awaits cancels the continuation it carries, and the frame list of a file source is only
    /// known once the pipeline has been evaluated, which happens after the import call has returned.
    Future<void> _noticeFuture;

    /// Whether the scene graph of the window has a graphics device. Empty until the scene graph was initialized.
    std::optional<bool> _graphicsDevice;
};

}   // End of namespace
