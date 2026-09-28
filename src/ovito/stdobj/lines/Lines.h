// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once

#include <ovito/stdobj/StdObj.h>
#include <ovito/stdobj/properties/PropertyContainer.h>
#include <ovito/core/utilities/linalg/LinAlg.h>

namespace Ovito {

/**
 * \brief Stores a set of (poly)lines.
 */
class OVITO_STDOBJ_EXPORT Lines : public PropertyContainer
{
public:
    /// Define a new property metaclass for this property container type.
    class OVITO_STDOBJ_EXPORT OOMetaClass : public PropertyContainerClass
    {
    public:
        /// Inherit constructor from base class.
        using PropertyContainerClass::PropertyContainerClass;

        /// Creates a storage object for standard properties.
        virtual PropertyPtr createStandardPropertyInternal(DataBuffer::BufferInitialization init, size_t elementCount, int type,
                                                           const ConstDataObjectPath& containerPath) const override;

        /// Indicates that this container can supply anchor positions for text labels.
        virtual bool supportsTextLabels() const override { return true; }

    protected:
        /// Is called by the system after construction of the meta-class instance.
        virtual void initialize() override;
    };

    OVITO_CLASS_META(Lines, OOMetaClass);

public:
    /// The list of standard properties.
    enum Type
    {
        ColorProperty = Property::GenericColorProperty,
        SelectionProperty = Property::GenericSelectionProperty,
        PositionProperty = Property::FirstSpecificProperty,
        SampleTimeProperty,  // Is used by the GenerateTrajectoryLinesModifier
        SectionProperty,
        Position1Property,
        Position2Property,
    };

    /// Constructor.
    void initializeObject(ObjectInitializationFlags flags);

    /// Returns the data for visualizing a vector property from this container using a VectorVis element.
    VectorVis::VectorData getVectorVisData(const ConstDataObjectPath& path, const PipelineFlowState& state,
                                           const RendererResourceCache::ResourceFrame& visCache) const override;

    /// Returns the data for visualizing a property from this container as text labels using a TextLabelsVis element.
    TextLabelsVis::LabelData getLabelVisData(const ConstDataObjectPath& path, const PipelineFlowState& state,
                                             const RendererResourceCache::ResourceFrame& visCache,
                                             TextLabelsVis::LabelDataRequest request,
                                             TextLabelsVis::ElementAnchor anchor) const override;

    /// Throws an exception if appending is not supported by this container type.
    /// This is used in the PropertyContainer.append() Python method.
    virtual void checkAppendability() const override { throw Exception(tr("Lines property containers cannot be appended to. You should use the Lines.create_line() method instead.")); }

private:

    /// Tests whether the given spatial point is culled by the cutting planes set for this object.
    bool isPointCulled(const Point3& p) const {
        return std::any_of(cuttingPlanes().begin(), cuttingPlanes().end(), [&](const Plane3& plane) { return plane.classifyPoint(p) > 0; });
    }

    /// The planar cuts to be applied to geometry after its has been transformed into a non-periodic representation.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(QVector<Plane3>{}, cuttingPlanes, setCuttingPlanes);

    /// The cached bounding box of the vertex coordinates.
    Box3 _boundingBox;
};

}  // namespace Ovito
