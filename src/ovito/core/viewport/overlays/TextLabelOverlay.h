// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/core/Core.h>
#include <ovito/core/dataset/scene/Pipeline.h>
#include <ovito/core/dataset/pipeline/PipelineFlowState.h>
#include <ovito/core/dataset/pipeline/PipelineEvaluationRequest.h>
#include <ovito/core/rendering/FrameBuffer.h>
#include "ViewportOverlay.h"

namespace Ovito {

/**
 * \brief A viewport overlay that displays a user-defined text label.
 */
class OVITO_CORE_EXPORT TextLabelOverlay : public ViewportOverlay
{
    OVITO_CLASS(TextLabelOverlay)

public:

    /// Lets the overlay paint its contents into the framebuffer (asynchronous coroutine; evaluates the optional pipeline).
    virtual Future<PipelineStatus> renderAsynchronous(OORef<FrameGraph> frameGraph, FrameGraph::RenderingCommandGroup& commandGroup, QRect logicalViewportRect, QRect physicalViewportRect, ViewProjectionParameters noninteractiveProjParams, OORef<const Scene> scene) override;

    /// Moves the position of the overlay in the viewport by the given amount,
    /// which is specified as a fraction of the viewport render size.
    virtual void moveLayerInViewport(const Vector2& delta) override {
        auto roundPercent = [](FloatType f) { return std::round(f * 1e4) / 1e4; };
        setOffsetX(roundPercent(offsetX() + delta.x()));
        setOffsetY(roundPercent(offsetY() + delta.y()));
    }

    /// Returns a short piece of information (typically a string or color) to be displayed next to the object's title in the pipeline editor.
    virtual QVariant getPipelineEditorShortInfo(Scene* scene) const override;

protected:

    /// Is called when the value of a property of this object has changed.
    virtual void propertyChanged(const PropertyFieldDescriptor* field) override;

private:

    /// This method paints the overlay contents onto the given canvas.
    PipelineStatus renderImplementation(FrameGraph& frameGraph, FrameGraph::RenderingCommandGroup& commandGroup, const QRect& targetRect, const PipelineFlowState& flowState);

    /// The corner of the viewport where the label is shown in.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(int{Qt::AlignLeft | Qt::AlignTop}, alignment, setAlignment, PROPERTY_FIELD_MEMORIZE);

    /// Controls the horizontal offset of label position.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{0}, offsetX, setOffsetX, PROPERTY_FIELD_MEMORIZE);

    /// Controls the vertical offset of label position.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{0}, offsetY, setOffsetY, PROPERTY_FIELD_MEMORIZE);

    /// Controls the label font.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(QFont{}, font, setFont, PROPERTY_FIELD_MEMORIZE);

    /// Controls the label font size.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{0.02}, fontSize, setFontSize, PROPERTY_FIELD_MEMORIZE);

    /// The label's text.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(QString{tr("Text label")}, labelText, setLabelText);

    /// The display color of the label.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS((Color{0,0,0.5}), textColor, setTextColor, PROPERTY_FIELD_MEMORIZE);

    /// The text outline color.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS((Color{1,1,1}), outlineColor, setOutlineColor, PROPERTY_FIELD_MEMORIZE);

    /// Controls the outlining of the font.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool{false}, outlineEnabled, setOutlineEnabled, PROPERTY_FIELD_MEMORIZE);

    /// Controls the formatting of floating-point variable values referenced in the text string.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(QString{"%.6g"}, valueFormatString, setValueFormatString);
};

}   // End of namespace
