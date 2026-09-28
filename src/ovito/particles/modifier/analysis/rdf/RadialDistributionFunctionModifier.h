// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once

#include <ovito/particles/Particles.h>
#include <ovito/core/dataset/pipeline/Modifier.h>
#include <ovito/stdobj/properties/PropertyReference.h>

namespace Ovito {

/**
 * \brief This modifier computes the coordination number of each particle (i.e. number of neighbors within a given cutoff range)
 *        as well as the radial pair distribution function (RDF) of the system.
 */
class OVITO_PARTICLES_EXPORT RadialDistributionFunctionModifier : public Modifier
{
    /// Give this modifier class its own metaclass.
    class RadialDistributionFunctionModifierClass : public Modifier::OOMetaClass
    {
    public:
        /// Inherit constructor from base metaclass.
        using Modifier::OOMetaClass::OOMetaClass;

        /// Asks the metaclass whether the modifier can be applied to the given input data.
        [[nodiscard]] virtual bool isApplicableTo(const DataCollection& input) const override;
    };

    OVITO_CLASS_META(RadialDistributionFunctionModifier, RadialDistributionFunctionModifierClass)

public:

    /// Identifier of the table produced by the modifier.
    static constexpr QStringView TableIdentifier = u"coordination-rdf";

    /// Constructor.
    void initializeObject(ObjectInitializationFlags flags);

    /// Is called by the pipeline system before a new modifier evaluation begins.
    virtual void preevaluateModifier(const ModifierEvaluationRequest& request,
                                     PipelineEvaluationResult::EvaluationTypes& evaluationTypes,
                                     TimeInterval& validityInterval) const override;

    /// Modifies the input data.
    virtual Future<PipelineFlowState> evaluateModifier(const ModifierEvaluationRequest& request, PipelineFlowState&& state) override;

    /// Indicates that a preliminary viewport update will be performed immediately after this modifier
    /// has computed new results.
    virtual bool shouldRefreshViewportsAfterEvaluation() override { return true; }

private:
    /// Controls the cutoff radius for the neighbor lists.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{3.2}, cutoff, setCutoff, PROPERTY_FIELD_MEMORIZE);

    /// Controls the number of RDF histogram bins.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(int{200}, numberOfBins, setNumberOfBins, PROPERTY_FIELD_MEMORIZE);

    /// Controls the computation of partials RDFs.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool{false}, computePartialRDF, setComputePartialRDF, PROPERTY_FIELD_MEMORIZE);

    /// Controls whether the modifier acts only on currently selected particles.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{false}, onlySelected, setOnlySelected);

    /// The particle property that is used as the source for type classification when computing partial RDFs.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(PropertyReference{}, typeProperty, setTypeProperty);
};

}  // namespace Ovito
