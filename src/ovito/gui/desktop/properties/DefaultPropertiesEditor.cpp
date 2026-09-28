// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/desktop/properties/DefaultPropertiesEditor.h>

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(DefaultPropertiesEditor);
SET_OVITO_OBJECT_EDITOR(RefTarget, DefaultPropertiesEditor);

/******************************************************************************
* Sets up the UI widgets of the editor.
******************************************************************************/
void DefaultPropertiesEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    _rolloutParams = rolloutParams;
}

/******************************************************************************
* Is called when the value of a reference field of this RefMaker changes.
******************************************************************************/
void DefaultPropertiesEditor::referenceReplaced(const PropertyFieldDescriptor* field, RefTarget* oldTarget, RefTarget* newTarget, int listIndex)
{
    PropertiesEditor::referenceReplaced(field, oldTarget, newTarget, listIndex);
    if(field == PROPERTY_FIELD(editObject)) {
        updateSubEditors();
    }
}

/******************************************************************************
* This method is called when a reference target changes.
******************************************************************************/
bool DefaultPropertiesEditor::referenceEvent(RefTarget* source, const ReferenceEvent& event)
{
    if(event.type() == ReferenceEvent::ReferenceChanged || event.type() == ReferenceEvent::ReferenceAdded || event.type() == ReferenceEvent::ReferenceRemoved) {
        updateSubEditors();
    }
    return PropertiesEditor::referenceEvent(source, event);
}

/******************************************************************************
* Rebuilds the list of sub-editors for the current edit object.
******************************************************************************/
void DefaultPropertiesEditor::updateSubEditors()
{
    handleExceptions([&] {
        auto subEditorIter = _subEditors.begin();
        if(editObject()) {
            // Automatically open sub-editors for reference fields that have the PROPERTY_FIELD_OPEN_SUBEDITOR flag.
            for(auto fieldIter = editObject()->getOOMetaClass().propertyFields().crbegin(); fieldIter != editObject()->getOOMetaClass().propertyFields().crend(); ++fieldIter) {
                const PropertyFieldDescriptor* field = *fieldIter;
                if(!field->isReferenceField()) continue;
                if(!field->flags().testFlag(PROPERTY_FIELD_OPEN_SUBEDITOR)) continue;
                if(!field->isVector()) {
                    if(RefTarget* subobject = editObject()->getReferenceFieldTarget(field)) {
                        // Open editor for this sub-object.
                        if(subEditorIter != _subEditors.end() && (*subEditorIter)->editObject() != nullptr
                                && (*subEditorIter)->editObject()->getOOClass() == subobject->getOOClass()) {
                            // Re-use existing editor.
                            (*subEditorIter)->setEditObject(subobject);
                            ++subEditorIter;
                        }
                        else {
                            // Create a new sub-editor for this sub-object.
                            OORef<PropertiesEditor> editor = PropertiesEditor::create(ui(), subobject);
                            if(editor) {
                                editor->initialize(container(), _rolloutParams, this);
                                editor->setEditObject(subobject);
                                _subEditors.erase(subEditorIter, _subEditors.end());
                                _subEditors.push_back(std::move(editor));
                                subEditorIter = _subEditors.end();
                            }
                        }
                    }
                }
                else {
                    int count = editObject()->getVectorReferenceFieldSize(field);
                    for(int i = 0; i < count; i++) {
                        if(RefTarget* subobject = editObject()->getVectorReferenceFieldTarget(field, i)) {
                            // Open editor for this sub-object.
                            if(subEditorIter != _subEditors.end() && (*subEditorIter)->editObject() != nullptr
                                    && (*subEditorIter)->editObject()->getOOClass() == subobject->getOOClass()) {
                                // Re-use existing editor.
                                (*subEditorIter)->setEditObject(subobject);
                                ++subEditorIter;
                            }
                            else {
                                // Create a new sub-editor for this sub-object.
                                OORef<PropertiesEditor> editor = PropertiesEditor::create(ui(), subobject);
                                if(editor) {
                                    editor->initialize(container(), _rolloutParams, this);
                                    editor->setEditObject(subobject);
                                    _subEditors.erase(subEditorIter, _subEditors.end());
                                    _subEditors.push_back(std::move(editor));
                                    subEditorIter = _subEditors.end();
                                }
                            }
                        }
                    }
                }
            }
        }
        // Close excess sub-editors.
        _subEditors.erase(subEditorIter, _subEditors.end());
    });
}

}   // End of namespace
