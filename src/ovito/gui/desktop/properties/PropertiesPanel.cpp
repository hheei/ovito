// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/desktop/properties/PropertiesPanel.h>
#include <ovito/core/app/undo/UndoableOperation.h>

namespace Ovito {

/******************************************************************************
* Constructor.
******************************************************************************/
PropertiesPanel::PropertiesPanel(MainWindowUI& ui, QWidget* parent) :
    RolloutContainer(ui, parent)
{
}

/******************************************************************************
* Destructor.
******************************************************************************/
PropertiesPanel::~PropertiesPanel()
{
}

/******************************************************************************
* Sets the target object being edited in the panel.
******************************************************************************/
void PropertiesPanel::setEditObject(RefTarget* newEditObject, OORef<PropertiesEditor> newEditor)
{
    if(newEditObject == editObject() && (newEditObject != nullptr) == (editor() != nullptr) && !newEditor)
        return;

    if(editor()) {
        OVITO_CHECK_OBJECT_POINTER(editor());

        // Can we re-use the old editor?
        if(newEditObject != nullptr && editor()->editObject() != nullptr
            && editor()->editObject()->getOOClass() == newEditObject->getOOClass()
            && !newEditor) {

            editor()->handleExceptions<true>([&]() {
                editor()->setEditObject(newEditObject);
            });
            return;
        }
        else {
            // Close previous editor.
            _editor.reset();
        }
    }

    if(newEditObject) {
        // Open new properties editor.
        if(!handleExceptions<true>([&]() {
            _editor = newEditor ? std::move(newEditor) : PropertiesEditor::create(ui(), newEditObject);
            if(editor()) {
                if(!editor()->container())
                    editor()->initialize(this, RolloutInsertionParameters(), nullptr);
                editor()->setEditObject(newEditObject);
            }
        })) {
            _editor.reset();
        }
    }
}

/******************************************************************************
* Returns the target object being edited in the panel
******************************************************************************/
RefTarget* PropertiesPanel::editObject() const
{
    return editor() ? editor()->editObject() : nullptr;
}

}   // End of namespace
