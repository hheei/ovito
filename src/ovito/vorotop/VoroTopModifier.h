// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/vorotop/VoroTopPlugin.h>
#include <ovito/stdobj/properties/Property.h>
#include <ovito/particles/modifier/analysis/StructureIdentificationModifier.h>
#include "Filter.h"

namespace voro {
    class voronoicell_neighbor; // Defined by Voro++
}

namespace Ovito::VoroTop {

/**
 * \brief This analysis modifier performs the Voronoi topology analysis developed by Emanuel A. Lazar.
 */
class OVITO_VOROTOP_EXPORT VoroTopModifier : public StructureIdentificationModifier
{
    OVITO_CLASS(VoroTopModifier)

public:

    /// Returns the VoroTop filter definition cached from the last analysis run.
    const std::shared_ptr<Filter>& filter() const { return _filter; }

protected:

    /// Is called when the value of a property of this object has changed.
    virtual void propertyChanged(const PropertyFieldDescriptor* field) override;

    /// Creates the engine that will perform the structure identification.
    virtual std::shared_ptr<Algorithm> createAlgorithm(const ModifierEvaluationRequest& request, const PipelineFlowState& input) override {
        return std::make_shared<VoroTopAnalysisAlgorithm>(*this, input, filterFile(), filter());
    }

private:

    /// Loads a new filter definition into the modifier.
    void loadFilterDefinition(const QString& filepath);

    /// Compute engine that performs the actual analysis in a background thread.
    class VoroTopAnalysisAlgorithm : public StructureIdentificationModifier::Algorithm
    {
    public:

        /// Constructor.
        VoroTopAnalysisAlgorithm(VoroTopModifier& modifier, const PipelineFlowState& input, const QString& filterFile, std::shared_ptr<Filter> filter) :
            Algorithm(modifier, input),
            _filterFile(filterFile),
            _filter(std::move(filter)),
            _radii(modifier.useRadii() ? particles()->inputParticleRadii() : nullptr) {}

        /// Performs the atomic structure classification.
        virtual void identifyStructures() override;

        /// Computes the structure identification statistics.
        virtual std::vector<int64_t> computeStructureStatistics(const Property* structures, PipelineFlowState& state, const OOWeakRef<const PipelineNode>& createdByNode, const std::any& modifierParameters) const override;

        /// Processes a single Voronoi cell.
        int processCell(voro::voronoicell_neighbor& vcell);

        /// Returns the VoroTop filter definition.
        const std::shared_ptr<Filter>& filter() const { return _filter; }

    private:

        /// The path of the external file containing the filter definition.
        QString _filterFile;

        /// The VoroTop filter definition.
        std::shared_ptr<Filter> _filter;

        /// The per-particle radii.
        ConstPropertyPtr _radii;
    };

private:

    /// Controls whether the weighted Voronoi tessellation is computed, which takes into account particle radii.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{false}, useRadii, setUseRadii);

    /// The external file path of the loaded filter file.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(QString{}, filterFile, setFilterFile);

    /// The VoroTop filter definition cached from the last analysis run.
    std::shared_ptr<Filter> _filter;
};

}   // End of namespace
