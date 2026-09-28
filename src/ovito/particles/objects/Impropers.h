// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/particles/Particles.h>
#include <ovito/stdobj/properties/PropertyContainer.h>

namespace Ovito {

/**
 * \brief This data object stores a list of molecular impropers, i.e. quadruplets of particles.
 */
class OVITO_PARTICLES_EXPORT Impropers : public PropertyContainer
{
    /// Define a new property metaclass for the property container.
    class OVITO_PARTICLES_EXPORT OOMetaClass : public PropertyContainerClass
    {
    public:

        /// Inherit constructor from base class.
        using PropertyContainerClass::PropertyContainerClass;

        /// Creates a storage object for standard properties.
        virtual PropertyPtr createStandardPropertyInternal(DataBuffer::BufferInitialization init, size_t elementCount, int type, const ConstDataObjectPath& containerPath) const override;

        /// Generates a human-readable string representation of the data object reference.
        virtual QString formatDataObjectPath(const ConstDataObjectPath& path) const override { return this->displayName(); }

    protected:

        /// Is called by the system after construction of the meta-class instance.
        virtual void initialize() override;
    };

    OVITO_CLASS_META(Impropers, OOMetaClass);

public:

    /// The list of standard improper properties.
    enum Type {
        UserProperty = Property::GenericUserProperty,
        TypeProperty = Property::GenericTypeProperty,
        TopologyProperty,
        ParticleIdentifiersProperty,
    };

    /// Constructor.
    void initializeObject(ObjectInitializationFlags flags);

    /// Convenience method that returns the impropers topology property.
    const Property* getTopology() const { return getProperty(TopologyProperty); }
};

}   // End of namespace
