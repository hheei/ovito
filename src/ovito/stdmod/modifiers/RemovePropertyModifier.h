// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once

#include <ovito/stdmod/StdMod.h>
#include <ovito/stdobj/properties/GenericPropertyModifier.h>
#include <ovito/stdobj/properties/PropertyReference.h>

namespace Ovito {

/**
 * \brief This modifier averages the values of a property.
 */
class OVITO_STDMOD_EXPORT RemovePropertyModifier : public GenericPropertyModifier
{
    OVITO_CLASS(RemovePropertyModifier)

public:
    /// Constructor.
    void initializeObject(ObjectInitializationFlags flags);

    /// Modifies the input data.
    virtual Future<PipelineFlowState> evaluateModifier(const ModifierEvaluationRequest& request, PipelineFlowState&& state) override;

    /// Returns a short piece of information (typically a string or color) to be displayed next to the modifier's title in the pipeline
    /// editor list.
    virtual QVariant getPipelineEditorShortInfo(Scene* scene, ModificationNode* node) const override;

protected:
    /// Is called when the value of a property of this object has changed.
    virtual void propertyChanged(const PropertyFieldDescriptor* field) override;

private:
    /// Properties to be removed from the container
    DECLARE_MODIFIABLE_PROPERTY_FIELD(QStringList{}, propertiesToRemove, setPropertiesToRemove);
};
}  // namespace Ovito