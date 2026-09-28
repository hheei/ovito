// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/stdmod/gui/StdModGui.h>
#include <ovito/stdmod/modifiers/ExpressionSelectionModifier.h>
#include <ovito/gui/desktop/properties/StringParameterUI.h>
#include <ovito/gui/desktop/properties/ObjectStatusDisplay.h>
#include <ovito/gui/desktop/properties/ModifierDelegateParameterUI.h>
#include <ovito/gui/desktop/widgets/general/AutocompleteTextEdit.h>
#include <ovito/gui/desktop/app/GuiApplication.h>
#include "ExpressionSelectionModifierEditor.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(ExpressionSelectionModifierEditor);
SET_OVITO_OBJECT_EDITOR(ExpressionSelectionModifier, ExpressionSelectionModifierEditor);

/******************************************************************************
* Sets up the UI widgets of the editor.
******************************************************************************/
void ExpressionSelectionModifierEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    QWidget* rollout = createRollout(tr("Expression selection"), rolloutParams, "manual:particles.modifiers.expression_select");

    // Create the rollout contents.
    QVBoxLayout* layout = new QVBoxLayout(rollout);
    layout->setContentsMargins(4,4,4,4);
    layout->setSpacing(0);

    ModifierDelegateParameterUI* delegateUI = createParamUI<ModifierDelegateParameterUI>(ExpressionSelectionModifierDelegate::OOClass());
    layout->addWidget(new QLabel(tr("Operate on:")));
    layout->addWidget(delegateUI->comboBox());

    layout->addWidget(new QLabel(tr("Boolean expression:")));
    StringParameterUI* expressionUI = createParamUI<StringParameterUI>(PROPERTY_FIELD(ExpressionSelectionModifier::expression));
    expressionEdit = new AutocompleteTextEdit();
    expressionUI->setTextBox(expressionEdit);
    layout->addWidget(expressionUI->textBox());

    // Status label.
    layout->addSpacing(12);
    layout->addWidget(createParamUI<ObjectStatusDisplay>()->statusWidget());

    QWidget* variablesRollout = createRollout(tr("Expression variables"), rolloutParams.after(rollout), "manual:particles.modifiers.expression_select");
    QVBoxLayout* variablesLayout = new QVBoxLayout(variablesRollout);
    variablesLayout->setContentsMargins(4,4,4,4);
    variableNamesList = new QLabel();
    variableNamesList->setWordWrap(true);
    variableNamesList->setTextInteractionFlags(Qt::TextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard | Qt::LinksAccessibleByMouse | Qt::LinksAccessibleByKeyboard));
    variablesLayout->addWidget(variableNamesList, 1);

    // Update input variables list if another modifier has been loaded into the editor.
    connect(this, &ExpressionSelectionModifierEditor::contentsReplaced, this, &ExpressionSelectionModifierEditor::updateEditorFields);
}

/******************************************************************************
* This method is called when a reference target changes.
******************************************************************************/
bool ExpressionSelectionModifierEditor::referenceEvent(RefTarget* source, const ReferenceEvent& event)
{
    if(source == editObject() && event.type() == ReferenceEvent::ObjectStatusChanged) {
        updateEditorFields();
    }
    return PropertiesEditor::referenceEvent(source, event);
}

/******************************************************************************
* Updates the enabled/disabled status of the editor's controls.
******************************************************************************/
void ExpressionSelectionModifierEditor::updateEditorFields()
{
    ExpressionSelectionModifier* mod = static_object_cast<ExpressionSelectionModifier>(editObject());
    if(!mod) return;

    QString descriptionStyle = GuiApplication::instance()->usingDarkTheme()
        ? QStringLiteral("color: #aaa; font-style: italic;")
        : QStringLiteral("color: #555; font-style: italic;");
    variableNamesList->setText(
        QStringLiteral("%1<p></p>").arg(mod->inputVariableTable()).replace(QStringLiteral("DESCRIPTION_STYLE_PLACEHOLDER"), descriptionStyle));
    container()->updateRolloutsLater();
    expressionEdit->setWordList(mod->inputVariableNames());
}

}   // End of namespace
