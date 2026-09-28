// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/stdmod/StdMod.h>
#include <ovito/stdobj/properties/GenericPropertyModifier.h>
#include <ovito/stdobj/properties/PropertyReference.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>

namespace Ovito {

/**
 * \brief Assigns colors to data elements based on a typed property.
 */
class OVITO_STDMOD_EXPORT ColorByTypeModifier : public GenericPropertyModifier
{
    OVITO_CLASS(ColorByTypeModifier)

public:

    /// Constructor.
    void initializeObject(ObjectInitializationFlags flags);

    /// This method is called by the system after the modifier has been inserted into a data pipeline.
    virtual void initializeModifier(const ModifierInitializationRequest& request) override;

    /// Modifies the input data.
    virtual Future<PipelineFlowState> evaluateModifier(const ModifierEvaluationRequest& request, PipelineFlowState&& state) override;

    /// Indicates whether the interactive viewports should be updated after a parameter of the modifier has
    /// been changed and before the entire pipeline is recomputed.
    virtual bool shouldRefreshViewportsAfterChange() override { return true; }

    /// Returns a short piece of information (typically a string or color) to be displayed next to the modifier's title in the pipeline editor list.
    virtual QVariant getPipelineEditorShortInfo(Scene* scene, ModificationNode* node) const override {
        // Note: Whenever the source property changes, we trigger a ReferenceEvent::ObjectStatusChanged event in propertyChanged().
        return sourceProperty().nameWithComponent();
    }

    /// Implementation of the color-by-type algorithm.
    /// Returns true if at least one element had an undefined type ID, which means that the modifier assigned a default color to it.
    static bool colorByType(const Property* typeProperty, PropertyContainer* container, const ConstDataObjectPath& containerPath = {}, const Property* selection = nullptr);

protected:

    /// Is called when the value of a property of this object has changed.
    virtual void propertyChanged(const PropertyFieldDescriptor* field) override;

private:

    /// The input type property that is used as data source for the selection.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(PropertyReference{}, sourceProperty, setSourceProperty);

    /// Controls whether the modifier assigns a color only to selected elements.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{false}, colorOnlySelected, setColorOnlySelected);

    /// Controls whether the input selection is preserved or not. If true, the current selection is cleared by the modifier to reveal the assigned colors in the interactive viewports of OVITO.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{true}, clearSelection, setClearSelection);
};

}   // End of namespace
