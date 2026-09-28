// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/desktop/properties/SubObjectParameterUI.h>
#include <ovito/core/app/undo/UndoableOperation.h>

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(SubObjectParameterUI);

/******************************************************************************
* Constructor.
******************************************************************************/
void SubObjectParameterUI::initializeObject(PropertiesEditor* parentEditor, const PropertyFieldDescriptor* refField, const RolloutInsertionParameters& rolloutParams)
{
    PropertyParameterUI::initializeObject(parentEditor, refField);

    _rolloutParams = rolloutParams;
}

/******************************************************************************
* This method is called when a new sub-object has been assigned to the reference field of the editable object
* this parameter UI is bound to. It is also called when the editable object itself has
* been replaced in the editor.
******************************************************************************/
void SubObjectParameterUI::resetUI()
{
    PropertyParameterUI::resetUI();

    handleExceptions<true>([&] {
        // Close editor if it is no longer needed.
        if(subEditor()) {
            if(!parameterObject() || subEditor()->editObject() == nullptr ||
                    subEditor()->editObject()->getOOClass() != parameterObject()->getOOClass() ||
                    !isEnabled()) {

                _subEditor.reset();
            }
        }
        if(!parameterObject() || !isEnabled()) return;
        if(!subEditor()) {
            _subEditor = PropertiesEditor::create(ui(), parameterObject());
            if(subEditor()) {
                subEditor()->initialize(editor()->container(), _rolloutParams, editor());
            }
        }

        if(subEditor()) {
            subEditor()->setEditObject(parameterObject());
        }
    });
}

/******************************************************************************
* Sets the enabled state of the UI.
******************************************************************************/
void SubObjectParameterUI::setEnabled(bool enabled)
{
    if(enabled != isEnabled()) {
        PropertyParameterUI::setEnabled(enabled);
        if(editObject())
            resetUI();
    }
}

}   // End of namespace
