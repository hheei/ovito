// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/desktop/properties/ModifierGroupEditor.h>
#include <ovito/core/dataset/pipeline/ModifierGroup.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(ModifierGroupEditor);
SET_OVITO_OBJECT_EDITOR(ModifierGroup, ModifierGroupEditor);

/******************************************************************************
* Sets up the UI widgets of the editor.
******************************************************************************/
void ModifierGroupEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    _rolloutParams = rolloutParams;
    connect(this, &PropertiesEditor::contentsReplaced, this, &ModifierGroupEditor::updateSubEditors);
}

/******************************************************************************
* This method is called when a reference target changes.
******************************************************************************/
bool ModifierGroupEditor::referenceEvent(RefTarget* source, const ReferenceEvent& event)
{
    if(source == editObject() && event.type() == ModifierGroup::ModifierAddedOrRemoved) {
        updateSubEditors();
    }
    return PropertiesEditor::referenceEvent(source, event);
}

/******************************************************************************
* Rebuilds the list of sub-editors for the current edit object.
******************************************************************************/
void ModifierGroupEditor::updateSubEditors()
{
    handleExceptions([&] {
        auto subEditorIter = _subEditors.begin();
        if(ModifierGroup* group = static_object_cast<ModifierGroup>(editObject())) {
            // Get the group's modifier nodes.
            QVector<ModificationNode*> nodes = group->nodes();
            for(ModificationNode* node : nodes) {
                // Open editor for this sub-object.
                if(subEditorIter != _subEditors.end() && (*subEditorIter)->editObject() != nullptr
                        && (*subEditorIter)->editObject()->getOOClass() == node->getOOClass()) {
                    // Re-use existing editor.
                    (*subEditorIter)->setEditObject(node);
                    ++subEditorIter;
                }
                else {
                    // Create a new sub-editor for this sub-object.
                    OORef<PropertiesEditor> editor = PropertiesEditor::create(ui(), node);
                    if(editor) {
                        editor->initialize(container(), _rolloutParams, this);
                        editor->setEditObject(node);
                        _subEditors.erase(subEditorIter, _subEditors.end());
                        _subEditors.push_back(std::move(editor));
                        subEditorIter = _subEditors.end();
                    }
                }
            }
        }
        // Close excess sub-editors.
        _subEditors.erase(subEditorIter, _subEditors.end());
    });
}

}   // End of namespace
