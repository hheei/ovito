// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/particles/Particles.h>
#include <ovito/particles/modifier/analysis/StructureIdentificationModifier.h>

namespace Ovito {

/**
 * \brief A modifier that performs the structure identification method developed by Ackland and Jones.
 *
 * See G. Ackland, PRB(2006)73:054104.
 */
class OVITO_PARTICLES_EXPORT AcklandJonesModifier : public StructureIdentificationModifier
{
    OVITO_CLASS(AcklandJonesModifier)

public:

    /// The structure types recognized by the bond angle analysis.
    enum StructureType {
        OTHER = 0,              //< Unidentified structure
        FCC,                    //< Face-centered cubic
        HCP,                    //< Hexagonal close-packed
        BCC,                    //< Body-centered cubic
        ICO,                    //< Icosahedral structure

        NUM_STRUCTURE_TYPES     //< This just counts the number of defined structure types.
    };
    Q_ENUM(StructureType);

public:

    /// Constructor.
    void initializeObject(ObjectInitializationFlags flags);

protected:

    /// Creates the engine that will perform the structure identification.
    virtual std::shared_ptr<Algorithm> createAlgorithm(const ModifierEvaluationRequest& request, const PipelineFlowState& input) override {
        return std::make_shared<AcklandJonesAnalysisAlgorithm>(*this, input);
    }

    /// Computes the modifier's results.
    class AcklandJonesAnalysisAlgorithm : public StructureIdentificationModifier::Algorithm
    {
    public:

        /// Constructor.
        using Algorithm::Algorithm;

        /// Performs the atomic structure classification.
        virtual void identifyStructures() override;

        /// Computes the structure identification statistics.
        virtual std::vector<int64_t> computeStructureStatistics(const Property* structures, PipelineFlowState& state, const OOWeakRef<const PipelineNode>& createdByNode, const std::any& modifierParameters) const override;

    private:

        /// Determines the coordination structure of a single particle using the bond-angle analysis method.
        StructureType determineStructure(NearestNeighborFinder& neighFinder, size_t particleIndex) const;
    };
};

}   // End of namespace
