// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/desktop/app/GuiApplication.h>
#include <ovito/gui/desktop/app/GuiApplicationService.h>
#include <ovito/gui/desktop/mainwin/MainWindow.h>
#include <ovito/gui/desktop/widgets/animation/AnimationTimeSpinner.h>
#include <ovito/gui/desktop/widgets/animation/AnimationTimeSlider.h>
#include <ovito/gui/desktop/widgets/animation/AnimationTrackBar.h>
#include <ovito/gui/desktop/widgets/rendering/FrameBufferWindow.h>
#include <ovito/gui/desktop/widgets/display/CoordinateDisplayWidget.h>
#include <ovito/gui/desktop/widgets/general/StatusBar.h>
#include <ovito/gui/desktop/widgets/selection/SceneNodeSelectionBox.h>
#include <ovito/gui/desktop/dialogs/MessageDialog.h>
#include <ovito/gui/desktop/dialogs/ImportFileDialog.h>
#include <ovito/gui/desktop/dialogs/HistoryFileDialog.h>
#include <ovito/gui/desktop/actions/WidgetActionManager.h>
#include <ovito/gui/desktop/properties/PropertiesEditor.h>
#include <ovito/gui/desktop/dataset/io/FileImporterEditor.h>
#include <ovito/gui/desktop/utilities/concurrent/ProgressDialog.h>
#include <ovito/gui/base/viewport/ViewportInputManager.h>
#include <ovito/gui/base/actions/ActionManager.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/core/dataset/io/FileImporter.h>
#include <ovito/core/dataset/io/FileSource.h>
#include <ovito/core/app/undo/UndoStack.h>
#include <ovito/core/app/StandaloneApplication.h>
#include <ovito/core/viewport/ViewportConfiguration.h>
#include <ovito/core/viewport/ViewportWindow.h>
#include <ovito/core/utilities/concurrent/TaskProgress.h>
#include "MainWindowUI.h"
#include "ViewportsPanel.h"

namespace Ovito {

#ifndef OVITO_BUILD_MONOLITHIC
#if defined(Q_CC_MSVC)
// Explicit class template instantiations to be exported by the core module:
template class UserInterfaceComponent<MainWindowUI>;
#endif
#endif

IMPLEMENT_ABSTRACT_OVITO_CLASS(MainWindowUI);

/******************************************************************************
* Initializes the UI object.
******************************************************************************/
void MainWindowUI::initializeObject()
{
    WorkbenchUI::initializeObject();

    // Create the widget.
    _mainWindow = new MainWindow(*this);

    // Create the input manager, the undo stack and the action manager. The main window owns them.
    initializeWorkbench(_mainWindow);

    // Initialize the widget.
    _mainWindow->initializeWindow();

#ifdef OVITO_DEBUG
    // Keep track of all MainWindowUI instances for debugging purposes.
    // This is to make sure that all instances have been properly deleted when the application is shut down.
    // If this assertion fails, it likely indicates a circular object reference, which prevents the MainWindowUI from being deleted.
    static std::vector<OOWeakRef<MainWindowUI>> allMainWindowUIs;
    allMainWindowUIs.push_back(this);
    QObject::connect(Application::instance(), &Application::destroyed, []() {
        OVITO_ASSERT(std::all_of(allMainWindowUIs.begin(), allMainWindowUIs.end(), [](const OOWeakRef<MainWindowUI>& ref) { return ref.expired(); })); // All MainWindowUI instances should have been deleted by now.
    });
#endif
}

/******************************************************************************
* Destructor.
******************************************************************************/
MainWindowUI::~MainWindowUI()
{
    OVITO_ASSERT(_mainWindow == nullptr);
    OVITO_ASSERT(!_progressTasksHead && !_progressTasksTail);
}

/******************************************************************************
* Creates the action manager of this workbench.
******************************************************************************/
ActionManager* MainWindowUI::createActionManager(QObject* parent)
{
    return new WidgetActionManager(parent, *this);
}

/******************************************************************************
* Cancels all running tasks associated with this user interface and closes the
* user interface as soon as possible (without asking user to save changes).
******************************************************************************/
bool MainWindowUI::shutdown()
{
    // Inform listeners that this window is being closed.
    if(mainWindow())
        Q_EMIT mainWindow()->closingWindow();

    // Stop all running tasks in this window and release program session objects.
    if(!UserInterface::shutdown())
        return false;

    // Save the list of most recently visited directories to the settings store.
    saveMostRecentlyUsedDirectories();

    if(mainWindow()) {
        // Save window geometry and layout in user settings file.
        if(mainWindow()->isVisible()) {
            mainWindow()->saveMainWindowGeometry();
            mainWindow()->saveLayout();
        }

        // Close frame buffer window if it exists.
        if(mainWindow()->frameBufferWindow())
            mainWindow()->frameBufferWindow()->close();

        // Close the main window.
        return mainWindow()->close();
    }
    else return true;
}

/******************************************************************************
* Closes the user interface and shuts down the entire application after displaying an error message.
******************************************************************************/
void MainWindowUI::exitWithFatalError(const Exception& ex)
{
    OVITO_ASSERT(this_task::isMainThread());

    // Avoid reentrance.
    if(_exitingDueToFatalError)
        return;

    // Set flag.
    _exitingDueToFatalError = true;

    // Display fatal error message to the user.
    reportError(ex, true);

    // The event loop may not be running yet. Use a timer to execute the following
    // once we enter the event loop - similar to a queued signal/slot connection.
    QTimer::singleShot(0, []() {
        MainWindow::visitMainWindows([](MainWindow* mainWindow) {
            if(!mainWindow->close()) { // Ask user if closing the window is ok. If not...
                // ... forcibly close the window anyway.
                Q_EMIT mainWindow->closingWindow();
                mainWindow->ui().shutdown();
            }
        });
        QCoreApplication::exit(1);
    });
}

/******************************************************************************
* Gives the active viewport the input focus.
******************************************************************************/
void MainWindowUI::setViewportInputFocus()
{
    mainWindow()->viewportsPanel()->setFocus(Qt::OtherFocusReason);
}

/******************************************************************************
* Displays a message string in the window's status bar.
******************************************************************************/
void MainWindowUI::showStatusBarMessage(const QString& message, int timeout)
{
    mainWindow()->_statusBar->showMessage(message, timeout);
}

/******************************************************************************
* Hides any messages currently displayed in the window's status bar.
******************************************************************************/
void MainWindowUI::clearStatusBarMessage()
{
    // Conditional call to clearMessage() because clearMessage() always repaints the status bar, even it is not showing any message (as of Qt 6.3.2).
    if(!mainWindow()->_statusBar->currentMessage().isEmpty())
        mainWindow()->_statusBar->clearMessage();
}

/******************************************************************************
* Creates a frame buffer of the requested size and displays it as a window in the user interface.
******************************************************************************/
std::shared_ptr<FrameBuffer> MainWindowUI::createAndShowFrameBuffer(int width, int height)
{
    if(_mainWindow)
        return _mainWindow->createAndShowFrameBuffer(width, height);
    else
        return UserInterface::createAndShowFrameBuffer(width, height);
}

/******************************************************************************
* Shows a progress bar or a similar UI to indicate the current rendering progress
* and let the user cancel the operation if necessary.
******************************************************************************/
void MainWindowUI::showRenderingProgress(const std::shared_ptr<FrameBuffer>& frameBuffer, SharedFuture<void> renderingFuture)
{
    if(_mainWindow)
        _mainWindow->showRenderingProgress(frameBuffer, std::move(renderingFuture));
    else
        UserInterface::showRenderingProgress(frameBuffer, std::move(renderingFuture));
}

/******************************************************************************
* Handler function for exceptions.
******************************************************************************/
void MainWindowUI::displayErrorMessage(const Exception& ex, bool blocking)
{
    // Pass the exception to the widget, which displays it to the user.
    if(_mainWindow)
        _mainWindow->reportError(ex, blocking);
}

/******************************************************************************
* Displays a modal message box to the user. Blocks until the user closes the message box.
* This method wraps the QMessageBox class of the Qt library.
******************************************************************************/
UserInterface::MessageBoxButton MainWindowUI::showMessageBox(UserInterface::MessageBoxIcon icon, const QString& title, const QString& text, int buttons, UserInterface::MessageBoxButton defaultButton, const QString& detailedText)
{
    if(_mainWindow)
        return _mainWindow->showMessageBoxImpl(nullptr, icon, title, text, buttons, defaultButton, detailedText);
    else
        return UserInterface::showMessageBox(icon, title, text, buttons, defaultButton, detailedText);
}

/******************************************************************************
* Checks (or even modifies) the contents of a DataSet after it has been loaded from a file.
* Returns false if loading the DataSet was rejected by the application.
******************************************************************************/
bool MainWindowUI::checkLoadedDataset(DataSet* dataset)
{
    if(!UserInterface::checkLoadedDataset(dataset))
        return false;

#ifndef OVITO_BUILD_PROFESSIONAL
    // Since version 3.8.0, OVITO Basic no longer supports multiple pipelines in the same scene.
    // Check if the state file contains more than one pipeline and inform user by displaying a dialog window.
    // Let the user pick one of the pipelines to be loaded and remove all others from the scene.
    std::vector<OORef<SceneNode>> fileSourcePipelines = WorkbenchUI::fileSourcePipelines(dataset);
    if(fileSourcePipelines.size() >= 2) {
        QStringList itemsList;
        for(const auto& sceneNode : fileSourcePipelines)
            itemsList.push_back(sceneNode->objectTitle());
        {
            QDialog dlg(mainWindow());
            dlg.setWindowTitle(tr("Multiple pipelines found"));
            QVBoxLayout* mainLayout = new QVBoxLayout(&dlg);
            mainLayout->setSpacing(2);
            QLabel* label = new QLabel(tr(
                "<html><p>The OVITO session file contains %1 pipelines.</p>"
                "<p><i>OVITO Pro</i> is required since version 3.8.0 to work with "
                "multiple pipelines in the same scene. Please pick one of the pipelines "
                "below to load only that pipeline in <i>OVITO Basic</i> now - or open the session "
                "file in <i>OVITO Pro</i> to load all pipelines together.</p></html>"
            ).arg(fileSourcePipelines.size()));
            label->setWordWrap(true);
            label->setMinimumWidth(440);
            mainLayout->addWidget(label);
            mainLayout->addSpacing(6);
            mainLayout->addWidget(new QLabel(tr("Available pipelines:")));
            QListWidget* listWidget = new QListWidget();
            mainLayout->addWidget(listWidget);
            listWidget->addItems(itemsList);
            listWidget->setCurrentRow(0);
            QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, &dlg);
            QObject::connect(buttonBox, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
            QObject::connect(buttonBox, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
            QObject::connect(listWidget, &QListWidget::itemSelectionChanged, &dlg, [&]() { buttonBox->button(QDialogButtonBox::Ok)->setEnabled(!listWidget->selectedItems().empty()); });
            mainLayout->addWidget(buttonBox);
            if(dlg.exec() == QDialog::Accepted) {
                QList<QListWidgetItem*> selectedItems = listWidget->selectedItems();
                if(selectedItems.empty())
                    return false; // Abort loading of the DataSet.
                int keepIndex = listWidget->row(selectedItems.front());
                if(Scene* scene = fileSourcePipelines[keepIndex]->scene())
                    scene->selection()->setNode(fileSourcePipelines[keepIndex]);
                for(const auto& sceneNode : fileSourcePipelines) {
                    if(sceneNode != fileSourcePipelines[keepIndex]) {
                        sceneNode->requestObjectDeletion();
                    }
                }
            }
            else {
                return false; // Abort loading of the DataSet.
            }
        }
    }
#endif

    return true;
}

/******************************************************************************
* Save the current dataset.
******************************************************************************/
bool MainWindowUI::fileSave()
{
    OVITO_ASSERT(this_task::get());
    OORef<DataSet> dataset = datasetContainer().currentSet();

    if(!dataset)
        return false;

    // Ask the user for a filename if there is no one set.
    if(dataset->filePath().isEmpty())
        return fileSaveAs();

    // Save dataset to file.
    return handleExceptions([&] {
        dataset->saveToFile(dataset->filePath());
        undoStack()->setClean();
    });
}

/******************************************************************************
* This is the implementation of the "Save As" action.
* Returns true, if the scene has been saved.
******************************************************************************/
bool MainWindowUI::fileSaveAs(const QString& filename)
{
    OVITO_ASSERT(this_task::get());
    OORef<DataSet> dataset = datasetContainer().currentSet();

    if(!dataset)
        return false;

    if(filename.isEmpty()) {

        QFileDialog dialog(mainWindow(), tr("Save Session State"));
        dialog.setNameFilter(tr("OVITO State Files (*.ovito);;All Files (*)"));
        dialog.setAcceptMode(QFileDialog::AcceptSave);
        dialog.setFileMode(QFileDialog::AnyFile);
        dialog.setDefaultSuffix("ovito");

        QSettings settings;
        settings.beginGroup("file/scene");

        if(dataset->filePath().isEmpty()) {
            if(HistoryFileDialog::keepWorkingDirectoryHistoryEnabled()) {
                QString defaultPath = settings.value("last_directory").toString();
                if(!defaultPath.isEmpty())
                    dialog.setDirectory(defaultPath);
            }
        }
        else {
#ifndef Q_OS_LINUX
            dialog.selectFile(dataset->filePath());
#else
            // Workaround for bug in QFileDialog on Linux (Qt 6.2.4) crashing in exec() when selectFile() is called before (OVITO issue #216).
            dialog.setDirectory(QFileInfo(dataset->filePath()).dir());
#endif
        }

        TaskManager::setNativeDialogActive(true);
        auto dlgResult = dialog.exec();
        TaskManager::setNativeDialogActive(false);

        if(dlgResult != QDialog::Accepted)
            return false;

        QStringList files = dialog.selectedFiles();
        if(files.isEmpty())
            return false;
        QString newFilename = files.front();

        if(HistoryFileDialog::keepWorkingDirectoryHistoryEnabled()) {
            // Remember directory for the next time...
            settings.setValue("last_directory", dialog.directory().absolutePath());
        }

        dataset->setFilePath(newFilename);
    }
    else {
        dataset->setFilePath(filename);
    }
    return fileSave();
}

/******************************************************************************
* If the scene has been changed this will ask the user if he wants
* to save the changes.
******************************************************************************/
void MainWindowUI::askForSaveChanges()
{
    OVITO_ASSERT(this_task::get());
    OORef<DataSet> dataset = datasetContainer().currentSet();

    if(!dataset || dataset->filePath().isEmpty() || undoStack()->isClean())
        return;

    QString message;
    if(dataset->filePath().isEmpty() == false) {
        message = tr("The current session state has been modified. Do you want to save the changes?");
        message += QString("\n\nFile: %1").arg(dataset->filePath());
    }
    else {
        message = tr("The current program session has not been saved. Do you want to save it?");
    }

    QMessageBox::StandardButton result = MessageDialog::question(mainWindow(), tr("Save changes"),
        message,
        QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, QMessageBox::Cancel);
    if(result == QMessageBox::Cancel) {
        this_task::cancelAndThrow();
    }
    else if(result != QMessageBox::No) {
        // Save scene first.
        fileSave();
    }
}

/******************************************************************************
* Displays the optional settings of every importer to the user.
******************************************************************************/
void MainWindowUI::inspectImporterFiles(const std::vector<std::pair<QUrl, OORef<FileImporter>>>& urlImporters)
{
    for(const auto& item : urlImporters) {
        const QUrl& url = item.first;
        const OORef<FileImporter>& importer = item.second;
        for(OvitoClassPtr clazz = &importer->getOOClass(); clazz != nullptr; clazz = clazz->superClass()) {
            OvitoClassPtr editorClass = PropertiesEditor::registry().getEditorClass(clazz);
            if(editorClass && editorClass->isDerivedFrom(FileImporterEditor::OOClass())) {
                OORef<FileImporterEditor> editor = dynamic_object_cast<FileImporterEditor>(editorClass->createInstance());
                if(editor) {
                    // The URL may contain a wildcard pattern. Resolve it to an actual file URL before passing it to the editor.
                    std::vector<QUrl> resolvedUrls = ProgressDialog::blockForFuture(FileSourceImporter::findWildcardMatchesResolved(url), *this, tr("Resolving wildcard pattern"));
                    if(!resolvedUrls.empty()) {
                        editor->setUserInterface(*this);
                        editor->inspectNewFile(importer, resolvedUrls.front());
                    }
                    this_task::throwIfCanceled();
                }
            }
        }
    }
}

/******************************************************************************
* Asks the user how the data to be imported should be merged into the current scene.
******************************************************************************/
FileImporter::ImportMode MainWindowUI::determineImportMode(Scene* scene, const std::vector<QUrl>& urls, FileImporter* importer)
{
    OVITO_ASSERT(scene && importer);

    // Determine how the file's data should be inserted into the current scene.
    FileImporter::ImportMode importMode = FileImporter::ResetScene;

    if(importer->isReplaceExistingPossible(scene, urls)) {
        // Ask user if the existing pipeline should be preserved or reset.
        MessageDialog msgBox(QMessageBox::Question, tr("Import file"),
                tr("Do you want to reset the existing pipeline?"),
                QMessageBox::Yes | QMessageBox::Cancel, mainWindow());
#ifdef OVITO_BUILD_PROFESSIONAL
        msgBox.setInformativeText(tr(
            "<p>Select <b>Yes</b> to start over and discard the existing pipeline before importing the new file.</p>"
            "<p>Select <b>No</b> to keep modifiers in the current pipeline and replace the input data with the selected file.</p>"
            "<p>Select <b>Add to scene</b> to create an additional pipeline and visualize multiple datasets.</p>"));
#else
        msgBox.setInformativeText(tr(
            "<p>Select <b>Yes</b> to start over and discard the existing pipeline before importing the new file.</p>"
            "<p>Select <b>No</b> to keep modifiers in the current pipeline and replace the input data with the selected file.</p>"
            "<p>Select <b>Add to scene</b> to create an additional pipeline and visualize multiple datasets (requires <a href=\"https://www.ovito.org/about/ovito-pro/\">OVITO Pro</a>).</p>"));
#endif
        msgBox.addButton(tr("No"), QMessageBox::NoRole);
        QPushButton* addToSceneButton = msgBox.addButton(tr("Add to scene"), QMessageBox::NoRole);
#ifndef OVITO_BUILD_PROFESSIONAL
        addToSceneButton->setEnabled(false);
#else
        (void)addToSceneButton;
#endif
        msgBox.setDefaultButton(QMessageBox::Yes);
        msgBox.setEscapeButton(QMessageBox::Cancel);
        msgBox.exec();

        if(msgBox.clickedButton() == msgBox.button(QMessageBox::Cancel)) {
            this_task::cancelAndThrow(); // Operation canceled by user.
        }
        else if(msgBox.clickedButton() == msgBox.button(QMessageBox::Yes)) {
            importMode = FileImporter::ResetScene;
            // Ask user if current scene should be saved before it is replaced by the imported data.
            askForSaveChanges();
        }
        else if(msgBox.clickedButton() == addToSceneButton) {
            importMode = FileImporter::AddToScene;
        }
        else {
            // No button
            importMode = FileImporter::ReplaceSelected;
        }
    }
    else if(scene->children().empty() == false) {
        // Ask user if the current scene should be completely replaced by the imported data.
        QMessageBox::StandardButton result = MessageDialog::question(mainWindow(), tr("Import file"),
            tr("Do you want to keep the existing objects in the current scene?"),
            QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, QMessageBox::Cancel);

        if(result == QMessageBox::Cancel) {
            this_task::cancelAndThrow(); // Operation canceled by user.
        }
        else if(result == QMessageBox::No) {
            importMode = FileImporter::ResetScene;

            // Ask user if current scene should be saved before it is replaced by the imported data.
            askForSaveChanges();
        }
        else {
            importMode = FileImporter::AddToScene;
        }
    }

    return importMode;
}

/******************************************************************************
* Runs the import operation in a progress dialog, which lets the user cancel it.
******************************************************************************/
void MainWindowUI::runFileImport(FileImporter& importer, Scene* scene, std::vector<std::pair<QUrl, OORef<FileImporter>>> urlImporters, FileImporter::ImportMode importMode)
{
    Future<OORef<Pipeline>> future = importer.importFileSet(scene, std::move(urlImporters), importMode, true, ImportFileDialog::multiFileImportMode());
    ProgressDialog::blockForFuture(std::move(future), *this, tr("Importing data"));
}

/******************************************************************************
* Remembers the directory of the imported file in the file import dialog history.
******************************************************************************/
void MainWindowUI::importDirectoryChanged(const QString& directoryPath)
{
    updateMostRecentlyUsedDirectory(QStringLiteral("import"), directoryPath); // Note: "import" is the dialog class for the FileImportDialog.
}

/******************************************************************************
* Opens the file import dialog.
******************************************************************************/
void MainWindowUI::openImportDialog(const QString& directoryPath)
{
    actionManager()->getAction(ACTION_FILE_IMPORT)->trigger();
}

/******************************************************************************
* Updates the progress display of the main window.
******************************************************************************/
void MainWindowUI::progressTasksChanged()
{
    if(hasMainWindow())
        Q_EMIT mainWindow()->taskProgressUpdate();
}

/******************************************************************************
* Waits for the pipelines in the current scene to be fully evaluated, then executes the given operation.
* A progress dialog may be displayed while waiting for the scene preparation to complete.
******************************************************************************/
void MainWindowUI::scheduleOperationAfterScenePreparation(Scene* scene, const QString& waitingMessage, operation_function&& operation)
{
    // Create a temporary scene preparation object that takes care of evaluating the scene pipelines.
    OORef<ScenePreparation> sceneProp = OORef<ScenePreparation>::create(*this, scene);

    // Get future object that becomes ready once the scene preparation has completed.
    SharedFuture<void> future = sceneProp->future();

    // Show a progress dialog while waiting. The dialog will self-destruct afterwards.
    ProgressDialog* progressDialog = new ProgressDialog(SharedFuture<void>{future}, *this, waitingMessage);

    // Keep the scene preparation object alive until it has completed its job.
    future.finally([sceneProp=std::move(sceneProp)]() noexcept {});

    // Schedule execution of the operation upon completion of the scene preparation.
    progressDialog->whenDone([this, operation=std::move(operation)]() mutable noexcept {
        handleExceptions([&] {
            operation();
        });
    });
}

/******************************************************************************
* Returns the history of most recently used directories for file selection
* dialog type (e.g. data files, state files, Python scripts, ...).
* This function is used by the HistoryFileDialog class to maintain a separate
* history of recently used directories for different file I/O operations.
******************************************************************************/
QStringList MainWindowUI::getRecentlyUsedDirectories(const QString& dialogClass)
{
    QStringList& list = _recentlyUsedDirectories[dialogClass];
    if(list.empty()) {
        QSettings settings;
        settings.beginGroup(QStringLiteral("filedialog/") + dialogClass);
        list = settings.value(QStringLiteral("history")).toStringList();
    }
    return list;
}

/******************************************************************************
* Updates the history of most recently used directories for file selection
* dialog type (e.g. data files, state files, Python scripts, ...).
* This function is used by the HistoryFileDialog class to maintain a separate
* history of recently used directories for different file I/O operations.
* The given directory is moved to the top of the history list.
******************************************************************************/
void MainWindowUI::updateMostRecentlyUsedDirectory(const QString& dialogClass, const QString& directory)
{
    constexpr int MAX_DIRECTORY_HISTORY_SIZE = 1;

    QStringList history = getRecentlyUsedDirectories(dialogClass);
    auto index = history.indexOf(directory);
    if(index >= 0)
        history.move(index, 0);
    else {
        history.push_front(directory);
        if(history.size() > MAX_DIRECTORY_HISTORY_SIZE)
            history.resize(MAX_DIRECTORY_HISTORY_SIZE);
    }
    _recentlyUsedDirectories[dialogClass] = std::move(history);
}

/******************************************************************************
* Saves the list of most recently visited directories to the settings store at shutdown time.
******************************************************************************/
void MainWindowUI::saveMostRecentlyUsedDirectories()
{
    for(const auto& [dialogClass, history] : _recentlyUsedDirectories) {
        QSettings settings;
        settings.beginGroup(QStringLiteral("filedialog/") + dialogClass);
        settings.setValue(QStringLiteral("history"), QVariant::fromValue(history));
    }
}

}   // End of namespace
