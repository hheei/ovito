// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/crystalanalysis/CrystalAnalysis.h>
#include <ovito/particles/modifier/analysis/StructureIdentificationModifier.h>
#include <ovito/crystalanalysis/modifier/structureanalysis/StructureAnalysis.h>

namespace Ovito {

/*
 * Extracts dislocation lines from a crystal.
 */
class OVITO_CRYSTALANALYSIS_EXPORT ElasticStrainModifier : public StructureIdentificationModifier
{
    OVITO_CLASS(ElasticStrainModifier)

public:

    /// Constructor.
    void initializeObject(ObjectInitializationFlags flags);

protected:

    /// Creates the engine that will perform the structure identification.
    virtual std::shared_ptr<Algorithm> createAlgorithm(const ModifierEvaluationRequest& request, const PipelineFlowState& input) override;

private:

    /// The type of crystal to be analyzed.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(StructureAnalysis::LatticeStructureType{StructureAnalysis::LATTICE_FCC}, inputCrystalStructure, setInputCrystalStructure, PROPERTY_FIELD_MEMORIZE);

    /// Controls whether atomic deformation gradient tensors should be computed and stored.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool{false}, calculateDeformationGradients, setCalculateDeformationGradients, PROPERTY_FIELD_MEMORIZE);

    /// Controls whether atomic strain tensors should be computed and stored.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool{true}, calculateStrainTensors, setCalculateStrainTensors, PROPERTY_FIELD_MEMORIZE);

    /// Controls whether the calculated strain tensors should be pushed forward to the spatial reference frame.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool{true}, pushStrainTensorsForward, setPushStrainTensorsForward, PROPERTY_FIELD_MEMORIZE);

    /// The lattice parameter of ideal crystal.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{1}, latticeConstant, setLatticeConstant, PROPERTY_FIELD_MEMORIZE);

    /// The c/a ratio of the ideal crystal.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{std::sqrt(8.0/3.0)}, axialRatio, setAxialRatio, PROPERTY_FIELD_MEMORIZE);
};

}   // End of namespace
