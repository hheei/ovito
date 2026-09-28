// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/particles/Particles.h>
#include "ParticleBondMap.h"
#include "Particles.h"

namespace Ovito {

/******************************************************************************
* Initializes the helper class.
******************************************************************************/
ParticleBondMap::ParticleBondMap(ConstPropertyPtr bondTopology, ConstPropertyPtr bondPeriodicImages) :
    _bondTopology(std::move(bondTopology)),
    _bondPeriodicImages(std::move(bondPeriodicImages)),
    _nextBond(_bondTopology.size()*2, _bondTopology.size()*2)
{
    for(size_t bondIndex = _bondTopology.size(); bondIndex-- != 0; ) {
        size_t index1 = _bondTopology[bondIndex][0];
        size_t index2 = _bondTopology[bondIndex][1];
        if(index1 >= _startIndices.size())
            _startIndices.resize(index1 + 1, endOfListValue());
        if(index2 >= _startIndices.size())
            _startIndices.resize(index2 + 1, endOfListValue());

        size_t evenIndex = bondIndex * 2;
        size_t oddIndex  = evenIndex + 1;
        _nextBond[evenIndex] = _startIndices[index1];
        _nextBond[oddIndex]  = _startIndices[index2];
        _startIndices[index1] = evenIndex;
        _startIndices[index2] = oddIndex;
    }
}

/******************************************************************************
* Initializes the helper class.
******************************************************************************/
ParticleBondMap::ParticleBondMap(const Bonds& bonds) :
    ParticleBondMap(bonds.expectProperty(Bonds::TopologyProperty), bonds.getProperty(Bonds::PeriodicImageProperty))
{
}

}   // End of namespace
