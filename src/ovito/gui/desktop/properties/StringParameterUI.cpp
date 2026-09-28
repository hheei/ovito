// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/desktop/properties/StringParameterUI.h>
#include <ovito/gui/desktop/widgets/general/AutocompleteTextEdit.h>
#include <ovito/gui/desktop/widgets/general/EnterLineEdit.h>
#include <ovito/core/app/undo/UndoableOperation.h>

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(StringParameterUI);

/******************************************************************************
* Constructor.
******************************************************************************/
void StringParameterUI::initializeObject(PropertiesEditor* parentEditor, const PropertyFieldDescriptor* propField)
{
    PropertyParameterUI::initializeObject(parentEditor, propField);

    // Create UI widget.
    _textBox = new EnterLineEdit();
    _textBox->setAccessibleName(propertyField()->displayName());
    connect(static_cast<QLineEdit*>(_textBox.data()), &QLineEdit::editingFinished, this, &StringParameterUI::updatePropertyValue);
}

/******************************************************************************
* Destructor.
******************************************************************************/
StringParameterUI::~StringParameterUI()
{
    // Release GUI widget.
    delete _textBox;
}

/******************************************************************************
* Replaces the text box managed by this ParameterUI.
* The ParameterUI becomes the owner of the new text box and the old widget is deleted.
******************************************************************************/
void StringParameterUI::setTextBox(QWidget* textBox)
{
    OVITO_ASSERT(textBox != nullptr);
    delete _textBox;
    _textBox = textBox;
    if(qobject_cast<QLineEdit*>(textBox))
        connect(static_cast<QLineEdit*>(textBox), &QLineEdit::editingFinished, this, &StringParameterUI::updatePropertyValue);
    else if(qobject_cast<AutocompleteTextEdit*>(textBox))
        connect(static_cast<AutocompleteTextEdit*>(textBox), &AutocompleteTextEdit::editingFinished, this, &StringParameterUI::updatePropertyValue);
    updateUI();
}

/******************************************************************************
* This method is called when a new editable object has been assigned to the properties owner this
* parameter UI belongs to.
******************************************************************************/
void StringParameterUI::resetUI()
{
    PropertyParameterUI::resetUI();

    if(textBox()) {
        if(editObject()) {
            textBox()->setEnabled(isEnabled() && !editor()->isReadOnly());
        }
        else {
            textBox()->setEnabled(false);
            if(qobject_cast<QLineEdit*>(textBox()))
                static_cast<QLineEdit*>(textBox())->clear();
            else if(qobject_cast<QTextEdit*>(textBox()))
                static_cast<QTextEdit*>(textBox())->clear();
            else if(qobject_cast<QPlainTextEdit*>(textBox()))
                static_cast<QPlainTextEdit*>(textBox())->clear();
        }
    }
}

/******************************************************************************
* This method is called when a new editable object has been assigned to the properties owner this
* parameter UI belongs to.
******************************************************************************/
void StringParameterUI::updateUI()
{
    PropertyParameterUI::updateUI();

    if(textBox() && editObject()) {
        QVariant val;
        if(isPropertyFieldUI()) {
            val = editObject()->getPropertyFieldValue(propertyField());
            OVITO_ASSERT(val.isValid());
        }
        if(QLineEdit* lineEdit = qobject_cast<QLineEdit*>(textBox()))
            lineEdit->setText(val.toString());
        else if(QTextEdit* textEdit = qobject_cast<QTextEdit*>(textBox())) {
            QString newText = val.toString();
            if(textEdit->toPlainText() != newText)
                textEdit->setPlainText(newText);
        }
        else if(QPlainTextEdit* textEdit = qobject_cast<QPlainTextEdit*>(textBox())) {
            QString newText = val.toString();
            if(textEdit->toPlainText() != newText)
                textEdit->setPlainText(newText);
        }
    }
}

/******************************************************************************
* Sets the enabled state of the UI.
******************************************************************************/
void StringParameterUI::setEnabled(bool enabled)
{
    if(enabled == isEnabled())
        return;
    PropertyParameterUI::setEnabled(enabled);
    if(textBox())
        textBox()->setEnabled(editObject() && isEnabled());
}

/******************************************************************************
* Takes the value entered by the user and stores it in the property field
* this property UI is bound to.
******************************************************************************/
void StringParameterUI::updatePropertyValue()
{
    QString text;
    if(qobject_cast<QLineEdit*>(textBox()))
        text = static_cast<QLineEdit*>(textBox())->text();
    else if(qobject_cast<QTextEdit*>(textBox()))
        text = static_cast<QTextEdit*>(textBox())->toPlainText();
    else if(qobject_cast<QPlainTextEdit*>(textBox()))
        text = static_cast<QPlainTextEdit*>(textBox())->toPlainText();
    else
        return;
    if(editObject()) {
        performTransaction(tr("Change parameter"), [this,text]() {
            if(isPropertyFieldUI()) {
                editObject()->setPropertyFieldValue(propertyField(), text);
            }
            Q_EMIT valueEntered();
        });
    }
}

}   // End of namespace
