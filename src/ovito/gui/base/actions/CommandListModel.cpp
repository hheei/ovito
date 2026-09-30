// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/gui/base/GUIBase.h>
#include <ovito/gui/base/actions/ActionManager.h>
#include "CommandListModel.h"

namespace Ovito {

/******************************************************************************
* Constructor.
******************************************************************************/
CommandListModel::CommandListModel(ActionManager& actionManager, QObject* parent) : QSortFilterProxyModel(parent)
{
    setSourceModel(&actionManager);
    // The commands of the action manager change while the application runs: plugins register theirs, the modifier
    // library registers one per modifier and template, and a frontend removes the ones whose dialog it does not offer.
    setDynamicSortFilter(true);
    setSortCaseSensitivity(Qt::CaseInsensitive);
    sort(0, Qt::AscendingOrder);

    // A command that becomes enabled, disabled or invisible does not change a role of the source model, so the proxy
    // cannot notice it by itself. The action manager announces exactly that case: it is asked to update its commands'
    // states whenever something happened that could change them.
    connect(&actionManager, &ActionManager::actionUpdateRequested, this, &CommandListModel::refreshFilter);
}

/******************************************************************************
* Sets the text the list is filtered by.
******************************************************************************/
void CommandListModel::setFilter(const QString& filter)
{
    if(_filter == filter)
        return;
    _filter = filter;
    refreshFilter();
    Q_EMIT filterChanged();
}

/******************************************************************************
* Runs the filter again, which is what a change of the filter or of the commands requires.
******************************************************************************/
void CommandListModel::refreshFilter()
{
    // The row-wise form of the invalidation keeps the columns of the source model and the scroll position a view has.
    beginFilterChange();
    endFilterChange(QSortFilterProxyModel::Direction::Rows);
}

/******************************************************************************
* Returns the id of the command in the given row.
******************************************************************************/
QString CommandListModel::commandIdAt(int row) const
{
    if(Command* rowCommand = command(row))
        return rowCommand->id();
    return {};
}

/******************************************************************************
* Invokes the command in the given row.
******************************************************************************/
bool CommandListModel::triggerAt(int row)
{
    Command* rowCommand = command(row);
    if(!rowCommand || !rowCommand->isEnabled())
        return false;
    rowCommand->trigger();
    return true;
}

/******************************************************************************
* Returns the command of a row of this model.
******************************************************************************/
Command* CommandListModel::command(int row) const
{
    if(row < 0 || row >= rowCount())
        return nullptr;
    return sourceCommand(mapToSource(index(row, 0)).row(), mapToSource(index(row, 0)).parent());
}

/******************************************************************************
* Returns the command of a row of the action manager.
******************************************************************************/
Command* CommandListModel::sourceCommand(int sourceRow, const QModelIndex& sourceParent) const
{
    if(!sourceModel() || sourceRow < 0)
        return nullptr;
    return sourceModel()->index(sourceRow, 0, sourceParent).data(ActionManager::CommandRole).value<Command*>();
}

/******************************************************************************
* Decides whether a command of the action manager is listed.
******************************************************************************/
bool CommandListModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
    Command* command = sourceCommand(sourceRow, sourceParent);
    if(!command || !command->isVisible())
        return false;
    if(_filter.isEmpty())
        return true;
    // The search text role is what the action manager offers for exactly this purpose: the label of the command and the
    // explanation it carries, so that a user who remembers the explanation finds the command as well.
    const QString searchText = sourceModel()->index(sourceRow, 0, sourceParent).data(ActionManager::SearchTextRole).toString();
    return searchText.contains(_filter, Qt::CaseInsensitive);
}

/******************************************************************************
* Orders the rows by the text of their command.
******************************************************************************/
bool CommandListModel::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
    const QString leftText = left.data(Qt::DisplayRole).toString();
    const QString rightText = right.data(Qt::DisplayRole).toString();
    return QString::localeAwareCompare(leftText, rightText) < 0;
}

}   // End of namespace Ovito
