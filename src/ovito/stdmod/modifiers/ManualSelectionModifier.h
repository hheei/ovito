// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/stdmod/StdMod.h>
#include <ovito/stdobj/util/ElementSelectionSet.h>
#include <ovito/stdobj/properties/GenericPropertyModifier.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>

namespace Ovito {

/**
 * Modifiers that allows the user to select individual elements, e.g. particles or bonds, by hand.
 */
class OVITO_STDMOD_EXPORT ManualSelectionModifier : public GenericPropertyModifier
{
    OVITO_CLASS(ManualSelectionModifier)

public:

    /// Constructor.
    void initializeObject(ObjectInitializationFlags flags);

    /// This method is called by the system after the modifier has been inserted into a data pipeline.
    virtual void initializeModifier(const ModifierInitializationRequest& request) override;

    /// Modifies the input data.
    virtual Future<PipelineFlowState> evaluateModifier(const ModifierEvaluationRequest& request, PipelineFlowState&& state) override;

    /// Adopts the selection state from the modifier's input.
    void resetSelection(ModificationNode* modApp, const PipelineFlowState& state);

    /// Selects all elements.
    void selectAll(ModificationNode* modApp, const PipelineFlowState& state);

    /// Deselects all elements.
    void clearSelection(ModificationNode* modApp, const PipelineFlowState& state);

    /// Inverts the selection state of all elements.
    void invertSelection(ModificationNode* modApp, const PipelineFlowState& state);

    /// Toggles the selection state of a single element.
    void toggleElementSelection(ModificationNode* modApp, const PipelineFlowState& state, size_t elementIndex);

    /// Replaces the selection.
    void setSelection(ModificationNode* modApp, const PipelineFlowState& state, ConstPropertyPtr selection, ElementSelectionSet::SelectionMode mode);

    /// Returns a short piece of information (typically a string or color) to be displayed next to the modifier's title in the pipeline editor list.
    virtual QVariant getPipelineEditorShortInfo(Scene* scene, ModificationNode* node) const override;

protected:

    /// Is called when the value of a property of this object has changed.
    virtual void propertyChanged(const PropertyFieldDescriptor* field) override;

    /// Returns the selection set object stored in the ModificationNode, or, if it does not exist, creates one when requested.
    ElementSelectionSet* getSelectionSet(ModificationNode* modApp, bool createIfNotExist);
};

/**
 * \brief The type of ModificationNode create for a ManualSelectionModifier
 *        when it is inserted into in a data pipeline.
 */
class OVITO_STDMOD_EXPORT ManualSelectionModificationNode : public ModificationNode
{
    OVITO_CLASS(ManualSelectionModificationNode)

protected:

    /// Is called when a RefTarget referenced by this object generated an event.
    virtual bool referenceEvent(RefTarget* source, const ReferenceEvent& event) override;

private:

    /// The per-application data of the modifier.
    DECLARE_MODIFIABLE_REFERENCE_FIELD_FLAGS(OORef<ElementSelectionSet>, selectionSet, setSelectionSet, PROPERTY_FIELD_ALWAYS_CLONE);
};

}   // End of namespace
