// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/stdmod/gui/StdModGui.h>
#include <ovito/gui/desktop/properties/PropertiesEditor.h>

namespace Ovito {

/**
 * \brief A properties editor for the ColorLegendOverlay class.
 */
class ColorLegendOverlayEditor : public PropertiesEditor
{
    OVITO_CLASS(ColorLegendOverlayEditor)
    Q_OBJECT

protected:

    /// Creates the user interface controls for the editor.
    virtual void createUI(const RolloutInsertionParameters& rolloutParams) override;

    /// Is called when a RefTarget referenced by this object generated an event.
    virtual bool referenceEvent(RefTarget* source, const ReferenceEvent& event) override;

private Q_SLOTS:

    /// Updates the combo-box showing the available color mappings.
    void updateColorMappingsList();

    /// Is called when the user selects a new source color mapping for the color legend.
    void colorMappingSelectedSelected();

    /// Updates the placeholder texts of the label input fields to reflect the current values.
    void updateLabelPlaceholderTexts();

private:

    PopupUpdateComboBox* _colorMappingsComboBox;
    StringParameterUI* _captionPUI;
    StringParameterUI* _label1PUI;
    StringParameterUI* _label2PUI;
    StringParameterUI* _valueFormatStringPUI;
    BooleanGroupBoxParameterUI* _tickEnabledPUI;
    FloatParameterUI* _aspectRatioPUI;
    BooleanParameterUI* _useTypeShapesPUI;
    BooleanParameterUI* _borderEnabledPUI;
    ColorParameterUI* _borderColorPUI;

    /// Indicates that the element types of the selected source property can be represented by 3d shapes.
    bool _sourceTypesHaveShapes = false;
};

}   // End of namespace
