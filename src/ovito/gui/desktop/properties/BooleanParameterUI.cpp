// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/desktop/properties/BooleanParameterUI.h>
#include <ovito/core/app/undo/UndoableOperation.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/core/dataset/animation/AnimationSettings.h>
#include <ovito/core/dataset/animation/controller/Controller.h>

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(BooleanParameterUI);

/******************************************************************************
* Constructor.
******************************************************************************/
void BooleanParameterUI::initializeObject(PropertiesEditor* parentEditor, const PropertyFieldDescriptor* propField)
{
    PropertyParameterUI::initializeObject(parentEditor, propField);

    // Create UI widget.
    _checkBox = new QCheckBox(propField->displayName());
    connect(_checkBox.data(), &QCheckBox::clicked, this, &BooleanParameterUI::updatePropertyValue);
}

/******************************************************************************
* Destructor.
******************************************************************************/
BooleanParameterUI::~BooleanParameterUI()
{
    // Release widget.
    delete checkBox();
}

/******************************************************************************
* This method is called when a new editable object has been assigned to the properties owner this
* parameter UI belongs to.
******************************************************************************/
void BooleanParameterUI::resetUI()
{
    PropertyParameterUI::resetUI();

    if(checkBox()) {
        if(isReferenceFieldUI())
            checkBox()->setEnabled(parameterObject() && isEnabled() && !editor()->isReadOnly());
        else
            checkBox()->setEnabled(editObject() && isEnabled() && !editor()->isReadOnly());
    }

    if(isReferenceFieldUI() && editObject()) {
        // Update the displayed value when the animation time has changed.
        connect(&datasetContainer(), &DataSetContainer::currentFrameChanged, this, &BooleanParameterUI::updateUI, Qt::UniqueConnection);
    }
}

/******************************************************************************
* This method is called when a new editable object has been assigned to the properties owner this
* parameter UI belongs to.
******************************************************************************/
void BooleanParameterUI::updateUI()
{
    PropertyParameterUI::updateUI();

    if(checkBox() && editObject()) {
        if(!isReferenceFieldUI()) {
            QVariant val(false);
            if(propertyField()) {
                val = editObject()->getPropertyFieldValue(propertyField());
                OVITO_ASSERT(val.isValid());
            }
            checkBox()->setChecked(val.toBool());
        }
    }
}

/******************************************************************************
* Sets the enabled state of the UI.
******************************************************************************/
void BooleanParameterUI::setEnabled(bool enabled)
{
    if(enabled == isEnabled())
        return;
    PropertyParameterUI::setEnabled(enabled);
    if(checkBox()) {
        if(isReferenceFieldUI())
            checkBox()->setEnabled(parameterObject() && isEnabled());
        else
            checkBox()->setEnabled(editObject() && isEnabled());
    }
}

/******************************************************************************
* Takes the value entered by the user and stores it in the property field
* this property UI is bound to.
******************************************************************************/
void BooleanParameterUI::updatePropertyValue()
{
    if(checkBox() && editObject()) {
        performTransaction(tr("Change parameter value"), [&]() {
            if(isPropertyFieldUI()) {
                editObject()->setPropertyFieldValue(propertyField(), checkBox()->isChecked());
            }
            Q_EMIT valueEntered();
        });
    }
}

}   // End of namespace
