// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/stdobj/StdObj.h>
#include <ovito/stdobj/properties/PropertyContainer.h>

namespace Ovito {

/**
 * \brief A data object type that consists of a set of data columns, which are typically used to generate 2d data plots.
 */
class OVITO_STDOBJ_EXPORT DataTable : public PropertyContainer
{
    /// Define a new property metaclass for data table property containers.
    class OVITO_STDOBJ_EXPORT OOMetaClass : public PropertyContainerClass
    {
    public:

        /// Inherit constructor from base class.
        using PropertyContainerClass::PropertyContainerClass;

        /// Creates a storage object for standard data table properties.
        virtual PropertyPtr createStandardPropertyInternal(DataBuffer::BufferInitialization init, size_t elementCount, int type, const ConstDataObjectPath& containerPath) const override;

        /// Indicates that this container can supply anchor positions for text labels.
        /// Note that a data table has no standard position property. The property serving as anchor
        /// points must be designated explicitly by setting the 'positions' field of the table.
        virtual bool supportsTextLabels() const override { return true; }

    protected:

        /// Is called by the system after construction of the meta-class instance.
        virtual void initialize() override;
    };

    OVITO_CLASS_META(DataTable, OOMetaClass);

public:

    enum PlotMode {
        None,
        Line,
        Histogram,
        BarChart,
        Scatter
    };
    Q_ENUM(PlotMode);

    /// Constructor.
    void initializeObject(ObjectInitializationFlags flags, PlotMode plotMode = Line, const QString& title = QString(), ConstPropertyPtr y = {}, ConstPropertyPtr x = {});

    /// Assigns a property array as x-coordinates of the data points (for the purpose of plotting).
    void setX(ConstPropertyPtr property);

    /// Overload used for Python bindings.
    void setXPYTHON(const Property* property) { setX(property); }

    /// Assigns a property array as y-coordinates of the data points (for the purpose of plotting).
    void setY(ConstPropertyPtr property);

    /// Overload used for Python bindings.
    void setYPYTHON(const Property* property) { setY(property); }

    /// Assigns a property array as 3d coordinates of the data points (for the purpose of placing text labels).
    void setPositions(ConstPropertyPtr property);

    /// Overload used for Python bindings.
    void setPositionsPYTHON(const Property* property) { setPositions(property); }

    /// Returns the data array containing the x-coordinates of the data points.
    /// If no explicit x-coordinate data is available, the array is dynamically generated
    /// from the x-axis interval set for this data table.
    ConstPropertyPtr getXValues() const;

    /// Returns the data for visualizing a property from this container as text labels using a TextLabelsVis element.
    /// The anchor points are taken from the property that has been designated as the table's position property.
    virtual TextLabelsVis::LabelData getLabelVisData(const ConstDataObjectPath& path, const PipelineFlowState& state,
                                                     const RendererResourceCache::ResourceFrame& visCache,
                                                     TextLabelsVis::LabelDataRequest request,
                                                     TextLabelsVis::ElementAnchor anchor) const override;

protected:

    /// From RefMaker.
    virtual void referenceRemoved(const PropertyFieldDescriptor* field, RefTarget* oldTarget, int listIndex) override;

private:

    /// The lower bound of the x-interval of the histogram if data points have no explicit x-coordinates.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(FloatType{0}, intervalStart, setIntervalStart);

    /// The upper bound of the x-interval of the histogram if data points have no explicit x-coordinates.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(FloatType{0}, intervalEnd, setIntervalEnd);

    /// The label of the x-axis (optional).
    DECLARE_MODIFIABLE_PROPERTY_FIELD(QString{}, axisLabelX, setAxisLabelX);

    /// The label of the y-axis (optional).
    DECLARE_MODIFIABLE_PROPERTY_FIELD(QString{}, axisLabelY, setAxisLabelY);

    /// The plotting mode for this data table.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(PlotMode{Line}, plotMode, setPlotMode);
    DECLARE_SNAPSHOT_PROPERTY_FIELD(plotMode);

    /// Property containing the X coordinates of data points for plotting.
    /// Note: Using OORef<> instead of DataOORef<> here, because the PropertyContainer already holds a strong reference to the same property object.
    /// Thus, there is no need to create a second strong reference here, because this would prevent modifying the property object even when it is exclusively owned by the data table.
    DECLARE_REFERENCE_FIELD_FLAGS(OORef<const Property>, x, PROPERTY_FIELD_DONT_PROPAGATE_MESSAGES | PROPERTY_FIELD_NO_SUB_ANIM);

    /// Property containing the Y coordinates of data points for plotting.
    /// Note: Using OORef<> instead of DataOORef<> here, because the PropertyContainer already holds a strong reference to the same property object.
    /// Thus, there is no need to create a second strong reference here, because this would prevent modifying the property object even when it is exclusively owned by the data table.
    DECLARE_REFERENCE_FIELD_FLAGS(OORef<const Property>, y, PROPERTY_FIELD_DONT_PROPAGATE_MESSAGES | PROPERTY_FIELD_NO_SUB_ANIM);

    /// Property containing the 3d coordinates of the data points, which serve as anchor points for text labels.
    /// A data table has no standard position property, so the column to be used must be designated explicitly.
    /// Note: Using OORef<> instead of DataOORef<> here, because the PropertyContainer already holds a strong reference to the same property object.
    /// Thus, there is no need to create a second strong reference here, because this would prevent modifying the property object even when it is exclusively owned by the data table.
    DECLARE_REFERENCE_FIELD_FLAGS(OORef<const Property>, positions, PROPERTY_FIELD_DONT_PROPAGATE_MESSAGES | PROPERTY_FIELD_NO_SUB_ANIM);
};

}   // End of namespace
