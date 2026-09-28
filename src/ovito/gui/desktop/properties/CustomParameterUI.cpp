// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/desktop/properties/CustomParameterUI.h>
#include <ovito/core/app/undo/UndoableOperation.h>

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(CustomParameterUI);

/******************************************************************************
* Constructor.
******************************************************************************/
void CustomParameterUI::initializeObject(PropertiesEditor* parentEditor, const PropertyFieldDescriptor* propField, QWidget* widget,
        std::function<void(const QVariant&)>&& updateWidgetFunction,
        std::function<QVariant()>&& updatePropertyFunction,
        std::function<void(RefTarget*)>&& resetUIFunction)
{
    PropertyParameterUI::initializeObject(parentEditor, propField);

    _widget = widget;
    _updateWidgetFunction = std::move(updateWidgetFunction);
    _updatePropertyFunction = std::move(updatePropertyFunction);
    _resetUIFunction = std::move(resetUIFunction);
}

/******************************************************************************
* Destructor.
******************************************************************************/
CustomParameterUI::~CustomParameterUI()
{
    // Release widget.
    delete widget();
}

/******************************************************************************
* This method is called when a new editable object has been assigned to the properties owner this
* parameter UI belongs to.
******************************************************************************/
void CustomParameterUI::resetUI()
{
    if(widget()) {
        widget()->setEnabled(editObject() && isEnabled() && !editor()->isReadOnly());
        if(_resetUIFunction)
            _resetUIFunction(editObject());
    }

    PropertyParameterUI::resetUI();
}

/******************************************************************************
* This method is called when a new editable object has been assigned to the properties owner this
* parameter UI belongs to.
******************************************************************************/
void CustomParameterUI::updateUI()
{
    PropertyParameterUI::updateUI();

    if(widget() && editObject()) {
        QVariant val;
        if(isPropertyFieldUI()) {
            val = editObject()->getPropertyFieldValue(propertyField());
            OVITO_ASSERT(val.isValid());
        }
        else if(isReferenceFieldUI() && !propertyField()->isVector()) {
            val = QVariant::fromValue(editObject()->getReferenceFieldTarget(propertyField()));
            OVITO_ASSERT(val.isValid());
        }
        else return;

        _updateWidgetFunction(val);
    }
}

/******************************************************************************
* Sets the enabled state of the UI.
******************************************************************************/
void CustomParameterUI::setEnabled(bool enabled)
{
    if(enabled == isEnabled()) return;
    PropertyParameterUI::setEnabled(enabled);
    if(widget())
        widget()->setEnabled(editObject() != NULL && isEnabled());
}

/******************************************************************************
* Takes the value entered by the user and stores it in the property field
* this property UI is bound to.
******************************************************************************/
void CustomParameterUI::updatePropertyValue()
{
    if(widget() && editObject()) {
        performTransaction(tr("Change parameter"), [this]() {
            QVariant newValue = _updatePropertyFunction();
            if(isPropertyFieldUI()) {
                editObject()->setPropertyFieldValue(propertyField(), newValue);
            }
            else if(isReferenceFieldUI() && !propertyField()->isVector()) {
                OORef<RefTarget> target = newValue.value<RefTarget*>();
                editObject()->setReferenceFieldTarget(propertyField(), std::move(target));
            }

            Q_EMIT valueEntered();
        });
    }
}

}   // End of namespace
