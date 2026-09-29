// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/gui/base/GUIBase.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/core/dataset/scene/SelectionSet.h>
#include <ovito/core/dataset/scene/Scene.h>
#include <ovito/core/dataset/animation/AnimationSettings.h>
#include <ovito/gui/base/viewport/NavigationModes.h>
#include <ovito/gui/base/viewport/ViewportInputMode.h>
#include <ovito/gui/base/viewport/ViewportInputManager.h>
#include <ovito/core/app/UserInterface.h>
#include <ovito/core/app/Application.h>
#include <ovito/core/app/undo/UndoStack.h>
#include <ovito/gui/base/actions/Command.h>
#include "ActionManager.h"

namespace Ovito {

/******************************************************************************
* Initializes the ActionManager.
******************************************************************************/
ActionManager::ActionManager(QObject* parent, UserInterface& ui) : QAbstractListModel(parent), UserInterfaceComponent<UserInterface>(ui)
{
    // Actions need to be updated whenever a new dataset is loaded or the current selection changes.
    connect(&datasetContainer(), &DataSetContainer::dataSetChanged, this, &ActionManager::onDataSetChanged);
    connect(&datasetContainer(), &DataSetContainer::selectionChangeComplete, this, &ActionManager::onSelectionChangeComplete);
    connect(&datasetContainer(), &DataSetContainer::animationIntervalChanged, this, &ActionManager::onAnimationIntervalChanged);
    connect(&datasetContainer(), &DataSetContainer::maximizedViewportChanged, this, &ActionManager::onMaximizedViewportChanged);
    connect(&datasetContainer(), &DataSetContainer::viewportLayoutChanged, this, &ActionManager::onViewportLayoutChanged);

    createCommand(ACTION_QUIT, tr("Quit"), "file_quit", tr("Quit the application."));
    createCommand(ACTION_FILE_OPEN, tr("Load Session State..."), "file_open", tr("Load a previously saved session from a file."), QKeySequence::Open);
    createCommand(ACTION_FILE_SAVE, tr("Save Session State"), "file_save", tr("Save the current program session to a file."), QKeySequence::Save);
    createCommand(ACTION_FILE_SAVEAS, tr("Save Session State As..."), "file_save_as", tr("Save the current program session to a new file."), QKeySequence::SaveAs);
    createCommand(ACTION_FILE_IMPORT, tr("Load File..."), "file_import", tr("Import data from a file on this computer."), QKeyCombination(Qt::CTRL, Qt::Key_I));
    createCommand(ACTION_FILE_REMOTE_IMPORT, tr("Load Remote File"), "file_import_remote", tr("Import a file from a remote location."), QKeyCombination(Qt::CTRL | Qt::SHIFT, Qt::Key_I));
    createCommand(ACTION_FILE_EXPORT, tr("Export File..."), "file_export", tr("Export data to a file."), QKeyCombination(Qt::CTRL, Qt::Key_E));
    createCommand(ACTION_FILE_NEW_WINDOW, tr("New Program Window"), "file_new_window", tr("Open another OVITO program window."), QKeySequence::New);
    createCommand(ACTION_HELP_ABOUT, tr("About OVITO"), "application_about", tr("Show information about this software."));
    createCommand(ACTION_HELP_SHOW_ONLINE_HELP, tr("User Manual"), "help_user_manual", tr("Open the OVITO user manual."), QKeySequence::HelpContents);
    createCommand(ACTION_HELP_SHOW_SCRIPTING_HELP, tr("Scripting Reference"), "help_scripting_manual", tr("Open the OVITO Python API documentation."));
    createCommand(ACTION_HELP_GRAPHICS_SYSINFO, tr("System Information..."), "help_system_info", tr("Display system and graphics hardware information."));
    createCommand(ACTION_HELP_REQUEST_FEATURE, tr("Request a Feature"), "help_request_feature", tr("Submit a request for a new feature to the OVITO developers."));

    Command* undoCommand = createCommand(ACTION_EDIT_UNDO, tr("Undo"), "edit_undo", tr("Reverse the last action."), QKeySequence::Undo);
    Command* redoCommand = createCommand(ACTION_EDIT_REDO, tr("Redo"), "edit_redo", tr("Restore the previously reversed action."), QKeySequence::Redo);
    Command* clearUndoStackCommand = createCommand(ACTION_EDIT_CLEAR_UNDO_STACK, tr("Clear Undo Stack"), nullptr, tr("Discards all existing undo records."));
    clearUndoStackCommand->setVisible(false);
    if(UndoStack* undoStack = this->undoStack()) {
        undoCommand->setEnabled(undoStack->canUndo());
        redoCommand->setEnabled(undoStack->canRedo());
        undoCommand->setText(tr("Undo %1").arg(undoStack->undoText()));
        redoCommand->setText(tr("Redo %1").arg(undoStack->redoText()));
        connect(undoStack, &UndoStack::canUndoChanged, undoCommand, &Command::setEnabled);
        connect(undoStack, &UndoStack::canRedoChanged, redoCommand, &Command::setEnabled);
        connect(undoStack, &UndoStack::undoTextChanged, undoCommand, [undoCommand](const QString& undoText) {
            undoCommand->setText(tr("Undo %1").arg(undoText));
        });
        connect(undoStack, &UndoStack::redoTextChanged, redoCommand, [redoCommand](const QString& redoText) {
            redoCommand->setText(tr("Redo %1").arg(redoText));
        });
        connect(undoCommand, &Command::triggered, undoStack, &UndoStack::undo);
        connect(redoCommand, &Command::triggered, undoStack, &UndoStack::redo);
        connect(clearUndoStackCommand, &Command::triggered, undoStack, &UndoStack::clear);
    }
    else {
        undoCommand->setEnabled(false);
        redoCommand->setEnabled(false);
        clearUndoStackCommand->setEnabled(false);
    }

    Command* createNewPipelineCommand = createCommand(ACTION_NEW_PIPELINE_FILESOURCE, tr("External data file"), "edit_create_pipeline", tr("Creates a new pipeline with an external file as data source."));
    Command* clonePipelineCommand = createCommand(ACTION_EDIT_CLONE_PIPELINE, tr("Clone Pipeline..."), "edit_clone_pipeline", tr("Duplicate the current pipeline to show multiple datasets side by side."));
#ifndef OVITO_BUILD_PROFESSIONAL
    createNewPipelineCommand->setText(createNewPipelineCommand->text() + QStringLiteral(" (Pro)"));
    clonePipelineCommand->setText(clonePipelineCommand->text() + QStringLiteral(" (Pro)"));
#else
    (void)createNewPipelineCommand;
    (void)clonePipelineCommand;
#endif
    createCommand(ACTION_EDIT_RENAME_PIPELINE, tr("Rename Pipeline..."), "edit_rename_pipeline", tr("Assign a new name to the selected pipeline."));
    createCommand(ACTION_EDIT_DELETE, tr("Delete Pipeline"), "edit_delete_pipeline", tr("Delete the selected object from the scene."));

    createCommand(ACTION_SETTINGS_DIALOG, tr("Application Settings..."), "application_preferences", tr("Open the application settings dialog."), QKeySequence::Preferences);

    createCommand(ACTION_RENDER_ACTIVE_VIEWPORT, tr("Render"), "render_active_viewport", tr("Render an image or animation of the current viewport."));

    createCommand(ACTION_VIEWPORT_MAXIMIZE, tr("Maximize Active Viewport"), "viewport_maximize", tr("Enlarge/reduce the active viewport."))->setCheckable(true);
    createCommand(ACTION_VIEWPORT_ZOOM_SCENE_EXTENTS, tr("Zoom Scene Extents"), "viewport_zoom_scene_extents",
#ifndef Q_OS_MACOS
        tr("Zoom active viewport to show everything. Use CONTROL key to zoom all viewports at once."));
#else
        tr("Zoom active viewport to show everything. Use COMMAND key to zoom all viewports at once."));
#endif
    createCommand(ACTION_VIEWPORT_ZOOM_SCENE_EXTENTS_ALL, tr("Zoom Scene Extents All"), nullptr, tr("Zoom all viewports to show everything."));
    createCommand(ACTION_VIEWPORT_ZOOM_SELECTION_EXTENTS, tr("Zoom Selection Extents"), nullptr, tr("Zoom active viewport to show the selected objects."));
    createCommand(ACTION_VIEWPORT_ZOOM_SELECTION_EXTENTS_ALL, tr("Zoom Selection Extents All"), nullptr, tr("Zoom all viewports to show the selected objects."));
    createCommand(ACTION_CONFIGURE_VIEWPORT_GRAPHICS, tr("Configure Grap&hics..."), nullptr, tr("Change graphics settings for real-time interactive viewports."));

    if(ViewportInputManager* vpInputManager = viewportInputManager()) {
        createViewportModeCommand(ACTION_VIEWPORT_ZOOM, vpInputManager->zoomMode(), tr("Zoom"), "viewport_mode_zoom", tr("Activate zoom mode."));
        createViewportModeCommand(ACTION_VIEWPORT_PAN, vpInputManager->panMode(), tr("Pan"), "viewport_mode_pan", tr("Activate pan mode to shift the region visible in the viewports."));
        createViewportModeCommand(ACTION_VIEWPORT_ORBIT, vpInputManager->orbitMode(), tr("Orbit Camera"), "viewport_mode_orbit", tr("Activate orbit mode to rotate the camera around the scene."));
        createViewportModeCommand(ACTION_VIEWPORT_FOV, vpInputManager->fovMode(), tr("Change Field Of View"), "viewport_mode_fov", tr("Activate field of view mode to change the perspective projection."));
        createViewportModeCommand(ACTION_VIEWPORT_PICK_ORBIT_CENTER, vpInputManager->pickOrbitCenterMode(), tr("Set Orbit Center"), nullptr, tr("Set the center of rotation of the viewport camera."))->setVisible(false);
        createViewportModeCommand(ACTION_SELECTION_MODE, vpInputManager->selectionMode(), tr("Select"), "edit_mode_select", tr("Select objects in the viewports."));
    }

    createCommand(ACTION_GOTO_START_OF_ANIMATION, tr("Go to Start of Animation"), "animation_goto_start", tr("Jump to the first frame of the animation."), Qt::Key_Home);
    createCommand(ACTION_GOTO_END_OF_ANIMATION, tr("Go to End of Animation"), "animation_goto_end", tr("Jump to the last frame of the animation."), Qt::Key_End);
    createCommand(ACTION_GOTO_PREVIOUS_FRAME, tr("Go to Previous Frame"), "animation_goto_previous_frame", tr("Move time slider one animation frame backward."), QKeyCombination(Qt::ALT, Qt::Key_Left));
    createCommand(ACTION_GOTO_NEXT_FRAME, tr("Go to Next Frame"), "animation_goto_next_frame", tr("Move time slider one animation frame forward."), QKeyCombination(Qt::ALT, Qt::Key_Right));
    createCommand(ACTION_START_ANIMATION_PLAYBACK, tr("Start Animation Playback"), "animation_play", tr("Start playing the animation in the viewports."));
    createCommand(ACTION_STOP_ANIMATION_PLAYBACK, tr("Stop Animation Playback"), "animation_stop", tr("Stop playing the animation in the viewports."));
    createCommand(ACTION_ANIMATION_SETTINGS, tr("Animation Settings"), "animation_settings", tr("Open the animation settings dialog."));
    createCommand(ACTION_AUTO_KEY_MODE_TOGGLE, tr("Auto Key Mode"), "animation_auto_key_mode", tr("Toggle auto-key mode for creating animation keys."))->setCheckable(true);

    Command* toggleAnimationPlaybackCommand = createCommand(ACTION_TOGGLE_ANIMATION_PLAYBACK, tr("Play Animation"), "animation_play", tr("Start/stop animation playback. Hold down Shift key to play backwards."), Qt::Key_Space);
    toggleAnimationPlaybackCommand->setCheckable(true);
    toggleAnimationPlaybackCommand->setChecked(datasetContainer().isPlaybackActive());
    connect(&datasetContainer(), &DataSetContainer::playbackChanged, toggleAnimationPlaybackCommand, &Command::setChecked);
    connect(toggleAnimationPlaybackCommand, &Command::toggled, &datasetContainer(), &DataSetContainer::setAnimationPlayback);

    connect(getCommand(ACTION_VIEWPORT_MAXIMIZE), &Command::triggered, this, &ActionManager::on_ViewportMaximize_triggered);
    connect(getCommand(ACTION_VIEWPORT_ZOOM_SCENE_EXTENTS), &Command::triggered, this, &ActionManager::on_ViewportZoomSceneExtents_triggered);
    connect(getCommand(ACTION_VIEWPORT_ZOOM_SELECTION_EXTENTS), &Command::triggered, this, &ActionManager::on_ViewportZoomSelectionExtents_triggered);
    connect(getCommand(ACTION_VIEWPORT_ZOOM_SCENE_EXTENTS_ALL), &Command::triggered, this, &ActionManager::on_ViewportZoomSceneExtentsAll_triggered);
    connect(getCommand(ACTION_VIEWPORT_ZOOM_SELECTION_EXTENTS_ALL), &Command::triggered, this, &ActionManager::on_ViewportZoomSelectionExtentsAll_triggered);
    connect(getCommand(ACTION_GOTO_START_OF_ANIMATION), &Command::triggered, this, &ActionManager::on_AnimationGotoStart_triggered);
    connect(getCommand(ACTION_GOTO_END_OF_ANIMATION), &Command::triggered, this, &ActionManager::on_AnimationGotoEnd_triggered);
    connect(getCommand(ACTION_GOTO_PREVIOUS_FRAME), &Command::triggered, this, &ActionManager::on_AnimationGotoPreviousFrame_triggered);
    connect(getCommand(ACTION_GOTO_NEXT_FRAME), &Command::triggered, this, &ActionManager::on_AnimationGotoNextFrame_triggered);
    connect(getCommand(ACTION_START_ANIMATION_PLAYBACK), &Command::triggered, this, &ActionManager::on_AnimationStartPlayback_triggered);
    connect(getCommand(ACTION_STOP_ANIMATION_PLAYBACK), &Command::triggered, this, &ActionManager::on_AnimationStopPlayback_triggered);
    connect(getCommand(ACTION_EDIT_DELETE), &Command::triggered, this, &ActionManager::on_EditDelete_triggered);
}

/******************************************************************************
* Returns dataset currently being edited in the main window.
******************************************************************************/
DataSet* ActionManager::dataset() const
{
    return datasetContainer().currentSet();
}

void ActionManager::onDataSetChanged(DataSet* newDataSet)
{
    // Turn off auto-key animation mode.
    getCommand(ACTION_AUTO_KEY_MODE_TOGGLE)->setChecked(false);
}

/******************************************************************************
* This is called when the active animation interval has changed.
******************************************************************************/
void ActionManager::onAnimationIntervalChanged(int firstFrame, int lastFrame)
{
    bool isAnimation = (lastFrame > firstFrame);
    getCommand(ACTION_GOTO_START_OF_ANIMATION)->setEnabled(isAnimation);
    getCommand(ACTION_GOTO_PREVIOUS_FRAME)->setEnabled(isAnimation);
    getCommand(ACTION_TOGGLE_ANIMATION_PLAYBACK)->setEnabled(isAnimation);
    getCommand(ACTION_GOTO_NEXT_FRAME)->setEnabled(isAnimation);
    getCommand(ACTION_GOTO_END_OF_ANIMATION)->setEnabled(isAnimation);
    getCommand(ACTION_AUTO_KEY_MODE_TOGGLE)->setEnabled(isAnimation);
    if(!isAnimation && getCommand(ACTION_AUTO_KEY_MODE_TOGGLE)->isChecked())
        getCommand(ACTION_AUTO_KEY_MODE_TOGGLE)->setChecked(false);
}

/******************************************************************************
* This is called when a different viewport become the maximized one.
******************************************************************************/
void ActionManager::onMaximizedViewportChanged(Viewport* maximizedViewport)
{
    getCommand(ACTION_VIEWPORT_MAXIMIZE)->setChecked(maximizedViewport != nullptr);
}

/******************************************************************************
* This is called when the viewport layout changes.
******************************************************************************/
void ActionManager::onViewportLayoutChanged(ViewportConfiguration* viewportConfig)
{
    getCommand(ACTION_VIEWPORT_MAXIMIZE)->setEnabled(viewportConfig && viewportConfig->layoutRootCell() && !viewportConfig->layoutRootCell()->children().empty());
}

/******************************************************************************
* This is called whenever the scene node selection changed.
******************************************************************************/
void ActionManager::onSelectionChangeComplete(SelectionSet* selection)
{
    getCommand(ACTION_EDIT_DELETE)->setEnabled(selection && !selection->nodes().empty());
    getCommand(ACTION_EDIT_CLONE_PIPELINE)->setEnabled(selection && !selection->nodes().empty());
    getCommand(ACTION_EDIT_RENAME_PIPELINE)->setEnabled(selection && !selection->nodes().empty());
}

/******************************************************************************
* Registers an action with the ActionManager.
******************************************************************************/
void ActionManager::addAction(QAction* action)
{
    OVITO_CHECK_POINTER(action);
    OVITO_ASSERT_MSG(action->parent() == this || findAction(action->objectName()) == nullptr, "ActionManager::addAction()", qPrintable(QStringLiteral("There is already an action with the same ID: %1").arg(action->objectName())));
    OVITO_ASSERT(!_actions.contains(action));

    // Make the action a child of this object.
    action->setParent(this);
    beginInsertRows(QModelIndex(), _actions.size(), _actions.size());
    _actions.push_back(action);
    endInsertRows();
}

/******************************************************************************
* Removes the given action from the ActionManager and deletes it.
******************************************************************************/
void ActionManager::deleteAction(QAction* action)
{
    OVITO_CHECK_POINTER(action);
    OVITO_ASSERT_MSG(action->parent() == this, "ActionManager::deleteAction()", "The action is not owned by the ActionManager.");
    OVITO_ASSERT_MSG(_actions.contains(action), "ActionManager::deleteAction()", "The action has not been registered with the ActionManager.");

    // Make the action a child of this object.
    int index = _actions.indexOf(action);
    beginRemoveRows(QModelIndex(), index, index);
    _actions.remove(index);
    delete action;
    endRemoveRows();
}

/******************************************************************************
* Registers a command with the ActionManager and creates its QAction view.
******************************************************************************/
Command* ActionManager::addCommand(Command* command)
{
    OVITO_CHECK_POINTER(command);
    OVITO_ASSERT_MSG(!command->id().isEmpty(), "ActionManager::addCommand()", "A registered command must have a non-empty identifier.");
    OVITO_ASSERT_MSG(findCommand(command->id()) == nullptr, "ActionManager::addCommand()", qPrintable(QStringLiteral("There is already a command with the same ID: %1").arg(command->id())));

    command->setParent(this);
    _commands.push_back(command);
    _commandsById.insert(command->id(), command);
    createActionView(command);
    Q_EMIT commandsChanged();
    return command;
}

/******************************************************************************
* Unregisters a command and deletes it together with its QAction view.
******************************************************************************/
void ActionManager::deleteCommand(Command* command)
{
    OVITO_CHECK_POINTER(command);
    OVITO_ASSERT_MSG(command->parent() == this, "ActionManager::deleteCommand()", "The command is not owned by the ActionManager.");
    OVITO_ASSERT_MSG(_commands.contains(command), "ActionManager::deleteCommand()", "The command has not been registered with the ActionManager.");

    // Remove the QAction that presents the command. It is the view the classic frontend and the widgets built from it
    // hold on to, so it has to go first.
    if(QAction* action = _actionViews.value(command, nullptr)) {
        _actionViews.remove(command);
        _commandOfAction.remove(action);
        deleteAction(action);
    }

    _commands.removeOne(command);
    _commandsById.remove(command->id());
    delete command;
    Q_EMIT commandsChanged();
}

/******************************************************************************
* Creates and registers a new command with the ActionManager.
******************************************************************************/
Command* ActionManager::createCommand(const QString& id, const QString& title, const char* iconPath, const QString& statusTip, const QKeySequence& shortcut)
{
    return addCommand(new Command(id, title, iconPath ? QString::fromLatin1(iconPath) : QString(), statusTip, shortcut));
}

/******************************************************************************
* Creates and registers a new command that activates a viewport input mode.
******************************************************************************/
Command* ActionManager::createViewportModeCommand(const QString& id, OORef<ViewportInputMode> inputMode, const QString& title, const char* iconPath, const QString& statusTip, const QKeySequence& shortcut, const QColor& highlightColor)
{
    return addCommand(new ViewportModeCommand(ui(), id, title, std::move(inputMode), highlightColor, iconPath ? QString::fromLatin1(iconPath) : QString(), statusTip, shortcut));
}

/******************************************************************************
* Creates the QAction that presents the given command in a QtWidgets frontend.
******************************************************************************/
QAction* ActionManager::createActionView(Command* command)
{
    OVITO_ASSERT(!_actionViews.contains(command));

    QAction* action = new QAction(this);
    action->setObjectName(command->id());
    _actionViews.insert(command, action);
    _commandOfAction.insert(action, command);

    // The command owns the state; the QAction only presents it. Conversely, the user operates the
    // QAction, which reports the new check state back to the command.
    connect(command, &Command::changed, action, [command, action]() { updateActionView(command, action); });
    connect(action, &QAction::triggered, command, &Command::trigger);
    connect(action, &QAction::toggled, command, &Command::setChecked);

    updateActionView(command, action);
    addAction(action);
    return action;
}

/******************************************************************************
* Copies the state of a command to its QAction view.
******************************************************************************/
void ActionManager::updateActionView(Command* command, QAction* action)
{
    action->setText(command->text());
    action->setToolTip(command->toolTip());
    action->setStatusTip(command->statusTip());
    action->setShortcut(command->shortcut());
    action->setCheckable(command->isCheckable());
    action->setChecked(command->isChecked());
    action->setEnabled(command->isEnabled());
    action->setVisible(command->isVisible());
    const QString& iconPath = command->iconPath();
    if(!iconPath.isEmpty())
        action->setIcon(iconPath.startsWith(QLatin1Char(':')) ? QIcon(iconPath) : QIcon::fromTheme(iconPath));
}

/******************************************************************************
* Returns a list of all commands, for use by a QML frontend.
******************************************************************************/
QVariantList ActionManager::commandList() const
{
    QVariantList list;
    list.reserve(_commands.size());
    for(Command* command : _commands)
        list.push_back(QVariant::fromValue(command));
    return list;
}

/******************************************************************************
* Invokes the command with the given ID.
******************************************************************************/
void ActionManager::triggerCommand(const QString& commandId) const
{
    if(Command* command = findCommand(commandId))
        command->trigger();
}

/******************************************************************************
* Returns the data stored in this list model under the given role.
******************************************************************************/
QVariant ActionManager::data(const QModelIndex& index, int role) const
{
    if(index.row() < 0) return {};
    QAction* action = _actions[index.row()];
    if(role == Qt::DisplayRole) {
        QString text = action->text();
        if(text.endsWith(QStringLiteral("...")))
            text.chop(3);
        return text;
    }
    else if(role == SearchTextRole)
        return QStringLiteral("%1 %2").arg(action->text(), action->statusTip());
    else if(role == ActionRole)
        return QVariant::fromValue(action);
    else if(role == CommandRole)
        return QVariant::fromValue(_commandOfAction.value(action, nullptr));
    else if(role == Qt::StatusTipRole)
        return action->statusTip();
    else if(role == Qt::DecorationRole)
        return action->icon();
    else if(role == ShortcutRole)
        return action->shortcut();
    else if(role == Qt::FontRole) {
        static QFont font = QGuiApplication::font();
        font.setBold(true);
        return font;
    }
    return {};
}

/******************************************************************************
* Returns the flags for an item in this list model.
******************************************************************************/
Qt::ItemFlags ActionManager::flags(const QModelIndex& index) const
{
    Qt::ItemFlags flags = QAbstractListModel::flags(index);
    if(index.row() >= 0 && index.row() < _actions.size()) {
        QAction* action = _actions[index.row()];
        if(!action->isEnabled())
            flags.setFlag(Qt::ItemIsEnabled, false);
    }
    return flags;
}

/******************************************************************************
* Updates the enabled/disabled state of all actions.
******************************************************************************/
void ActionManager::updateActionStates()
{
    Q_EMIT actionUpdateRequested();
}

/******************************************************************************
* Handles ACTION_EDIT_DELETE command.
******************************************************************************/
void ActionManager::on_EditDelete_triggered()
{
    performTransaction(tr("Delete pipeline"), [&]() {
        // Get active scene.
        if(Scene* scene = datasetContainer().activeScene()) {
            // Delete all nodes in selection set.
            while(!scene->selection()->nodes().empty())
                scene->selection()->nodes().front()->requestObjectDeletion();

            // Automatically select one of the remaining nodes.
            if(scene->children().isEmpty() == false)
                scene->selection()->setNode(scene->children().front());
        }
    });
}

/******************************************************************************
* Handles user clicks on an action link.
******************************************************************************/
void ActionManager::handleActionLink(const QString& link)
{
    if(link.startsWith("action:")) {
        QString actionId = link.mid(7);
        if(QAction* action = findAction(actionId)) {
            action->trigger();
        }
        else {
            ui().reportError(tr("Action not found: %1").arg(actionId));
        }
    }
    else if(link.startsWith("manual:")) {
        openHelpTopic(link);
    }
    else {
        ui().reportError(tr("Invalid action link: %1").arg(link));
    }
}

/******************************************************************************
* Shows the online manual and opens the given help page.
******************************************************************************/
void ActionManager::openHelpTopic(const QString& helpTopicId)
{
    // Determine the filesystem path where OVITO's documentation files are installed.
#ifndef Q_OS_WASM
    QDir prefixDir(Application::instance()->applicationDirPath());
    QDir helpDir = QDir(prefixDir.absolutePath() + QChar('/') + QStringLiteral(OVITO_DOCUMENTATION_PATH));
    QUrl url;
#else
    QDir helpDir(QStringLiteral(":/doc/manual/"));
    QUrl baseUrl(QStringLiteral("https://docs.ovito.org/"));
    QUrl url = baseUrl;
#endif

    // Resolve the help topic ID.
    if(helpTopicId.endsWith(".html") || helpTopicId.contains(".html#")) {
        // If a HTML file name has been specified, open it directly.
        url = QUrl::fromLocalFile(helpDir.absoluteFilePath(helpTopicId));
    }
    else if(helpTopicId.startsWith("manual:")) {
        // If a Sphinx link target has been specified, resolve it to a HTML file path using the
        // Intersphinx inventory. The file 'objects.txt' is generated by the script 'ovito/doc/manual/CMakeLists.txt'
        // and gets distributed together with the application.
        QFile inventoryFile(helpDir.absoluteFilePath("objects.txt"));
        if(!inventoryFile.open(QIODevice::ReadOnly | QIODevice::Text))
            qWarning() << "WARNING: Could not open Intersphinx inventory file to resolve help topic reference:" << inventoryFile.fileName() << inventoryFile.errorString();
        else {
            QTextStream stream(&inventoryFile);
            // Skip file until to the line "std:label":
            while(!stream.atEnd()) {
                QString line = stream.readLine();
                if(line.startsWith("std:label"))
                    break;
            }
            // Now parse the link target list.
            QString searchString = helpTopicId.mid(7) + QChar(' ');
            while(!stream.atEnd()) {
                QString line = stream.readLine().trimmed();
                if(line.startsWith(searchString)) {
                    int startIndex = line.lastIndexOf(QChar(' '));
                    QString filePath = line.mid(startIndex + 1).trimmed();
                    QString anchor;
                    int anchorIndex = filePath.indexOf(QChar('#'));
                    if(anchorIndex >= 0) {
                        anchor = filePath.mid(anchorIndex + 1);
                        filePath.truncate(anchorIndex);
                    }
#ifndef Q_OS_WASM
                    url = QUrl::fromLocalFile(helpDir.absoluteFilePath(filePath));
#else
                    url.setPath(QChar('/') + filePath);
#endif
                    url.setFragment(anchor);
                    break;
                }
            }
            OVITO_ASSERT(!url.isEmpty());
        }
    }

#ifndef Q_OS_WASM
    if(url.isEmpty()) {
        // If no help topic has been specified, open the main index page of the user manual.
        url = QUrl::fromLocalFile(helpDir.absoluteFilePath(QStringLiteral("index.html")));
    }
#endif

    // Workaround for a limitation of the Microsoft Edge and Apple Safari browsers:
    // These browsers drop any # fragment in local URLs to be opened, thus making it difficult to reference sub-topics within a HTML help page.
    // Solution is to generate a temporary HTML file which redirects to the actual help page including the # fragment.
    // See also https://forums.madcapsoftware.com/viewtopic.php?f=9&t=28376#p130613
    // and https://stackoverflow.com/questions/26305322/shellexecute-fails-for-local-html-or-file-urls
#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
    if(url.isLocalFile() && url.hasFragment()) {
        static QTemporaryFile* temporaryHtmlFile = nullptr;
        if(temporaryHtmlFile)
            delete temporaryHtmlFile;
        temporaryHtmlFile = new QTemporaryFile(QDir::temp().absoluteFilePath(QStringLiteral("ovito-help-XXXXXX.html")), qApp);
        if(temporaryHtmlFile->open()) {
            // Write a small HTML file that just contains a redirect directive to the actual help page including the # fragment.
            QTextStream(temporaryHtmlFile) << QStringLiteral("<html><meta http-equiv=Refresh content=\"0; url=%1\"><body><a href=\"%1\">Continue to help topic...</a></body></html>").arg(url.toString(QUrl::FullyEncoded));
            temporaryHtmlFile->close();
            // Let the web browser open the redirect page instead of the original help page.
            url = QUrl::fromLocalFile(temporaryHtmlFile->fileName());
        }
    }
#endif

    // Use the local web browser to display the help page.
    if(!QDesktopServices::openUrl(url)) {
        ui().reportError(QStringLiteral("Failed to launch browser to display OVITO user manual. The requested URL was:\n%1").arg(url.toDisplayString()));
    }
}

}   // End of namespace
