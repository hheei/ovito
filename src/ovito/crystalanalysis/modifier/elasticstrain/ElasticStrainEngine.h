// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/crystalanalysis/CrystalAnalysis.h>
#include <ovito/particles/modifier/analysis/StructureIdentificationModifier.h>
#include <ovito/crystalanalysis/modifier/structureanalysis/StructureAnalysis.h>

namespace Ovito {

/*
 * Computation engine of the ElasticStrainModifier, which performs the actual strain tensor calculation.
 */
class ElasticStrainEngine : public StructureIdentificationModifier::Algorithm
{
public:

    /// Constructor.
    ElasticStrainEngine(const StructureIdentificationModifier& modifier, const PipelineFlowState& input,
            int inputCrystalStructure, std::vector<Matrix3> preferredCrystalOrientations,
            bool calculateDeformationGradients, bool calculateStrainTensors,
            FloatType latticeConstant, FloatType caRatio, bool pushStrainTensorsForward);

    /// Performs the atomic structure classification.
    virtual void identifyStructures() override;

    /// Computes the structure identification statistics.
    virtual std::vector<int64_t> computeStructureStatistics(const Property* structures, PipelineFlowState& state, const OOWeakRef<const PipelineNode>& createdByNode, const std::any& modifierParameters) const override;

    /// Returns the array of atom cluster IDs.
    const PropertyPtr& atomClusters() const { return _atomClusters; }

    /// Assigns the array of atom cluster IDs.
    void setAtomClusters(PropertyPtr prop) { _atomClusters = std::move(prop); }

    /// Returns the created cluster graph.
    const DataOORef<ClusterGraph>& clusterGraph() const { return _clusterGraph; }

    /// Returns the property storage that contains the computed per-particle volumetric strain values.
    const PropertyPtr& volumetricStrains() const { return _volumetricStrains; }

    /// Returns the property storage that contains the computed per-particle strain tensors.
    const PropertyPtr& strainTensors() const { return _strainTensors; }

    /// Returns the property storage that contains the computed per-particle deformation gradient tensors.
    const PropertyPtr& deformationGradients() const { return _deformationGradients; }

private:

    const int _inputCrystalStructure;
    FloatType _latticeConstant;
    FloatType _axialScaling;
    const bool _pushStrainTensorsForward;
    std::vector<Matrix3> _preferredCrystalOrientations;
    std::optional<StructureAnalysis> _structureAnalysis;

    /// Atom-to-cluster assignments computed by the modifier.
    PropertyPtr _atomClusters;

    /// Cluster graph computed by the modifier.
    DataOORef<ClusterGraph> _clusterGraph = DataOORef<ClusterGraph>::create();

    /// Results of the modifier.
    const PropertyPtr _volumetricStrains;

    /// Results of the modifier.
    const PropertyPtr _strainTensors;

    /// Results of the modifier.
    const PropertyPtr _deformationGradients;
};

}   // End of namespace
