// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/mesh/Mesh.h>
#include <ovito/stdobj/properties/PropertyContainer.h>

namespace Ovito {

/**
 * \brief Stores all volumetric region-related properties of a SurfaceMesh.
 */
class OVITO_MESH_EXPORT SurfaceMeshRegions : public PropertyContainer
{
    /// Define a new property metaclass for this container type.
    class OOMetaClass : public PropertyContainerClass
    {
    public:

        /// Inherit constructor from base class.
        using PropertyContainerClass::PropertyContainerClass;

        /// Create a storage object for standard region properties.
        virtual PropertyPtr createStandardPropertyInternal(DataBuffer::BufferInitialization init, size_t elementCount, int type, const ConstDataObjectPath& containerPath) const override;

        /// Generates a human-readable string representation of the data object reference.
        virtual QString formatDataObjectPath(const ConstDataObjectPath& path) const override;

    protected:

        /// Is called by the system after construction of the meta-class instance.
        virtual void initialize() override;
    };

    OVITO_CLASS_META(SurfaceMeshRegions, OOMetaClass);

public:

    /// \brief The list of standard region properties.
    enum Type {
        UserProperty = Property::GenericUserProperty, //< This is reserved for user-defined properties.
        SelectionProperty = Property::GenericSelectionProperty,
        ColorProperty = Property::GenericColorProperty,
        PhaseProperty = Property::FirstSpecificProperty,
        VolumeProperty,
        SurfaceAreaProperty,
        IsFilledProperty,
        LatticeCorrespondenceProperty,
        IsExteriorProperty,
    };

    /// Constructor.
    void initializeObject(ObjectInitializationFlags flags) {
        PropertyContainer::initializeObject(flags);
        // Assign the default data object identifier.
        setIdentifier(OOClass().pythonName());
    }

    /// Override method to prevent a direct deletion of elements from this container as it would leave the SurfaceMesh in an inconsistent state.
    virtual size_t deleteElements(ConstDataBufferPtr selection, size_t selectionCount = std::numeric_limits<size_t>::max()) override {
       OVITO_ASSERT(false);
        throw Exception(tr("Deleting regions from a SurfaceMesh is not supported via this method. Call SurfaceMesh.delete_regions() on the parent object instead."));
    }
};

}   // End of namespace
