// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/stdmod/gui/StdModGui.h>
#include <ovito/stdmod/modifiers/EditSimulationCellModifier.h>
#include <ovito/stdmod/modifiers/DeleteSelectedModifier.h>
#include <ovito/gui/desktop/widgets/general/ActionsItemDelegate.h>
#include <ovito/gui/desktop/properties/PropertiesEditor.h>
#include <ovito/core/utilities/units/PrescribedScaleUnit.h>

namespace Ovito {

/**
 * A properties editor for the EditSimulationCellModifier class.
 */
class EditSimulationCellModifierEditor : public PropertiesEditor
{
    OVITO_CLASS(EditSimulationCellModifierEditor)
    Q_OBJECT

public:

    /// Returns the EditSimulationCellModifier object being edited.
    EditSimulationCellModifier* modifier() const { return static_object_cast<EditSimulationCellModifier>(editObject()); }

protected:

    /// Creates the user interface controls for the editor.
    virtual void createUI(const RolloutInsertionParameters& rolloutParams) override;

private Q_SLOTS:

    /// Updates the values displayed in the editor panel.
    void updateSimulationCellFields();

    /// Auto-adjusts the increment steps of the numeric parameter spinner widgets.
    void updateParameterUnitScales();

private:

    BooleanParameterUI* _pbczPUI;
    QLineEdit* _cellVectorFields[4][3];
    std::optional<PrescribedScaleUnit> _cellUnits[3][3];
    std::optional<PrescribedScaleUnit> _originUnits[3];
};

}   // End of namespace
