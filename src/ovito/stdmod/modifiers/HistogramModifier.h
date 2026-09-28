// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/stdmod/StdMod.h>
#include <ovito/stdobj/properties/GenericPropertyModifier.h>
#include <ovito/stdobj/properties/PropertyReference.h>
#include <ovito/stdobj/table/DataTable.h>

namespace Ovito {

/**
 * \brief This modifier computes a value histogram for a property.
 */
class OVITO_STDMOD_EXPORT HistogramModifier : public GenericPropertyModifier
{
    OVITO_CLASS(HistogramModifier)

public:

    /// The different ways the bin counts of the histogram can be normalized.
    enum NormalizationMode {
        AbsoluteCount,      ///< No normalization, the bin counts are absolute values (default).
        RelativeFrequency,  ///< The bin counts are normalized to the total number of elements.
        ProbabilityDensity  ///< The bin counts are normalized to the total number of elements and the bin width.
    };
    Q_ENUM(NormalizationMode);

    /// Constructor.
    void initializeObject(ObjectInitializationFlags flags);

    /// This method is called by the system after the modifier has been inserted into a data pipeline.
    virtual void initializeModifier(const ModifierInitializationRequest& request) override;

    /// Modifies the input data.
    virtual Future<PipelineFlowState> evaluateModifier(const ModifierEvaluationRequest& request, PipelineFlowState&& state) override;

    /// Set start and end value of the x-axis.
    void setXAxisRange(FloatType start, FloatType end) {
        setXAxisRangeStart(start);
        setXAxisRangeEnd(end);
    }

    /// Set start and end value of the y-axis.
    void setYAxisRange(FloatType start, FloatType end) {
        setYAxisRangeStart(start);
        setYAxisRangeEnd(end);
    }

    /// Returns a short piece of information (typically a string or color) to be displayed next to the modifier's title in the pipeline editor list.
    virtual QVariant getPipelineEditorShortInfo(Scene* scene, ModificationNode* node) const override {
        // Note: Whenever the source property changes, we trigger a ReferenceEvent::ObjectStatusChanged event in propertyChanged().
        return sourceProperty().nameWithComponent();
    }

protected:

    /// Is called when the value of a property of this object has changed.
    virtual void propertyChanged(const PropertyFieldDescriptor* field) override;

private:

    /// The property that serves as data source of the histogram.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(PropertyReference{}, sourceProperty, setSourceProperty);

    /// Controls the number of histogram bins.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(int{200}, numberOfBins, setNumberOfBins, PROPERTY_FIELD_MEMORIZE);

    /// Controls the whether elements within the specified range should be selected.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{false}, selectInRange, setSelectInRange);

    /// Controls the start value of the selection interval.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{0}, selectionRangeStart, setSelectionRangeStart, PROPERTY_FIELD_MEMORIZE);

    /// Controls the end value of the selection interval.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{1}, selectionRangeEnd, setSelectionRangeEnd, PROPERTY_FIELD_MEMORIZE);

    /// Controls the whether the range of the x-axis of the histogram should be fixed.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{false}, fixXAxisRange, setFixXAxisRange);

    /// Controls the start value of the x-axis.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{0}, xAxisRangeStart, setXAxisRangeStart, PROPERTY_FIELD_MEMORIZE);

    /// Controls the end value of the x-axis.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{0}, xAxisRangeEnd, setXAxisRangeEnd, PROPERTY_FIELD_MEMORIZE);

    /// Controls the whether the range of the y-axis of the histogram should be fixed.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{false}, fixYAxisRange, setFixYAxisRange);

    /// Controls the start value of the y-axis.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{0}, yAxisRangeStart, setYAxisRangeStart, PROPERTY_FIELD_MEMORIZE);

    /// Controls the end value of the y-axis.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{0}, yAxisRangeEnd, setYAxisRangeEnd, PROPERTY_FIELD_MEMORIZE);

    /// Controls whether the modifier should take into account only selected elements.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{false}, onlySelectedElements, setOnlySelectedElements);

    /// Controls how bin counts are normalized.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(NormalizationMode{AbsoluteCount}, normalizationMode, setNormalizationMode, PROPERTY_FIELD_MEMORIZE);
};

}   // End of namespace
