// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/base/GUIBase.h>
#include <ovito/core/app/UserInterface.h>
#include <ovito/core/dataset/io/FileImporter.h>

namespace Ovito {

/**
 * \brief Base class for the workbench implementations of the graphical user interface frontends.
 *
 * This class implements the parts of the UserInterface contract that are independent of how a frontend presents itself
 * to the user: the import of simulation data files (built on the shared FileImporter implementations), the bookkeeping
 * of the task progress records that drive progress displays, error reporting, and the auto-key mode.
 *
 * The concrete workbenches — the classic QtWidgets main window of the desktop frontend and the Qt Quick workbench of
 * the QML frontend — implement the presentation by overriding the virtual methods declared here, so that a user
 * interaction that only differs in its presentation (e.g. "which files should be imported?" or "how is the import
 * progress shown?") exists only once in the code base.
 *
 * Note that the frontend-specific parts of the import workflow that require user input are virtual methods, not
 * callbacks: a frontend that has no way to ask the user has to make a decision, and this class documents the decision
 * it makes by default.
 */
class OVITO_GUIBASE_EXPORT WorkbenchUI : public UserInterface
{
    OVITO_CLASS(WorkbenchUI)

public:

    /// Imports a set of files into the current dataset of this workbench.
    /// \param urls The locations of the files to import.
    /// \param importerType The FileImporter type to use. If null, the format of each file is auto-detected.
    /// \param importerFormat The sub-format of the selected importer class to use.
    /// \throw Exception if a file cannot be imported, or if the user cancels the operation.
    void importFiles(const std::vector<QUrl>& urls, const FileImporterClass* importerType = nullptr, const QString& importerFormat = {});

    /// Makes the given directory the working directory and lets the user select files to import into the current dataset.
    void openWorkingDirectory(const QString& directoryPath);

    /// \brief Returns whether animation recording is active and animation keys should be automatically generated.
    /// \return \c true if animating is currently turned on and not suspended; \c false otherwise.
    ///
    /// When animating is turned on, controllers should automatically set keys when their value is changed.
    virtual bool isAutoGenerateAnimationKeysEnabled() const override { return _autoKeyModeOn && _animSuspendCount == 0; }

    /// Turns auto-key animation recording on or off. The frontend connects this to the control that lets the user toggle it.
    void setAutoKeyModeEnabled(bool enabled) { _autoKeyModeOn = enabled; }

    /// Destructor. Every task of the workbench must have finished by the time the workbench is destroyed.
    /// Note: OVITO objects are never destroyed through a base class pointer, so this destructor is deliberately not
    /// marked \c override (the base class of UserInterface has no virtual destructor).
    ~WorkbenchUI();

    /// Registers a new task progress record with this user interface.
    /// This method gets called when a new TaskProgress instance is created from a running task.
    virtual std::mutex* taskProgressBegin(TaskProgress* progress) override;

    /// Unregisters a task progress record from this user interface.
    /// This method gets called when a previously registered task finishes.
    virtual void taskProgressEnd(TaskProgress* progress) override;

    /// Informs the user interface that a task's progress state has changed.
    virtual void taskProgressChanged(TaskProgress* progress) override;

    /// Lets the caller visit all registered tasks that are still in progress.
    void visitRunningTasks(const std::function<void(const QString& text, int progressValue, int progressMaximum)>& visitor);

    /// Returns the model that lists the running tasks of this workbench and their progress, as the frontend displays
    /// them.
    /// \return The task progress model, or null before the workbench has been initialized (see initializeWorkbench()).
    TaskProgressModel* taskProgressModel() const { return _taskProgressModel; }

    /// Displays an error message to the user.
    virtual void reportError(const Exception& ex, bool blocking = false) override;

protected:

    /// Creates the viewport input manager, the undo stack and the action manager of this workbench.
    /// \param parent The QObject that owns the created objects, or null if the workbench has no such object.
    ///
    /// The concrete workbench calls this once it has created the object that its presentation is attached to, because
    /// the created objects are parented to it.
    void initializeWorkbench(QObject* parent);

    /// Creates the action manager of this workbench. Called by initializeWorkbench().
    /// The returned object must be owned by the given parent object.
    virtual ActionManager* createActionManager(QObject* parent) = 0;

    /// Presents an error message to the user. reportError() has already written the message to the terminal.
    virtual void displayErrorMessage(const Exception& ex, bool blocking) = 0;

    /// Is called when the progress state of the registered tasks has changed, at most once every 100 milliseconds.
    /// The task progress model of the workbench has been refreshed by the time an override of this method runs, so a
    /// frontend only has to react to it if it presents the progress in a way of its own.
    virtual void progressTasksChanged() {}

    /// Shows the frontend's user interface that lets the user select the files to import.
    /// The default implementation reports that this frontend cannot select files interactively.
    virtual void openImportDialog(const QString& directoryPath);

    /// Lets the user inspect and adjust the settings of the importers that were created for the files to be imported.
    /// The default implementation does not present the importers to the user.
    virtual void inspectImporterFiles(const std::vector<std::pair<QUrl, OORef<FileImporter>>>& urlImporters) {}

    /// Determines how the data to be imported is merged into the given scene.
    ///
    /// The default implementation discards the existing pipeline whenever the new data can replace it, and appends the
    /// new data otherwise, so that a frontend without a dialog never discards work of the user silently. Frontends
    /// that can ask the user override this method.
    virtual FileImporter::ImportMode determineImportMode(Scene* scene, const std::vector<QUrl>& urls, FileImporter* importer);

    /// Runs the import operation and waits for its completion, showing the progress to the user where the frontend can.
    virtual void runFileImport(FileImporter& importer, Scene* scene, std::vector<std::pair<QUrl, OORef<FileImporter>>> urlImporters, FileImporter::ImportMode importMode);

    /// Is called after the working directory of the file import operation has changed.
    /// The default implementation does not remember the directory.
    virtual void importDirectoryChanged(const QString& directoryPath) {}

    /// Returns all pipelines in the given dataset's scene whose input data comes from a file.
    /// Used by frontends that need to restrict a session to a subset of its pipelines.
    static std::vector<OORef<SceneNode>> fileSourcePipelines(DataSet* dataset);

private:

    /// Notifies the frontend that the progress display should be updated.
    void notifyProgressTasksChanged();

private:

    /// Indicates whether the user has activated auto-key animation mode.
    bool _autoKeyModeOn = false;

    /// Lists the running tasks of this workbench for the frontend. Owned by the object passed to initializeWorkbench().
    TaskProgressModel* _taskProgressModel = nullptr;

    /// Head of doubly-linked list of all registered task progress records.
    TaskProgress* _progressTasksHead = nullptr;

    /// Tail of doubly-linked list of all registered task progress records.
    TaskProgress* _progressTasksTail = nullptr;

    /// Guards thread-safe access to the task list.
    std::mutex _progressTaskListMutex;

    /// Indicates that a delayed task progress update is underway.
    std::atomic_bool _progressUpdateScheduled{false};
};

}   // End of namespace
