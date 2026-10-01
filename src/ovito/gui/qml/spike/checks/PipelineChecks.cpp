// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/gui/qml/spike/SpikeHarness.h>
#include <ovito/gui/qml/models/QmlPipelineController.h>
#include <ovito/gui/base/mainwin/PipelineListItem.h>
#include <ovito/gui/base/actions/Command.h>
#include <ovito/gui/base/actions/CommandListModel.h>
#include <ovito/gui/base/app/GuiTaskScope.h>
#include <ovito/core/app/undo/UndoStack.h>
#include <ovito/core/automation/AutomationSession.h>
#include <ovito/core/automation/AutomationObjectId.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/core/dataset/io/FileSource.h>

namespace Ovito::Spike {

/******************************************************************************
* Verifies the model side of the pipeline panel: the roles a QML view reads, the stable identity of a row's object, the
* selection and its coherence while the pipeline is edited, and the command list model.
*
* This is the check of Phase 3 slice S1 (deliverables 1, 3, 4 and 5 of the plan). It verifies the *model and command
* APIs* and not a panel workflow - the panel itself is Phase 4 (audit decision D53) - so everything it touches is what
* such a panel binds to:
*
*   * the roles of the shared PipelineListModel as a QML delegate reads them, including the folded-state role that only
*     became reachable from QML in this slice ("iscollapsed" in roleNames), and the rule that the icon role of a row is a
*     name or a path for QML and never a QIcon,
*   * the stable ID of a row's object (D55): a row carries an ID of the automation contract, the same ID names the same
*     object again after an undo and a redo, and a caller that held an ID whose object is gone is refused instead of
*     writing to whoever holds the row now,
*   * the selection as an ID list, and what happens to it when the selected object is deleted and the deletion is undone,
*   * the edits the panel offers - insertion through the shared modifier library (the very Command objects the classic
*     frontend triggers), reordering, deletion and enabling - each as one undo step with its undo,
*   * and the command list model that a menu search or the Phase 8 command palette presents.
*
* The check imports a data set of its own and replaces the data set at the end, which is why it runs after the checks
* that rely on the scene they leave behind (the option table in Main.cpp records the order).
*
* Objects are created, transactions are opened and commands are triggered, so the phases run inside a GuiTaskScope like
* the other checks that touch the pipeline. Between the phases the check waits, because the shared model refreshes its
* rows asynchronously (a 200 ms timer) and the pipeline is evaluated in the background.
******************************************************************************/

namespace {

/// The state the phases of the check share.
struct PipelineCheckState
{
    /// The frontend under test and the continuation that lets the next check run.
    QmlMainWindowUI* ui = nullptr;
    std::function<void()> continuation;

    /// The number of modification rows before the check inserted anything.
    int modifiersBefore = 0;

    /// The IDs of the modification nodes the check inserted.
    QString objectId;
    QString secondObjectId;

    /// The revision of the session before the data set was replaced.
    quint64 revisionBefore = 0;
};

using StatePtr = std::shared_ptr<PipelineCheckState>;

/// Runs a phase of the check once the model and the pipeline had time to settle.
template<class Function>
void afterSettle(const StatePtr& state, Function&& phase)
{
    scheduleDelayed(state->ui, 400, [state, phase]() { phase(state); });
}

/// Returns the controller under test, or reports that the frontend does not offer one.
QmlPipelineController* pipelineController(const StatePtr& state)
{
    if(QmlPipelineController* controller = state->ui->pipelineController())
        return controller;
    reportVerificationFailure(QStringLiteral("the frontend offers no pipeline controller, so nothing can be verified"));
    return nullptr;
}

/// Counts the rows that hold a modification node.
int modifierRowCount(QmlPipelineController* controller)
{
    QAbstractItemModel* model = controller->model();
    int count = 0;
    for(int row = 0; row < model->rowCount(); row++) {
        if(model->data(model->index(row, 0), PipelineListModel::ItemTypeRole).toInt() == PipelineListItem::Modifier)
            count++;
    }
    return count;
}

/// Returns the row of the modification node with the given ID, or -1.
int rowOfModifier(QmlPipelineController* controller, const QString& objectId)
{
    const int row = controller->rowForObjectId(objectId);
    if(row < 0)
        return -1;
    if(controller->model()->data(controller->model()->index(row, 0), PipelineListModel::ItemTypeRole).toInt() != PipelineListItem::Modifier)
        return -1;
    return row;
}

/// Finds the first library entry whose insert command is enabled, i.e. a modifier that applies to the imported data.
bool findEnabledLibraryEntry(const StatePtr& state, QmlPipelineController* controller, int& category, int& row)
{
    for(int c = 0; c < controller->modifierCategoryCount(); c++) {
        for(int r = 0; r < controller->modifierCount(c); r++) {
            const QString commandId = controller->modifierCommandId(c, r);
            if(commandId.isEmpty()) {
                reportVerificationFailure(QStringLiteral("the modifier library entry %1/%2 has no command").arg(c).arg(r));
                continue;
            }
            // The enabled state of the command is what "this modifier applies to the selected pipeline" means, and the
            // command is the shared one the classic frontend's modifier library triggers as well.
            if(Command* command = state->ui->actionManager()->findCommand(commandId); command && command->isEnabled()) {
                category = c;
                row = r;
                return true;
            }
        }
    }
    return false;
}

/// Triggers the insert command of the first enabled library entry.
///
/// The insertion is asynchronous: the shared model rebuilds its rows on a timer and selects the object the command asked
/// for, so the caller reads the resulting row and selection after settling instead of right here.
bool insertModifier(const StatePtr& state, QmlPipelineController* controller, const QString& what)
{
    int category = -1;
    int row = -1;
    if(!findEnabledLibraryEntry(state, controller, category, row)) {
        reportVerificationFailure(QStringLiteral("no enabled modifier library entry was found, so no %1 could be inserted").arg(what));
        return false;
    }
    if(!controller->insertModifier(category, row)) {
        reportVerificationFailure(QStringLiteral("inserting a modifier through the library was refused although its command is enabled"));
        return false;
    }
    return true;
}

/// Names an item type for the log, so that the record of a run says which kind of row the check looked at.
const char* itemTypeName(int itemType)
{
    switch(itemType) {
    case PipelineListItem::DeletedObject: return "deleted object";
    case PipelineListItem::DeletedVisualElement: return "deleted visual element";
    case PipelineListItem::VisualElement: return "visual element";
    case PipelineListItem::Modifier: return "modifier";
    case PipelineListItem::DataSource: return "data source";
    case PipelineListItem::ModifierGroup: return "modifier group";
    case PipelineListItem::VisualElementsHeader: return "visual elements header";
    case PipelineListItem::ModificationsHeader: return "modifications header";
    case PipelineListItem::DataSourceHeader: return "data source header";
    case PipelineListItem::PipelineBranch: return "pipeline branch";
    default: return "unknown";
    }
}

/// Reports whether an ID is a well-formed object ID of the automation contract.
bool isContractId(const QString& id)
{
    return AutomationObjectId::parse(id).has_value();
}

// ----------------------------------------------------------------------------------------------------------------
// The phases, in the order in which they run.
// ----------------------------------------------------------------------------------------------------------------

void checkRolesAndIdentity(const StatePtr& state);
void checkInsertion(const StatePtr& state);
void checkUndoneInsertionAndReordering(const StatePtr& state);
void checkDeletionAndSelection(const StatePtr& state);
void checkEnablement(const StatePtr& state);
void checkDataSetReplacement(const StatePtr& state);
void checkCommandList(const StatePtr& state);

/// Verifies what a QML view reads from the model and what identity a row carries.
void checkRolesAndIdentity(const StatePtr& state)
{
    GuiTaskScope taskScope(*state->ui);
    QmlPipelineController* controller = pipelineController(state);
    if(!controller) {
        state->continuation();
        return;
    }
    QAbstractItemModel* model = controller->model();

    // A QML delegate can only read roles that have a name; the folded state of a modifier group is one of them since
    // this slice, because the role existed in C++ but not in roleNames().
    const QList<QByteArray> roleNames = model->roleNames().values();
    for(const char* name : { "title", "type", "ischecked", "iscollapsed", "decoration", "tooltip", "statusinfo" }) {
        if(!roleNames.contains(QByteArray(name)))
            reportVerificationFailure(QStringLiteral("the pipeline model does not offer the \"%1\" role to QML").arg(QLatin1String(name)));
    }

    if(model->rowCount() < 2) {
        reportVerificationFailure(QStringLiteral("the pipeline model shows %1 rows, which is too few to verify").arg(model->rowCount()));
        state->continuation();
        return;
    }

    int objectRows = 0;
    int idRows = 0;
    for(int row = 0; row < model->rowCount(); row++) {
        const QModelIndex index = model->index(row, 0);
        const int type = model->data(index, PipelineListModel::ItemTypeRole).toInt();
        const QString title = model->data(index, PipelineListModel::TitleRole).toString();
        const QVariant decoration = model->data(index, PipelineListModel::DecorationRole);

        // The icon role must be a name or a path: a QIcon is a value QML cannot display (the role has a second,
        // widgets-only branch for that).
        if(decoration.isValid() && !decoration.canConvert<QString>())
            reportVerificationFailure(QStringLiteral("the decoration of row %1 is a %2, which a QML delegate cannot display")
                .arg(row).arg(QLatin1String(decoration.typeName())));

        if(type >= PipelineListItem::VisualElementsHeader)
            continue;   // A header row stands for no object of its own.
        objectRows++;
        if(title.isEmpty())
            reportVerificationFailure(QStringLiteral("the object row %1 has no title").arg(row));

        const QString objectId = controller->objectIdAt(row);
        qInfo() << "PIPELINE_TEST row" << row << ":" << itemTypeName(type) << title
                << (objectId.isEmpty() ? QStringLiteral("(no object ID)") : objectId);
        if(objectId.isEmpty()) {
            // A visual element and a data source are not part of the contract's identity vocabulary (audit D55), so such a
            // row is addressed by row index within one synchronous call; the phase that first needs to address one from
            // outside the frontend extends the vocabulary (an additive contract change). A modification node *is* part of
            // it, so a modifier row without an ID is a defect.
            if(type != PipelineListItem::DataSource && type != PipelineListItem::VisualElement)
                reportVerificationFailure(QStringLiteral("the %1 row \"%2\" has no object ID").arg(QLatin1String(itemTypeName(type)), title));
            continue;
        }
        idRows++;
        // A modification-node row has to be named the way the contract names modification nodes.
        if(type == PipelineListItem::Modifier && !objectId.startsWith(QStringLiteral("modifier:")))
            reportVerificationFailure(QStringLiteral("the modifier row %1 is named \"%2\"").arg(row).arg(objectId));
        if(!isContractId(objectId))
            reportVerificationFailure(QStringLiteral("the row %1 carries \"%2\", which is not an object ID of the contract").arg(row).arg(objectId));
        if(controller->rowForObjectId(objectId) != row)
            reportVerificationFailure(QStringLiteral("the ID \"%1\" of row %2 resolves to another row").arg(objectId, QString::number(row)));
    }
    if(objectRows == 0)
        reportVerificationFailure(QStringLiteral("the pipeline model shows no object rows (no visual element and no data source)"));
    if(controller->rowForObjectId(QStringLiteral("modifier:m123456")) != -1)
        reportVerificationFailure(QStringLiteral("an object ID that was never issued resolves to a row"));

    qInfo() << "PIPELINE_TEST roles:" << model->rowCount() << "rows," << objectRows << "object rows," << idRows << "with a contract ID";

    reportCheckPhase("roles and identity");
    afterSettle(state, checkInsertion);
}

/// Verifies that a modifier inserted through the library appears, is named and is selected.
void checkInsertion(const StatePtr& state)
{
    GuiTaskScope taskScope(*state->ui);
    QmlPipelineController* controller = pipelineController(state);
    if(!controller) {
        state->continuation();
        return;
    }

    state->modifiersBefore = modifierRowCount(controller);
    if(!insertModifier(state, controller, QStringLiteral("modifier"))) {
        state->continuation();
        return;
    }

    // The library command asks the model to select what it inserted, but only once the rows have been rebuilt.
    afterSettle(state, [](const StatePtr& state) {
        GuiTaskScope taskScope(*state->ui);
        QmlPipelineController* controller = pipelineController(state);
        if(!controller) {
            state->continuation();
            return;
        }
        if(modifierRowCount(controller) != state->modifiersBefore + 1) {
            reportVerificationFailure(QStringLiteral("the pipeline model shows %1 modifier rows after the insertion instead of %2")
                .arg(modifierRowCount(controller)).arg(state->modifiersBefore + 1));
            state->continuation();
            return;
        }
        state->objectId = controller->selectedObjectId();
        const int row = rowOfModifier(controller, state->objectId);
        if(!state->objectId.startsWith(QStringLiteral("modifier:")) || row < 0) {
            reportVerificationFailure(QStringLiteral("the inserted modifier is selected as \"%1\" and has row %2")
                .arg(state->objectId.isEmpty() ? QStringLiteral("<nothing>") : state->objectId).arg(row));
            state->continuation();
            return;
        }
        qInfo() << "PIPELINE_TEST insertion:" << state->objectId << "in row" << row << "of" << controller->model()->rowCount();

        // The insertion is one undo step, and the undo takes the row and the validity of the ID away with it.
        state->ui->undoStack()->undo();
        reportCheckPhase("insertion");
        afterSettle(state, checkUndoneInsertionAndReordering);
    });
}

/// Verifies that an undone insertion ends the validity of the ID, that a stale write is refused, that redo brings the
/// object back under the same ID, and that reordering works with undo.
void checkUndoneInsertionAndReordering(const StatePtr& state)
{
    GuiTaskScope taskScope(*state->ui);
    QmlPipelineController* controller = pipelineController(state);
    if(!controller) {
        state->continuation();
        return;
    }

    if(modifierRowCount(controller) != state->modifiersBefore)
        reportVerificationFailure(QStringLiteral("undoing the insertion left %1 modifier rows instead of %2")
            .arg(modifierRowCount(controller)).arg(state->modifiersBefore));
    if(controller->rowForObjectId(state->objectId) != -1)
        reportVerificationFailure(QStringLiteral("the ID \"%1\" of the undone insertion still resolves to a row").arg(state->objectId));
    // A deferred caller that still holds the ID of the removed object has to be refused, not redirected to whatever
    // holds the row now.
    if(controller->moveObjectUp(state->objectId))
        reportVerificationFailure(QStringLiteral("a write through the ID of a removed object was accepted"));
    if(controller->setCheckedForObject(state->objectId, true))
        reportVerificationFailure(QStringLiteral("enabling the object of a removed ID was accepted"));

    // Redo brings the object back under the same ID, because an identity belongs to the session and not to the row list.
    state->ui->undoStack()->redo();
    afterSettle(state, [](const StatePtr& state) {
        GuiTaskScope taskScope(*state->ui);
        QmlPipelineController* controller = pipelineController(state);
        if(!controller) {
            state->continuation();
            return;
        }
        if(rowOfModifier(controller, state->objectId) < 0) {
            reportVerificationFailure(QStringLiteral("the redone insertion did not bring the modifier \"%1\" back").arg(state->objectId));
            state->continuation();
            return;
        }
        if(modifierRowCount(controller) != state->modifiersBefore + 1)
            reportVerificationFailure(QStringLiteral("the redone insertion shows %1 modifier rows instead of %2")
                .arg(modifierRowCount(controller)).arg(state->modifiersBefore + 1));

        if(!insertModifier(state, controller, QStringLiteral("second modifier"))) {
            state->continuation();
            return;
        }

        afterSettle(state, [](const StatePtr& state) {
            GuiTaskScope taskScope(*state->ui);
            QmlPipelineController* controller = pipelineController(state);
            if(!controller) {
                state->continuation();
                return;
            }
            state->secondObjectId = controller->selectedObjectId();
            if(!state->secondObjectId.startsWith(QStringLiteral("modifier:"))) {
                reportVerificationFailure(QStringLiteral("the second insertion selected \"%1\" instead of a modifier")
                    .arg(state->secondObjectId.isEmpty() ? QStringLiteral("<nothing>") : state->secondObjectId));
                state->continuation();
                return;
            }
            const int rowBefore = rowOfModifier(controller, state->secondObjectId);
            const int otherRowBefore = rowOfModifier(controller, state->objectId);
            if(rowBefore <= 0 || otherRowBefore < 0) {
                reportVerificationFailure(QStringLiteral("the two modifiers are not in the rows the reordering needs (%1 and %2)")
                    .arg(rowBefore).arg(otherRowBefore));
                state->continuation();
                return;
            }
            if(!controller->moveObjectUp(state->secondObjectId)) {
                reportVerificationFailure(QStringLiteral("moving the modifier \"%1\" up was refused").arg(state->secondObjectId));
                state->continuation();
                return;
            }

            afterSettle(state, [](const StatePtr& state) {
                GuiTaskScope taskScope(*state->ui);
                QmlPipelineController* controller = pipelineController(state);
                if(!controller) {
                    state->continuation();
                    return;
                }
                const int rowAfter = rowOfModifier(controller, state->secondObjectId);
                const int otherRowAfter = rowOfModifier(controller, state->objectId);
                if(rowAfter < 0 || otherRowAfter < 0 || rowAfter >= otherRowAfter)
                    reportVerificationFailure(QStringLiteral("moving up did not exchange the two modifiers (rows %1 and %2)").arg(rowAfter).arg(otherRowAfter));
                if(controller->selectedObjectId() != state->secondObjectId)
                    reportVerificationFailure(QStringLiteral("moving a modifier changed the selection to \"%1\"").arg(controller->selectedObjectId()));
                qInfo() << "PIPELINE_TEST reordering: the two modifiers are rows" << rowAfter << "and" << otherRowAfter;

                state->ui->undoStack()->undo();
                reportCheckPhase("reordering");
                afterSettle(state, checkDeletionAndSelection);
            });
        });
    });
}

/// Verifies deletion, the coherence of the selection afterwards, and that undoing the deletion restores object and
/// selection.
void checkDeletionAndSelection(const StatePtr& state)
{
    GuiTaskScope taskScope(*state->ui);
    QmlPipelineController* controller = pipelineController(state);
    if(!controller) {
        state->continuation();
        return;
    }
    if(modifierRowCount(controller) != state->modifiersBefore + 2)
        reportVerificationFailure(QStringLiteral("undoing the reordering left %1 modifier rows instead of %2")
            .arg(modifierRowCount(controller)).arg(state->modifiersBefore + 2));

    // Delete the *selected* modifier. The panel has to end up with a selection that exists, and undoing the deletion has
    // to bring back the object *and* the selection it carried (UI_DESIGN.md section 5.3).
    if(!controller->selectObjectId(state->secondObjectId)) {
        reportVerificationFailure(QStringLiteral("the modifier \"%1\" could not be selected").arg(state->secondObjectId));
        state->continuation();
        return;
    }
    if(!controller->deleteSelectedObjects()) {
        reportVerificationFailure(QStringLiteral("deleting the selected modifier was refused"));
        state->continuation();
        return;
    }

    afterSettle(state, [](const StatePtr& state) {
        GuiTaskScope taskScope(*state->ui);
        QmlPipelineController* controller = pipelineController(state);
        if(!controller) {
            state->continuation();
            return;
        }
        if(rowOfModifier(controller, state->secondObjectId) >= 0)
            reportVerificationFailure(QStringLiteral("the deleted modifier \"%1\" still has a row").arg(state->secondObjectId));
        if(controller->rowForObjectId(state->secondObjectId) != -1)
            reportVerificationFailure(QStringLiteral("the ID of the deleted modifier still resolves"));
        for(const QVariant& id : controller->selectedObjectIds()) {
            if(controller->rowForObjectId(id.toString()) < 0)
                reportVerificationFailure(QStringLiteral("the selection holds \"%1\", which has no row").arg(id.toString()));
        }

        state->ui->undoStack()->undo();
        afterSettle(state, [](const StatePtr& state) {
            GuiTaskScope taskScope(*state->ui);
            QmlPipelineController* controller = pipelineController(state);
            if(!controller) {
                state->continuation();
                return;
            }
            const int restoredRow = rowOfModifier(controller, state->secondObjectId);
            if(restoredRow < 0) {
                reportVerificationFailure(QStringLiteral("undoing the deletion did not bring the modifier \"%1\" back").arg(state->secondObjectId));
                state->continuation();
                return;
            }
            if(controller->selectedObjectId() != state->secondObjectId)
                reportVerificationFailure(QStringLiteral("undoing the deletion restored the modifier but selected \"%1\"").arg(controller->selectedObjectId()));

            qInfo() << "PIPELINE_TEST deletion: the deletion and its undo restored" << state->secondObjectId << "in row" << restoredRow;

            reportCheckPhase("deletion and undo");
            afterSettle(state, checkEnablement);
        });
    });
}

/// Verifies that enabling and disabling a modifier is one undo step.
void checkEnablement(const StatePtr& state)
{
    GuiTaskScope taskScope(*state->ui);
    QmlPipelineController* controller = pipelineController(state);
    if(!controller) {
        state->continuation();
        return;
    }
    const int row = rowOfModifier(controller, state->objectId);
    if(row < 0) {
        reportVerificationFailure(QStringLiteral("the modifier \"%1\" has no row for the enablement check").arg(state->objectId));
        state->continuation();
        return;
    }
    if(!controller->isCheckedAt(row))
        reportVerificationFailure(QStringLiteral("the modifier \"%1\" is not enabled before the check").arg(state->objectId));
    if(!controller->setCheckedForObject(state->objectId, false)) {
        reportVerificationFailure(QStringLiteral("disabling the modifier \"%1\" was refused").arg(state->objectId));
        state->continuation();
        return;
    }

    afterSettle(state, [](const StatePtr& state) {
        GuiTaskScope taskScope(*state->ui);
        QmlPipelineController* controller = pipelineController(state);
        if(!controller) {
            state->continuation();
            return;
        }
        const int row = rowOfModifier(controller, state->objectId);
        if(row < 0) {
            reportVerificationFailure(QStringLiteral("the modifier \"%1\" lost its row while being disabled").arg(state->objectId));
            state->continuation();
            return;
        }
        if(controller->isCheckedAt(row))
            reportVerificationFailure(QStringLiteral("the modifier \"%1\" is still enabled after disabling it").arg(state->objectId));

        state->ui->undoStack()->undo();
        afterSettle(state, [](const StatePtr& state) {
            GuiTaskScope taskScope(*state->ui);
            QmlPipelineController* controller = pipelineController(state);
            if(!controller) {
                state->continuation();
                return;
            }
            const int row = rowOfModifier(controller, state->objectId);
            if(row < 0 || !controller->isCheckedAt(row))
                reportVerificationFailure(QStringLiteral("undoing the disablement did not enable the modifier \"%1\" again").arg(state->objectId));
            qInfo() << "PIPELINE_TEST enablement: disabling and undoing it left the modifier enabled";

            reportCheckPhase("enablement");
            afterSettle(state, checkDataSetReplacement);
        });
    });
}

/// Verifies that replacing the data set ends the validity of every identity and refuses stale writes.
void checkDataSetReplacement(const StatePtr& state)
{
    GuiTaskScope taskScope(*state->ui);
    QmlPipelineController* controller = pipelineController(state);
    AutomationSession* session = state->ui->automationSession();
    if(!controller || !session) {
        reportVerificationFailure(QStringLiteral("the workbench has no automation session, so the identity of an object cannot be verified"));
        state->continuation();
        return;
    }
    if(controller->rowForObjectId(state->objectId) < 0) {
        reportVerificationFailure(QStringLiteral("the modifier \"%1\" is gone before the data set is replaced").arg(state->objectId));
        state->continuation();
        return;
    }
    state->revisionBefore = session->revision();

    // Open a data set that has nothing to do with the one the edits were made in, exactly as loading a session does.
    state->ui->datasetContainer().setCurrentSet(OORef<DataSet>::create());

    afterSettle(state, [](const StatePtr& state) {
        GuiTaskScope taskScope(*state->ui);
        QmlPipelineController* controller = pipelineController(state);
        AutomationSession* session = state->ui->automationSession();
        if(!controller || !session) {
            state->continuation();
            return;
        }
        if(session->revision() <= state->revisionBefore)
            reportVerificationFailure(QStringLiteral("replacing the data set did not advance the session revision"));
        const OORef<OvitoObject> resolved = session->objects().resolve(state->objectId);
        if(resolved)
            reportVerificationFailure(QStringLiteral("the ID \"%1\" still resolves after the data set was replaced").arg(state->objectId));
        if(controller->rowForObjectId(state->objectId) != -1)
            reportVerificationFailure(QStringLiteral("the ID \"%1\" still names a row after the data set was replaced").arg(state->objectId));
        if(controller->moveObjectUp(state->objectId))
            reportVerificationFailure(QStringLiteral("a write through a pre-replacement ID was accepted"));
        if(controller->setCheckedForObject(state->objectId, true))
            reportVerificationFailure(QStringLiteral("enabling the object of a pre-replacement ID was accepted"));
        if(!controller->selectedObjectIds().empty())
            reportVerificationFailure(QStringLiteral("the panel still has a selection after the data set was replaced"));

        qInfo() << "PIPELINE_TEST data set replacement: revision" << session->revision() << "and the old IDs resolve to nothing";

        reportCheckPhase("data set replacement");
        checkCommandList(state);
    });
}

/// Verifies the command list model that a menu search or the command palette presents.
void checkCommandList(const StatePtr& state)
{
    GuiTaskScope taskScope(*state->ui);
    CommandListModel* list = state->ui->commandList();
    ActionManager* manager = state->ui->actionManager();
    if(!list || !manager) {
        reportVerificationFailure(QStringLiteral("the frontend offers no command list model"));
        state->continuation();
        return;
    }

    // Every listed row presents a command, the rows are ordered by their text, and an invisible command is never listed.
    if(list->rowCount() == 0)
        reportVerificationFailure(QStringLiteral("the command list is empty, so nothing was verified"));
    QString previousText;
    for(int row = 0; row < list->rowCount(); row++) {
        Command* command = list->command(row);
        if(!command || command->id().isEmpty()) {
            reportVerificationFailure(QStringLiteral("the command list row %1 holds no command").arg(row));
            continue;
        }
        if(!command->isVisible())
            reportVerificationFailure(QStringLiteral("the command list lists the invisible command \"%1\"").arg(command->id()));
        const QString text = list->index(row, 0).data(Qt::DisplayRole).toString();
        if(!previousText.isEmpty() && QString::localeAwareCompare(previousText, text) > 0)
            reportVerificationFailure(QStringLiteral("the command list is not sorted: \"%1\" comes after \"%2\"").arg(text, previousText));
        previousText = text;
    }

    // The filter matches the text and the status tip of a command, and a filter that matches nothing empties the list.
    const int rowsUnfiltered = list->rowCount();
    list->setFilter(QStringLiteral("undo"));
    bool foundUndo = false;
    for(int row = 0; row < list->rowCount(); row++) {
        if(Command* command = list->command(row))
            foundUndo = foundUndo || command->id() == QStringLiteral("EditUndo");
    }
    if(!foundUndo)
        reportVerificationFailure(QStringLiteral("the command list filter \"undo\" does not keep the undo command"));
    list->setFilter(QStringLiteral("no-such-command-exists"));
    if(list->rowCount() != 0)
        reportVerificationFailure(QStringLiteral("a filter that matches nothing still lists %1 commands").arg(list->rowCount()));

    // A command of the check's own verifies the rules a view depends on: an invisible command is not listed, and a
    // disabled one cannot be triggered.
    list->setFilter(QString());
    Command* probe = manager->createCommand(QStringLiteral("SpikePipelineCheck.Probe"), QStringLiteral("Pipeline check probe"));
    if(!probe) {
        reportVerificationFailure(QStringLiteral("the action manager refused the command of the check"));
        state->continuation();
        return;
    }
    const auto rowOfProbe = [list, probe]() {
        for(int row = 0; row < list->rowCount(); row++) {
            if(list->command(row) == probe)
                return row;
        }
        return -1;
    };
    probe->setVisible(false);
    list->refresh();
    if(rowOfProbe() >= 0)
        reportVerificationFailure(QStringLiteral("an invisible command is listed"));
    probe->setVisible(true);
    probe->setEnabled(false);
    list->refresh();
    const int probeRow = rowOfProbe();
    if(probeRow < 0)
        reportVerificationFailure(QStringLiteral("a visible command is not listed"));
    else if(list->triggerAt(probeRow))
        reportVerificationFailure(QStringLiteral("triggerAt() invoked a disabled command"));
    manager->deleteCommand(probe);

    qInfo() << "PIPELINE_TEST command list:" << rowsUnfiltered << "commands, filtering, visibility and enablement verified";

    reportCheckPhase("command list");
    state->continuation();
}

}   // namespace

void runPipelineTest(QmlMainWindowUI* ui, std::function<void()> continuation)
{
    // The phases this check walks through, in the order the check functions hand over to each other.
    declareCheckPhases({ "import", "roles and identity", "insertion", "reordering", "deletion and undo", "enablement",
                         "data set replacement", "command list" });

    auto state = std::make_shared<PipelineCheckState>();
    state->ui = ui;
    state->continuation = std::move(continuation);

    if(!ui->pipelineController() || !ui->automationSession()) {
        reportVerificationFailure(QStringLiteral("the frontend offers no pipeline controller or no automation session, so the check cannot run"));
        state->continuation();
        return;
    }

    // The scene the earlier checks left behind may hold no data at all, so the check imports a lattice of its own and
    // waits until the file source has evaluated it.
    const QString dataFile = writeLatticeFile(QDir::tempPath() + QStringLiteral("/ovito-qml-pipeline-check.xyz"), 6, 3.6);
    if(dataFile.isEmpty()) {
        state->continuation();
        return;
    }
    if(QmlWorkbenchController* controller = ui->workbenchController())
        controller->importFiles({QUrl::fromLocalFile(dataFile)});
    else {
        reportVerificationFailure(QStringLiteral("the workbench has no controller that could import the data file"));
        state->continuation();
        return;
    }

    pollUntil(ui, 50, 20000,
        [ui]() {
            const FileSource* fileSource = firstFileSource(ui);
            return fileSource && fileSource->numberOfSourceFrames() >= 1;
        },
        [state](bool ready) {
            if(!ready) {
                reportVerificationFailure(QStringLiteral("the data file of the pipeline check was not imported"));
                state->continuation();
                return;
            }
            reportCheckPhase("import");
            scheduleDelayed(state->ui, 600, [state]() { checkRolesAndIdentity(state); });
        });
}

}   // namespace Ovito::Spike
