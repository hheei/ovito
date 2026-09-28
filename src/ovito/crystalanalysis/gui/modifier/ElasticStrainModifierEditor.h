// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/crystalanalysis/CrystalAnalysis.h>
#include <ovito/gui/desktop/properties/PropertiesEditor.h>

namespace Ovito {

/**
 * Properties editor for the ElasticStrainModifier class.
 */
class ElasticStrainModifierEditor : public PropertiesEditor
{
    OVITO_CLASS(ElasticStrainModifierEditor)
    Q_OBJECT

protected:

    /// Creates the user interface controls for the editor.
    virtual void createUI(const RolloutInsertionParameters& rolloutParams) override;

private Q_SLOTS:

    /// Is called each time the parameters of the modifier have changed.
    void modifierChanged(RefTarget* editObject);

private:

    FloatParameterUI* _caRatioUI;
};

}   // End of namespace
