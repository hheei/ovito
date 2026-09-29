// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/gui/base/GUIBase.h>
#include <ovito/gui/base/app/WorkbenchUI.h>
#include "TaskProgressModel.h"

namespace Ovito {

/******************************************************************************
* Constructor.
******************************************************************************/
TaskProgressModel::TaskProgressModel(WorkbenchUI& ui, QObject* parent) : QAbstractListModel(parent), _ui(ui)
{
}

/******************************************************************************
* Returns the text, progress or fraction of the given task.
******************************************************************************/
QVariant TaskProgressModel::data(const QModelIndex& index, int role) const
{
    if(index.row() < 0 || index.row() >= (int)_tasks.size())
        return {};

    const Task& task = _tasks[index.row()];
    switch(role) {
    case Qt::DisplayRole:
    case TextRole:
        return task.text;
    case ValueRole:
        return task.value;
    case MaximumRole:
        return task.maximum;
    case FractionRole:
        return task.maximum > 0 ? qreal(task.value) / qreal(task.maximum) : qreal(-1);
    default:
        return {};
    }
}

/******************************************************************************
* Returns the roles of this model.
******************************************************************************/
QHash<int, QByteArray> TaskProgressModel::roleNames() const
{
    return {
        { TextRole, "text" },
        { ValueRole, "value" },
        { MaximumRole, "maximum" },
        { FractionRole, "fraction" }
    };
}

/******************************************************************************
* Returns the fraction of the displayed task between 0 and 1, or -1 if it cannot report progress.
******************************************************************************/
qreal TaskProgressModel::activeFraction() const
{
    const Task& task = activeTask();
    return task.maximum > 0 ? qreal(task.value) / qreal(task.maximum) : qreal(-1);
}

/******************************************************************************
* Re-reads the running tasks of the workbench.
******************************************************************************/
void TaskProgressModel::refresh()
{
    std::vector<Task> tasks;

    // Take a snapshot of the running tasks. The workbench's list is protected by a mutex, which visitRunningTasks()
    // holds while the visitor runs - so no view can read the model in between, and the signal below is emitted after
    // the list has been left again.
    _ui.visitRunningTasks([&](const QString& text, int progressValue, int progressMaximum) {
        tasks.push_back(Task{text, progressValue, progressMaximum});
    });

    if(tasks == _tasks)
        return;

    // The number of rows changed only when tasks started or finished; a task updating its progress is a data change of
    // an existing row. Both are reported, because the views care about different parts of it: a list view about the
    // rows, the status bar about the progress of the displayed one.
    if(tasks.size() != _tasks.size()) {
        beginResetModel();
        _tasks = std::move(tasks);
        endResetModel();
    }
    else {
        _tasks = std::move(tasks);
        Q_EMIT dataChanged(index(0, 0), index((int)_tasks.size() - 1, 0),
                           {TextRole, ValueRole, MaximumRole, FractionRole});
    }
    Q_EMIT progressChanged();
}

/******************************************************************************
* Returns the task the status bar should display, i.e. the first one that describes itself.
******************************************************************************/
const TaskProgressModel::Task& TaskProgressModel::activeTask() const
{
    // Tasks that report no text have nothing to display; they count as running work (busy) but the status bar shows the
    // first task that describes itself.
    for(const Task& task : _tasks) {
        if(!task.text.isEmpty())
            return task;
    }
    return _noTask;
}

}   // End of namespace
