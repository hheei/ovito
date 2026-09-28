// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/particles/Particles.h>
#include <ovito/particles/modifier/analysis/StructureIdentificationModifier.h>

namespace Ovito {

/**
 * \brief A modifier that identifies local diamond structures.
 */
class OVITO_PARTICLES_EXPORT IdentifyDiamondModifier : public StructureIdentificationModifier
{
    OVITO_CLASS(IdentifyDiamondModifier)

public:

    /// The structure types recognized by the modifier.
    enum StructureType {
        OTHER = 0,                  //< Unidentified structure
        CUBIC_DIAMOND,              //< Cubic diamond structure
        CUBIC_DIAMOND_FIRST_NEIGH,  //< First neighbor of a cubic diamond atom
        CUBIC_DIAMOND_SECOND_NEIGH, //< Second neighbor of a cubic diamond atom
        HEX_DIAMOND,                //< Hexagonal diamond structure
        HEX_DIAMOND_FIRST_NEIGH,    //< First neighbor of a hexagonal diamond atom
        HEX_DIAMOND_SECOND_NEIGH,   //< Second neighbor of a hexagonal diamond atom

        NUM_STRUCTURE_TYPES     //< This just counts the number of defined structure types.
    };
    Q_ENUM(StructureType);

public:

    /// Constructor.
    void initializeObject(ObjectInitializationFlags flags);

protected:

    /// Creates the engine that will perform the structure identification.
    virtual std::shared_ptr<Algorithm> createAlgorithm(const ModifierEvaluationRequest& request, const PipelineFlowState& input) override {
        return std::make_shared<DiamondIdentificationAlgorithm>(*this, input);
    }

private:

    /// Analysis engine that performs the structure identification
    class DiamondIdentificationAlgorithm : public StructureIdentificationModifier::Algorithm
    {
    public:

        /// Constructor.
        using Algorithm::Algorithm;

        /// Performs the atomic structure classification.
        virtual void identifyStructures() override;

        /// Computes the structure identification statistics.
        virtual std::vector<int64_t> computeStructureStatistics(const Property* structures, PipelineFlowState& state, const OOWeakRef<const PipelineNode>& createdByNode, const std::any& modifierParameters) const override;
    };
};

}   // End of namespace
