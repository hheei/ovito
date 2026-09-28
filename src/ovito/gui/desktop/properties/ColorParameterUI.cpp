// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/desktop/properties/ColorParameterUI.h>
#include <ovito/core/app/undo/UndoableOperation.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/core/dataset/animation/controller/Controller.h>
#include <ovito/core/dataset/animation/AnimationSettings.h>

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(ColorParameterUI);

/******************************************************************************
* Constructor.
******************************************************************************/
void ColorParameterUI::initializeObject(PropertiesEditor* parentEditor, const PropertyFieldDescriptor* propField)
{
    PropertyParameterUI::initializeObject(parentEditor, propField);

    _label = new QLabel(propField->displayName() + QStringLiteral(":"));
    _colorPicker = new ColorPickerWidget();
    _colorPicker->setObjectName("colorButton");
    _colorPicker->setAccessibleName(propertyField()->displayName());
    _label->setBuddy(_colorPicker);
    connect(_colorPicker.data(), &ColorPickerWidget::colorChanged, this, &ColorParameterUI::onColorPickerChanged);
}

/******************************************************************************
* Destructor.
******************************************************************************/
ColorParameterUI::~ColorParameterUI()
{
    // Release GUI controls.
    delete label();
    delete colorPicker();
}

/******************************************************************************
* This method is called when a new editable object has been assigned to the properties owner this
* parameter UI belongs to.
******************************************************************************/
void ColorParameterUI::resetUI()
{
    PropertyParameterUI::resetUI();

    if(colorPicker())  {
        if(editObject() && (!isReferenceFieldUI() || parameterObject())) {
            colorPicker()->setEnabled(isEnabled() && !editor()->isReadOnly());
            colorPicker()->setReadOnly(editor()->isReadOnly());
        }
        else {
            colorPicker()->setEnabled(false);
            colorPicker()->setColor(Color(1,1,1));
        }
    }

    if(isReferenceFieldUI() && editObject()) {
        // Update the displayed value when the animation time has changed.
        connect(&datasetContainer(), &DataSetContainer::currentFrameChanged, this, &ColorParameterUI::updateUI, Qt::UniqueConnection);
    }
}

/******************************************************************************
* This method updates the displayed value of the parameter UI.
******************************************************************************/
void ColorParameterUI::updateUI()
{
    if(editObject() && colorPicker()) {
        if(isReferenceFieldUI()) {
            if(Controller* ctrl = dynamic_object_cast<Controller>(parameterObject())) {
                colorPicker()->setColor(ctrl->getColorValue(currentAnimationTime()));
            }
        }
        else if(isPropertyFieldUI()) {
            QVariant currentValue = editObject()->getPropertyFieldValue(propertyField());
            OVITO_ASSERT(currentValue.isValid());
            if(currentValue.canConvert<Color>()) {
                colorPicker()->setColor(currentValue.value<Color>());
            }
            else if(currentValue.canConvert<QColor>()) {
                colorPicker()->setColor(currentValue.value<QColor>());
            }
        }
    }
}

/******************************************************************************
* Sets the enabled state of the UI.
******************************************************************************/
void ColorParameterUI::setEnabled(bool enabled)
{
    if(enabled == isEnabled()) return;
    PropertyParameterUI::setEnabled(enabled);
    if(colorPicker()) {
        if(isReferenceFieldUI())
            colorPicker()->setEnabled(parameterObject() && isEnabled());
        else
            colorPicker()->setEnabled(editObject() && isEnabled());
    }
}

/******************************************************************************
* Is called when the user has changed the color.
******************************************************************************/
void ColorParameterUI::onColorPickerChanged()
{
    if(colorPicker() && editObject()) {
        performTransaction(tr("Change color"), [this]() {
            if(isReferenceFieldUI()) {
                if(Controller* ctrl = dynamic_object_cast<Controller>(parameterObject())) {
                    ctrl->setColorValue(currentAnimationTime(), colorPicker()->color());
                }
            }
            else if(isPropertyFieldUI()) {
                editObject()->setPropertyFieldValue(propertyField(), QVariant::fromValue((QColor)colorPicker()->color()));
            }
            Q_EMIT valueEntered();
        });
    }
}

}   // End of namespace
