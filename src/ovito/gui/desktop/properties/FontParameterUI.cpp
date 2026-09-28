// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/desktop/properties/FontParameterUI.h>
#include <ovito/gui/desktop/dialogs/FontSelectionDialog.h>
#include <ovito/core/app/undo/UndoableOperation.h>
#include <ovito/core/dataset/DataSetContainer.h>

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(FontParameterUI);

/******************************************************************************
* Constructor.
******************************************************************************/
void FontParameterUI::initializeObject(PropertiesEditor* parentEditor, const PropertyFieldDescriptor* propField)
{
    PropertyParameterUI::initializeObject(parentEditor, propField);

    _label = new QLabel(propField->displayName() + ":");
    _fontPicker = new QPushButton();
    connect(_fontPicker.data(), &QPushButton::clicked, this, &FontParameterUI::onButtonClicked);
}

/******************************************************************************
* Destructor.
******************************************************************************/
FontParameterUI::~FontParameterUI()
{
    // Release GUI controls.
    delete label();
    delete fontPicker();
}

/******************************************************************************
* This method is called when a new editable object has been assigned to the properties owner this
* parameter UI belongs to.
******************************************************************************/
void FontParameterUI::resetUI()
{
    PropertyParameterUI::resetUI();

    if(fontPicker())  {
        if(editObject() && (!isReferenceFieldUI() || parameterObject())) {
            fontPicker()->setEnabled(isEnabled() && !editor()->isReadOnly());
        }
        else {
            fontPicker()->setEnabled(false);
            fontPicker()->setText(QString());
        }
    }
}

/******************************************************************************
* This method updates the displayed value of the parameter UI.
******************************************************************************/
void FontParameterUI::updateUI()
{
    if(editObject() && fontPicker()) {
        if(isPropertyFieldUI()) {
            QVariant currentValue = editObject()->getPropertyFieldValue(propertyField());
            OVITO_ASSERT(currentValue.isValid());
            if(currentValue.canConvert<QFont>())
                fontPicker()->setText(currentValue.value<QFont>().family());
            else
                fontPicker()->setText(QString());
        }
    }
}

/******************************************************************************
* Sets the enabled state of the UI.
******************************************************************************/
void FontParameterUI::setEnabled(bool enabled)
{
    if(enabled == isEnabled()) return;
    PropertyParameterUI::setEnabled(enabled);
    if(fontPicker()) {
        if(isReferenceFieldUI())
            fontPicker()->setEnabled(parameterObject() != NULL && isEnabled());
        else
            fontPicker()->setEnabled(editObject() != NULL && isEnabled());
    }
}

/******************************************************************************
* Is called when the user has pressed the font picker button.
******************************************************************************/
void FontParameterUI::onButtonClicked()
{
    if(fontPicker() && editObject() && isPropertyFieldUI()) {
        QVariant currentValue = editObject()->getPropertyFieldValue(propertyField());
        OVITO_ASSERT(currentValue.isValid());
        QFont currentFont;
        if(currentValue.canConvert<QFont>())
            currentFont = currentValue.value<QFont>();
        bool ok;
        QFont font = FontSelectionDialog::getFont(&ok, currentFont, fontPicker()->window());
        if(ok && font != currentFont && editObject()) {
            performTransaction(tr("Change font"), [this, &font]() {
                editObject()->setPropertyFieldValue(propertyField(), QVariant::fromValue(font));
                Q_EMIT valueEntered();
            });
        }
    }
}

}   // End of namespace
