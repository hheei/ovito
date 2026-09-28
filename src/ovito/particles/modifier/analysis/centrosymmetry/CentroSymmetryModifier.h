// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/particles/Particles.h>
#include <ovito/core/dataset/pipeline/Modifier.h>

namespace Ovito {

/**
 * \brief Calculates the centrosymmetry parameter (CSP) for particles.
 */
class OVITO_PARTICLES_EXPORT CentroSymmetryModifier : public Modifier
{
    /// Give this modifier class its own metaclass.
    class CentroSymmetryModifierClass : public Modifier::OOMetaClass
    {
    public:

        /// Inherit constructor from base metaclass.
        using Modifier::OOMetaClass::OOMetaClass;

        /// Asks the metaclass whether the modifier can be applied to the given input data.
        virtual bool isApplicableTo(const DataCollection& input) const override;
    };

    OVITO_CLASS_META(CentroSymmetryModifier, CentroSymmetryModifierClass)

public:

    enum CSPMode {
        ConventionalMode,   ///< Performs the conventional CSP.
        MatchingMode,       ///< Performs the minimum-weight matching CSP.
    };
    Q_ENUM(CSPMode);

    /// The maximum number of neighbors that can be taken into account to compute the CSP.
    enum { MAX_CSP_NEIGHBORS = 32 };

public:

    /// Is called by the pipeline system before a new modifier evaluation begins.
    virtual void preevaluateModifier(const ModifierEvaluationRequest& request, PipelineEvaluationResult::EvaluationTypes& evaluationTypes, TimeInterval& validityInterval) const override;

    /// Modifies the input data.
    virtual Future<PipelineFlowState> evaluateModifier(const ModifierEvaluationRequest& request, PipelineFlowState&& state) override;

    /// Indicates that a preliminary viewport update will be performed immediately after this modifier
	/// has computed new results.
    virtual bool shouldRefreshViewportsAfterEvaluation() override { return true; }

protected:

    /// Computes the centrosymmetry parameter of a single particle.
    static FloatType computeCSP(NearestNeighborFinder& neighList, size_t particleIndex, CSPMode mode);

private:

    /// Specifies the number of nearest neighbors to take into account when computing the CSP.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(int{12}, numNeighbors, setNumNeighbors, PROPERTY_FIELD_MEMORIZE);

    /// Controls how the CSP is performed.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(CSPMode{ConventionalMode}, mode, setMode, PROPERTY_FIELD_MEMORIZE);

    /// Controls whether analysis should take into account only selected particles.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{false}, onlySelectedParticles, setOnlySelectedParticles);
};

}   // End of namespace
