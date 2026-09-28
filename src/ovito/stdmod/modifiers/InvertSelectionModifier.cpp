// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/stdmod/StdMod.h>
#include <ovito/stdobj/properties/Property.h>
#include <ovito/stdobj/properties/PropertyContainer.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include "InvertSelectionModifier.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(InvertSelectionModifier);
OVITO_CLASSINFO(InvertSelectionModifier, "DisplayName", "Invert selection");
OVITO_CLASSINFO(InvertSelectionModifier, "Description", "Invert the current selection state of each element.");
OVITO_CLASSINFO(InvertSelectionModifier, "ModifierCategory", "Selection");

/******************************************************************************
* Constructor.
******************************************************************************/
void InvertSelectionModifier::initializeObject(ObjectInitializationFlags flags)
{
    GenericPropertyModifier::initializeObject(flags);

    // Operate on particles by default.
    setDefaultSubject(QStringLiteral("Particles"), QStringLiteral("Particles"));
}

/******************************************************************************
* Modifies the input data.
******************************************************************************/
Future<PipelineFlowState> InvertSelectionModifier::evaluateModifier(const ModifierEvaluationRequest& request, PipelineFlowState&& state)
{
    if(!subject())
        throw Exception(tr("No data element type set."));

    PropertyContainer* container = state.expectMutableLeafObject(subject());
    if(!container->getOOMetaClass().isValidStandardPropertyId(Property::GenericSelectionProperty))
        throw Exception(tr("Cannot invert selection, because property container type %1 does not support element selections.").arg(container->getOOMetaClass().name()));

    ConstPropertyPtr inputSelection = container->getProperty(Property::GenericSelectionProperty);
    PropertyPtr outputSelection = container->createProperty(DataBuffer::Uninitialized, Property::GenericSelectionProperty);

    // The actual computation can be performed in a separate worker thread.
    return asyncLaunch([
            state = std::move(state),
            inputSelection = std::move(inputSelection),
            outputSelection = std::move(outputSelection)]() mutable
    {
        if(inputSelection) {
#ifdef OVITO_USE_SYCL
            this_task::ui()->taskManager().syclQueue().submit([&](sycl::handler& cgh) {
                SyclBufferAccess<SelectionIntType, access_mode::read> inputAcc(inputSelection, cgh);
                SyclBufferAccess<SelectionIntType, access_mode::discard_write> outputAcc(outputSelection, cgh);
                OVITO_SYCL_PARALLEL_FOR(cgh, InvertSelection_kernel)(sycl::range(inputAcc.size()), [=](size_t i) {
                    outputAcc[i] = !inputAcc[i];
                });
            });
#else
            BufferReadAccess<SelectionIntType> inputAcc(inputSelection);
            auto i = inputAcc.begin();
            for(auto& o : BufferWriteAccess<SelectionIntType, access_mode::discard_write>(outputSelection))
                o = !(*i++);
#endif
        }
        else {
            outputSelection->fill(SelectionIntType{1});
        }

        return std::move(state);
    });
}

}   // End of namespace
