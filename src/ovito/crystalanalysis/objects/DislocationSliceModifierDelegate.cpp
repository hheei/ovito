// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/crystalanalysis/CrystalAnalysis.h>
#include <ovito/crystalanalysis/objects/DislocationNetwork.h>
#include <ovito/stdobj/simcell/SimulationCell.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <ovito/core/dataset/DataSet.h>
#include "DislocationSliceModifierDelegate.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(DislocationSliceModifierDelegate);
OVITO_CLASSINFO(DislocationSliceModifierDelegate, "DisplayName", "Dislocations");

/******************************************************************************
* Indicates which data objects in the given input data collection the modifier
* delegate is able to operate on.
******************************************************************************/
QVector<DataObjectReference> DislocationSliceModifierDelegate::OOMetaClass::getApplicableObjects(const DataCollection& input) const
{
    if(input.containsObject<DislocationNetwork>())
        return { DataObjectReference(&DislocationNetwork::OOClass()) };
    return {};
}

/******************************************************************************
 * Applies the modifier operation to the data in a pipeline flow state.
 ******************************************************************************/
Future<PipelineFlowState> DislocationSliceModifierDelegate::apply(const ModifierEvaluationRequest& request, PipelineFlowState&& state, const PipelineFlowState& originalState, const std::vector<PipelineFlowState>& additionalInputs)
{
    SliceModifier* modifier = static_object_cast<SliceModifier>(request.modifier());

    if(modifier->createSelection())
        return std::move(state);

    // Obtain modifier parameter values.
    Plane3 plane;
    FloatType sliceWidth;
    std::tie(plane, sliceWidth) = modifier->slicingPlane(request.time(), state.mutableStateValidity(), state);

    visitObjectsToBeProcessed<DislocationNetwork>(state, inputDataObject(), request.modificationNodeWeak(), [&](const DislocationNetwork* inputDislocations) {
        QVector<Plane3> planes = inputDislocations->cuttingPlanes();
        if(sliceWidth <= 0) {
            planes.push_back(plane);
        }
        else {
            planes.push_back(Plane3( plane.normal,  plane.dist + sliceWidth/2));
            planes.push_back(Plane3(-plane.normal, -plane.dist + sliceWidth/2));
        }
        DislocationNetwork* outputDislocations = state.makeMutable(inputDislocations);
        outputDislocations->setCuttingPlanes(std::move(planes));
    });

    return std::move(state);
}

}   // End of namespace
