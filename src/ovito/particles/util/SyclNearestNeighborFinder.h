// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/particles/Particles.h>
#include "SyclNeighborFinderBase.h"

namespace Ovito {

/**
 * \brief This utility class finds all neighbor particles within a cutoff radius of a central particle.
 */
class OVITO_PARTICLES_EXPORT SyclNearestNeighborFinder : public SyclNeighborFinderBase
{
public:

    class Accessor;

public:

    /// \brief Prepares the neighbor finder by sorting particles into a tree node structure.
    /// \param positions The data buffer containing the particle coordinates.
    /// \param simCell The input simulation cell geometry and boundary conditions.
    /// \param selection Determines which particles are included in the neighbor search (optional).
    /// \throw Exception on error.
    void prepare(const Property* positions, const SimulationCell* simCell, const Property* selection = nullptr);

private:

    /// The wrapped particle coordinates.
    DataBufferPtr _positions;

    /// The reduced particle coordinates.
    DataBufferPtr _reducedPositions;
};

}   // End of namespace
