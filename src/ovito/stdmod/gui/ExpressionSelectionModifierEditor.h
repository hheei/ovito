// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/stdmod/gui/StdModGui.h>
#include <ovito/gui/desktop/properties/PropertiesEditor.h>

namespace Ovito {

/**
 * A properties editor for the ExpressionSelectionModifier class.
 */
class ExpressionSelectionModifierEditor : public PropertiesEditor
{
    OVITO_CLASS(ExpressionSelectionModifierEditor)
    Q_OBJECT

protected:

    /// Creates the user interface controls for the editor.
    virtual void createUI(const RolloutInsertionParameters& rolloutParams) override;

    /// This method is called when a reference target changes.
    virtual bool referenceEvent(RefTarget* source, const ReferenceEvent& event) override;

protected Q_SLOTS:

    /// Updates the enabled/disabled status of the editor's controls.
    void updateEditorFields();

private:

    QLabel* variableNamesList;
    AutocompleteTextEdit* expressionEdit;
};

}   // End of namespace
