// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/particles/Particles.h>
#include <ovito/particles/objects/Particles.h>
#include <ovito/core/dataset/pipeline/PipelineFlowState.h>
#include <ovito/core/dataset/io/FileExporter.h>

namespace Ovito {

/**
 * \brief Abstract base class for export services that write particle datasets to an output file.
 */
class OVITO_PARTICLES_EXPORT ParticleExporter : public FileExporter
{
    OVITO_CLASS(ParticleExporter)

public:

    /// Returns the type(s) of data objects that this exporter service can export.
    virtual std::vector<DataObjectClassPtr> exportableDataObjectClass() override {
        return { &Particles::OOClass() };
    }

    /// Evaluates the pipeline whose data is to be exported.
    virtual Future<PipelineFlowState> getPipelineDataToBeExported(int frameNumber) const override;
};

}   // End of namespace
