// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/stdmod/gui/StdModGui.h>
#include <ovito/gui/desktop/properties/PropertiesEditor.h>
#include <ovito/stdobj/gui/widgets/PropertyReferenceParameterUI.h>

namespace Ovito {

/**
 * A properties editor for the TextLabelsModifier class.
 */
class TextLabelsModifierEditor : public PropertiesEditor
{
    OVITO_CLASS(TextLabelsModifierEditor)

protected:

    /// Creates the user interface controls for the editor.
    virtual void createUI(const RolloutInsertionParameters& rolloutParams) override;

private:

    /// Selection box for the input property.
    PropertyReferenceParameterUI* _sourcePropertyUI;

    /// Selection box for the property providing the anchor positions of the labels.
    PropertyReferenceParameterUI* _positionPropertyUI;

    /// The caption of the anchor position selection box. Both are shown only for input containers
    /// that do not provide anchor positions of their own.
    QLabel* _positionPropertyLabel;
};

}   // End of namespace
