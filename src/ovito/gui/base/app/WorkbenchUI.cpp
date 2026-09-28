// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/gui/base/GUIBase.h>
#include <ovito/gui/base/actions/ActionManager.h>
#include <ovito/gui/base/viewport/ViewportInputManager.h>
#include <ovito/core/app/undo/UndoStack.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/core/dataset/scene/Scene.h>
#include <ovito/core/dataset/scene/Pipeline.h>
#include <ovito/core/dataset/io/FileSource.h>
#include <ovito/core/utilities/concurrent/TaskProgress.h>
#include <QDir>
#include <QFileInfo>
#include "WorkbenchUI.h"

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(WorkbenchUI);

/******************************************************************************
* Creates the viewport input manager, the undo stack and the action manager of this workbench.
******************************************************************************/
void WorkbenchUI::initializeWorkbench(QObject* parent)
{
    // Create the input manager, which provides the navigation and selection modes of the viewports.
    setViewportInputManager(new ViewportInputManager(parent, *this));

    // Create the undo stack.
    setUndoStack(new UndoStack(*this, parent));

    // Create the actions, which is how the frontends and the plugins expose commands to the user.
    setActionManager(createActionManager(parent));

    // Start with a clean undo stack whenever a new dataset is loaded.
    QObject::connect(&datasetContainer(), &DataSetContainer::dataSetChanged, undoStack(), &UndoStack::clear);
}

/******************************************************************************
* Imports a set of files into the current dataset of this workbench.
******************************************************************************/
void WorkbenchUI::importFiles(const std::vector<QUrl>& urls, const FileImporterClass* importerType, const QString& importerFormat)
{
    OVITO_ASSERT(this_task::get());
    OVITO_ASSERT(!urls.empty());

    // Create references to the active dataset and scene to keep them alive during this long-running operation.
    OORef<DataSet> dataset = datasetContainer().currentSet();
    OORef<Scene> scene = datasetContainer().activeScene();
    if(!dataset || !scene)
        throw Exception(tr("Cannot import because there is no active scene."));

    // Create an importer for every file to be imported.
    std::vector<std::pair<QUrl, OORef<FileImporter>>> urlImporters;
    for(const QUrl& url : urls) {
        if(!url.isValid())
            throw Exception(tr("Failed to import file. URL is not valid: %1").arg(url.toString()));

        OORef<FileImporter> importer;
        if(!importerType) {
            // Detect the file format.
            importer = FileImporter::autodetectFileFormat(url).blockForResult();
            if(!importer)
                throw Exception(tr("Could not auto-detect the format of the file %1. The file format might not be supported.").arg(url.fileName()));
        }
        else {
            importer = static_object_cast<FileImporter>(importerType->createInstance());
            if(!importer)
                throw Exception(tr("Failed to import file. Could not initialize import service."));
            importer->setSelectedFileFormat(importerFormat);
        }

        urlImporters.emplace_back(url, std::move(importer));
    }

    // Order the files and their importers, so that those which contribute the simulation data are read first.
    std::stable_sort(urlImporters.begin(), urlImporters.end(), [](const auto& a, const auto& b) {
        int pa = a.second->importerPriority();
        int pb = b.second->importerPriority();
        if(pa > pb) return true;
        if(pa < pb) return false;
        return a.second->getOOClass().name() < b.second->getOOClass().name();
    });

    // Let the frontend present the import settings of each file to the user.
    inspectImporterFiles(urlImporters);

    // Find out how the imported data should be merged into the current scene.
    const FileImporter::ImportMode importMode = determineImportMode(scene, urls, urlImporters.front().second.get());

    // Run the import operation.
    runFileImport(*urlImporters.front().second, scene, std::move(urlImporters), importMode);

    if(importMode == FileImporter::ResetScene) {
        undoStack()->clear();
        dataset->setFilePath({});
    }

    // Make the directory of the imported file the working directory for the next file selection.
    QString directoryPath = urls.back().toLocalFile();
    if(!directoryPath.isEmpty()) {
        directoryPath = QFileInfo(directoryPath).absolutePath();
        QDir::setCurrent(directoryPath);
        importDirectoryChanged(directoryPath);
    }
}

/******************************************************************************
* Makes the given directory the working directory and lets the user select files to import.
******************************************************************************/
void WorkbenchUI::openWorkingDirectory(const QString& directoryPath)
{
    QDir::setCurrent(directoryPath);
    importDirectoryChanged(directoryPath);
    openImportDialog(directoryPath);
}

/******************************************************************************
* Shows the frontend's user interface that lets the user select the files to import.
******************************************************************************/
void WorkbenchUI::openImportDialog(const QString& directoryPath)
{
    throw Exception(tr("This user interface cannot open the file selection dialog. Specify the data files to import on the command line instead."));
}

/******************************************************************************
* Determines how the data to be imported is merged into the given scene.
******************************************************************************/
FileImporter::ImportMode WorkbenchUI::determineImportMode(Scene* scene, const std::vector<QUrl>& urls, FileImporter* importer)
{
    OVITO_ASSERT(scene && importer);

    // A frontend that cannot ask the user replaces the current scene only if the new data can take its place, and
    // appends the imported data otherwise. That way no existing work of the user is ever discarded silently.
    if(scene->children().empty() || importer->isReplaceExistingPossible(scene, urls))
        return FileImporter::ResetScene;
    return FileImporter::AddToScene;
}

/******************************************************************************
* Runs the import operation and waits for its completion.
******************************************************************************/
void WorkbenchUI::runFileImport(FileImporter& importer, Scene* scene, std::vector<std::pair<QUrl, OORef<FileImporter>>> urlImporters, FileImporter::ImportMode importMode)
{
    // A frontend that can show the progress of the import operation (and let the user cancel it) overrides this method.
    Future<OORef<Pipeline>> future = importer.importFileSet(scene, std::move(urlImporters), importMode, true, FileImporter::ImportAsTrajectory);
    (void)future.blockForResult();
}

/******************************************************************************
* Registers a new task progress record with this user interface.
******************************************************************************/
std::mutex* WorkbenchUI::taskProgressBegin(TaskProgress* progress)
{
    std::lock_guard<std::mutex> lock(_progressTaskListMutex);
    if(!_progressTasksHead)
        _progressTasksHead = progress;
    progress->setPrevInList(_progressTasksTail);
    progress->setNextInList(nullptr);
    if(_progressTasksTail)
        _progressTasksTail->setNextInList(progress);
    _progressTasksTail = progress;
    return &_progressTaskListMutex;
}

/******************************************************************************
* Unregisters a task progress record from this user interface.
******************************************************************************/
void WorkbenchUI::taskProgressEnd(TaskProgress* progress)
{
    // Note: Mutex is already locked by the TaskProgress class.
    if(_progressTasksHead == progress)
        _progressTasksHead = progress->nextInList();
    if(_progressTasksTail == progress)
        _progressTasksTail = progress->prevInList();
    if(TaskProgress* prev = progress->prevInList())
        prev->setNextInList(progress->nextInList());
    if(TaskProgress* next = progress->nextInList())
        next->setPrevInList(progress->prevInList());
    notifyProgressTasksChanged();
}

/******************************************************************************
* Informs the user interface that a task's progress state has changed.
******************************************************************************/
void WorkbenchUI::taskProgressChanged(TaskProgress* progress)
{
    // Note: Mutex is already locked by the TaskProgress class.
    notifyProgressTasksChanged();
}

/******************************************************************************
* Notifies the frontend that the progress display should be updated.
******************************************************************************/
void WorkbenchUI::notifyProgressTasksChanged()
{
    // The following timer code ensures that the GUI task display is updated only once every 100 ms.
    // It also ensures that the UI update is done in the main thread and that short-lived
    // tasks don't show up in the GUI at all.
    if(!_progressUpdateScheduled.exchange(true)) {
        QTimer::singleShot(100, QCoreApplication::instance(), [self = OORef<WorkbenchUI>(this)]() {
            self->_progressUpdateScheduled.store(false);
            self->progressTasksChanged();
        });
    }
}

/******************************************************************************
* Lets the caller visit all registered tasks that are still in progress.
******************************************************************************/
void WorkbenchUI::visitRunningTasks(const std::function<void(const QString& text, int progressValue, int progressMaximum)>& visitor)
{
    std::lock_guard<std::mutex> lock(_progressTaskListMutex);
    for(TaskProgress* taskProgress = _progressTasksHead; taskProgress != nullptr; taskProgress = taskProgress->nextInList()) {
        // Compute overall progress, taking into account nested sub-steps of the task.
        auto [totalProgressValue, totalProgressMaximum] = taskProgress->computeTotalProgress();
        // Call visitor function.
        visitor(taskProgress->text(), totalProgressValue, totalProgressMaximum);
    }
}

/******************************************************************************
* Displays an error message to the user.
******************************************************************************/
void WorkbenchUI::reportError(const Exception& ex, bool blocking)
{
    OVITO_ASSERT(this_task::isMainThread());

    // Always display errors in the terminal window too.
    UserInterface::reportError(ex, blocking);

    // Let the frontend present the error message to the user.
    displayErrorMessage(ex, blocking);
}

/******************************************************************************
* Returns all pipelines in the given dataset's scene whose input data comes from a file.
******************************************************************************/
std::vector<OORef<SceneNode>> WorkbenchUI::fileSourcePipelines(DataSet* dataset)
{
    std::vector<OORef<SceneNode>> pipelines;
    if(!dataset)
        return pipelines;
    ViewportConfiguration* viewportConfig = dataset->viewportConfig();
    if(!viewportConfig)
        return pipelines;
    Viewport* activeViewport = viewportConfig->activeViewport();
    if(!activeViewport)
        return pipelines;
    Scene* scene = activeViewport->scene();
    if(!scene)
        return pipelines;
    scene->visitPipelines([&](SceneNode* sceneNode) {
        if(dynamic_object_cast<FileSource>(sceneNode->pipeline()->source()))
            pipelines.emplace_back(sceneNode);
    });
    return pipelines;
}

}   // End of namespace
