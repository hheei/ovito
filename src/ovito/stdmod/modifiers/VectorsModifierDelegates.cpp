// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/stdobj/properties/PropertyExpressionEvaluator.h>
#include <ovito/core/dataset/pipeline/ModifierEvaluationRequest.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include "VectorsModifierDelegates.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(VectorsExpressionSelectionModifierDelegate);
OVITO_CLASSINFO(VectorsExpressionSelectionModifierDelegate, "DisplayName", "Vectors");

/******************************************************************************
 * Indicates which data objects in the given input data collection the modifier
 * delegate is able to operate on.
 ******************************************************************************/
QVector<DataObjectReference> VectorsExpressionSelectionModifierDelegate::OOMetaClass::getApplicableObjects(
    const DataCollection& input) const
{
    // Gather list of all vector objects in the input data collection.
    QVector<DataObjectReference> objects;
    for(const ConstDataObjectPath& path : input.getObjectsRecursive(Vectors::OOClass())) {
        objects.push_back(path);
    }
    return objects;
}

IMPLEMENT_CREATABLE_OVITO_CLASS(VectorsDeleteSelectedModifierDelegate);
OVITO_CLASSINFO(VectorsDeleteSelectedModifierDelegate, "DisplayName", "Vectors");

/******************************************************************************
 * Indicates which data objects in the given input data collection the modifier
 * delegate is able to operate on.
 ******************************************************************************/
QVector<DataObjectReference> VectorsDeleteSelectedModifierDelegate::OOMetaClass::getApplicableObjects(const DataCollection& input) const
{
    // Gather list of all vector objects in the input data collection.
    QVector<DataObjectReference> objects;
    for(const ConstDataObjectPath& path : input.getObjectsRecursive(Vectors::OOClass())) {
        objects.push_back(path);
    }
    return objects;
}

/******************************************************************************
 * Applies this modifier delegate to the data.
 ******************************************************************************/
Future<PipelineFlowState> VectorsDeleteSelectedModifierDelegate::apply(
    const ModifierEvaluationRequest& request, PipelineFlowState&& state, const PipelineFlowState& originalState,
    const std::vector<PipelineFlowState>& additionalInputs)
{
    // The actual computation can be performed in a separate worker thread.
    return asyncLaunch([state = std::move(state), createdByNode = request.modificationNodeWeak(), inputObjectRef = inputDataObject()]() mutable {
        size_t numVectors = 0;
        size_t numSelected = 0;

        // Process all Vectors objects in the data collection.
        visitObjectsToBeProcessed<Vectors>(state, inputObjectRef, createdByNode, [&](const Vectors* inputVectors) {
            inputVectors->verifyIntegrity();
            numVectors += inputVectors->elementCount();

            // Get the vectors (base points) selection.
            if(ConstPropertyPtr selProperty = inputVectors->getProperty(Vectors::SelectionProperty)) {
                // Make sure we can safely modify the vectors object.
                Vectors* outputVectors = state.makeMutable(inputVectors);

                // Remove selection property.
                outputVectors->removeProperty(selProperty);

                // Delete the selected vector base points / positions.
                numSelected += outputVectors->deleteElements(std::move(selProperty));
            }
        });

        // Report some statistics:
        QString statusMessage = tr("%1 of %2 vectors deleted (%3%)")
                                    .arg(numSelected)
                                    .arg(numVectors)
                                    .arg((FloatType)numSelected * (FloatType)100 / (FloatType)std::max(numVectors, (size_t)1), 0, 'f', 1);
        // Show the number of deleted vectors next to the modifier's title in the pipeline editor.
        if(numSelected != 0)
            state.combineStatus(std::move(statusMessage),
                numSelected == 1 ? tr("1 vector") : tr("%1 vectors").arg(formatNumberForUI(numSelected)));
        else
            state.combineStatus(std::move(statusMessage));

        return std::move(state);
    });
}

IMPLEMENT_CREATABLE_OVITO_CLASS(VectorsComputePropertyModifierDelegate);
OVITO_CLASSINFO(VectorsComputePropertyModifierDelegate, "DisplayName", "Vectors");

/******************************************************************************
 * Indicates which data objects in the given input data collection the modifier
 * delegate is able to operate on.
 ******************************************************************************/
QVector<DataObjectReference> VectorsComputePropertyModifierDelegate::OOMetaClass::getApplicableObjects(const DataCollection& input) const
{
    // Gather list of all Vectors objects in the input data collection.
    QVector<DataObjectReference> objects;
    for(const ConstDataObjectPath& path : input.getObjectsRecursive(Vectors::OOClass())) {
        objects.push_back(path);
    }
    return objects;
}

}  // namespace Ovito
