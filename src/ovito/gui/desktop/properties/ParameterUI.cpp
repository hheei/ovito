// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/desktop/properties/ParameterUI.h>
#include <ovito/gui/desktop/properties/PropertiesEditor.h>

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(ParameterUI);
DEFINE_REFERENCE_FIELD(ParameterUI, editObject);

/******************************************************************************
* Constructor.
******************************************************************************/
void ParameterUI::initializeObject(PropertiesEditor* editor)
{
    RefMaker::initializeObject();

    OVITO_ASSERT(editor);
    _editor = editor;
    setUserInterface(editor->ui());

    // Connect to the contentsReplaced() signal of the editor to synchronize the
    // parameter UI's edit object with the editor's edit object.
    connect(editor, &PropertiesEditor::contentsReplaced, this, &ParameterUI::setEditObject);
}

/******************************************************************************
* This method gets called by OORef<T>::create() right after the object is fully initialized.
******************************************************************************/
void ParameterUI::completeObjectInitialization()
{
    RefMaker::completeObjectInitialization();

    // Automatically adopt the editor's edit object if none was set yet.
    if(!editObject() && editor()->editObject())
        setEditObject(editor()->editObject());
}

}   // End of namespace
