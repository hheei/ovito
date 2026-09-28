// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once

#include <ovito/particles/gui/ParticlesGui.h>
#include <ovito/stdobj/gui/widgets/DataTablePlotWidget.h>
#include <ovito/stdobj/gui/widgets/PropertyReferenceParameterUI.h>
#include <ovito/gui/desktop/properties/PropertiesEditor.h>

namespace Ovito {

/**
 * A properties editor for the RadialDistributionFunctionModifier class.
 */
class RadialDistributionFunctionModifierEditor : public PropertiesEditor
{
    OVITO_CLASS(RadialDistributionFunctionModifierEditor)
    Q_OBJECT

protected:
    /// Creates the user interface controls for the editor.
    virtual void createUI(const RolloutInsertionParameters& rolloutParams) override;

protected Q_SLOTS:

    /// Replots the RDF computed by the modifier.
    void plotRDF();

private:
    /// The plotting widget for displaying the computed RDFs.
    DataTablePlotWidget* _rdfPlot;

    /// Selection box for the type property.
    PropertyReferenceParameterUI* _typePropertyUI;
};

}  // namespace Ovito
