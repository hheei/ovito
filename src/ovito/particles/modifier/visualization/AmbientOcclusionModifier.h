// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/particles/Particles.h>
#include <ovito/core/dataset/pipeline/Modifier.h>

namespace Ovito {

/**
 * \brief Calculates ambient occlusion lighting for particles.
 */
class OVITO_PARTICLES_EXPORT AmbientOcclusionModifier : public Modifier
{
    /// Give this modifier class its own metaclass.
    class AmbientOcclusionModifierClass : public ModifierClass
    {
    public:

        /// Inherit constructor from base class.
        using ModifierClass::ModifierClass;

        /// Asks the metaclass whether the modifier can be applied to the given input data.
        virtual bool isApplicableTo(const DataCollection& input) const override;
    };

    OVITO_CLASS_META(AmbientOcclusionModifier, AmbientOcclusionModifierClass)

public:

    enum { MAX_AO_RENDER_BUFFER_RESOLUTION = 4 };

    /// Is called by the pipeline system before a new modifier evaluation begins.
    virtual void preevaluateModifier(const ModifierEvaluationRequest& request, PipelineEvaluationResult::EvaluationTypes& evaluationTypes, TimeInterval& validityInterval) const override;

    /// Modifies the input data.
    virtual Future<PipelineFlowState> evaluateModifier(const ModifierEvaluationRequest& request, PipelineFlowState&& state) override;

    /// Indicates that a preliminary viewport update will be performed immediately after this modifier
	/// has computed new results.
    virtual bool shouldRefreshViewportsAfterEvaluation() override { return true; }

    /// Indicates whether the modifier wants to keep its partial compute results after one of its parameters has been changed.
    virtual bool shouldKeepPartialResultsAfterChange(const PropertyFieldEvent& event) override {
        // Avoid a full recomputation if the user toggles just the intensity.
        if(event.field() == PROPERTY_FIELD(intensity))
            return true;
        return Modifier::shouldKeepPartialResultsAfterChange(event);
    }

private:

    /// Calculates the ambient occlusion values for the given particles and returns them in a data buffer.
    Future<ConstDataBufferPtr> computeAmbientOcclusion(DataOORef<const Particles> particles) const;

private:

    /// This controls the intensity of the shading effect.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(FloatType{0.7}, intensity, setIntensity);

    /// Controls the quality of the lighting computation.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(int{40}, samplingCount, setSamplingCount);

    /// Controls the resolution of the offscreen rendering buffer.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(int{3}, bufferResolution, setBufferResolution);
};

}   // End of namespace
