// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/qml/QmlFrontend.h>
#include <ovito/gui/base/actions/Command.h>
#include <ovito/gui/base/mainwin/AvailableModifiersModel.h>
#include <ovito/gui/base/mainwin/PipelineListModel.h>
#include <ovito/gui/base/mainwin/PipelineListItem.h>
#include <ovito/core/automation/AutomationObjectId.h>

namespace Ovito {

class WorkbenchUI;

/**
 * \brief The model side of the pipeline panel: the shared pipeline list and modifier library, plus the stable identity
 *        of the objects they present.
 *
 * The panel itself belongs to Phase 4 of the migration plan; what this class provides is everything such a panel binds
 * to, without presenting anything (audit decision D53). It owns the two models the panel reads - the shared
 * PipelineListModel (which is also what the classic frontend's pipeline widget is built on) and the shared
 * AvailableModifiersModel (the modifier library) - and it adds the two things those models cannot offer a QML view by
 * themselves:
 *
 * 1. **Identity.** A row is a position in a list that is rebuilt whenever the pipeline changes, so a deferred caller
 *    that remembered a row would write to whatever object holds that row later. A row whose object the automation
 *    contract can name (a modification node) therefore also has a stable ID from the workbench's automation session
 *    (D55), and the mutations of this class take that ID rather than a row.
 * 2. **Selection.** The shared model selects through a QItemSelectionModel, which QML cannot drive; this class offers
 *    the selection as an id list and as invokable commands, and announces changes with one signal.
 *
 * The identity of a row the contract cannot name - the data source, a visual element, a modifier group, a header row -
 * is deliberately empty (audit decision D62): those rows can be acted on where the row is known (a delegate passes its
 * row right back within the same call), but a name for them outside the frontend would have to be an ID that does not
 * resolve back to the row, which is worse than no name.
 *
 * \note Both models are single-instance-per-process by construction (their constructors register commands with fixed
 *       ids), so a workbench must create exactly one of these controllers. The one that exists lives in QmlMainWindowUI.
 */
class OVITO_GUIQML_EXPORT QmlPipelineController : public QObject
{
    Q_OBJECT

    /// The rows of the pipeline: the visual elements, the modification nodes, the modifier groups and the data source.
    /// Its roles are the shared ones (title, type, ischecked, iscollapsed, decoration, tooltip, statusinfo).
    Q_PROPERTY(QAbstractItemModel* model READ model CONSTANT)

    /// The library of modifiers the user can insert: a tree whose root items are the categories and whose children are
    /// the insert commands, which carry their Command in CommandRole.
    Q_PROPERTY(QAbstractItemModel* modifierLibrary READ modifierLibrary CONSTANT)

    /// Whether the pipeline panel has a selection, i.e. whether there is an object the panel's commands apply to.
    Q_PROPERTY(bool hasSelection READ hasSelection NOTIFY selectionChanged)

    /// The ID of the selected object if exactly one row is selected, or an empty string otherwise.
    Q_PROPERTY(QString selectedObjectId READ selectedObjectId NOTIFY selectionChanged)

    /// The IDs of the selected rows that name an object of the automation contract, in list order.
    Q_PROPERTY(QVariantList selectedObjectIds READ selectedObjectIds NOTIFY selectionChanged)

    /// The title of the selected row if exactly one is selected, or an empty string otherwise.
    Q_PROPERTY(QString selectedTitle READ selectedTitle NOTIFY selectionChanged)

public:

    /// Constructor. Creates the two models it owns; \a ui supplies the action manager, the user interface the models
    /// belong to and the automation session that hands out the object IDs.
    explicit QmlPipelineController(WorkbenchUI& ui, QObject* parent = nullptr);

    /// Returns the rows of the pipeline.
    QAbstractItemModel* model() const { return _pipelineModel; }

    /// Returns the rows of the pipeline as the shared model, which the C++ side of the frontend and the checks use.
    PipelineListModel* pipelineModel() const { return _pipelineModel; }

    /// Returns the library of modifiers.
    QAbstractItemModel* modifierLibrary() const { return _modifierLibrary; }

    /// Returns the modifier library as the shared model.
    AvailableModifiersModel* modifierListModel() const { return _modifierLibrary; }

    /// Returns whether a row is selected.
    bool hasSelection() const { return !_pipelineModel->selectedItems().empty(); }

    /// Returns the ID of the selected object, or an empty string.
    QString selectedObjectId() const;

    /// Returns the IDs of the selected objects.
    QVariantList selectedObjectIds() const;

    /// Returns the title of the selected row, or an empty string.
    QString selectedTitle() const;

    // ---- Identity of a row ----

    /// Returns the stable ID of the object of the given row, or an empty string when the row holds no such object or
    /// the object's kind has no name in the automation contract (see the class documentation).
    Q_INVOKABLE QString objectIdAt(int row) const;

    /// Returns the row that currently shows the object of the given ID, or -1 when no row does.
    Q_INVOKABLE int rowForObjectId(const QString& objectId) const;

    /// Returns the title of the row, or an empty string when it does not exist.
    Q_INVOKABLE QString titleAt(int row) const;

    /// Returns whether the row's object is enabled, or false when the row does not exist or has no such state.
    Q_INVOKABLE bool isCheckedAt(int row) const;

    // ---- Selection ----

    /// Selects the given row, optionally in addition to the current selection (a view with Ctrl held).
    Q_INVOKABLE void selectRow(int row, bool toggle = false);

    /// Selects the row that shows the given object, optionally in addition to the current selection.
    /// \return \c false if no row shows that object.
    Q_INVOKABLE bool selectObjectId(const QString& objectId, bool toggle = false);

    /// Clears the selection of the panel.
    Q_INVOKABLE void clearSelection();

    // ---- Editing ----

    /// Enables or disables the object of the given row. This is the immediate form: the row is used in the call that
    /// resolves it, and every row kind (including the visual elements and the groups) can be toggled with it.
    Q_INVOKABLE bool setCheckedAt(int row, bool checked);

    /// Enables or disables the object of the given ID. The ID is resolved first, so a deferred caller can never reach
    /// an object that replaced the one it meant (D55).
    /// \return \c false if the ID does not resolve to an object of the pipeline.
    Q_INVOKABLE bool setCheckedForObject(const QString& objectId, bool checked);

    /// Moves the object of the given ID one position up or down in the pipeline, as one undo step.
    Q_INVOKABLE bool moveObjectUp(const QString& objectId);
    Q_INVOKABLE bool moveObjectDown(const QString& objectId);

    /// Removes the object of the given ID from the pipeline, as one undo step.
    Q_INVOKABLE bool deleteObject(const QString& objectId);

    /// Removes the selected objects from the pipeline, as one undo step.
    Q_INVOKABLE bool deleteSelectedObjects();

    // ---- Modifier library ----

    /// Returns the number of categories of the modifier library.
    Q_INVOKABLE int modifierCategoryCount() const;

    /// Returns the name of a category of the modifier library.
    Q_INVOKABLE QString modifierCategoryName(int category) const;

    /// Returns the number of modifiers in a category.
    Q_INVOKABLE int modifierCount(int category) const;

    /// Returns the id of the command that inserts the modifier of a library row, or an empty string.
    Q_INVOKABLE QString modifierCommandId(int category, int row) const;

    /// Inserts the modifier of a library row into the selected pipeline, which is exactly what the classic frontend's
    /// modifier library does. The insertion is undoable and selects the new modifier.
    /// \return \c false if the row does not exist or its command is currently disabled (i.e. the modifier does not
    /// apply to the selected pipeline).
    Q_INVOKABLE bool insertModifier(int category, int row);

Q_SIGNALS:

    /// Is emitted when the selection of the panel changed, including when a list update changed it.
    void selectionChanged();

private:

    /// Returns the list item of a row, or null when there is no such row.
    PipelineListItem* itemAt(int row) const;

    /// Returns the list item that shows the object of an ID, or null when no item does.
    PipelineListItem* itemForObjectId(const QString& objectId) const;

    /// Returns the index of an item in the row list, or -1 when the item is not in it.
    int rowOfItem(PipelineListItem* item) const;

    /// Returns the ID of an object, or an empty string when its kind has no name in the automation contract.
    QString idOfObject(OvitoObject* object) const;

private:

    /// The rows of the pipeline. Owned by this object.
    PipelineListModel* _pipelineModel;

    /// The library of modifiers. Owned by this object.
    AvailableModifiersModel* _modifierLibrary;

    /// The session whose registry hands out the object IDs.
    WorkbenchUI& _ui;
};

}   // End of namespace Ovito
