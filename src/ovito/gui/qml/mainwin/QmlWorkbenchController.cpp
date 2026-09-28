// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/gui/qml/QmlFrontend.h>
#include <ovito/gui/base/app/GuiTaskScope.h>
#include <ovito/gui/base/app/WorkbenchUI.h>
#include <ovito/core/app/Application.h>
#include <ovito/core/app/UserInterface.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/core/dataset/scene/Scene.h>
#include <ovito/core/dataset/scene/SceneNode.h>
#include <QtCore/QCoreApplication>
#include <QtCore/QEventLoop>
#include <QtCore/QFileInfo>
#include <QtCore/QTimer>
#include <QtCore/QVariantMap>
#include <QtCore/QUrl>
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
    if(const DataSet* dataset = _ui.datasetContainer().currentSet()) {
        if(!dataset->filePath().isEmpty())
            return tr("%1 - %2").arg(QFileInfo(dataset->filePath()).fileName(), Application::applicationName());
    }
    return Application::applicationName();
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
* Updates the state derived from the registered task progress records.
******************************************************************************/
void QmlWorkbenchController::updateTaskState()
{
    bool busy = false;
    QString taskText;
    int progress = 0;
    int maximum = 0;

    // The status bar shows the first operation that describes itself. Operations that do not report any text are
    // counted as running work but are not displayed, because there is nothing to display.
    _ui.visitRunningTasks([&](const QString& text, int progressValue, int progressMaximum) {
        busy = true;
        if(taskText.isEmpty() && !text.isEmpty()) {
            taskText = text;
            progress = progressValue;
            maximum = progressMaximum;
        }
    });

    const bool cancellable = _runningOperation && !_runningOperation->isFinished();
    if(!cancellable)
        _cancelling = false;
    if(_busy != busy || _taskText != taskText || _taskProgress != progress || _taskMaximum != maximum
            || _cancellable != cancellable) {
        _busy = busy;
        _taskText = taskText;
        _taskProgress = progress;
        _taskMaximum = maximum;
        _cancellable = cancellable;
        Q_EMIT taskStateChanged();
    }

    // A finished operation may have added or removed objects, so the empty state has to be re-evaluated with it.
    if(!busy)
        refreshDataSetState();
}

/******************************************************************************
* Makes the given task the operation that the Cancel command cancels.
******************************************************************************/
void QmlWorkbenchController::setRunningOperation(TaskPtr task)
{
    _runningOperation = std::move(task);
    _cancelling = false;
    updateTaskState();
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

    // A QML signal handler runs outside of any OVITO task, so the import takes place in a task of its own (see the note
    // on task contexts in the Phase 0 audit). The whole operation is handed to the shell, which is what lets the user
    // cancel it - from the detection of the file formats to the loading of the data.
    GuiTaskScope taskScope(_ui);
    setRunningOperation(taskScope.task());
    try {
        _ui.importFiles(urlList);
        setStatusMessage(tr("Imported %1.").arg(urlList.back().fileName()));
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
    updateTaskState();
}

/******************************************************************************
* Asks the scene to open the file selection dialog.
******************************************************************************/
void QmlWorkbenchController::showImportDialog()
{
    Q_EMIT importDialogRequested(_importDialogDirectory.isEmpty() ? QUrl() : QUrl::fromLocalFile(_importDialogDirectory));
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
