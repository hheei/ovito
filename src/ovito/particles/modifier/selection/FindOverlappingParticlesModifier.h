// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once

#include <ovito/particles/Particles.h>
#include <ovito/particles/objects/Particles.h>
#include <ovito/core/dataset/pipeline/Modifier.h>

namespace Ovito {

/**
 * \brief Modifier that removes overlapping
 */
class OVITO_PARTICLES_EXPORT FindOverlappingParticlesModifier : public Modifier
{
    /// Give this modifier class its own metaclass.
    class OVITO_PARTICLES_EXPORT FindOverlappingParticlesModifierClass : public Modifier::OOMetaClass
    {
    public:
        /// Inherit constructor from base metaclass.
        using Modifier::OOMetaClass::OOMetaClass;

        /// Asks the metaclass whether the modifier can be applied to the given input data.
        [[nodiscard]] virtual bool isApplicableTo(const DataCollection& input) const override;
    };

    OVITO_CLASS_META(FindOverlappingParticlesModifier, FindOverlappingParticlesModifierClass)

public:
    /// Modifies the input data.
    [[nodiscard]] virtual Future<PipelineFlowState> evaluateModifier(const ModifierEvaluationRequest& request,
                                                                     PipelineFlowState&& state) override;

private:
    /// The maximum distance between two atoms to be considered overlapping.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(float{0.05}, overlapDistance, setOverlapDistance, PROPERTY_FIELD_MEMORIZE);

    /// Apply to selected particles only
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool{false}, applyToSelection, setApplyToSelection, PROPERTY_FIELD_MEMORIZE);

    /// Keep single particle
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool{true}, keepOne, setKeepOne, PROPERTY_FIELD_MEMORIZE);

    /// Move particles to mean position
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool{false}, moveToMeanPosition, setMoveToMeanPosition, PROPERTY_FIELD_MEMORIZE);
};

}  // namespace Ovito