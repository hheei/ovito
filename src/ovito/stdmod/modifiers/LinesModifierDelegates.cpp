// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/stdobj/properties/PropertyExpressionEvaluator.h>
#include <ovito/core/dataset/pipeline/ModifierEvaluationRequest.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include "LinesModifierDelegates.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(LinesAssignColorModifierDelegate);
OVITO_CLASSINFO(LinesAssignColorModifierDelegate, "DisplayName", "Lines");

/******************************************************************************
 * Indicates which data objects in the given input data collection the modifier
 * delegate is able to operate on.
 ******************************************************************************/
QVector<DataObjectReference> LinesAssignColorModifierDelegate::OOMetaClass::getApplicableObjects(const DataCollection& input) const
{
    // Gather list of all Lines objects in the input data collection.
    QVector<DataObjectReference> objects;
    for(const ConstDataObjectPath& path : input.getObjectsRecursive(Lines::OOClass())) {
        objects.push_back(path);
    }
    return objects;
}

IMPLEMENT_CREATABLE_OVITO_CLASS(LinesExpressionSelectionModifierDelegate);
OVITO_CLASSINFO(LinesExpressionSelectionModifierDelegate, "DisplayName", "Lines");

/******************************************************************************
* Indicates which data objects in the given input data collection the modifier
* delegate is able to operate on.
******************************************************************************/
QVector<DataObjectReference> LinesExpressionSelectionModifierDelegate::OOMetaClass::getApplicableObjects(const DataCollection& input) const
{
    // Gather list of all Lines objects in the input data collection.
    QVector<DataObjectReference> objects;
    for(const ConstDataObjectPath& path : input.getObjectsRecursive(Lines::OOClass())) {
        objects.push_back(path);
    }
    return objects;
}

IMPLEMENT_CREATABLE_OVITO_CLASS(LinesDeleteSelectedModifierDelegate);
OVITO_CLASSINFO(LinesDeleteSelectedModifierDelegate, "DisplayName", "Lines");

/******************************************************************************
* Indicates which data objects in the given input data collection the modifier
* delegate is able to operate on.
******************************************************************************/
QVector<DataObjectReference> LinesDeleteSelectedModifierDelegate::OOMetaClass::getApplicableObjects(const DataCollection& input) const
{
    // Gather list of all Lines objects in the input data collection.
    QVector<DataObjectReference> objects;
    for(const ConstDataObjectPath& path : input.getObjectsRecursive(Lines::OOClass())) {
        objects.push_back(path);
    }
    return objects;
}

/******************************************************************************
* Applies this modifier delegate to the data.
******************************************************************************/
Future<PipelineFlowState> LinesDeleteSelectedModifierDelegate::apply(
    const ModifierEvaluationRequest& request, PipelineFlowState&& state, const PipelineFlowState& originalState,
    const std::vector<PipelineFlowState>& additionalInputs)
{
    // The actual computation can be performed in a separate worker thread.
    return asyncLaunch([state = std::move(state), createdByNode = request.modificationNodeWeak(), inputObjectRef = inputDataObject()]() mutable {
        size_t numLines = 0;
        size_t numSelected = 0;

        // Process each line object in the data collection.
        visitObjectsToBeProcessed<Lines>(state, inputObjectRef, createdByNode, [&](const Lines* inputLines) {
            inputLines->verifyIntegrity();
            numLines += inputLines->elementCount();
            // Get the lines (vertex) selection.
            if(ConstPropertyPtr selProperty = inputLines->getProperty(Lines::SelectionProperty)) {
                // Make sure we can safely modify the lines object.
                Lines* outputLines = state.makeMutable(inputLines);

                // Remove selection property.
                outputLines->removeProperty(selProperty);

                // Delete the selected line vertices.
                numSelected += outputLines->deleteElements(std::move(selProperty));
            }
        });

        // Report some statistics:
        QString statusMessage = tr("%1 of %2 lines deleted (%3%)")
                                    .arg(numSelected)
                                    .arg(numLines)
                                    .arg((FloatType)numSelected * (FloatType)100 / (FloatType)std::max(numLines, (size_t)1), 0, 'f', 1);
        // Show the number of deleted line vertices next to the modifier's title in the pipeline editor.
        if(numSelected != 0)
            state.combineStatus(std::move(statusMessage),
                numSelected == 1 ? tr("1 line") : tr("%1 lines").arg(formatNumberForUI(numSelected)));
        else
            state.combineStatus(std::move(statusMessage));

        return std::move(state);
    });
}

IMPLEMENT_CREATABLE_OVITO_CLASS(LinesComputePropertyModifierDelegate);
OVITO_CLASSINFO(LinesComputePropertyModifierDelegate, "DisplayName", "Lines");

/******************************************************************************
* Indicates which data objects in the given input data collection the modifier
* delegate is able to operate on.
******************************************************************************/
QVector<DataObjectReference> LinesComputePropertyModifierDelegate::OOMetaClass::getApplicableObjects(const DataCollection& input) const
{
    // Gather list of all Lines objects in the input data collection.
    QVector<DataObjectReference> objects;
    for(const ConstDataObjectPath& path : input.getObjectsRecursive(Lines::OOClass())) {
        objects.push_back(path);
    }
    return objects;
}

}  // namespace Ovito
