// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/gui/qml/QmlFrontend.h>
#include <ovito/gui/base/app/GuiSettings.h>
#include <ovito/gui/base/app/GuiTaskScope.h>
#include <ovito/gui/base/app/TaskProgressModel.h>
#include <ovito/gui/base/app/WorkbenchUI.h>
#include <ovito/gui/base/mainwin/RecentFilesList.h>
#include <ovito/core/app/Application.h>
#include <ovito/core/app/UserInterface.h>
#include <ovito/core/app/undo/UndoStack.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/core/dataset/io/FileImporter.h>
#include <ovito/core/dataset/scene/Scene.h>
#include <ovito/core/dataset/scene/SceneNode.h>
#include <QtCore/QCoreApplication>
#include <QtCore/QEventLoop>
#include <QtCore/QFileInfo>
#include <QtCore/QTimer>
#include <QtCore/QUrl>
#include <QtCore/QVariantMap>
#include <QtQuick/QQuickView>
#include "QmlMainWindowUI.h"
#include "QmlWorkbenchController.h"

namespace Ovito {

/******************************************************************************
* Constructor.
******************************************************************************/
QmlWorkbenchController::QmlWorkbenchController(QmlMainWindowUI& ui, QObject* parent) : QObject(parent), _ui(ui)
{
    // Everything the shell displays about the data set follows from the current data set and the session file it
    // was loaded from.
    connect(&ui.datasetContainer(), &DataSetContainer::dataSetChanged, this, &QmlWorkbenchController::refreshDataSetState);
    connect(&ui.datasetContainer(), &DataSetContainer::filePathChanged, this, &QmlWorkbenchController::refreshDataSetState);
    // The title carries the modified marker of the session, so it follows the clean state of the undo stack as well -
    // the same signal the classic main window marks its window title with (MainWindow.cpp:238).
    if(UndoStack* undoStack = ui.undoStack())
        connect(undoStack, &UndoStack::cleanChanged, this, &QmlWorkbenchController::refreshDataSetState);
    // The File menu lists the recent files of the shared list, which both frontends write to.
    connect(&RecentFilesList::instance(), &RecentFilesList::listChanged, this, &QmlWorkbenchController::recentFilesChanged);
    refreshDataSetState();
}

/******************************************************************************
* Updates the state derived from the current data set.
******************************************************************************/
void QmlWorkbenchController::refreshDataSetState()
{
    const QString title = determineWindowTitle();
    if(_windowTitle != title) {
        _windowTitle = title;
        Q_EMIT windowTitleChanged();
    }

    const bool hasData = determineHasData();
    if(_hasData != hasData) {
        _hasData = hasData;
        Q_EMIT hasDataChanged();
    }
}

/******************************************************************************
* Determines the title of the workbench window from the current data set.
******************************************************************************/
QString QmlWorkbenchController::determineWindowTitle() const
{
    // The desktop frontend shows the session file in the window title (MainWindow::setWindowFilePath()). There is no
    // session file until the session is saved, so the title is the application name until then.
    QString title = Application::applicationName();
    if(const DataSet* dataset = _ui.datasetContainer().currentSet()) {
        if(!dataset->filePath().isEmpty())
            title = tr("%1 - %2").arg(QFileInfo(dataset->filePath()).fileName(), Application::applicationName());
    }

    // A modified session is marked the way the classic frontend marks it: it calls setWindowModified(), which renders
    // the '*' of the window title's placeholder. The shell shows the same marker in its own title bar and in the libc
    // window title, so a user who is asked about unsaved changes can see where they come from.
    if(_ui.isSessionModified())
        title += QStringLiteral(" *");
    return title;
}

/******************************************************************************
* Determines whether the current scene contains data to display.
******************************************************************************/
bool QmlWorkbenchController::determineHasData() const
{
    // The shell shows its empty state while the scene has no object to display, i.e. while the viewports only show the
    // construction grid. Everything that adds or removes objects goes through the shared workbench code (import) or
    // through the pipeline view, which belongs to Phase 4, so the state is refreshed when those operations report
    // their progress and when the data set changes.
    const Scene* scene = _ui.datasetContainer().activeScene();
    return scene && !scene->children().empty();
}

/******************************************************************************
* Replaces the message shown in the status line.
******************************************************************************/
void QmlWorkbenchController::setStatusMessage(const QString& message)
{
    if(_statusMessage != message) {
        _statusMessage = message;
        Q_EMIT statusMessageChanged();
    }
}

/******************************************************************************
* Replaces the report about the last import.
******************************************************************************/
void QmlWorkbenchController::setNotice(const QString& notice)
{
    if(_notice != notice) {
        _notice = notice;
        Q_EMIT noticeChanged();
    }
}

/******************************************************************************
* Updates the state derived from the registered task progress records.
******************************************************************************/
void QmlWorkbenchController::updateOperationState()
{
    const bool cancellable = _runningOperation && !_runningOperation->isFinished();
    if(!cancellable)
        _cancelling = false;
    if(_cancellable != cancellable) {
        _cancellable = cancellable;
        Q_EMIT taskStateChanged();
    }

    // A finished operation may have added or removed objects, so the empty state is re-evaluated with it. The
    // running tasks themselves are presented by the workbench's task progress model.
    TaskProgressModel* model = _ui.taskProgressModel();
    if(!model || !model->isBusy())
        refreshDataSetState();
}

/******************************************************************************
* Makes the given task the operation that the Cancel command cancels.
******************************************************************************/
void QmlWorkbenchController::setRunningOperation(TaskPtr task)
{
    _runningOperation = std::move(task);
    _cancelling = false;
    updateOperationState();
}

/******************************************************************************
* Requests the cancellation of the import operation that is currently running.
******************************************************************************/
void QmlWorkbenchController::cancelCurrentOperation()
{
    if(_runningOperation && !_runningOperation->isFinished()) {
        // The operation notices the cancellation at its next checkpoint and throws OperationCanceled, which the
        // importing code turns into the "cancelled" state. Until then the operation is winding down.
        _cancelling = true;
        Q_EMIT taskStateChanged();
        _runningOperation->cancel();
    }
}

/******************************************************************************
* Imports the given files into the current data set, reporting the states they produce.
******************************************************************************/
void QmlWorkbenchController::importFiles(const QVariantList& urls)
{
    std::vector<QUrl> urlList;
    urlList.reserve(urls.size());
    for(const QVariant& value : urls) {
        QUrl url = value.toUrl();
        if(url.isEmpty())
            url = QUrl::fromLocalFile(value.toString());
        if(url.isValid())
            urlList.push_back(std::move(url));
    }
    if(urlList.empty()) {
        setStatusMessage(tr("No file was selected for import."));
        return;
    }

    // A single .ovito file is a session state, not a data file: the classic frontend redirects it to its session
    // loading path as well (WidgetActionManager::on_FileImport_triggered). Loading it replaces the session, so this has
    // to be a task of this user interface and asks about unsaved changes first.
    if(urlList.size() == 1 && urlList.front().fileName().endsWith(QStringLiteral(".ovito"), Qt::CaseInsensitive)) {
        GuiTaskScope taskScope(_ui);
        try {
            _ui.askForSaveChanges();
            _ui.loadSessionFile(urlList.front());
        }
        catch(const OperationCanceled&) {
            setStatusMessage(tr("Import cancelled."));
        }
        catch(const Exception& ex) {
            _ui.reportError(ex);
        }
        return;
    }

    // A QML signal handler runs outside of any OVITO task, so the import takes place in a task of its own (see the note
    // on task contexts in the Phase 0 audit). The whole operation is handed to the shell, which is what lets the user
    // cancel it - from the detection of the file formats to the loading of the data.
    GuiTaskScope taskScope(_ui);
    setRunningOperation(taskScope.task());
    try {
        _ui.importFiles(urlList);
        // The import itself reports what it did with the file (which format it was read as and how many source frames
        // it holds), so no second, less precise message is put on top of it here.
    }
    catch(const OperationCanceled&) {
        // WorkbenchUI::importFiles() removed the objects of the canceled operation, so the data set is unchanged
        // except for whatever the import mode replaced before the import started.
        setStatusMessage(tr("Import cancelled."));
    }
    catch(const Exception& ex) {
        // reportError() writes the message to the terminal and hands it to displayErrorMessage().
        _ui.reportError(ex);
    }
    refreshDataSetState();
    updateOperationState();
}

/******************************************************************************
* Asks the scene to open the file selection dialog.
******************************************************************************/
void QmlWorkbenchController::showImportDialog()
{
    Q_EMIT importDialogRequested(importDirectoryUrl());
}

/******************************************************************************
* Returns the directory that the file selection dialog opens in.
******************************************************************************/
QUrl QmlWorkbenchController::importDirectoryUrl() const
{
    if(!_importDialogDirectory.isEmpty())
        return QUrl::fromLocalFile(_importDialogDirectory);

    // Nothing asked for a directory, so the dialog reopens where the last import was - the same history the classic
    // frontend's file dialog keeps. GuiSettings drops the history when the user turned it off.
    const QStringList directories = GuiSettings::instance().recentDirectories(QStringLiteral("import"));
    return directories.isEmpty() ? QUrl() : QUrl::fromLocalFile(directories.front());
}

/******************************************************************************
* Returns the recently opened data and session files.
******************************************************************************/
QVariantList QmlWorkbenchController::recentFiles() const
{
    QVariantList list;
    for(const RecentFilesList::Entry& entry : RecentFilesList::instance().entries()) {
        OVITO_ASSERT(!entry.urls.empty());
        QString title = entry.urls.front().toDisplayString(QUrl::PreferLocalFile | QUrl::NormalizePathSegments);
        if(entry.urls.size() > 1)
            title += tr(" + %1 more").arg(entry.urls.size() - 1);
        list.append(QVariantMap{
            { QStringLiteral("title"), title },
            { QStringLiteral("isSession"), entry.isSessionFile() }
        });
    }
    return list;
}

/******************************************************************************
* Opens the file of a recent files list entry.
******************************************************************************/
void QmlWorkbenchController::openRecentFile(int index)
{
    const QList<RecentFilesList::Entry> entries = RecentFilesList::instance().entries();
    if(index < 0 || index >= entries.size())
        return;

    // A copy of the entry, because opening a file can change the list (the opened file moves to the front).
    const RecentFilesList::Entry entry = entries[index];

    // Opening a file replaces the session, which is work that needs a task context of this user interface.
    GuiTaskScope taskScope(_ui);
    bool openFailed = false;
    try {
        if(entry.isSessionFile()) {
            OVITO_ASSERT(entry.urls.size() == 1);
            // The unsaved changes of the current session would be gone, so this asks first - the same question the
            // classic frontend asks when it opens a recent session file.
            _ui.askForSaveChanges();
            _ui.loadSessionFile(entry.urls.front());
        }
        else {
            // The entry remembers the importer and the format the user selected, so a data file whose format cannot be
            // detected any more is still opened the way it was opened before.
            const FileImporterClass* importerClass = dynamic_cast<const FileImporterClass*>(OvitoClass::decodeFromString(entry.importerClassName));
            _ui.performTransaction(tr("Import data"), [&] {
                _ui.importFiles(entry.urls, importerClass, entry.importerFormat);
            });
        }
    }
    catch(const OperationCanceled&) {
        // The user aborted the question about the unsaved changes, so nothing happened.
    }
    catch(...) {
        // The entry cannot be opened any more (the file is gone, or its format is unsupported now), so it is dropped -
        // an entry that stays in the menu and fails every time helps nobody.
        openFailed = true;
    }

    if(openFailed) {
        RecentFilesList::instance().removeEntry(index);
        _ui.reportError(Exception(tr("The file %1 could not be opened.").arg(entry.urls.front().toString())));
    }
}

/******************************************************************************
* Asks the scene to open the file selection dialog, starting in the given directory.
******************************************************************************/
void QmlWorkbenchController::requestImportDialog(const QString& directoryPath)
{
    if(!directoryPath.isEmpty())
        _importDialogDirectory = directoryPath;
    showImportDialog();
}

/******************************************************************************
* Presents a message dialog and blocks until the user answers it.
******************************************************************************/
UserInterface::MessageBoxButton QmlWorkbenchController::presentMessageBox(UserInterface::MessageBoxIcon icon, const QString& title, const QString& text, int buttons, UserInterface::MessageBoxButton defaultButton, const QString& detailedText)
{
    // Without a QML scene there is nobody who could answer the dialog. Report the message and continue with the
    // default answer, so that a frontend that is still starting up cannot block on a dialog that never appears.
    if(_ui.view() == nullptr || _ui.view()->rootObject() == nullptr) {
        qWarning().noquote() << (title.isEmpty() ? Application::applicationName() : title) << ":" << text;
        return defaultButton;
    }

    _messageBoxTitle = title.isEmpty() ? Application::applicationName() : title;
    _messageBoxText = detailedText.isEmpty() ? text : text + QStringLiteral("\n\n") + detailedText;
    _messageBoxIcon = static_cast<int>(icon);
    _messageBoxButtons = makeButtonList(buttons, defaultButton);
    _messageBoxAnswer = defaultButton;
    _messageBoxVisible = true;
    Q_EMIT messageBoxChanged();

    // Wait for the scene to answer the dialog. This is a nested event loop, exactly like the modal dialogs of the
    // desktop frontend (QMessageBox::exec() in MainWindow::showMessageBoxImpl()), so the rest of the application keeps
    // running while the dialog is open.
    QEventLoop eventLoop;
    _messageBoxLoop = &eventLoop;
    const QMetaObject::Connection connection = connect(this, &QmlWorkbenchController::messageBoxAnswered, &eventLoop, &QEventLoop::quit);
    eventLoop.exec();
    disconnect(connection);
    _messageBoxLoop = nullptr;

    _messageBoxVisible = false;
    Q_EMIT messageBoxChanged();
    return _messageBoxAnswer;
}

/******************************************************************************
* Returns the name of the application.
******************************************************************************/
QString QmlWorkbenchController::applicationName() const
{
    return Application::applicationName();
}

/******************************************************************************
* Returns the version of the application.
******************************************************************************/
QString QmlWorkbenchController::applicationVersion() const
{
    return Application::applicationVersionString();
}

/******************************************************************************
* Returns the edition this build is.
******************************************************************************/
QString QmlWorkbenchController::buildType() const
{
#ifdef OVITO_BUILD_PROFESSIONAL
    return tr("Professional");
#else
    return tr("Basic");
#endif
}

/******************************************************************************
* Returns the copyright notice of this build.
******************************************************************************/
QString QmlWorkbenchController::copyrightNotice() const
{
    // The text comes from the build (see cmake/Version.cmake). It can contain placeholders that plugins fill in at run
    // time by attaching a dynamic property to the application object - the same mechanism the classic About dialog uses.
    QString text = QStringLiteral(OVITO_COPYRIGHT_NOTICE);
    for(const QByteArray& name : Application::instance()->dynamicPropertyNames())
        text.replace(QStringLiteral("[[%1]]").arg(QString::fromLatin1(name)), Application::instance()->property(name.data()).toString());
    return text;
}

/******************************************************************************
* Asks the scene to display the About dialog of the workbench.
******************************************************************************/
void QmlWorkbenchController::showAboutDialog()
{
    Q_EMIT aboutDialogRequested();
}

/******************************************************************************
* Answers the message dialog the frontend is waiting on.
******************************************************************************/
void QmlWorkbenchController::answerMessageBox(int button)
{
    if(!_messageBoxVisible)
        return;
    _messageBoxAnswer = static_cast<UserInterface::MessageBoxButton>(button);
    Q_EMIT messageBoxAnswered();
}

/******************************************************************************
* Builds the list of buttons of the message dialog from the given button mask.
******************************************************************************/
QVariantList QmlWorkbenchController::makeButtonList(int buttons, UserInterface::MessageBoxButton defaultButton)
{
    using MessageButton = UserInterface::MessageBoxButton;
    // The order of the buttons is the conventional order of the dialog buttons; the scene lays them out from left to
    // right and puts the one the caller declared as default last, where the affirmative answer belongs.
    static const std::pair<MessageButton, const char*> standardButtons[] = {
        { MessageButton::Ok, QT_TR_NOOP("OK") },
        { MessageButton::Yes, QT_TR_NOOP("Yes") },
        { MessageButton::No, QT_TR_NOOP("No") },
        { MessageButton::Apply, QT_TR_NOOP("Apply") },
        { MessageButton::Retry, QT_TR_NOOP("Retry") },
        { MessageButton::Ignore, QT_TR_NOOP("Ignore") },
        { MessageButton::Discard, QT_TR_NOOP("Discard") },
        { MessageButton::Abort, QT_TR_NOOP("Abort") },
        { MessageButton::Cancel, QT_TR_NOOP("Cancel") },
    };

    QVariantList result;
    for(const auto& [button, text] : standardButtons) {
        if(buttons & static_cast<int>(button)) {
            result.push_back(QVariantMap{
                { QStringLiteral("button"), static_cast<int>(button) },
                { QStringLiteral("text"), tr(text) },
                { QStringLiteral("isDefault"), button == defaultButton }
            });
        }
    }
    // A dialog without any of the standard buttons would be impossible to answer: fall back to a single OK button.
    if(result.empty()) {
        result.push_back(QVariantMap{
            { QStringLiteral("button"), static_cast<int>(MessageButton::Ok) },
            { QStringLiteral("text"), tr("OK") },
            { QStringLiteral("isDefault"), true }
        });
    }
    return result;
}

}   // End of namespace
