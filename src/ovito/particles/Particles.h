// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

//
// Standard precompiled header file included by all source files in this module
//

#ifndef __OVITO_PARTICLES_
#define __OVITO_PARTICLES_

#include <ovito/core/Core.h>
#include <ovito/mesh/Mesh.h>
#include <ovito/grid/Grid.h>
#include <ovito/stdobj/StdObj.h>

namespace Ovito {
class ParticleType;
class Particles;
class BondType;
class Bonds;
class Angles;
class Dihedrals;
class Impropers;
class ParticlesVis;
class BondsVis;
class ParticleBondMap;
class ParticleImporter;
class NearestNeighborFinder;
class CutoffNeighborFinder;
}  // namespace Ovito

#endif
