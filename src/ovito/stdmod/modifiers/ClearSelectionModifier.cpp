// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/stdmod/StdMod.h>
#include <ovito/stdobj/properties/Property.h>
#include <ovito/stdobj/properties/PropertyContainer.h>
#include "ClearSelectionModifier.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(ClearSelectionModifier);
OVITO_CLASSINFO(ClearSelectionModifier, "DisplayName", "Clear selection");
OVITO_CLASSINFO(ClearSelectionModifier, "Description", "Reset the selection state of all elements.");
OVITO_CLASSINFO(ClearSelectionModifier, "ModifierCategory", "Selection");

/******************************************************************************
* Constructor.
******************************************************************************/
void ClearSelectionModifier::initializeObject(ObjectInitializationFlags flags)
{
    GenericPropertyModifier::initializeObject(flags);

    // Operate on particles by default.
    setDefaultSubject(QStringLiteral("Particles"), QStringLiteral("Particles"));
}

/******************************************************************************
* Modifies the input data.
******************************************************************************/
Future<PipelineFlowState> ClearSelectionModifier::evaluateModifier(const ModifierEvaluationRequest& request, PipelineFlowState&& state)
{
    if(!subject())
        throw Exception(tr("No input element type selected."));

    PropertyContainer* container = state.expectMutableLeafObject(subject());
    if(const Property* selProperty = container->getProperty(Property::GenericSelectionProperty))
        container->removeProperty(selProperty);

    return std::move(state);
}

}   // End of namespace
