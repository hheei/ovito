// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/mesh/Mesh.h>
#include <ovito/stdobj/properties/PropertyContainer.h>

namespace Ovito {

/**
 * \brief Stores all face-related properties of a SurfaceMesh.
 */
class OVITO_MESH_EXPORT SurfaceMeshFaces : public PropertyContainer
{
    /// Define a new property metaclass for this container type.
    class OOMetaClass : public PropertyContainerClass
    {
    public:

        /// Inherit constructor from base class.
        using PropertyContainerClass::PropertyContainerClass;

        /// Create a storage object for standard face properties.
        virtual PropertyPtr createStandardPropertyInternal(DataBuffer::BufferInitialization init, size_t elementCount, int type, const ConstDataObjectPath& containerPath) const override;

        /// Generates a human-readable string representation of the data object reference.
        virtual QString formatDataObjectPath(const ConstDataObjectPath& path) const override;

        /// Indicates that this container can supply anchor positions for text labels.
        virtual bool supportsTextLabels() const override { return true; }

    protected:

        /// Is called by the system after construction of the meta-class instance.
        virtual void initialize() override;
    };

    OVITO_CLASS_META(SurfaceMeshFaces, OOMetaClass);

public:

    /// \brief The list of standard face properties.
    enum Type {
        UserProperty = Property::GenericUserProperty, //< This is reserved for user-defined properties.
        SelectionProperty = Property::GenericSelectionProperty,
        ColorProperty = Property::GenericColorProperty,
        FaceTypeProperty = Property::GenericTypeProperty,
        RegionProperty = Property::FirstSpecificProperty,
        BurgersVectorProperty,
        CrystallographicNormalProperty,
    };

    /// Constructor.
    void initializeObject(ObjectInitializationFlags flags) {
        PropertyContainer::initializeObject(flags);
        // Assign the default data object identifier.
        setIdentifier(OOClass().pythonName());
    }

    /// Returns the data for visualizing a vector property from this container using a VectorVis element.
    virtual VectorVis::VectorData getVectorVisData(const ConstDataObjectPath& path, const PipelineFlowState& state,
                                                   const RendererResourceCache::ResourceFrame& visCache) const override;

    /// Returns the data for visualizing a property from this container as text labels using a TextLabelsVis element.
    virtual TextLabelsVis::LabelData getLabelVisData(const ConstDataObjectPath& path,
                                                     const PipelineFlowState& state,
                                                     const RendererResourceCache::ResourceFrame& visCache,
                                                     TextLabelsVis::LabelDataRequest request,
                                                     TextLabelsVis::ElementAnchor anchor) const override;

    /// Override method to prevent a direct deletion of elements from this container as it would leave the SurfaceMesh in an inconsistent state.
    virtual size_t deleteElements(ConstDataBufferPtr selection, size_t selectionCount = std::numeric_limits<size_t>::max()) override {
        OVITO_ASSERT(false);
        throw Exception(tr("Deleting faces from a SurfaceMesh is not supported via this method. Call SurfaceMesh.delete_faces() on the parent object instead."));
    }

    /// Throws an exception if appending is not supported by this container type.
    /// This is used in the PropertyContainer.append() Python method.
    virtual void checkAppendability() const override { throw Exception(tr("Mesh face containers cannot be appended to. You should use the SurfaceMesh.create_face() method instead.")); }

private:
    /// Computes the centroids of the mesh faces, which serve as anchor points for visual elements.
    ConstDataBufferPtr faceCentroids(const SurfaceMesh* mesh, const RendererResourceCache::ResourceFrame& visCache) const;
};

}   // End of namespace
