// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once

#include <ovito/particles/Particles.h>
#include <ovito/particles/modifier/analysis/StructureIdentificationModifier.h>

#include <complex>
#include <boost/math/special_functions/spherical_harmonic.hpp>
#include <boost/numeric/ublas/matrix.hpp>

namespace Ovito {

/**
 * \brief This modifier implements the Chill+ algorithm [Nguyen & Molinero, J. Phys. Chem. B 2015, 119, 9369-9376]
 *        for identifying various water phases.
 */
class OVITO_PARTICLES_EXPORT ChillPlusModifier : public StructureIdentificationModifier
{
    OVITO_CLASS(ChillPlusModifier)

public:

    /// The structure types recognized by the Chill+ algorithm.
    enum StructureType {
        OTHER = 0,              //< Unidentified structure
        HEXAGONAL_ICE,          //< Hexagonal ice
        CUBIC_ICE,              //< Cubic ice
        INTERFACIAL_ICE,        //< Interfacial ice
        HYDRATE,                //< Hydrate
        INTERFACIAL_HYDRATE,    //< Interfacial hydrate

        NUM_STRUCTURE_TYPES     //< This just counts the number of defined structure types.
    };
    Q_ENUM(StructureType);

    /// Constructor.
    void initializeObject(ObjectInitializationFlags flags);

protected:

    /// Creates the engine that will perform the structure identification.
    virtual std::shared_ptr<Algorithm> createAlgorithm(const ModifierEvaluationRequest& request, const PipelineFlowState& input) override {
        return std::make_shared<ChillPlusAlgorithm>(*this, input, cutoff());
    }

    /// Computes the modifier's results.
    class ChillPlusAlgorithm : public StructureIdentificationModifier::Algorithm
    {
    public:

        /// Constructor.
        ChillPlusAlgorithm(const StructureIdentificationModifier& modifier, const PipelineFlowState& input, FloatType cutoff) :
            Algorithm(modifier, input),
            _cutoff(cutoff) {}

        /// Performs the atomic structure classification.
        virtual void identifyStructures() override;

        /// Computes the structure identification statistics.
        virtual std::vector<int64_t> computeStructureStatistics(const Property* structures, PipelineFlowState& state, const OOWeakRef<const PipelineNode>& createdByNode, const std::any& modifierParameters) const override;

        /// Returns the value of the cutoff parameter.
        FloatType cutoff() const { return _cutoff; }

    private:

        /// Implementation of the identification algorithm.
        static StructureType determineStructure(const CutoffNeighborFinder& neighFinder, size_t particleIndex, const boost::numeric::ublas::matrix<std::complex<float>>& q_values);

        const FloatType _cutoff;
    };

    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{3.5}, cutoff, setCutoff, PROPERTY_FIELD_MEMORIZE);
};

}   // End of namespace
