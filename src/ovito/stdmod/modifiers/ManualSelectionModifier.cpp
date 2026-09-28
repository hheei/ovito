// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/stdmod/StdMod.h>
#include <ovito/stdobj/properties/Property.h>
#include <ovito/stdobj/properties/PropertyContainer.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/core/app/UserInterface.h>
#include "ManualSelectionModifier.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(ManualSelectionModifier);
OVITO_CLASSINFO(ManualSelectionModifier, "DisplayName", "Manual selection");
OVITO_CLASSINFO(ManualSelectionModifier, "Description", "Select individual particles or bonds using the mouse.");
OVITO_CLASSINFO(ManualSelectionModifier, "ModifierCategory", "Selection");

IMPLEMENT_CREATABLE_OVITO_CLASS(ManualSelectionModificationNode);
OVITO_CLASSINFO(ManualSelectionModificationNode, "ClassNameAlias", "ManualSelectionModifierApplication");  // For backward compatibility with OVITO 3.9.2
SET_MODIFICATION_NODE_TYPE(ManualSelectionModifier, ManualSelectionModificationNode);
DEFINE_REFERENCE_FIELD(ManualSelectionModificationNode, selectionSet);
SET_PROPERTY_FIELD_LABEL(ManualSelectionModificationNode, selectionSet, "Element selection set");

/******************************************************************************
* Constructor.
******************************************************************************/
void ManualSelectionModifier::initializeObject(ObjectInitializationFlags flags)
{
    GenericPropertyModifier::initializeObject(flags);

    // Operate on particles by default.
    setDefaultSubject(QStringLiteral("Particles"), QStringLiteral("Particles"));
}

/******************************************************************************
* This method is called by the system when the modifier has been inserted
* into a pipeline.
******************************************************************************/
void ManualSelectionModifier::initializeModifier(const ModifierInitializationRequest& request)
{
    Modifier::initializeModifier(request);

    // Take a snapshot of the existing selection state at the time the modifier is created.
    if(!getSelectionSet(request.modificationNode(), false)) {
        resetSelection(request.modificationNode(), request.modificationNode()->evaluateInput(request).blockForResult());
    }
}

/******************************************************************************
* Is called when the value of a property of this object has changed.
******************************************************************************/
void ManualSelectionModifier::propertyChanged(const PropertyFieldDescriptor* field)
{
    // Whenever the subject of this modifier is changed, reset the selection.
    if(field == PROPERTY_FIELD(GenericPropertyModifier::subject) && !shouldIgnoreChanges() && !isUndoingOrRedoing() && this_task::isInteractive()) {
        PipelineEvaluationRequest request(this_task::ui()->datasetContainer().currentAnimationTime());
        for(ModificationNode* node : nodes()) {
            try {
                resetSelection(node, node->evaluateInput(request).blockForResult());
            }
            catch(...) {
                // Ignore exceptions that occur during upstream pipeline evaluation.
            }
        }
    }
    GenericPropertyModifier::propertyChanged(field);
}

/******************************************************************************
* Modifies the input data.
******************************************************************************/
Future<PipelineFlowState> ManualSelectionModifier::evaluateModifier(const ModifierEvaluationRequest& request, PipelineFlowState&& state)
{
    // Retrieve the selection stored in the modifier application.
    ElementSelectionSet* selectionSet = getSelectionSet(request.modificationNode(), false);
    if(!selectionSet)
        throw Exception(tr("No stored selection set available. Please reset the selection state."));

    if(subject()) {
        PropertyContainer* container = state.expectMutableLeafObject(subject());
        container->verifyIntegrity();

        PipelineStatus status = selectionSet->applySelection(
                container,
                container->getOOMetaClass().isValidStandardPropertyId(Property::GenericIdentifierProperty) ?
                    container->getProperty(Property::GenericIdentifierProperty) : nullptr);

        state.setStatus(std::move(status));
    }

    return std::move(state);
}

/******************************************************************************
* Returns a short piece of information (typically a string or color) to be
* displayed next to the modifier's title in the pipeline editor list.
******************************************************************************/
QVariant ManualSelectionModifier::getPipelineEditorShortInfo(Scene* scene, ModificationNode* node) const
{
    OVITO_ASSERT(this_task::isMainThread());

    if(subject()) {
        // Note: Whenever the stored selection set changes, ManualSelectionModificationNode::referenceEvent()
        // triggers a ReferenceEvent::ObjectStatusChanged event to refresh the displayed info.
        if(const ManualSelectionModificationNode* myNode = dynamic_object_cast<ManualSelectionModificationNode>(node)) {
            if(const ElementSelectionSet* selectionSet = myNode->selectionSet()) {
                if(size_t count = selectionSet->selectedCount()) {
                    return QStringLiteral("%1 %2")
                        .arg(formatNumberForUI(count))
                        .arg(subject().dataClass()->elementDescriptionName());
                }
            }
        }
    }
    return {};
}

/******************************************************************************
* Returns the selection set object stored in the ModificationNode, or, if
* it does not exist, creates one.
******************************************************************************/
ElementSelectionSet* ManualSelectionModifier::getSelectionSet(ModificationNode* modApp, bool createIfNotExist)
{
    ManualSelectionModificationNode* myModApp = dynamic_object_cast<ManualSelectionModificationNode>(modApp);
    if(!myModApp)
        throw Exception(tr("Manual selection modifier is not associated with a ManualSelectionModificationNode."));

    ElementSelectionSet* selectionSet = myModApp->selectionSet();
    if(!selectionSet && createIfNotExist) {
        myModApp->setSelectionSet(OORef<ElementSelectionSet>::create());
        selectionSet = myModApp->selectionSet();
    }

    return selectionSet;
}

/******************************************************************************
* Adopts the selection state from the modifier's input.
******************************************************************************/
void ManualSelectionModifier::resetSelection(ModificationNode* modApp, const PipelineFlowState& state)
{
    if(subject()) {
        const PropertyContainer* container = state.expectLeafObject(subject());
        getSelectionSet(modApp, true)->resetSelection(container);
    }
}

/******************************************************************************
* Selects all elements.
******************************************************************************/
void ManualSelectionModifier::selectAll(ModificationNode* modApp, const PipelineFlowState& state)
{
    if(subject()) {
        const PropertyContainer* container = state.expectLeafObject(subject());
        getSelectionSet(modApp, true)->selectAll(container);
    }
}

/******************************************************************************
* Deselects all elements.
******************************************************************************/
void ManualSelectionModifier::clearSelection(ModificationNode* modApp, const PipelineFlowState& state)
{
    if(subject()) {
        const PropertyContainer* container = state.expectLeafObject(subject());
        getSelectionSet(modApp, true)->clearSelection(container);
    }
}

/******************************************************************************
* Inverts the selection state of all elements.
******************************************************************************/
void ManualSelectionModifier::invertSelection(ModificationNode* modApp, const PipelineFlowState& state)
{
    if(subject()) {
        const PropertyContainer* container = state.expectLeafObject(subject());
        getSelectionSet(modApp, true)->invertSelection(container);
    }
}

/******************************************************************************
* Toggles the selection state of a single element.
******************************************************************************/
void ManualSelectionModifier::toggleElementSelection(ModificationNode* modApp, const PipelineFlowState& state, size_t elementIndex)
{
    ElementSelectionSet* selectionSet = getSelectionSet(modApp, false);
    if(!selectionSet)
        throw Exception(tr("No stored selection set available. Please reset the selection state."));
    if(subject()) {
        const PropertyContainer* container = state.expectLeafObject(subject());
        selectionSet->toggleElement(container, elementIndex);
    }
}

/******************************************************************************
* Replaces the selection.
******************************************************************************/
void ManualSelectionModifier::setSelection(ModificationNode* modApp, const PipelineFlowState& state, ConstPropertyPtr selection, ElementSelectionSet::SelectionMode mode)
{
    if(subject()) {
        const PropertyContainer* container = state.expectLeafObject(subject());
        getSelectionSet(modApp, true)->setSelection(container, std::move(selection), mode);
    }
}

/******************************************************************************
* Is called when a RefTarget referenced by this object generated an event.
******************************************************************************/
bool ManualSelectionModificationNode::referenceEvent(RefTarget* source, const ReferenceEvent& event)
{
    if(event.type() == ReferenceEvent::TargetChanged && source == selectionSet()) {
        // Changes of the stored selection set affect the result of ManualSelectionModifier::getPipelineEditorShortInfo().
        notifyDependents(ReferenceEvent::ObjectStatusChanged);
    }

    return ModificationNode::referenceEvent(source, event);
}

}   // End of namespace
