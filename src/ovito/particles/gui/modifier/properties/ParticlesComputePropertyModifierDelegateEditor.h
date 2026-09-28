// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/particles/gui/ParticlesGui.h>
#include <ovito/gui/desktop/properties/PropertiesEditor.h>
#include <ovito/gui/desktop/utilities/DeferredMethodInvocation.h>

namespace Ovito {

/**
 * A properties editor for the ParticlesComputePropertyModifierDelegate class.
 */
class ParticlesComputePropertyModifierDelegateEditor : public PropertiesEditor
{
    OVITO_CLASS(ParticlesComputePropertyModifierDelegateEditor)
    Q_OBJECT

protected:

    /// Creates the user interface controls for the editor.
    virtual void createUI(const RolloutInsertionParameters& rolloutParams) override;

    /// This method is called when a reference target changes.
    virtual bool referenceEvent(RefTarget* source, const ReferenceEvent& event) override;

protected Q_SLOTS:

    /// Is called when the user has typed in an expression.
    void onExpressionEditingFinished();

    /// Updates the editor's input fields for the expressions.
    void updateExpressionFields();

    /// Updates the editor's display of the available expression variables.
    void updateVariablesList();

private:

    QGroupBox* neighborExpressionsGroupBox;
    QList<AutocompleteLineEdit*> neighborExpressionLineEdits;
    QList<AutocompleteTextEdit*> neighborExpressionTextEdits;
    QList<QLabel*> neighborExpressionLabels;
    QGridLayout* neighborExpressionsLayout;
	QLabel* expandFieldsLabel;

    // For deferred invocation of the UI update functions.
    DeferredMethodInvocation<ParticlesComputePropertyModifierDelegateEditor, &ParticlesComputePropertyModifierDelegateEditor::updateExpressionFields> updateExpressionFieldsLater;
    DeferredMethodInvocation<ParticlesComputePropertyModifierDelegateEditor, &ParticlesComputePropertyModifierDelegateEditor::updateVariablesList> updateVariablesListLater;
};

}   // End of namespace
