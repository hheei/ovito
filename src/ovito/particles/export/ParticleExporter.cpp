// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/particles/Particles.h>
#include <ovito/particles/objects/Particles.h>
#include <ovito/particles/objects/Bonds.h>
#include "ParticleExporter.h"

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(ParticleExporter);

/******************************************************************************
* Evaluates the pipeline whose data is to be exported.
******************************************************************************/
Future<PipelineFlowState> ParticleExporter::getPipelineDataToBeExported(int frameNumber) const
{
    const PipelineFlowState state = co_await FileExporter::getPipelineDataToBeExported(frameNumber);

    const Particles* particles = state.getObject<Particles>();
    if(!particles)
        throw Exception(tr("The selected data collection does not contain any particles that can be exported."));
    if(!particles->getProperty(Particles::PositionProperty))
        throw Exception(tr("The particles to be exported do not have any coordinates ('Position' property is missing)."));

    // Verify per-particle data, make sure array lengths are consistent.
    particles->verifyIntegrity();

    // Verify topological data, make sure array lengths are consistent.
    if(particles->bonds())
        particles->bonds()->verifyIntegrity();
    if(particles->angles())
        particles->angles()->verifyIntegrity();
    if(particles->dihedrals())
        particles->dihedrals()->verifyIntegrity();
    if(particles->impropers())
        particles->impropers()->verifyIntegrity();

    co_return state;
}

}   // End of namespace
