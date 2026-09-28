// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once

#include <ovito/stdobj/StdObj.h>
#include <ovito/stdobj/properties/PropertyContainer.h>

namespace Ovito {

/**
 * \brief Stores a set of vectors for visualization.
 */
class OVITO_STDOBJ_EXPORT Vectors : public PropertyContainer
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

        /// Indicates that vectors are elongated elements, whose labels can be anchored at either end.
        virtual bool supportLabelAnchors() const override { return true; }

    protected:
        /// Is called by the system after construction of the meta-class instance.
        virtual void initialize() override;
    };

    OVITO_CLASS_META(Vectors, OOMetaClass);

public:
    /// The list of standard properties.
    enum Type
    {
        ColorProperty = Property::GenericColorProperty,
        SelectionProperty = Property::GenericSelectionProperty,
        PositionProperty = Property::FirstSpecificProperty,
        TransparencyProperty,
        DirectionProperty,
    };

    /// Constructor.
    void initializeObject(ObjectInitializationFlags flags);

    /// Returns the base point and vector information for visualizing a vector property from this container using a VectorVis element.
    virtual VectorVis::VectorData getVectorVisData(const ConstDataObjectPath& path, const PipelineFlowState& state,
                                                   const RendererResourceCache::ResourceFrame& visCache) const override
    {
        return {.positions = getProperty(PositionProperty),
                .directions = getProperty(DirectionProperty),
                .colors = getProperty(ColorProperty),
                .transparencies = getProperty(TransparencyProperty),
                .selection = getProperty(SelectionProperty)};
    }

    virtual std::array<bool, 2> hasVectorVisColorsAndTransparencies() const override
    {
        return {getProperty(ColorProperty) != nullptr, getProperty(TransparencyProperty) != nullptr};
    }

    /// Returns the data for visualizing a property from this container as text labels using a TextLabelsVis element.
    virtual TextLabelsVis::LabelData getLabelVisData(const ConstDataObjectPath& path,
                                                     const PipelineFlowState& state,
                                                     const RendererResourceCache::ResourceFrame& visCache,
                                                     TextLabelsVis::LabelDataRequest request,
                                                     TextLabelsVis::ElementAnchor anchor) const override;
};

}  // namespace Ovito
