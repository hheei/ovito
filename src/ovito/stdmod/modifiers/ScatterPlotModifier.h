// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/stdmod/StdMod.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <ovito/stdobj/properties/GenericPropertyModifier.h>
#include <ovito/stdobj/properties/PropertyReference.h>

namespace Ovito {

/**
 * \brief This modifier computes a scatter plot for two properties.
 */
class OVITO_STDMOD_EXPORT ScatterPlotModifier : public GenericPropertyModifier
{
    OVITO_CLASS(ScatterPlotModifier)

public:

    /// Constructor.
    void initializeObject(ObjectInitializationFlags flags);

    /// This method is called by the system after the modifier has been inserted into a data pipeline.
    virtual void initializeModifier(const ModifierInitializationRequest& request) override;

    /// Modifies the input data.
    virtual Future<PipelineFlowState> evaluateModifier(const ModifierEvaluationRequest& request, PipelineFlowState&& state) override;

    /// Set start and end value of the x-axis.
    void setXAxisRange(FloatType start, FloatType end) { setXAxisRangeStart(start); setXAxisRangeEnd(end); }

    /// Set start and end value of the y-axis.
    void setYAxisRange(FloatType start, FloatType end) { setYAxisRangeStart(start); setYAxisRangeEnd(end); }

    /// Returns a short piece of information (typically a string or color) to be displayed next to the modifier's title in the pipeline editor list.
    virtual QVariant getPipelineEditorShortInfo(Scene* scene, ModificationNode* node) const override {
        // Note: Whenever the x-axis or y-axis property changes, we trigger a ReferenceEvent::ObjectStatusChanged event in propertyChanged().
        return tr("%1 vs. %2").arg(yAxisProperty().nameWithComponent()).arg(xAxisProperty().nameWithComponent());
    }

protected:

    /// Is called when the value of a property of this object has changed.
    virtual void propertyChanged(const PropertyFieldDescriptor* field) override;

    /// Sends an event to all dependents of this RefTarget.
    virtual void notifyDependentsImpl(const ReferenceEvent& event) noexcept override;

private:

    /// The property that is used as source for the x-axis.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(PropertyReference{}, xAxisProperty, setXAxisProperty);

    /// The property that is used as source for the y-axis.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(PropertyReference{}, yAxisProperty, setYAxisProperty);

    /// Controls the whether elements within the specified range should be selected (x-axis).
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{false}, selectXAxisInRange, setSelectXAxisInRange);

    /// Controls the start value of the selection interval (x-axis).
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{0}, selectionXAxisRangeStart, setSelectionXAxisRangeStart, PROPERTY_FIELD_MEMORIZE);

    /// Controls the end value of the selection interval (x-axis).
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{1}, selectionXAxisRangeEnd, setSelectionXAxisRangeEnd, PROPERTY_FIELD_MEMORIZE);

    /// Controls the whether elements within the specified range should be selected (y-axis).
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{false}, selectYAxisInRange, setSelectYAxisInRange);

    /// Controls the start value of the selection interval (y-axis).
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{0}, selectionYAxisRangeStart, setSelectionYAxisRangeStart, PROPERTY_FIELD_MEMORIZE);

    /// Controls the end value of the selection interval (y-axis).
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{1}, selectionYAxisRangeEnd, setSelectionYAxisRangeEnd, PROPERTY_FIELD_MEMORIZE);

    /// Controls the whether the range of the x-axis of the scatter plot should be fixed.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{false}, fixXAxisRange, setFixXAxisRange);

    /// Controls the start value of the x-axis.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{0}, xAxisRangeStart, setXAxisRangeStart, PROPERTY_FIELD_MEMORIZE);

    /// Controls the end value of the x-axis.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{0}, xAxisRangeEnd, setXAxisRangeEnd, PROPERTY_FIELD_MEMORIZE);

    /// Controls the whether the range of the y-axis of the scatter plot should be fixed.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{false}, fixYAxisRange, setFixYAxisRange);

    /// Controls the start value of the y-axis.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{0}, yAxisRangeStart, setYAxisRangeStart, PROPERTY_FIELD_MEMORIZE);

    /// Controls the end value of the y-axis.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{0}, yAxisRangeEnd, setYAxisRangeEnd, PROPERTY_FIELD_MEMORIZE);
};

}   // End of namespace
