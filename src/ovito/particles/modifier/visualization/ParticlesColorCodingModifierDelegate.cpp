// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/particles/Particles.h>
#include <ovito/particles/objects/Particles.h>
#include <ovito/stdobj/vectors/VectorVis.h>
#include <ovito/stdobj/properties/PropertyContainer.h>
#include "ParticlesColorCodingModifierDelegate.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(ParticlesColorCodingModifierDelegate);
OVITO_CLASSINFO(ParticlesColorCodingModifierDelegate, "DisplayName", "Particles");
IMPLEMENT_CREATABLE_OVITO_CLASS(ParticleVectorsColorCodingModifierDelegate);
OVITO_CLASSINFO(ParticleVectorsColorCodingModifierDelegate, "DisplayName", "Particle vectors");
IMPLEMENT_CREATABLE_OVITO_CLASS(BondsColorCodingModifierDelegate);
OVITO_CLASSINFO(BondsColorCodingModifierDelegate, "DisplayName", "Bonds");

/******************************************************************************
* Indicates which data objects in the given input data collection the modifier
* delegate is able to operate on.
******************************************************************************/
QVector<DataObjectReference> ParticlesColorCodingModifierDelegate::OOMetaClass::getApplicableObjects(const DataCollection& input) const
{
    if(input.containsObject<Particles>())
        return { DataObjectReference(&Particles::OOClass()) };
    return {};
}

/******************************************************************************
* Indicates which data objects in the given input data collection the modifier
* delegate is able to operate on.
******************************************************************************/
QVector<DataObjectReference> ParticleVectorsColorCodingModifierDelegate::OOMetaClass::getApplicableObjects(const DataCollection& input) const
{
    if(const Particles* particles = input.getObject<Particles>()) {
        for(const Property* property : particles->properties()) {
            if(property->visElement<VectorVis>() != nullptr)
                return { DataObjectReference(&Particles::OOClass(), {}, ParticleVectorsColorCodingModifierDelegate::OOClass().displayName()) };
        }
    }
    return {};
}

/******************************************************************************
* Indicates which data objects in the given input data collection the modifier
* delegate is able to operate on.
******************************************************************************/
QVector<DataObjectReference> BondsColorCodingModifierDelegate::OOMetaClass::getApplicableObjects(const DataCollection& input) const
{
    if(const Particles* particles = input.getObject<Particles>()) {
        if(particles->bonds())
            return { DataObjectReference(ConstDataObjectPath({particles, particles->bonds()})) };
    }
    return {};
}

}   // End of namespace
