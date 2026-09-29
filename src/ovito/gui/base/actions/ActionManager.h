// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/base/GUIBase.h>
#include <ovito/core/dataset/animation/TimeInterval.h>
#include <ovito/gui/base/actions/Command.h>

namespace Ovito {

//////////////////////// Action identifiers ///////////////////////////

/// This action closes the main window and exits the application.
#define ACTION_QUIT             "Quit"
/// This action shows the file open dialog.
#define ACTION_FILE_OPEN        "FileOpen"
/// This action saves the current file.
#define ACTION_FILE_SAVE        "FileSave"
/// This action shows the file save as dialog.
#define ACTION_FILE_SAVEAS      "FileSaveAs"
/// This action shows the file import dialog.
#define ACTION_FILE_IMPORT      "FileImport"
/// This action shows the remote file import dialog.
#define ACTION_FILE_REMOTE_IMPORT "FileRemoteImport"
/// This action shows the file export dialog.
#define ACTION_FILE_EXPORT      "FileExport"
/// This action opens another main window.
#define ACTION_FILE_NEW_WINDOW  "FileNewWindow"

/// This action shows the about dialog.
#define ACTION_HELP_ABOUT               "HelpAbout"
/// This action shows the online help.
#define ACTION_HELP_SHOW_ONLINE_HELP    "HelpShowOnlineHelp"
/// This action shows the scripting reference manual.
#define ACTION_HELP_SHOW_SCRIPTING_HELP "HelpShowScriptingReference"
/// This action displays graphics hardware information.
#define ACTION_HELP_GRAPHICS_SYSINFO    "HelpSystemInfo"
/// This action opens the feature request form.
#define ACTION_HELP_REQUEST_FEATURE     "HelpRequestFeature"

/// This action undoes the last operation.
#define ACTION_EDIT_UNDO                "EditUndo"
/// This action does the last undone operation again.
#define ACTION_EDIT_REDO                "EditRedo"
/// This action deletes the selected scene object.
#define ACTION_EDIT_DELETE              "EditDelete"
/// This action duplicates the selected scene object.
#define ACTION_EDIT_CLONE_PIPELINE      "ClonePipeline"
/// This action opens the rename pipeline dialog.
#define ACTION_EDIT_RENAME_PIPELINE     "RenamePipeline"
/// This action clears the current undo stack.
#define ACTION_EDIT_CLEAR_UNDO_STACK    "EditClearUndoStack"

/// This action maximizes the active viewport.
#define ACTION_VIEWPORT_MAXIMIZE                    "ViewportMaximize"
/// This action activates the viewport zoom mode.
#define ACTION_VIEWPORT_ZOOM                        "ViewportZoom"
/// This action activates the viewport pan mode.
#define ACTION_VIEWPORT_PAN                         "ViewportPan"
/// This action activates the viewport orbit mode.
#define ACTION_VIEWPORT_ORBIT                       "ViewportOrbit"
/// This action activates the field of view viewport mode.
#define ACTION_VIEWPORT_FOV                         "ViewportFOV"
/// This action activates the 'pick center of rotation' input mode.
#define ACTION_VIEWPORT_PICK_ORBIT_CENTER           "ViewportOrbitPickCenter"
/// This zooms the current viewport to the scene extents
#define ACTION_VIEWPORT_ZOOM_SCENE_EXTENTS          "ViewportZoomSceneExtents"
/// This zooms the current viewport to the selection extents
#define ACTION_VIEWPORT_ZOOM_SELECTION_EXTENTS      "ViewportZoomSelectionExtents"
/// This zooms all viewports to the scene extents
#define ACTION_VIEWPORT_ZOOM_SCENE_EXTENTS_ALL      "ViewportZoomSceneExtentsAll"
/// This zooms all viewports to the selection extents
#define ACTION_VIEWPORT_ZOOM_SELECTION_EXTENTS_ALL  "ViewportZoomSelectionExtentsAll"
/// Opens the viewport graphics configuration dialog.
#define ACTION_CONFIGURE_VIEWPORT_GRAPHICS          "ConfigureViewportGraphics"

/// This action deletes the currently selected modifier from the modifier stack.
#define ACTION_MODIFIER_DELETE                    "ModifierDelete"
/// This action moves the currently selected modifier up one entry in the modifier stack.
#define ACTION_MODIFIER_MOVE_UP                   "ModifierMoveUp"
/// This action moves the currently selected modifier down one entry in the modifier stack.
#define ACTION_MODIFIER_MOVE_DOWN                 "ModifierMoveDown"
/// This action opens the dialog box for managing modifier templates.
#define ACTION_MODIFIER_MANAGE_MODIFIER_TEMPLATES "ModifierManageTemplates"
/// This action creates a unique copy of the selected pipeline item.
#define ACTION_PIPELINE_MAKE_INDEPENDENT          "PipelineMakeUnique"
/// This action creates or dissolves a modifier group in the pipeline editor.
#define ACTION_PIPELINE_TOGGLE_MODIFIER_GROUP     "PipelineToggleModifierGroup"
/// This action renames the selected entry in the pipeline editor.
#define ACTION_PIPELINE_RENAME_ITEM               "PipelineItemRename"
/// Copies/clones a modifier or source from one pipeline to another.
#define ACTION_PIPELINE_COPY_ITEM                 "PipelineItemCopyTo"
/// Unites several compatible visual elements into one element.
#define ACTION_PIPELINE_GROUP_VIS_ELEMENTS        "PipelineGroupVisElements"
/// Exports the selected modifier(s) as text snippet.
#define ACTION_MODIFIER_EXPORT_SNIPPET            "ModifierExportSnippet"
/// Imports one or more modifiers from a text snippet.
#define ACTION_MODIFIER_IMPORT_SNIPPET            "ModifierImportSnippet"

/// This action deletes the currently selected viewport layer.
#define ACTION_VIEWPORT_LAYER_DELETE             "ViewportLayerDelete"
/// This action moves the currently selected viewport layer up one entry in the stack.
#define ACTION_VIEWPORT_LAYER_MOVE_UP            "ViewportLayerMoveUp"
/// This action moves the currently selected viewport layer down one entry in the stack.
#define ACTION_VIEWPORT_LAYER_MOVE_DOWN          "ViewportLayerMoveDown"
/// This action renames the currently selected viewport layer.
#define ACTION_VIEWPORT_LAYER_RENAME             "ViewportLayerRename"
/// This action opens the dialog box for managing viewport layer templates.
#define ACTION_VIEWPORT_MANAGE_OVERLAY_TEMPLATES "ViewportLayerManageTemplates"

/// This action jumps to the start of the animation
#define ACTION_GOTO_START_OF_ANIMATION      "AnimationGotoStart"
/// This action jumps to the end of the animation
#define ACTION_GOTO_END_OF_ANIMATION        "AnimationGotoEnd"
/// This action jumps to previous frame in the animation
#define ACTION_GOTO_PREVIOUS_FRAME          "AnimationGotoPreviousFrame"
/// This action jumps to next frame in the animation
#define ACTION_GOTO_NEXT_FRAME              "AnimationGotoNextFrame"
/// This action toggles animation playback
#define ACTION_TOGGLE_ANIMATION_PLAYBACK    "AnimationTogglePlayback"
/// This action starts the animation playback
#define ACTION_START_ANIMATION_PLAYBACK     "AnimationStartPlayback"
/// This action starts the animation playback
#define ACTION_STOP_ANIMATION_PLAYBACK      "AnimationStopPlayback"
/// This action shows the animation settings dialog
#define ACTION_ANIMATION_SETTINGS           "AnimationSettings"
/// This action activates/deactivates the animation mode
#define ACTION_AUTO_KEY_MODE_TOGGLE         "AnimationToggleRecording"

/// This action starts rendering of the current view.
#define ACTION_RENDER_ACTIVE_VIEWPORT       "RenderActiveViewport"

/// This actions open the application's "Settings" dialog.
#define ACTION_SETTINGS_DIALOG              "Settings"
/// Opens a list of commands for quick access by the user.
#define ACTION_COMMAND_QUICKSEARCH          "CommandQuickSearch"
/// Activates the "Modify" tab of the command panel.
#define ACTION_COMMAND_PANEL_MODIFY         "CommandPanel.Modify"
/// Activates the "Render" tab of the command panel.
#define ACTION_COMMAND_PANEL_RENDER         "CommandPanel.Render"
/// Activates the "Viewport layers" tab of the command panel.
#define ACTION_COMMAND_PANEL_OVERLAYS       "CommandPanel.Overlays"
/// Activates the "Utilities" tab of the command panel.
#define ACTION_COMMAND_PANEL_UTILITIES      "CommandPanel.Utilities"

/// This actions activates the scene node selection mode.
#define ACTION_SELECTION_MODE               "SelectionMode"
/// This actions activates the scene node translation mode.
#define ACTION_XFORM_MOVE_MODE              "XFormMoveMode"
/// This actions activates the scene node rotation mode.
#define ACTION_XFORM_ROTATE_MODE            "XFormRotateMode"

/// This actions lets the user select a script file to run.
#define ACTION_SCRIPTING_RUN_FILE           "ScriptingRunFile"
/// This actions lets the user generate script code from the selected data pipeline.
#define ACTION_SCRIPTING_GENERATE_CODE      "ScriptingGenerateCode"
/// This actions shows the Python extensions gallery.
#define ACTION_SCRIPTING_EXTENSIONS_GALLERY "ScriptingShowExtensionsGallery"
#define ACTION_SCRIPTING_EXTENSIONS_GALLERY_MODIFIERS   "ScriptingShowExtensionsGallery.Modifiers"
#define ACTION_SCRIPTING_EXTENSIONS_GALLERY_FILEREADERS "ScriptingShowExtensionsGallery.FileReaders"
#define ACTION_SCRIPTING_EXTENSIONS_GALLERY_FILEWRITERS "ScriptingShowExtensionsGallery.FileWriters"
#define ACTION_SCRIPTING_EXTENSIONS_GALLERY_OVERLAYS    "ScriptingShowExtensionsGallery.ViewportLayers"
#define ACTION_SCRIPTING_EXTENSIONS_GALLERY_UTILITIES   "ScriptingShowExtensionsGallery.Utilities"

/// This action adds a new pipeline to the scene with a FileSource.
#define ACTION_NEW_PIPELINE_FILESOURCE      "NewPipeline.FileSource"
/// This action adds a new pipeline to the scene with a PythonSource.
#define ACTION_NEW_PIPELINE_PYTHONSOURCE    "NewPipeline.PythonSource"
/// This action adds a new pipeline to the scene with a LammpsScriptSource.
#define ACTION_NEW_PIPELINE_LAMMPSSOURCE    "NewPipeline.LammpsScriptSource"

/**
 * \brief Manages all available user interface actions.
 */
class OVITO_GUIBASE_EXPORT ActionManager : public QAbstractListModel, public UserInterfaceComponent<UserInterface>
{
    Q_OBJECT

public:

    /// Item model roles supported by this QAbstractListModel.
    enum ModelRoles {
        ActionRole = Qt::UserRole,  ///< Pointer to the QAction object.
        CommandRole,                ///< Pointer to the Command object.
        ShortcutRole,               ///< QKeySequence of the action's shortcut.
        SearchTextRole              ///< The text string used for seaching commands.
    };

    /// Constructor.
    ActionManager(QObject* parent, UserInterface& ui);

    /// Returns dataset currently being edited in the main window.
    DataSet* dataset() const;

    /// \brief Returns the action with the given ID or NULL.
    /// \param actionId The identifier string of the action to return.
    QAction* findAction(const QString& actionId) const {
        return findChild<QAction*>(actionId);
    }

    /// \brief Returns the action with the given ID.
    /// \param actionId The unique identifier string of the action to return.
    QAction* getAction(const QString& actionId) const {
        QAction* action = findAction(actionId);
        OVITO_ASSERT_MSG(action != nullptr, "ActionManager::getAction()", "Action does not exist.");
        return action;
    }

    /// Returns the list of registered actions. Each command owns one action, which is its view in a QtWidgets frontend.
    const QVector<QAction*>& actions() const { return _actions; }

    /// Returns the QAction view belonging to the given command, or null if the command is not registered here.
    QAction* actionView(const Command* command) const { return _actionViews.value(command, nullptr); }

    /// \brief Returns the command with the given ID or null.
    /// \param commandId The identifier string of the command to return.
    Command* findCommand(const QString& commandId) const { return _commandsById.value(commandId, nullptr); }

    /// \brief Returns the command with the given ID. Asserts that the command exists.
    Command* getCommand(const QString& commandId) const {
        Command* command = findCommand(commandId);
        OVITO_ASSERT_MSG(command != nullptr, "ActionManager::getCommand()", "Command does not exist.");
        return command;
    }

    /// Returns the list of registered commands in the order in which they were created.
    const QVector<Command*>& commands() const { return _commands; }

    /// The list of all commands as a QVariantList of Command objects, for use by a QML frontend.
    Q_PROPERTY(QVariantList commandList READ commandList NOTIFY commandsChanged)

    /// \brief Returns a list of all commands, for use by a QML frontend.
    QVariantList commandList() const;

    /// \brief Returns the command with the given ID, or null if there is no such command. Intended for use by a QML frontend.
    Q_INVOKABLE Ovito::Command* command(const QString& commandId) const { return findCommand(commandId); }

    /// \brief Invokes the command with the given ID. Does nothing if the command does not exist. Intended for use by a QML frontend.
    Q_INVOKABLE void triggerCommand(const QString& commandId) const;

    /// \brief Registers a new action with the ActionManager.
    /// \param action The action to be registered. The ActionManager will take ownership of the object.
    void addAction(QAction* action);

    /// \brief Registers a command with the ActionManager, which takes ownership of it and creates its QAction view.
    /// \returns The registered command.
    Command* addCommand(Command* command);

    /// \brief Unregisters a command and deletes it together with its QAction view.
    /// Used for the commands of the modifier and viewport layer libraries, which appear and disappear while the
    /// application runs (see AvailableModifiersModel::refreshTemplates).
    void deleteCommand(Command* command);

    /// \brief Creates and registers a new command with the ActionManager. All state changes go through the returned command.
    Command* createCommand(const QString& id,
                        const QString& title,
                        const char* iconPath = nullptr,
                        const QString& statusTip = QString(),
                        const QKeySequence& shortcut = QKeySequence());

    /// \brief Creates and registers a new command that activates a viewport input mode.
    Command* createViewportModeCommand(const QString& id,
                        OORef<ViewportInputMode> inputMode,
                        const QString& title,
                        const char* iconPath = nullptr,
                        const QString& statusTip = QString(),
                        const QKeySequence& shortcut = QKeySequence(),
                        const QColor& highlightColor = QColor());

    /// \brief Removes the given action from the ActionManager and deletes it.
    /// \param action The action to be deletes.
    void deleteAction(QAction* action);

    /// \brief Returns the number of rows in this list model.
    virtual int rowCount(const QModelIndex& parent) const override { return _actions.size(); }

    /// \brief Returns the data stored in this list model under the given role.
    virtual QVariant data(const QModelIndex& index, int role) const override;

    /// \brief Returns the flags for an item in this list model.
    virtual Qt::ItemFlags flags(const QModelIndex& index) const override;

public Q_SLOTS:

    /// Shows the online manual and opens the given help page.
    void openHelpTopic(const QString& page);

    /// Handles user clicks on an action link.
    void handleActionLink(const QString& link);

Q_SIGNALS:

    /// \brief This signal is emitted by the ActionManager when the quick command search is activated. It tells the system to refresh the enabled/disabled state of actions as needed.
    void actionUpdateRequested();

    /// This signal is emitted when a command has been added to the ActionManager.
    void commandsChanged();

private Q_SLOTS:

    /// This is called when a new dataset has been loaded.
    void onDataSetChanged(DataSet* newDataSet);

    /// This is called when the active animation interval has changed.
    void onAnimationIntervalChanged(int firstFrame, int lastFrame);

    /// This is called when a different viewport become the maximized one.
    void onMaximizedViewportChanged(Viewport* maximizedViewport);

    /// This is called when the viewport layout changes.
    void onViewportLayoutChanged(ViewportConfiguration* viewportConfig);

    /// This is called whenever the scene node selection changed.
    void onSelectionChangeComplete(SelectionSet* selection);

    void on_ViewportMaximize_triggered();
    void on_ViewportZoomSceneExtents_triggered();
    void on_ViewportZoomSelectionExtents_triggered();
    void on_ViewportZoomSceneExtentsAll_triggered();
    void on_ViewportZoomSelectionExtentsAll_triggered();
    void on_AnimationGotoStart_triggered();
    void on_AnimationGotoEnd_triggered();
    void on_AnimationGotoPreviousFrame_triggered();
    void on_AnimationGotoNextFrame_triggered();
    void on_AnimationStartPlayback_triggered();
    void on_AnimationStopPlayback_triggered();
    void on_EditDelete_triggered();

protected:

    void updateActionStates();

private:

    /// Creates the QAction that presents the given command in a QtWidgets frontend and keeps it in sync with the command.
    QAction* createActionView(Command* command);

    /// Copies the state of a command to its QAction view.
    static void updateActionView(Command* command, QAction* action);

    /// The list of registered actions.
    QVector<QAction*> _actions;

    /// The registered commands in creation order, and the same commands indexed by their ID.
    QVector<Command*> _commands;
    QHash<QString, Command*> _commandsById;

    /// The QAction that presents each command in a QtWidgets frontend, and the reverse mapping.
    QHash<const Command*, QAction*> _actionViews;
    QHash<const QAction*, Command*> _commandOfAction;
};

}   // End of namespace
