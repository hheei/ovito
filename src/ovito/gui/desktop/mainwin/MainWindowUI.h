// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/base/app/WorkbenchUI.h>

namespace Ovito {

/**
 * \brief Implementation of the abstract UserInterface that represents a physical OVITO MainWindow.
 *
 * The parts of the user interface contract that do not depend on the presentation are provided by the WorkbenchUI base
 * class (see gui/base), this class adds the QtWidgets main window and everything that requires a widget.
 */
class OVITO_GUI_EXPORT MainWindowUI : public WorkbenchUI
{
    OVITO_CLASS(MainWindowUI)

public:

    /// Initialization function.
    void initializeObject();

    /// Destructor.
    ~MainWindowUI();

    /// Indicates whether the physical main window widget still exists.
    /// The main window may be closed while the UI object can remain alive for some more time.
    bool hasMainWindow() const { return _mainWindow != nullptr; }

    /// Returns the main window widget associated with this UI object.
    /// Note: This method may only be called while the UI object is still associated with a main window.
    MainWindow* mainWindow() const { OVITO_ASSERT(hasMainWindow()); return _mainWindow; }

    /// Displays a message string in the window's status bar.
    virtual void showStatusBarMessage(const QString& message, int timeout = 0) override;

    /// Hides any messages currently displayed in the window's status bar.
    virtual void clearStatusBarMessage() override;

    /// Gives the active viewport the input focus.
    virtual void setViewportInputFocus() override;

    /// Closes the user interface and shuts down the entire application after displaying an error message.
    virtual void exitWithFatalError(const Exception& ex) override;

    /// Creates a frame buffer of the requested size for rendering and displays it in a window in the user interface.
    virtual std::shared_ptr<FrameBuffer> createAndShowFrameBuffer(int width, int height) override;

    /// Shows a progress bar or a similar UI to indicate the current rendering progress and let the user cancel the operation if necessary.
    virtual void showRenderingProgress(const std::shared_ptr<FrameBuffer>& frameBuffer, SharedFuture<void> renderingFuture) override;

    /// Cancels all running tasks associated with this user interface and closes the user interface as soon as possible (without asking user to save changes).
    virtual bool shutdown() override;

    /// Displays a modal message box to the user. Blocks until the user closes the message box.
    /// This method wraps the QMessageBox class of the Qt library.
    virtual MessageBoxButton showMessageBox(MessageBoxIcon icon, const QString& title, const QString& text, int buttons, MessageBoxButton defaultButton = NoButton, const QString& detailedText = {}) override;

    /// Checks (or even modifies) the contents of a DataSet after it has been loaded from a file.
    /// Returns false if loading the DataSet was rejected by the application.
    virtual bool checkLoadedDataset(DataSet* dataset) override;

    /// \brief Save the current dataset.
    /// \return \c true, if the dataset has been saved; \c false if the operation has been canceled by the user.
    /// \throw Exception on error.
    ///
    /// If the current dataset has not been assigned a file path, then this method
    /// displays a file selector dialog by calling fileSaveAs() to let the user select a file path.
    bool fileSave();

    /// \brief Lets the user select a new destination filename for the current dataset. Then saves the dataset by calling fileSave().
    /// \param filename If \a filename is an empty string that this method asks the user for a filename. Otherwise
    ///                 the provided filename is used.
    /// \return \c true, if the dataset has been saved; \c false if the operation has been canceled by the user.
    /// \throw Exception on error.
    bool fileSaveAs(const QString& filename = QString());

    /// \brief Asks the user if changes made to the dataset should be saved.
    ///
    /// If the current dataset has been changed, this method asks the user if changes should be saved.
    /// If yes, then the dataset is saved by calling fileSave().
    void askForSaveChanges();

    /// The type-erased function object type to be passed to scheduleOperationAfterScenePreparation().
    using operation_function = fu2::function_base<
        true, // IsOwning = true: The function object owns the callable object and is responsible for its destruction.
        false, // IsCopyable = false: The function object is not copyable.
        fu2::capacity_fixed<3 * sizeof(std::shared_ptr<OvitoObject>)>, // Capacity: Defines the internal capacity of the function for small functor optimization.
        false, // IsThrowing = false: Do not throw an exception on empty function call, call `std::abort` instead.
        true, // HasStrongExceptGuarantee = true: All objects satisfy the strong exception guarantee
        void()>;

    /// Waits for the pipelines in the current scene to be fully evaluated, then executes the given operation.
    /// A progress dialog may be displayed while waiting for the scene preparation to complete.
    void scheduleOperationAfterScenePreparation(Scene* scene, const QString& waitingMessage, operation_function&& operation);

    /// Returns the history of most recently used directories for file selection dialog type (e.g. data files, state files, Python scripts, ...).
    /// This function is used by the HistoryFileDialog class to maintain a separate history of recently used directories for different file I/O operations.
    QStringList getRecentlyUsedDirectories(const QString& dialogClass);

    /// Updates the history of most recently used directories for file selection dialog type (e.g. data files, state files, Python scripts, ...).
    /// This function is used by the HistoryFileDialog class to maintain a separate history of recently used directories for different file I/O operations.
    /// The given directory is moved to the top of the history list.
    void updateMostRecentlyUsedDirectory(const QString& dialogClass, const QString& directory);

private:

    /// Saves the list of most recently visited directories to the settings store at shutdown time.
    void saveMostRecentlyUsedDirectories();

protected:

    // Implementation of the presentation hooks of WorkbenchUI (see gui/base/app/WorkbenchUI.h).

    /// Creates the action manager of this workbench.
    virtual ActionManager* createActionManager(QObject* parent) override;

    /// Presents an error message to the user and lets them acknowledge it.
    virtual void displayErrorMessage(const Exception& ex, bool blocking) override;

    /// Updates the progress display of the main window.
    virtual void progressTasksChanged() override;

    /// Triggers the action that opens the file import dialog.
    virtual void openImportDialog(const QString& directoryPath) override;

    /// Displays the optional settings of every importer (see FileImporterEditor) to the user before the import starts.
    virtual void inspectImporterFiles(const std::vector<std::pair<QUrl, OORef<FileImporter>>>& urlImporters) override;

    /// Asks the user how the data to be imported should be merged into the current scene.
    virtual FileImporter::ImportMode determineImportMode(Scene* scene, const std::vector<QUrl>& urls, FileImporter* importer) override;

    /// Runs the import operation in a progress dialog, which lets the user cancel it.
    virtual void runFileImport(FileImporter& importer, Scene* scene, std::vector<std::pair<QUrl, OORef<FileImporter>>> urlImporters, FileImporter::ImportMode importMode) override;

    /// Remembers the directory of the imported file in the history of the file import dialog.
    virtual void importDirectoryChanged(const QString& directoryPath) override;

private:

    /// The main window widget associated with this UI object.
    MainWindow* _mainWindow = nullptr;

    /// History of most recently used directories, grouped by file selection dialog type (e.g. data files, state files, Python scripts, ...).
    std::map<QString, QStringList> _recentlyUsedDirectories;

    friend class MainWindow; // Allow direct access to the _mainWindow pointer.
};

// Instantiate class templates.
#ifndef OVITO_BUILD_MONOLITHIC
#if defined(Q_CC_MSVC)
extern template class OVITO_GUI_EXPORT UserInterfaceComponent<MainWindowUI>;
#endif
#endif

}   // End of namespace
