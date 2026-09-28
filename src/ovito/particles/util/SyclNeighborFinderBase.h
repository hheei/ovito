// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/particles/Particles.h>
#include <ovito/stdobj/simcell/SimulationCell.h>

namespace Ovito {

/**
 * \brief This utility class finds all neighbor particles within a cutoff radius of a central particle.
 */
class OVITO_PARTICLES_EXPORT SyclNeighborFinderBase
{
public:

    /// Default constructor.
    SyclNeighborFinderBase() = default;

    /// Copying is not supported by this class.
    SyclNeighborFinderBase(const SyclNeighborFinderBase&) = delete;

    /// Copying is not supported by this class.
    SyclNeighborFinderBase& operator=(const SyclNeighborFinderBase&) = delete;

    /// Returns the simulation cell used by the neighbor finder. This may be an ad-hoc cell constructed from the bounding box of particle coordinates.
    const SimulationCell* simulationCell() const { return _simCell; }

    /// Returns the number of particles stored by the neighbor finder.
    size_t localParticleCount() const { return _localParticleCount; }

protected:

    /// \brief Prepares the neighbor finder by sorting particles into a grid of bin cells.
    /// \param positions The data buffer containing the particle coordinates.
    /// \param simCell The input simulation cell geometry and boundary conditions.
    /// \param selection Determines which particles are included in the neighbor search (optional).
    /// \throw Exception on error.
    void prepare(const Property* positions, const SimulationCell* simCell, const Property* selection);

protected:

    /// The number of particles stored by the neighbor finder.
    size_t _localParticleCount;

    /// The simulation cell and boundary conditions.
    DataOORef<const SimulationCell> _simCell;

    /// Mapping of global particle indices to local indices.
    ConstDataBufferPtr _packMapping;

    /// Mapping of local particle indices to global indices.
    ConstDataBufferPtr _unpackMapping;
};

}   // End of namespace
