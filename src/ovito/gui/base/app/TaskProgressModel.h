// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/base/GUIBase.h>

namespace Ovito {

class WorkbenchUI;

/**
 * \brief Lists the tasks that are currently running in a workbench and reports the progress of the operation the user
 *        interface should display.
 *
 * The data of this model is the list of running tasks of a WorkbenchUI (which collects them from the task system
 * through taskProgressBegin/End/Changed()). It replaces the progress bookkeeping of the individual frontends: the
 * status bar of the classic main window and the status line of the Qt Quick workbench read the same rows and the same
 * aggregate values instead of walking the task list themselves.
 *
 * The rows are the running tasks; a task can be started by any part of the application (a pipeline evaluation, an
 * import, a file export, ...), so the model has more than one row whenever several operations overlap. The aggregate
 * properties (busy/text/value/maximum) describe the one task a status bar displays, which is the first task in the
 * list that describes itself - exactly the choice both frontends made on their own before.
 *
 * Cancelling is not part of this model: OVITO's TaskProgress carries the text and the progress of an operation but no
 * handle to its task, so only the code that started an operation can cancel it. The workbench offers that to its
 * frontend for the operations the frontend itself started (see QmlWorkbenchController).
 */
class OVITO_GUIBASE_EXPORT TaskProgressModel : public QAbstractListModel
{
    Q_OBJECT

public:

    /// The roles through which a view reads a task of this model.
    enum ModelRole {
        /// The text that describes the task to the user.
        TextRole = Qt::UserRole + 1,
        /// The progress of the task so far.
        ValueRole,
        /// The progress value that means "done". A value of zero or less means that the task cannot report progress.
        MaximumRole,
        /// The progress of the task as a fraction between 0 and 1, or -1 if the task cannot report progress.
        FractionRole,
    };

    /// Constructor taking the workbench whose running tasks this model lists.
    TaskProgressModel(WorkbenchUI& ui, QObject* parent = nullptr);

    /// Returns the number of tasks that are currently running.
    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override { return (int)_tasks.size(); }

    /// Returns the text, progress or fraction of the given task.
    virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    /// Returns the roles of this model.
    virtual QHash<int, QByteArray> roleNames() const override;

    /// The number of tasks that are running, for QML code that binds to the model rather than to a delegate.
    Q_PROPERTY(int count READ rowCount NOTIFY progressChanged)

    /// Indicates whether the workbench is busy, i.e. whether at least one task is running.
    Q_PROPERTY(bool busy READ isBusy NOTIFY progressChanged)

    /// The text of the task that the status bar displays, or an empty string if there is nothing to display.
    Q_PROPERTY(QString text READ activeText NOTIFY progressChanged)

    /// The progress value of the task that the status bar displays.
    Q_PROPERTY(int value READ activeValue NOTIFY progressChanged)

    /// The progress value that means "done" for the displayed task, or zero if it cannot report progress.
    Q_PROPERTY(int maximum READ activeMaximum NOTIFY progressChanged)

    /// The fraction of the displayed task between 0 and 1, or -1 if the task cannot report progress.
    Q_PROPERTY(qreal fraction READ activeFraction NOTIFY progressChanged)

    /// Indicates whether at least one task is running.
    bool isBusy() const { return !_tasks.empty(); }

    /// Returns the text of the task that the status bar displays.
    QString activeText() const { return activeTask().text; }

    /// Returns the progress value of the task that the status bar displays.
    int activeValue() const { return activeTask().value; }

    /// Returns the progress value that means "done" for the displayed task.
    int activeMaximum() const { return activeTask().maximum; }

    /// Returns the fraction of the displayed task between 0 and 1, or -1 if it cannot report progress.
    qreal activeFraction() const;

public Q_SLOTS:

    /// Re-reads the running tasks of the workbench. The workbench calls this when a task started, changed its progress
    /// or finished.
    void refresh();

Q_SIGNALS:

    /// Emitted whenever the list of running tasks or the progress of one of them has changed. It is the NOTIFY signal
    /// of all aggregate properties of this model.
    void progressChanged();

private:

    /// A running task, copied out of the workbench's task list.
    struct Task
    {
        QString text;
        int value = 0;
        int maximum = 0;

        /// Compares two tasks to detect changes without emitting a signal for an unchanged list.
        bool operator==(const Task& other) const { return text == other.text && value == other.value && maximum == other.maximum; }
    };

    /// Returns the task the status bar should display, i.e. the first one that describes itself, or an empty task.
    const Task& activeTask() const;

    /// The workbench whose running tasks this model lists.
    WorkbenchUI& _ui;

    /// The running tasks of the workbench as of the last refresh.
    std::vector<Task> _tasks;

    /// The empty task that activeTask() returns when no task describes itself.
    const Task _noTask;
};

}   // End of namespace
