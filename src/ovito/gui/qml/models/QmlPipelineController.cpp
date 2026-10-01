// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/gui/qml/QmlFrontend.h>
#include <ovito/gui/qml/models/QmlPipelineController.h>
#include <ovito/gui/base/app/WorkbenchUI.h>
#include <ovito/core/automation/AutomationSession.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <ovito/core/dataset/scene/Pipeline.h>
#include <ovito/core/dataset/scene/SceneNode.h>
#include <ovito/core/viewport/Viewport.h>
#include <QItemSelectionModel>
#include "QmlPipelineController.h"

namespace Ovito {

/******************************************************************************
* Constructor.
******************************************************************************/
QmlPipelineController::QmlPipelineController(WorkbenchUI& ui, QObject* parent) : QObject(parent), _ui(ui)
{
    // The two models of the pipeline panel. Their constructors register the commands of the panel with the action
    // manager, which is why a workbench has exactly one instance of each (see the class documentation).
    _pipelineModel = new PipelineListModel(ui, this);
    _modifierLibrary = new AvailableModifiersModel(this, ui, _pipelineModel);

    // The selection of the panel is the model's; a QML view cannot read a QItemSelectionModel, so its changes are
    // announced as one signal that a binding can follow.
    connect(_pipelineModel, &PipelineListModel::selectedItemChanged, this, &QmlPipelineController::selectionChanged);
}

/******************************************************************************
* Returns the ID of the selected object.
******************************************************************************/
QString QmlPipelineController::selectedObjectId() const
{
    if(PipelineListItem* item = _pipelineModel->selectedItem())
        return idOfObject(item->object());
    return {};
}

/******************************************************************************
* Returns the IDs of the selected objects.
******************************************************************************/
QVariantList QmlPipelineController::selectedObjectIds() const
{
    QVariantList ids;
    for(PipelineListItem* item : _pipelineModel->selectedItems()) {
        const QString id = idOfObject(item->object());
        if(!id.isEmpty())
            ids.push_back(id);
    }
    return ids;
}

/******************************************************************************
* Returns the title of the selected row.
******************************************************************************/
QString QmlPipelineController::selectedTitle() const
{
    if(PipelineListItem* item = _pipelineModel->selectedItem())
        return item->title();
    return {};
}

/******************************************************************************
* Returns the stable ID of the object of a row.
******************************************************************************/
QString QmlPipelineController::objectIdAt(int row) const
{
    PipelineListItem* item = itemAt(row);
    return item ? idOfObject(item->object()) : QString();
}

/******************************************************************************
* Returns the row that currently shows the object of an ID.
******************************************************************************/
int QmlPipelineController::rowForObjectId(const QString& objectId) const
{
    return rowOfItem(itemForObjectId(objectId));
}

/******************************************************************************
* Returns the title of a row.
******************************************************************************/
QString QmlPipelineController::titleAt(int row) const
{
    PipelineListItem* item = itemAt(row);
    return item ? item->title() : QString();
}

/******************************************************************************
* Returns whether the object of a row is enabled.
******************************************************************************/
bool QmlPipelineController::isCheckedAt(int row) const
{
    if(itemAt(row) == nullptr)
        return false;
    return _pipelineModel->data(_pipelineModel->index(row), PipelineListModel::CheckedRole).toBool();
}

/******************************************************************************
* Selects a row.
******************************************************************************/
void QmlPipelineController::selectRow(int row, bool toggle)
{
    if(itemAt(row) == nullptr)
        return;
    _pipelineModel->selectionModel()->select(_pipelineModel->index(row),
        toggle ? QItemSelectionModel::Toggle : QItemSelectionModel::ClearAndSelect);
}

/******************************************************************************
* Selects the row that shows the object of an ID.
******************************************************************************/
bool QmlPipelineController::selectObjectId(const QString& objectId, bool toggle)
{
    const int row = rowForObjectId(objectId);
    if(row < 0)
        return false;
    selectRow(row, toggle);
    return true;
}

/******************************************************************************
* Clears the selection of the panel.
******************************************************************************/
void QmlPipelineController::clearSelection()
{
    _pipelineModel->selectionModel()->clearSelection();
}

/******************************************************************************
* Enables or disables the object of a row.
******************************************************************************/
bool QmlPipelineController::setCheckedAt(int row, bool checked)
{
    if(itemAt(row) == nullptr)
        return false;
    return _pipelineModel->setData(_pipelineModel->index(row), checked, PipelineListModel::CheckedRole);
}

/******************************************************************************
* Enables or disables the object of an ID.
******************************************************************************/
bool QmlPipelineController::setCheckedForObject(const QString& objectId, bool checked)
{
    return setCheckedAt(rowForObjectId(objectId), checked);
}

/******************************************************************************
* Moves the object of an ID one position up in the pipeline.
******************************************************************************/
bool QmlPipelineController::moveObjectUp(const QString& objectId)
{
    PipelineListItem* item = itemForObjectId(objectId);
    if(!item)
        return false;
    _pipelineModel->moveItemUp(item);
    return true;
}

/******************************************************************************
* Moves the object of an ID one position down in the pipeline.
******************************************************************************/
bool QmlPipelineController::moveObjectDown(const QString& objectId)
{
    PipelineListItem* item = itemForObjectId(objectId);
    if(!item)
        return false;
    _pipelineModel->moveItemDown(item);
    return true;
}

/******************************************************************************
* Removes the object of an ID from the pipeline.
******************************************************************************/
bool QmlPipelineController::deleteObject(const QString& objectId)
{
    PipelineListItem* item = itemForObjectId(objectId);
    if(!item)
        return false;
    _pipelineModel->deleteItems({item});
    return true;
}

/******************************************************************************
* Removes the selected objects from the pipeline.
******************************************************************************/
bool QmlPipelineController::deleteSelectedObjects()
{
    const QVector<PipelineListItem*> items = _pipelineModel->selectedItems();
    if(items.empty())
        return false;
    _pipelineModel->deleteItems(items);
    return true;
}

/******************************************************************************
* Returns the number of categories of the modifier library.
******************************************************************************/
int QmlPipelineController::modifierCategoryCount() const
{
    return _modifierLibrary->rowCount();
}

/******************************************************************************
* Returns the name of a category of the modifier library.
******************************************************************************/
QString QmlPipelineController::modifierCategoryName(int category) const
{
    if(category < 0 || category >= _modifierLibrary->rowCount())
        return {};
    return _modifierLibrary->categoryName(category);
}

/******************************************************************************
* Returns the number of modifiers in a category.
******************************************************************************/
int QmlPipelineController::modifierCount(int category) const
{
    if(category < 0 || category >= _modifierLibrary->rowCount())
        return 0;
    return _modifierLibrary->rowCount(_modifierLibrary->index(category, 0));
}

/******************************************************************************
* Returns the id of the command that inserts a modifier of the library.
******************************************************************************/
QString QmlPipelineController::modifierCommandId(int category, int row) const
{
    if(Command* command = _modifierLibrary->commandAt(category, row))
        return command->id();
    return {};
}

/******************************************************************************
* Inserts a modifier of the library into the selected pipeline.
******************************************************************************/
bool QmlPipelineController::insertModifier(int category, int row)
{
    Command* command = _modifierLibrary->commandAt(category, row);
    if(!command)
        return false;
    // The command is the shared one the classic frontend's modifier library triggers as well, and its enabled state is
    // what "the modifier applies to the selected pipeline" means. Triggering it inserts the modifier and selects it.
    if(!command->isEnabled())
        return false;
    command->trigger();
    return true;
}

/******************************************************************************
* Returns the list item of a row, or null when there is no such row.
******************************************************************************/
PipelineListItem* QmlPipelineController::itemAt(int row) const
{
    if(row < 0 || row >= (int)_pipelineModel->items().size())
        return nullptr;
    return _pipelineModel->item(row);
}

/******************************************************************************
* Returns the list item that shows the object of an ID.
******************************************************************************/
PipelineListItem* QmlPipelineController::itemForObjectId(const QString& objectId) const
{
    AutomationSession* session = _ui.automationSession();
    if(!session || objectId.isEmpty())
        return nullptr;
    // Resolving through the session is what makes a write safe: an ID whose object is gone (deleted, or replaced by
    // another data set) resolves to nothing, and the caller is refused instead of writing to whoever holds the row now.
    OORef<OvitoObject> object = session->objects().resolve(objectId);
    if(!object)
        return nullptr;
    for(const OORef<PipelineListItem>& item : _pipelineModel->items()) {
        if(item->object() == object.get())
            return item.get();
    }
    return nullptr;
}

/******************************************************************************
* Returns the index of an item in the row list.
******************************************************************************/
int QmlPipelineController::rowOfItem(PipelineListItem* item) const
{
    if(!item)
        return -1;
    const std::vector<OORef<PipelineListItem>>& items = _pipelineModel->items();
    auto iter = std::find_if(items.begin(), items.end(), [item](const OORef<PipelineListItem>& candidate) { return candidate.get() == item; });
    if(iter == items.end())
        return -1;
    return (int)std::distance(items.begin(), iter);
}

/******************************************************************************
* Returns the ID of an object.
******************************************************************************/
QString QmlPipelineController::idOfObject(OvitoObject* object) const
{
    if(!object)
        return {};
    AutomationSession* session = _ui.automationSession();
    if(!session)
        return {};
    // A modification node is the only object of this list the automation contract can name: no row ever holds a
    // pipeline, a scene node or a viewport, and the rows that hold a visual element, a modifier group or the source
    // node are named by their row instead (audit decisions D55 and D62).
    if(dynamic_object_cast<ModificationNode>(object))
        return session->objects().idFor(object, AutomationObjectId::Kind::Modifier);
    return {};
}

}   // End of namespace Ovito
