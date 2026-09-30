// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/base/GUIBase.h>
#include <ovito/gui/base/actions/ActionManager.h>
#include <QSortFilterProxyModel>

namespace Ovito {

/**
 * \brief A searchable list of the commands of a workbench, without presenting any of them.
 *
 * The commands of both frontends are frontend-neutral Command objects owned by the ActionManager (see audit decision
 * D26), and the ActionManager is itself a list model over them. What a search field or a command palette needs on top of
 * that is a *view* of that list: rows that can be filtered by what the user typed, in a stable order, and reachable
 * without naming a command id.
 *
 * This model is that view, and it goes through CommandRole rather than ActionRole, so a frontend that does not have
 * QActions (the Qt Quick one) can present and invoke the same commands, with the same enablement and the same shortcut,
 * as the classic frontend's QAction-based views.
 *
 * The rows of the model are the commands the filter lets through, sorted by their text: a command that is not visible is
 * never listed, and an empty filter lists every visible command, which is what a frontend that wants to show the whole
 * command surface to the user displays.
 *
 * \note The ranking of the classic frontend's own quick search (how often the user invoked a command, in
 *       gui/desktop/actions/SearchActions.cpp) is deliberately not part of this model yet; adopting the shared model
 *       there is Phase 8 work (audit open item O22).
 */
class OVITO_GUIBASE_EXPORT CommandListModel : public QSortFilterProxyModel
{
    Q_OBJECT

    /// The text the user typed. An empty filter lists every visible command.
    Q_PROPERTY(QString filter READ filter WRITE setFilter NOTIFY filterChanged)

public:

    /// Constructor. \a actionManager is the model this one presents; it has to outlive this object.
    explicit CommandListModel(ActionManager& actionManager, QObject* parent = nullptr);

    /// Returns the text the list is filtered by, or an empty string when nothing is filtered out.
    const QString& filter() const { return _filter; }

    /// Sets the text the list is filtered by. The filter is matched against the text and the status tip of a command,
    /// case-insensitively.
    void setFilter(const QString& filter);

    /// Returns the id of the command in the given row, or an empty string when the row does not exist.
    Q_INVOKABLE QString commandIdAt(int row) const;

    /// Invokes the command in the given row, exactly as the command's own control would.
    /// \return \c false if the row does not exist or holds a command that is currently disabled.
    Q_INVOKABLE bool triggerAt(int row);

    /// Returns the command of the given row, or null when the row does not exist.
    Command* command(int row) const;

    /// Runs the filter again, which is what a consumer calls after the visible or enabled state of the commands may have
    /// changed - the action manager announces that case with actionUpdateRequested(), and a frontend that updates its
    /// command states on its own asks for it here.
    Q_INVOKABLE void refresh() { refreshFilter(); }

Q_SIGNALS:

    /// Is emitted when the filter changed.
    void filterChanged();

protected:

    /// Decides whether a command of the ActionManager is listed.
    virtual bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

    /// Orders the rows by the text of their command.
    virtual bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;

private:

    /// Returns the command of a row of the action manager, or null when the row holds none.
    Command* sourceCommand(int sourceRow, const QModelIndex& sourceParent) const;

    /// Runs the filter again, which is what a change of the filter or of the commands requires.
    void refreshFilter();

private:

    /// The text the list is filtered by.
    QString _filter;
};

}   // End of namespace Ovito
