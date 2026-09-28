// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/particles/Particles.h>
#include <ovito/particles/objects/Particles.h>
#include <ovito/particles/objects/Bonds.h>
#include <ovito/stdobj/properties/PropertyExpressionEvaluator.h>

namespace Ovito {

/**
 * \brief Helper class that evaluates one or more math expressions for every particle.
 *
 * This class is used by the ComputePropertyModifier and the ExpressionSelectionModifier.
 */
class OVITO_PARTICLES_EXPORT ParticleExpressionEvaluator : public PropertyExpressionEvaluator
{
public:

    /// Constructor.
    ParticleExpressionEvaluator() {
        setIndexVarName("ParticleIndex");
    }

protected:

    /// Initializes the list of input variables from the given input state.
    virtual void createInputVariables(const std::vector<ConstPropertyPtr>& inputProperties, const SimulationCell* simCell, const QVariantMap& attributes, int animationFrame) override;
};

/**
 * \brief Helper class that evaluates one or more math expressions for every bond.
 */
class OVITO_PARTICLES_EXPORT BondExpressionEvaluator : public PropertyExpressionEvaluator
{
public:

    /// Constructor.
    BondExpressionEvaluator() {
        setIndexVarName("BondIndex");
    }

    /// Specifies the expressions to be evaluated for each bond and creates the input variables.
    virtual void initializeInputs(const PipelineFlowState& state, const ConstDataObjectPath& containerPath, int animationFrame) override;

    /// Returns a human-readable text listing the input variables.
    virtual QString inputVariableTable() const override;

protected:

    /// Updates the stored value of variables that depends on the current element index.
    virtual void updateVariables(Worker& worker, size_t elementIndex) override;

private:

    /// Holds a reference to the bond topology property.
    BufferReadAccessAndRef<ParticleIndexPair> _topologyArray;
};

}   // End of namespace
