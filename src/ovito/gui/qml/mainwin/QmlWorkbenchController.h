// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/qml/QmlFrontend.h>
#include <ovito/core/utilities/concurrent/Task.h>

class QEventLoop;

namespace Ovito {

class QmlMainWindowUI;

/**
 * \brief Exposes the state and the commands of the workbench shell to the QML scene.
 *
 * This class is registered as the "workbenchController" context property of the QML engine. It carries everything the
 * shell displays that is not a viewport: the status line, the progress of the running operations, the window title, the
 * empty state of the scene, and the message dialog the frontend is waiting on.
 *
 * The commands it offers are the shell's entry points into the shared workbench code of gui/base: importing data files
 * (from the file dialog, from a drop onto the window, or from the command line) and cancelling the import that is
 * currently running. It deliberately does not own the presentation - the QML scene decides how the states it reports
 * look - and it does not own the viewports, which belong to QmlViewportController.
 */
class OVITO_GUIQML_EXPORT QmlWorkbenchController : public QObject
{
    Q_OBJECT

    /// The title of the workbench window, derived from the data set or session file.
    Q_PROPERTY(QString windowTitle READ windowTitle NOTIFY windowTitleChanged)

    /// The message displayed in the status line of the workbench window. It is transient: the workbench clears it when
    /// the mouse leaves a viewport, exactly like the classic frontend does.
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)

    /// The one thing the workbench wants the user to know beyond the current status: what the last import did with the
    /// file it was given (which format it was read as, how many source frames it holds), or that the viewports cannot
    /// be rendered at all. Unlike the status message this survives user interaction and is replaced only by the next
    /// such report, because it is the only visible hint of a file that was read as an unexpected format (defect F6) or
    /// of a platform plugin that provides no graphics device.
    Q_PROPERTY(QString notice READ notice NOTIFY noticeChanged)

    /// Whether the current scene contains any data to display. False means the shell shows its empty state.
    /// The recently opened data and session files the File menu offers, as entries that carry a `title` and whether they
    /// are a session file (`isSession`). It is the shared RecentFilesList, so both frontends offer the same files.
    Q_PROPERTY(QVariantList recentFiles READ recentFiles NOTIFY recentFilesChanged)

    Q_PROPERTY(bool hasData READ hasData NOTIFY hasDataChanged)

    /// Whether the operation that is currently running can be cancelled by the user.
    Q_PROPERTY(bool cancellable READ cancellable NOTIFY taskStateChanged)

    /// Whether the user has requested the cancellation of the running operation, i.e. the operation is winding down.
    Q_PROPERTY(bool cancelling READ cancelling NOTIFY taskStateChanged)

    /// The message dialog the frontend is waiting on. The scene displays it while it is visible and answers it by
    /// calling answerMessageBox().
    Q_PROPERTY(bool messageBoxVisible READ messageBoxVisible NOTIFY messageBoxChanged)
    Q_PROPERTY(QString messageBoxTitle READ messageBoxTitle NOTIFY messageBoxChanged)
    Q_PROPERTY(QString messageBoxText READ messageBoxText NOTIFY messageBoxChanged)
    Q_PROPERTY(int messageBoxIcon READ messageBoxIcon NOTIFY messageBoxChanged)
    Q_PROPERTY(QVariantList messageBoxButtons READ messageBoxButtons NOTIFY messageBoxChanged)

    /// The button value the dialog answers with when it is dismissed instead of answered (the caller's default).
    Q_PROPERTY(int defaultMessageBoxButton READ defaultMessageBoxButton NOTIFY messageBoxChanged)

    /// The name of the application, as the About dialog and the error messages show it.
    Q_PROPERTY(QString applicationName READ applicationName CONSTANT)

    /// The version of the application, as the About dialog shows it.
    Q_PROPERTY(QString applicationVersion READ applicationVersion CONSTANT)

    /// The edition this build is ("Basic" or "Professional").
    Q_PROPERTY(QString buildType READ buildType CONSTANT)

    /// The copyright notice of this build, as rich text.
    Q_PROPERTY(QString copyrightNotice READ copyrightNotice CONSTANT)

public:

    /// Constructor.
    explicit QmlWorkbenchController(QmlMainWindowUI& ui, QObject* parent = nullptr);

    /// Returns the title of the workbench window.
    QString windowTitle() const { return _windowTitle; }

    /// Returns the message currently shown in the status line.
    QString statusMessage() const { return _statusMessage; }

    /// Replaces the message shown in the status line.
    void setStatusMessage(const QString& message);

    /// Returns the report the workbench keeps showing until the next one replaces it.
    QString notice() const { return _notice; }

    /// Replaces the report the workbench keeps showing until the next one replaces it.
    void setNotice(const QString& notice);

    /// Returns whether the current scene contains data to display.
    bool hasData() const { return _hasData; }

    /// Returns whether the running operation can be cancelled by the user.
    bool cancellable() const { return _cancellable; }

    /// Returns whether the cancellation of the running operation has been requested.
    bool cancelling() const { return _cancelling; }

    /// Returns whether the frontend is waiting on a message dialog.
    bool messageBoxVisible() const { return _messageBoxVisible; }

    /// Returns the title of the message dialog.
    QString messageBoxTitle() const { return _messageBoxTitle; }

    /// Returns the text of the message dialog.
    QString messageBoxText() const { return _messageBoxText; }

    /// Returns the icon of the message dialog, as a UserInterface::MessageBoxIcon value.
    int messageBoxIcon() const { return _messageBoxIcon; }

    /// Returns the buttons of the message dialog as a list of { button, text, isDefault } maps.
    QVariantList messageBoxButtons() const { return _messageBoxButtons; }

    /// Returns the button value the message dialog answers with when it is dismissed instead of answered.
    int defaultMessageBoxButton() const { return static_cast<int>(_messageBoxAnswer); }

    /// Returns the name of the application.
    QString applicationName() const;

    /// Returns the version of the application.
    QString applicationVersion() const;

    /// Returns the edition this build is.
    QString buildType() const;

    /// Returns the copyright notice of this build.
    QString copyrightNotice() const;

    /// Imports the given files (QUrl values) into the current dataset, reporting the states they produce.
    Q_INVOKABLE void importFiles(const QVariantList& urls);

    /// Asks the scene to open the file selection dialog, starting in the last used directory.
    Q_INVOKABLE void showImportDialog();

    /// Opens the recently opened file with the given index of recentFiles(): a session file replaces the current session
    /// (asking about unsaved changes first), a data file is imported again through the importer and the format the entry
    /// remembers.
    Q_INVOKABLE void openRecentFile(int index);

    /// Returns the recently opened data and session files, most recent first (see the recentFiles property).
    QVariantList recentFiles() const;

    /// Returns the directory the file selection dialog opens in: the one the frontend asked for, else the one the last
    /// import used (the file dialog history that the shared GuiSettings owns).
    QUrl importDirectoryUrl() const;

    /// Asks the scene to open the file selection dialog, starting in the given directory.
    void requestImportDialog(const QString& directoryPath);

    /// Requests the cancellation of the import operation that is currently running.
    Q_INVOKABLE void cancelCurrentOperation();

    /// Answers the message dialog the frontend is waiting on with one of its buttons.
    Q_INVOKABLE void answerMessageBox(int button);

    /// Asks the scene to display the About dialog of the workbench.
    Q_INVOKABLE void showAboutDialog();

    /// Updates the state derived from the current dataset: the window title and the empty state.
    /// Called by the frontend when the data set changes or an operation that may have changed it has finished.
    void refreshDataSetState();

    /// Updates the state of the operation the shell started: whether it can still be cancelled and whether its
    /// finishing changed the scene. Called by the frontend's progressTasksChanged() hook and when the operation is set.
    /// The progress of the running tasks themselves is presented by the workbench's TaskProgressModel.
    void updateOperationState();

    /// Makes the given task the operation that the Cancel command cancels, or clears it when passing a null task.
    /// Called by the frontend while it runs an operation on behalf of the user.
    void setRunningOperation(TaskPtr task);

    /// Presents a message dialog and blocks until the user answers it, like the modal dialog of the desktop frontend.
    UserInterface::MessageBoxButton presentMessageBox(UserInterface::MessageBoxIcon icon, const QString& title, const QString& text, int buttons, UserInterface::MessageBoxButton defaultButton, const QString& detailedText);

Q_SIGNALS:

    /// Is emitted when the window title has changed.
    void windowTitleChanged();

    /// Is emitted when the status line message has changed.
    void statusMessageChanged();

    /// Is emitted when the report about the last import has changed.
    void noticeChanged();

    /// Is emitted when the current scene became empty or gained its first object.
    void hasDataChanged();

    /// Is emitted when the progress state of the running operations has changed.
    void taskStateChanged();

    /// Is emitted when the message dialog appeared, changed or was answered.
    void messageBoxChanged();

    /// Is emitted when the scene should open the file selection dialog for the given directory.
    void importDialogRequested(const QUrl& directoryUrl);

    /// Is emitted when the list of recently opened files changed.
    void recentFilesChanged();

    /// Is emitted when the scene should display the About dialog.
    void aboutDialogRequested();

    /// Is emitted when the message dialog has been answered.
    void messageBoxAnswered();

private:

    /// Determines the title of the workbench window from the current data set.
    QString determineWindowTitle() const;

    /// Determines whether the current scene contains data to display.
    bool determineHasData() const;

    /// Builds the list of buttons of the message dialog from the given UserInterface button mask.
    static QVariantList makeButtonList(int buttons, UserInterface::MessageBoxButton defaultButton);

private:

    /// The user interface this controller belongs to.
    QmlMainWindowUI& _ui;

    /// The title of the workbench window.
    QString _windowTitle;

    /// The message shown in the status line.
    QString _statusMessage;

    /// The report about the last import, shown in the status line while no other message is displayed.
    QString _notice;

    /// Whether the current scene contains data to display.
    bool _hasData = false;

    /// Whether the running operation can be cancelled by the user, and whether its cancellation was requested.
    bool _cancellable = false;
    bool _cancelling = false;

    /// The operation that the Cancel command cancels.
    TaskPtr _runningOperation;

    /// The state of the message dialog the frontend is waiting on.
    bool _messageBoxVisible = false;
    QString _messageBoxTitle;
    QString _messageBoxText;
    int _messageBoxIcon = 0;
    QVariantList _messageBoxButtons;
    UserInterface::MessageBoxButton _messageBoxAnswer = UserInterface::MessageBoxButton::NoButton;

    /// The event loop that presentMessageBox() blocks in, or null when no message dialog is open.
    QEventLoop* _messageBoxLoop = nullptr;

    /// The directory the file selection dialog should start in.
    QString _importDialogDirectory;
};

}   // End of namespace
