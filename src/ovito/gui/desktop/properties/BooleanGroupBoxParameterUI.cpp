// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/desktop/properties/BooleanGroupBoxParameterUI.h>
#include <ovito/core/app/undo/UndoableOperation.h>
#include <ovito/core/dataset/animation/controller/Controller.h>
#include <ovito/core/dataset/animation/AnimationSettings.h>

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(BooleanGroupBoxParameterUI);

/******************************************************************************
* Constructor.
******************************************************************************/
void BooleanGroupBoxParameterUI::initializeObject(PropertiesEditor* parentEditor, const PropertyFieldDescriptor* propField)
{
    PropertyParameterUI::initializeObject(parentEditor, propField);

    // Create UI widget.
    _groupBox = new QGroupBox(propField->displayName());
    _groupBox->setCheckable(true);
    _childContainer = new QWidget(_groupBox);
    QVBoxLayout* layout = new QVBoxLayout(_groupBox);
    layout->setContentsMargins(0,0,0,0);
    layout->setSpacing(0);
    layout->addWidget(_childContainer, 1);
    connect(_groupBox.data(), &QGroupBox::clicked, this, &BooleanGroupBoxParameterUI::updatePropertyValue);
}

/******************************************************************************
* Destructor.
******************************************************************************/
BooleanGroupBoxParameterUI::~BooleanGroupBoxParameterUI()
{
    // Release GUI controls.
    delete groupBox();
}

/******************************************************************************
* This method is called when a new editable object has been assigned to the properties owner this
* parameter UI belongs to.
******************************************************************************/
void BooleanGroupBoxParameterUI::resetUI()
{
    PropertyParameterUI::resetUI();

    if(groupBox()) {
        if(isReferenceFieldUI())
            groupBox()->setEnabled(parameterObject() && isEnabled() && !editor()->isReadOnly());
        else
            groupBox()->setEnabled(editObject() && isEnabled() && !editor()->isReadOnly());
    }
}

/******************************************************************************
* This method is called when a new editable object has been assigned to the properties owner this
* parameter UI belongs to.
******************************************************************************/
void BooleanGroupBoxParameterUI::updateUI()
{
    PropertyParameterUI::updateUI();

    if(groupBox() && editObject()) {
        if(!isReferenceFieldUI()) {
            QVariant val(false);
            if(isPropertyFieldUI()) {
                val = editObject()->getPropertyFieldValue(propertyField());
                OVITO_ASSERT(val.isValid());
            }
            groupBox()->setChecked(val.toBool());
        }
    }
}

/******************************************************************************
* Sets the enabled state of the UI.
******************************************************************************/
void BooleanGroupBoxParameterUI::setEnabled(bool enabled)
{
    if(enabled == isEnabled()) return;
    PropertyParameterUI::setEnabled(enabled);
    if(groupBox()) {
        if(isReferenceFieldUI())
            groupBox()->setEnabled(parameterObject() != NULL && isEnabled());
        else
            groupBox()->setEnabled(editObject() != NULL && isEnabled());
    }
}

/******************************************************************************
* Takes the value entered by the user and stores it in the property field
* this property UI is bound to.
******************************************************************************/
void BooleanGroupBoxParameterUI::updatePropertyValue()
{
    if(groupBox() && editObject()) {
        performTransaction(tr("Change parameter value"), [&]() {
            if(isPropertyFieldUI()) {
                editObject()->setPropertyFieldValue(propertyField(), groupBox()->isChecked());
            }
            Q_EMIT valueEntered();
        });
    }
}

}   // End of namespace
