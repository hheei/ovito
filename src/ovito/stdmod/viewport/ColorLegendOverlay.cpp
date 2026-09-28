// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/stdmod/StdMod.h>
#include <ovito/core/viewport/Viewport.h>
#include <ovito/core/rendering/RenderSettings.h>
#include <ovito/core/rendering/FrameGraph.h>
#include <ovito/core/rendering/TextPrimitive.h>
#include <ovito/core/app/Application.h>
#include <ovito/core/app/UserInterface.h>
#include <ovito/core/dataset/scene/Scene.h>
#include <ovito/core/dataset/scene/Pipeline.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <ovito/core/rendering/ColorMapHelper.h>
#include <ovito/core/rendering/RenderThread.h>
#include <ovito/core/rendering/standard/StandardRenderer.h>
#include <ovito/core/dataset/data/BufferAccess.h>
#include <ovito/core/utilities/concurrent/NoninteractiveContext.h>
#include "ColorLegendOverlay.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(ColorLegendOverlay);
OVITO_CLASSINFO(ColorLegendOverlay, "DisplayName", "Color legend");
OVITO_CLASSINFO(ColorLegendOverlay, "Description", "Legend for color-mapped properties.");
DEFINE_PROPERTY_FIELD(ColorLegendOverlay, alignment);
DEFINE_PROPERTY_FIELD(ColorLegendOverlay, orientation);
DEFINE_PROPERTY_FIELD(ColorLegendOverlay, legendSize);
DEFINE_PROPERTY_FIELD(ColorLegendOverlay, font);
DEFINE_PROPERTY_FIELD(ColorLegendOverlay, fontSize);
DEFINE_PROPERTY_FIELD(ColorLegendOverlay, relLabelFontSize);
DEFINE_PROPERTY_FIELD(ColorLegendOverlay, offsetX);
DEFINE_PROPERTY_FIELD(ColorLegendOverlay, offsetY);
DEFINE_PROPERTY_FIELD(ColorLegendOverlay, aspectRatio);
DEFINE_PROPERTY_FIELD(ColorLegendOverlay, textColor);
DEFINE_PROPERTY_FIELD(ColorLegendOverlay, outlineColor);
DEFINE_PROPERTY_FIELD(ColorLegendOverlay, outlineEnabled);
DEFINE_PROPERTY_FIELD(ColorLegendOverlay, caption);
DEFINE_PROPERTY_FIELD(ColorLegendOverlay, label1);
DEFINE_PROPERTY_FIELD(ColorLegendOverlay, label2);
DEFINE_PROPERTY_FIELD(ColorLegendOverlay, valueFormatString);
DEFINE_REFERENCE_FIELD(ColorLegendOverlay, modifier);
DEFINE_REFERENCE_FIELD(ColorLegendOverlay, colorMapping);
DEFINE_PROPERTY_FIELD(ColorLegendOverlay, sourceProperty);
DEFINE_PROPERTY_FIELD(ColorLegendOverlay, useTypeShapes);
DEFINE_PROPERTY_FIELD(ColorLegendOverlay, borderEnabled);
DEFINE_PROPERTY_FIELD(ColorLegendOverlay, borderColor);
DEFINE_PROPERTY_FIELD(ColorLegendOverlay, ticksEnabled);
DEFINE_PROPERTY_FIELD(ColorLegendOverlay, tickSpacing);
DEFINE_PROPERTY_FIELD(ColorLegendOverlay, titleRotationEnabled);
DEFINE_PROPERTY_FIELD(ColorLegendOverlay, backgroundEnabled);
DEFINE_PROPERTY_FIELD(ColorLegendOverlay, backgroundColor);
SET_PROPERTY_FIELD_LABEL(ColorLegendOverlay, alignment, "Position");
SET_PROPERTY_FIELD_LABEL(ColorLegendOverlay, orientation, "Orientation");
SET_PROPERTY_FIELD_LABEL(ColorLegendOverlay, legendSize, "Legend size");
SET_PROPERTY_FIELD_LABEL(ColorLegendOverlay, font, "Font");
SET_PROPERTY_FIELD_LABEL(ColorLegendOverlay, fontSize, "Font size");
SET_PROPERTY_FIELD_LABEL(ColorLegendOverlay, relLabelFontSize, "Label size");
SET_PROPERTY_FIELD_LABEL(ColorLegendOverlay, offsetX, "Offset X");
SET_PROPERTY_FIELD_LABEL(ColorLegendOverlay, offsetY, "Offset Y");
SET_PROPERTY_FIELD_LABEL(ColorLegendOverlay, aspectRatio, "Aspect ratio");
SET_PROPERTY_FIELD_LABEL(ColorLegendOverlay, textColor, "Font color");
SET_PROPERTY_FIELD_LABEL(ColorLegendOverlay, outlineColor, "Outline color");
SET_PROPERTY_FIELD_LABEL(ColorLegendOverlay, outlineEnabled, "Text outline");
SET_PROPERTY_FIELD_LABEL(ColorLegendOverlay, caption, "Caption");
SET_PROPERTY_FIELD_ALIAS_IDENTIFIER(ColorLegendOverlay, caption, "title"); // For backward compatibility with OVITO 3.16.0
SET_PROPERTY_FIELD_LABEL(ColorLegendOverlay, label1, "Label 1");
SET_PROPERTY_FIELD_LABEL(ColorLegendOverlay, label2, "Label 2");
SET_PROPERTY_FIELD_LABEL(ColorLegendOverlay, valueFormatString, "Number format");
SET_PROPERTY_FIELD_LABEL(ColorLegendOverlay, sourceProperty, "Source property");
SET_PROPERTY_FIELD_LABEL(ColorLegendOverlay, useTypeShapes, "Show type shapes");
SET_PROPERTY_FIELD_LABEL(ColorLegendOverlay, borderEnabled, "Draw border");
SET_PROPERTY_FIELD_LABEL(ColorLegendOverlay, borderColor, "Border color");
SET_PROPERTY_FIELD_LABEL(ColorLegendOverlay, ticksEnabled, "Draw ticks");
SET_PROPERTY_FIELD_LABEL(ColorLegendOverlay, tickSpacing, "Spacing");
SET_PROPERTY_FIELD_LABEL(ColorLegendOverlay, titleRotationEnabled, "Rotate title");
SET_PROPERTY_FIELD_LABEL(ColorLegendOverlay, backgroundEnabled, "Background enabled");
SET_PROPERTY_FIELD_LABEL(ColorLegendOverlay, backgroundColor, "Background color");
SET_PROPERTY_FIELD_UNITS(ColorLegendOverlay, offsetX, PercentParameterUnit);
SET_PROPERTY_FIELD_UNITS(ColorLegendOverlay, offsetY, PercentParameterUnit);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(ColorLegendOverlay, legendSize, FloatParameterUnit, 0);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(ColorLegendOverlay, aspectRatio, FloatParameterUnit, 1);
SET_PROPERTY_FIELD_UNITS_AND_RANGE(ColorLegendOverlay, fontSize, FloatParameterUnit, 0, 1);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(ColorLegendOverlay, relLabelFontSize, PercentParameterUnit, 0);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(ColorLegendOverlay, tickSpacing, FloatParameterUnit, 0);

/******************************************************************************
* Compiles the list of typed properties in a pipeline output which can serve as
* source for a discrete color legend.
******************************************************************************/
std::vector<ConstDataObjectPath> ColorLegendOverlay::listTypedProperties(const PipelineFlowState& state)
{
    // Pick out those properties which are typed properties, i.e. which have one or more ElementType objects attached to them.
    std::vector<ConstDataObjectPath> paths;
    for(ConstDataObjectPath& dataPath : state.getObjectsRecursive(Property::OOClass())) {
        if(dataPath.size() >= 2 && static_object_cast<Property>(dataPath.back())->isTypedProperty())
            paths.push_back(std::move(dataPath));
    }

    // The paths are in the depth-first order in which the data objects were visited. While the top-level objects of the
    // data collection and the properties of a container are enumerated in their natural order, the sub-objects of a
    // container are visited in the reverse order of their declaration. For a molecular dataset, this puts the improper
    // types before the particle types. Reorder the paths by comparing them at the level at which they branch off from
    // each other, such that the typed properties of a container come before those of its nested sub-containers, and
    // sibling sub-containers appear in their declaration order, i.e. bonds, angles, dihedrals, and impropers.
    std::vector<size_t> ordering(paths.size());
    std::iota(ordering.begin(), ordering.end(), 0);
    std::sort(ordering.begin(), ordering.end(), [&](size_t a, size_t b) {
        // Determine the level at which the two paths branch off from each other.
        const ConstDataObjectPath& pathA = paths[a];
        const ConstDataObjectPath& pathB = paths[b];
        auto [branchA, branchB] = std::mismatch(pathA.begin(), pathA.end(), pathB.begin(), pathB.end());
        OVITO_ASSERT(branchA != pathA.end() && branchB != pathB.end()); // Paths are all distinct, and none is a prefix of another one.

        // Two top-level data objects of the collection already are in their natural order.
        if(branchA == pathA.begin()) {
            OVITO_ASSERT(branchB == pathB.begin());
            return a < b;
        }

        // At the branching point, a typed property of the shared parent container takes precedence over a sub-container.
        bool isPropertyA = (std::next(branchA) == pathA.end());
        bool isPropertyB = (std::next(branchB) == pathB.end());
        if(isPropertyA != isPropertyB)
            return isPropertyA;

        // Two typed properties of the same container are kept in their natural order, whereas two sub-container branches
        // are swapped, which restores the declaration order of the underlying reference fields.
        return isPropertyA ? (a < b) : (a > b);
    });

    std::vector<ConstDataObjectPath> results;
    results.reserve(ordering.size());
    for(size_t index : ordering)
        results.push_back(std::move(paths[index]));
    return results;
}

/******************************************************************************
* Is called when the overlay is being newly attached to a viewport.
******************************************************************************/
void ColorLegendOverlay::initializeOverlay(Viewport* viewport)
{
    if(this_task::isInteractive() && !pipeline()) {

        // Find a ColorCodingModifier in the scene that we can connect to.
        if(!modifier() && !sourceProperty() && !colorMapping() && viewport->scene()) {
            viewport->scene()->visitPipelines([&](SceneNode* sceneNode) {
                PipelineNode* node = sceneNode->pipeline()->head();
                for(;;) {
                    if(ModificationNode* modNode = dynamic_object_cast<ModificationNode>(node)) {
                        if(ColorCodingModifier* mod = dynamic_object_cast<ColorCodingModifier>(modNode->modifier())) {
                            setPipeline(sceneNode->pipeline());
                            setModifier(mod);
                            if(mod->isEnabled())
                                return false; // Stop search if modifier is enabled; otherwise, keep looking for an alternative.
                        }
                        node = modNode->input();
                    }
                    else break;
                }
                return true;
            });
        }

        // If there is no ColorCodingModifier in the scene, initialize the overlay to use
        // the most relevant typed property as color mapping source.
        if(!modifier() && !sourceProperty() && !colorMapping() && viewport->scene()) {
            viewport->scene()->visitPipelines([&](SceneNode* sceneNode) {
                const PipelineFlowState& state = sceneNode->pipeline()->getCachedPipelineOutput(viewport->currentTime());
                std::vector<ConstDataObjectPath> typedProperties = listTypedProperties(state);
                if(!typedProperties.empty()) {
                    setPipeline(sceneNode->pipeline());
                    setSourceProperty(typedProperties.front());
                    return false; // Stop search.
                }
                return true;
            });
        }

        // If we still don't have a valid source, look for a visual element in the scene which uses pseudo-color mapping.
        if(!modifier() && !sourceProperty() && !colorMapping() && viewport->scene()) {
            viewport->scene()->visitPipelines([&](SceneNode* sceneNode) {
                for(DataVis* vis : sceneNode->pipeline()->visElements()) {
                    if(vis->isEnabled()) {
                        for(const PropertyFieldDescriptor* field : vis->getOOMetaClass().propertyFields()) {
                            if(field->isReferenceField() && field->targetClass()->isDerivedFrom(PropertyColorMapping::OOClass()) && !field->flags().testFlag(PROPERTY_FIELD_NO_SUB_ANIM) && !field->isVector()) {
                                if(PropertyColorMapping* mapping = static_object_cast<PropertyColorMapping>(vis->getReferenceFieldTarget(field))) {
                                    if(mapping->sourceProperty()) {
                                        setPipeline(sceneNode->pipeline());
                                        setColorMapping(mapping);
                                        return false; // Stop search.
                                    }
                                }
                                break;
                            }
                        }
                    }
                }
                return true;
            });
        }
    }
}

/******************************************************************************
* Is called when the value of a property of this object has changed.
******************************************************************************/
void ColorLegendOverlay::propertyChanged(const PropertyFieldDescriptor* field)
{
    if(field == PROPERTY_FIELD(alignment) && !shouldIgnoreChanges() && !isUndoingOrRedoing() && this_task::isInteractive()) {
        // Automatically reset offset to zero when user changes the alignment of the overlay in the viewport.
        setOffsetX(0);
        setOffsetY(0);
    }
    else if(field == PROPERTY_FIELD(ColorLegendOverlay::sourceProperty) && !shouldIgnoreChanges()) {
        // Changes of some the overlay's parameters affect the result of getPipelineEditorShortInfo().
        notifyDependents(ReferenceEvent::ObjectStatusChanged);
    }
    else if(field == PROPERTY_FIELD(ColorLegendOverlay::useTypeShapes)) {
        // Toggling the option also gives offscreen rendering another chance after an earlier failure.
        invalidateSymbolImageCache();
    }

    ViewportOverlay::propertyChanged(field);
}

/******************************************************************************
* Is called when a RefTarget referenced by this object generated an event.
******************************************************************************/
bool ColorLegendOverlay::referenceEvent(RefTarget* source, const ReferenceEvent& event)
{
    if(event.type() == ReferenceEvent::TargetChanged && source == modifier()) {
        // Changes of some the object's parameters affect the result of getPipelineEditorShortInfo().
        notifyDependents(ReferenceEvent::ObjectStatusChanged);
    }

    return ViewportOverlay::referenceEvent(source, event);
}

/******************************************************************************
* Is called when the value of a reference field of this RefMaker changes.
******************************************************************************/
void ColorLegendOverlay::referenceReplaced(const PropertyFieldDescriptor* field, RefTarget* oldTarget, RefTarget* newTarget, int listIndex)
{
    if((field == PROPERTY_FIELD(modifier) || field == PROPERTY_FIELD(colorMapping)) && !shouldIgnoreChanges()) {
        // Changes of some the object's parameters affect the result of getPipelineEditorShortInfo().
        notifyDependents(ReferenceEvent::ObjectStatusChanged);
    }

    ViewportOverlay::referenceReplaced(field, oldTarget, newTarget, listIndex);
}

/******************************************************************************
* Returns a short piece of information (typically a string or color) to be
* displayed next to the modifier's title in the pipeline editor list.
******************************************************************************/
QVariant ColorLegendOverlay::getPipelineEditorShortInfo(Scene* scene) const
{
    // Note: Whenever the source property, color mapping, or modifier changes, we trigger a ReferenceEvent::ObjectStatusChanged event in propertyChanged() or referenceEvent() or referenceReplaced().
    if(modifier()) {
        return modifier()->sourceProperty().nameWithComponent();
    }
    else if(colorMapping()) {
        return colorMapping()->sourceProperty().nameWithComponent();
    }
    else if(sourceProperty()) {
        return sourceProperty().dataTitleOrPath();
    }
    return {};
}

/******************************************************************************
* Computes the position and size of the color bar rectangle within the viewport.
******************************************************************************/
QRectF ColorLegendOverlay::computeColorBarRect(const QRect& physicalViewportRect, FloatType legendSize, FloatType aspectRatio) const
{
    FloatType colorBarWidth = legendSize;
    FloatType colorBarHeight = colorBarWidth / std::max(FloatType(0.01), aspectRatio);
    bool vertical = (orientation() == Qt::Vertical);
    if(vertical)
        std::swap(colorBarWidth, colorBarHeight);

    QPointF origin(offsetX() * physicalViewportRect.width() + physicalViewportRect.left(), -offsetY() * physicalViewportRect.height() + physicalViewportRect.top());
    FloatType hmargin = FloatType(0.01) * physicalViewportRect.width();
    FloatType vmargin = FloatType(0.01) * physicalViewportRect.height();

    if(alignment() & Qt::AlignLeft) origin.rx() += hmargin;
    else if(alignment() & Qt::AlignRight) origin.rx() += physicalViewportRect.width() - hmargin - colorBarWidth;
    else if(alignment() & Qt::AlignHCenter) origin.rx() += FloatType(0.5) * physicalViewportRect.width() - FloatType(0.5) * colorBarWidth;

    if(alignment() & Qt::AlignTop) origin.ry() += vmargin;
    else if(alignment() & Qt::AlignBottom) origin.ry() += physicalViewportRect.height() - vmargin - colorBarHeight;
    else if(alignment() & Qt::AlignVCenter) origin.ry() += FloatType(0.5) * physicalViewportRect.height() - FloatType(0.5) * colorBarHeight;

    return QRectF(origin, QSizeF(colorBarWidth, colorBarHeight));
}

/******************************************************************************
* Lets the overlay paint its contents into the framebuffer.
******************************************************************************/
Future<PipelineStatus> ColorLegendOverlay::renderAsynchronous(OORef<FrameGraph> frameGraph, FrameGraph::RenderingCommandGroup& commandGroup, QRect logicalViewportRect, QRect physicalViewportRect, ViewProjectionParameters noninteractiveProjParams, OORef<const Scene> scene)
{
    // Reset auto-generated label texts. Will be newly set by rendering code.
    _autoTitleText.clear();
    _autoLabel1Text.clear();
    _autoLabel2Text.clear();

    // Check alignment parameter.
    if(!frameGraph->isInteractive())
        checkAlignmentParameterValue(alignment());

    // Calculate position and size of color legend rectangle.
    FloatType legendSize = this->legendSize() * physicalViewportRect.height();
    if(legendSize <= 0) co_return PipelineStatus::Success;

    QRectF colorBarRect = computeColorBarRect(physicalViewportRect, legendSize, aspectRatio());

    // Determine the source pipeline.
    Pipeline* sourcePipeline = this->pipeline();
    if(!sourcePipeline) {
        // If no source pipeline has been specified by the user, use the first pipeline found in the current scene as a fallback.
        scene->visitPipelines([&](SceneNode* sceneNode) {
            sourcePipeline = sceneNode->pipeline();
            return false;
        });
    }

    if(sourcePipeline) {
        if(modifier()) {
            // Get modifier's parameters.
            _autoTitleText = modifier()->sourceProperty().nameWithComponent();

            // If the auto-adjust option is enabled for the color coding modifier, we have to do some more work to figure out
            // the current value range of the color mapping. It requires a partial pipeline evaluation up to the color coding modifier.
            if(modifier()->autoAdjustRange() && (label1().isEmpty() || label2().isEmpty())) {
                // Figure out which of the modifier's modification nodes belongs to the pipeline associated with this viewport overlay.
                ModificationNode* modNode = nullptr;
                PipelineNode* node = sourcePipeline->head();
                for(;;) {
                    if((modNode = dynamic_object_cast<ModificationNode>(node))) {
                        if(modNode->modifier() == modifier())
                            break;
                        node = modNode->input();
                    }
                    else break;
                }
                if(!modNode)
                    throw Exception(tr("Selected color coding could not be found in the selected pipeline."));

                // Request the modifier's output and asynchronously wait for the results.
                const PipelineFlowState state = co_await FutureAwaiter(ObjectExecutor(this),
                    modNode->evaluate(PipelineEvaluationRequest(frameGraph->time(), frameGraph->stopOnPipelineError(), frameGraph->isInteractive())).asFuture());

                FloatType startValue = std::numeric_limits<FloatType>::quiet_NaN();
                FloatType endValue = std::numeric_limits<FloatType>::quiet_NaN();
                QVariant minValue = state.getAttributeValue(modNode, QStringLiteral("ColorCoding.RangeMin"));
                QVariant maxValue = state.getAttributeValue(modNode, QStringLiteral("ColorCoding.RangeMax"));
                if(minValue.isValid() && maxValue.isValid()) {
                    startValue = minValue.value<FloatType>();
                    endValue = maxValue.value<FloatType>();
                }
                if(modifier() && modifier()->useDiscreteColorMap() && minValue.isValid() && maxValue.isValid()) {
                    // Reverse discrete colormap if vertical orientation is used to match the continuous colormap's direction.
                    drawDiscreteColorMap(*frameGraph, commandGroup, colorBarRect, legendSize,
                                         getDiscreteColorMapLabels(modifier()->colorGradient(), startValue, endValue, orientation()));
                }
                else if(modifier()) {
                    drawContinuousColorMap(*frameGraph, commandGroup, colorBarRect, legendSize,
                                           PseudoColorMapping(startValue, endValue, modifier()->colorGradient()));
                }
                co_return PipelineStatus::Success;
            }
            else {
                if(modifier()->useDiscreteColorMap() && std::isfinite(modifier()->startValue()) && std::isfinite(modifier()->endValue())) {
                    // Reverse discrete colormap if vertical orientation is used to match the continuous colormap's direction.
                    drawDiscreteColorMap(*frameGraph, commandGroup, colorBarRect, legendSize,
                                         getDiscreteColorMapLabels(modifier()->colorGradient(), modifier()->startValue(),
                                                                   modifier()->endValue(), orientation()));
                }
                else {
                    drawContinuousColorMap(
                        *frameGraph, commandGroup, colorBarRect, legendSize,
                        PseudoColorMapping(modifier()->startValue(), modifier()->endValue(), modifier()->colorGradient()));
                }
                co_return PipelineStatus::Success;
            }
        }
        else if(colorMapping()) {
            _autoTitleText = colorMapping()->sourceProperty().nameWithComponent();
            if(colorMapping()->useDiscreteColorMap() && std::isfinite(colorMapping()->startValue()) &&
               std::isfinite(colorMapping()->endValue())) {
                // Reverse discrete colormap if vertical orientation is used to match the continuous colormap's direction.
                drawDiscreteColorMap(*frameGraph, commandGroup, colorBarRect, legendSize,
                                     getDiscreteColorMapLabels(colorMapping()->pseudoColorMapping().gradient(),
                                                               colorMapping()->startValue(), colorMapping()->endValue(), orientation()));
            }
            else {
                drawContinuousColorMap(*frameGraph, commandGroup, colorBarRect, legendSize, colorMapping()->pseudoColorMapping());
            }
            co_return PipelineStatus::Success;
        }
        else if(sourceProperty()) {
            // Evaluate the pipeline and asynchronously wait for the results.
            const PipelineFlowState state = co_await FutureAwaiter(ObjectExecutor(this),
                sourcePipeline->evaluatePipeline(PipelineEvaluationRequest(frameGraph->time(), frameGraph->stopOnPipelineError(), frameGraph->isInteractive())).asFuture());

            // Look up the typed property. The full data object path is requested, because the visual element of the
            // parent property container determines how the element types are rendered in the viewports.
            const ConstDataObjectPath propertyPath = state.getObject(sourceProperty());
            DataOORef<const Property> typedProperty = propertyPath.lastAs<Property>();
            const DataVis* containerVis = (propertyPath.size() >= 2) ? propertyPath[propertyPath.size() - 2]->visElement() : nullptr;

            // Verify that the typed property, which has been selected as the source of the color legend, is available.
            if(!typedProperty) {
                // Escalate to an error state if in console mode.
                if(!this_task::isInteractive())
                    throw Exception(tr("The property '%1' set as source of the color legend is not present in the data pipeline output.").arg(sourceProperty().dataTitleOrPath()));
                else
                    co_return PipelineStatus(PipelineStatus::Warning, tr("The property '%1' is not available in the pipeline output.").arg(sourceProperty().dataTitleOrPath()));
            }
            else if(!typedProperty->isTypedProperty()) {
                // Escalate to an error state if in console mode.
                if(!this_task::isInteractive())
                    throw Exception(tr("The property '%1' set as source of the color legend is not a typed property, i.e., it has no ElementType(s) attached.").arg(sourceProperty().dataTitleOrPath()));
                else
                    co_return PipelineStatus(PipelineStatus::Warning, tr("The property '%1' is not a typed property.").arg(sourceProperty().dataTitleOrPath()));
            }

            _autoTitleText = typedProperty->objectTitle();

            // Optionally render the actual 3d shapes of the element types instead of flat color boxes.
            QImage symbolImage;
            bool symbolsRequested = false;
            if(useTypeShapes() && !_symbolRenderingUnavailable) {
                std::vector<ElementTypeSymbol> symbols = getTypeSymbols(typedProperty, containerVis);
                if(!symbols.empty()) {
                    symbolsRequested = true;
                    // In this mode the legend is laid out in square cells, so the aspect ratio parameter does not apply.
                    colorBarRect = computeColorBarRect(physicalViewportRect, legendSize, static_cast<FloatType>(symbols.size()));
                    // Guard against excessively large offscreen images.
                    QSize symbolImageSize = colorBarRect.size().toSize();
                    constexpr int maxSymbolImageExtent = 4096;
                    if(symbolImageSize.width() > maxSymbolImageExtent || symbolImageSize.height() > maxSymbolImageExtent)
                        symbolImageSize.scale(maxSymbolImageExtent, maxSymbolImageExtent, Qt::KeepAspectRatio);
                    try {
                        symbolImage = co_await FutureAwaiter(ObjectExecutor(this),
                            lookupOrRenderTypeSymbolImage(std::move(symbols), symbolImageSize, orientation(),
                                                          acquireSymbolRenderer(frameGraph->renderer())));
                    }
                    catch(const Exception& ex) {
                        // The legend is a decorative element and must never abort the rendering of a frame.
                        // Remember the failure to avoid retrying the offscreen rendering for every frame.
                        ex.logError();
                        invalidateSymbolImageCache();
                        _symbolRenderingUnavailable = true;
                        symbolImage = QImage{};
                    }
                }
            }

            drawDiscreteColorMap(*frameGraph, commandGroup, colorBarRect, legendSize,
                                 getDiscreteColorMapLabels(typedProperty, containerVis), symbolImage);

            if(symbolsRequested && symbolImage.isNull()) {
                co_return PipelineStatus(PipelineStatus::Warning,
                    tr("The 3d shapes of the element types cannot be displayed, because offscreen rendering is not available on this system. "
                       "Falling back to flat color boxes."));
            }
            co_return PipelineStatus::Success;
        }
    }

    // Escalate to an error state if in console mode.
    if(!this_task::isInteractive()) {
        if(!sourcePipeline)
            throw Exception(tr("You are rendering a viewport with an attached ColorLegendOverlay that has no "
                                "source pipeline set. Make sure you set the legend's 'pipeline' field."));
        else
            throw Exception(tr("You are rendering a viewport with an attached ColorLegendOverlay that has no "
                                "source color mapping set. Did you forget to specify a color mapping for the color legend? "
                                "Make sure you set the legend's 'modifier', 'property', or 'color_mapping_source' field. "));
    }
    else {
        // Set warning status to be displayed in the GUI.
        co_return PipelineStatus(PipelineStatus::Warning, tr("No source color mapping has been specified for the color legend."));
    }
}

namespace {
/******************************************************************************
 * Estimates the order of magnitude of a given value
 * estimate since this approach has no mathematical proof
 * might not behave well for all edge cases
 ******************************************************************************/
[[nodiscard]] inline int estimateOrderOfMagnitude(const FloatType value)
{
    FloatType result = std::abs(value);
    constexpr FloatType eps{1e-18};
    if(result < eps) {
        return 0;
    }
    result = std::floor(std::log10(result));
    return static_cast<int>(result);
}

/******************************************************************************
 * Estimates the nearest multiple of tickSpacing from start
 * returns static if static is an integer multiple of tickSpacing
 * estimate since this approach has no mathematical proof
 * might not behave well for all edge cases
 ******************************************************************************/
[[nodiscard]] inline FloatType getFirstTickValidValue(const FloatType start, const FloatType tickSpacing)
{
    return tickSpacing * std::ceil(start / tickSpacing);
}

/******************************************************************************
 * Returns the starting value and the tick spacing as function of a control parameter N. Increment (decrement) N to increase
 * (decrease) the tick spacing. Ideally N should start at 0.
 ******************************************************************************/
[[nodiscard]] inline std::tuple<FloatType, FloatType> getTickPositionsFromN(FloatType lowerLimit, FloatType upperLimit, const int N)
{
    constexpr std::array<FloatType, 4> steps{{2, 4, 5, 10}};
    constexpr int num_steps = std::size(steps);

    // a % b = (b + (a % b)) % b <- correct for negative values of a
    // Selects a valid multiple (step width) from the steps array (based on N)
    const int index{(N % num_steps + num_steps) % num_steps};
    // guarantees flooring division (even for negative numbers)
    // first scaling factor for step width
    // N==-5 gives index==1 and pow==-2 (if steps[][2,5,10}])
    const int pow = static_cast<int>(std::floor(N / static_cast<FloatType>(num_steps)));

    const int oom = estimateOrderOfMagnitude(upperLimit - lowerLimit);
    // inter * 10^pow * 10^(oom-1) = inter * 10^(pow+oom-1)
    // const FloatType inter{steps[index] * std::pow(10, pow) * std::pow(10, oom - 1)};
    const FloatType inter = steps[index] * std::pow(10, pow + oom - 1);
    return {getFirstTickValidValue(lowerLimit, inter), inter};
}

[[nodiscard]] inline int get_number_of_ticks(FloatType lowerLimit, FloatType upperLimit, FloatType inter)
{
    return static_cast<int>(std::round(std::abs(upperLimit - lowerLimit) / inter));
}
}  // namespace

/******************************************************************************
 * Determine the starting value and the tick spacing for a given color bar length and character size. Ticks should be estimate
 * from most to least dense resulting in generally more dense ticks.
 ******************************************************************************/
[[nodiscard]] std::tuple<FloatType, FloatType> ColorLegendOverlay::getAutomaticTickPositions(
    FloatType lowerLimit, FloatType upperLimit, const FloatType lenColorbar, const QFontMetricsF& fontMetrics,
    const QByteArray& labelFormat, const int maxIter) const
{
    // Sort upper and lower limit
    if(lowerLimit > upperLimit) std::swap(lowerLimit, upperLimit);

    // If the format string is empty (or format is %s) 4 ticks are shown as fallback
    if(labelFormat.isNull()) {
        return {(upperLimit - lowerLimit) / 4, (upperLimit - lowerLimit) / 4};
    }

    int scale{0};
    FloatType totalLabelSize;
    for(int i{0}; i < maxIter; i++) {
        const auto [start, inter]{getTickPositionsFromN(lowerLimit, upperLimit, scale)};
        int num_ticks{get_number_of_ticks(lowerLimit, upperLimit, inter)};
        if(num_ticks < 1) {
            scale--;
            continue;
        }
        if(orientation() == Qt::Horizontal) {
            // Sometimes start or start + inter might fall on a "shorter" string label. Two subsequent values need to be checked
            // to guarantee at least one "long" label (num_ticks + 1) to give some more space as usually ticks are not distributed
            // all the way to the color bar boundary
            totalLabelSize =
                (num_ticks + 1) *
                std::max(
                    fontMetrics.horizontalAdvance(QString::asprintf(labelFormat.constData(), start + inter)),
                    fontMetrics.horizontalAdvance(QString::asprintf(labelFormat.constData(), start + inter * (num_ticks - 1))));
        }
        else {  // Vertical
                // num_ticks+1 to account for the top and bottom label denoting the color bar limits
                // lineSpacing gives the character height + the line separation
            totalLabelSize = (num_ticks + 1) * fontMetrics.lineSpacing();
        }
        if(totalLabelSize < lenColorbar) {
            if(num_ticks > 1) return {start, inter};
            return {(upperLimit - lowerLimit) / 2, (upperLimit - lowerLimit)};
        }
        scale++;
    }
    // Fallback, if no good ticks can be found, a single tick is added in the center
    return {(upperLimit - lowerLimit) / 2, (upperLimit - lowerLimit)};
}

/******************************************************************************
 * Determine the starting value for a given tick spacing.
 ******************************************************************************/
[[nodiscard]] FloatType ColorLegendOverlay::getUserDefinedTickPositions(FloatType lowerLimit, FloatType upperLimit,
                                                                        const FloatType inter)
{
    // Sort upper and lower limit
    if(lowerLimit > upperLimit) std::swap(lowerLimit, upperLimit);
    return getFirstTickValidValue(lowerLimit, inter);
}

/******************************************************************************
* Draws the color legend for a Color Coding modifier.
******************************************************************************/
void ColorLegendOverlay::drawContinuousColorMap(FrameGraph& frameGraph, FrameGraph::RenderingCommandGroup& commandGroup,
                                                const QRectF& colorBarRect, FloatType legendSize, const PseudoColorMapping& mapping)
{
    const qreal devicePixelRatio = frameGraph.devicePixelRatio();

    // Controls the tick color: Currently the order is:
    // Border color -> text color
    const Color tickColor{(borderEnabled() ? borderColor() : textColor())};

    // Width of the ticks in pixel.
    const int tickWidth{(int)std::ceil(2.0 * devicePixelRatio)};

    // Relative height of the ticks (as fraction of gradient image size)
    constexpr FloatType innerTickHeight{0.4};
    constexpr FloatType outerTickHeight{0.2};

    // Enforces a minimum distance of ticks from the color bar limits.
    // This prevents duplication of the start and end values, especially for the horizontal color bar
    constexpr FloatType minTickDistanceFromEdge{0.005};

    // Allows the second to last and last tick of a vertical color bar to overlap slightly to get a more
    // pleasant look.
    constexpr FloatType tickOverlapFactor{0.8};

    if(!mapping.gradient())
        return;

    // Compute bounding box of the entire legend to draw the background rectangle.
    QRectF boundingBox;

    int borderWidth = borderEnabled() ? tickWidth : 0;

    // Look up the image primitive for the color bar in the cache.
    const auto& [image, offset] = frameGraph.visCache().lookup<std::tuple<QImage, QPointF>>(
        RendererResourceKey<struct ColorBarImageCache, OORef<ColorCodingGradient>, FloatType, int, bool, Color, QSizeF>{
            mapping.gradient(), devicePixelRatio, orientation(), borderEnabled(), borderColor(), colorBarRect.size()},
        [&](QImage& image, QPointF& offset) {
            // Render the color bar into an image texture.
            // Allocate the image buffer.
            QSize gradientSize = colorBarRect.size().toSize();
            image = QImage(gradientSize.width() + 2 * borderWidth, gradientSize.height() + 2 * borderWidth,
                        frameGraph.preferredImageFormat());
            if(borderEnabled()) image.fill((QColor)borderColor());

            // Create the color gradient image.
            if(orientation() == Qt::Vertical) {
                for(int y = 0; y < gradientSize.height(); y++) {
                    FloatType t = (FloatType)y / (FloatType)std::max(1, gradientSize.height() - 1);
                    unsigned int color = QColor(mapping.gradient()->valueToColor(1.0 - t)).rgb();
                    for(int x = 0; x < gradientSize.width(); x++) {
                        image.setPixel(x + borderWidth, y + borderWidth, color);
                    }
                }
            }
            else {
                for(int x = 0; x < gradientSize.width(); x++) {
                    FloatType t = (FloatType)x / (FloatType)std::max(1, gradientSize.width() - 1);
                    unsigned int color = QColor(mapping.gradient()->valueToColor(t)).rgb();
                    for(int y = 0; y < gradientSize.height(); y++) {
                        image.setPixel(x + borderWidth, y + borderWidth, color);
                    }
                }
            }
            offset = QPointF(-borderWidth, -borderWidth);
        });

    QPoint alignedPos = (colorBarRect.topLeft() + offset).toPoint();
    std::unique_ptr<ImagePrimitive> imagePrimitive = std::make_unique<ImagePrimitive>();
    imagePrimitive->setRectWindow(QRect(alignedPos, image.size()));
    imagePrimitive->setImage(image);

    // Actual bounding box of the rendered color bar including the border (if set).
    const QRectF colorBarImageRect{imagePrimitive->windowRect()};
    boundingBox |= colorBarImageRect;

    QByteArray format = valueFormatString().toUtf8();
    if(format.contains("%s"))
        format.clear();

    _autoLabel1Text = std::isfinite(mapping.maxValue()) ? QString::asprintf(format.constData(), mapping.maxValue()) : QStringLiteral("###");
    _autoLabel2Text = std::isfinite(mapping.minValue()) ? QString::asprintf(format.constData(), mapping.minValue()) : QStringLiteral("###");

    // Notify the UI that the automatic label texts were recalculated during rendering.
    notifyDependents(ColorLegendOverlay::AutoLabelsUpdated);

    QString titleLabel = caption().isEmpty() ? _autoTitleText : caption();
    QString topLabel = label1().isEmpty() ? _autoLabel1Text : label1();
    QString bottomLabel = label2().isEmpty() ? _autoLabel2Text : label2();

    // Determine effective font size.
    const qreal fontSize{legendSize * std::max(FloatType(0), this->fontSize())};
    const qreal textMargin = 0.2 * legendSize / std::max(FloatType(0.01), aspectRatio());

    // Font size is always in logical units.
    FloatType labelFontSize{fontSize * relLabelFontSize() / devicePixelRatio};
    QFont labelFont = this->font();
    TextPrimitive::setFontPixelSize(labelFont, labelFontSize);

    int topFlags = 0;
    int bottomFlags = 0;
    QPointF topPos;
    QPointF bottomPos;

    if(orientation() == Qt::Horizontal) {
        bottomFlags = Qt::AlignRight | Qt::AlignVCenter;
        topFlags = Qt::AlignLeft | Qt::AlignVCenter;
        bottomPos = QPointF(colorBarImageRect.left() - textMargin, colorBarImageRect.top() + 0.5 * colorBarImageRect.height());
        topPos = QPointF(colorBarImageRect.right() + textMargin, colorBarImageRect.top() + 0.5 * colorBarImageRect.height());
    }
    else {  // Vertical
            // If ticks are drawn, the labels are top/bottom labels are drawn further out to align with the tick labels
        FloatType tickSpacing{static_cast<int>(ticksEnabled()) * outerTickHeight * colorBarImageRect.width()};
        if((alignment() & Qt::AlignLeft) || (alignment() & Qt::AlignHCenter)) {
            bottomFlags = Qt::AlignLeft | Qt::AlignVCenter;
            topFlags = Qt::AlignLeft | Qt::AlignVCenter;
            bottomPos = QPointF(colorBarImageRect.right() + textMargin + tickSpacing, colorBarImageRect.bottom());
            topPos = QPointF(colorBarImageRect.right() + textMargin + tickSpacing, colorBarImageRect.top());
        }
        else if(alignment() & Qt::AlignRight) {
            bottomFlags = Qt::AlignRight | Qt::AlignVCenter;
            topFlags = Qt::AlignRight | Qt::AlignVCenter;
            bottomPos = QPointF(colorBarImageRect.left() - textMargin - tickSpacing, colorBarImageRect.bottom());
            topPos = QPointF(colorBarImageRect.left() - textMargin - tickSpacing, colorBarImageRect.top());
        }
    }

    // Prepare limit labels.
    std::unique_ptr<TextPrimitive> label1Primitive, label2Primitive;
    QRectF topLabelBoundingBox;

    if(!topLabel.trimmed().isEmpty()) {
        label1Primitive = std::make_unique<TextPrimitive>();
        label1Primitive->setFont(labelFont);
        label1Primitive->setText(topLabel);
        label1Primitive->setAlignment(topFlags);
        label1Primitive->setPositionWindow(topPos);
        label1Primitive->setColor(textColor());
        label1Primitive->setTextFormat(Qt::AutoText);
        if(outlineEnabled())
            label1Primitive->setOutlineColor(outlineColor());
        topLabelBoundingBox = label1Primitive->computeBounds(frameGraph.visCache(), devicePixelRatio);
        boundingBox |= topLabelBoundingBox;
    }

    if(!bottomLabel.trimmed().isEmpty()) {
        label2Primitive = std::make_unique<TextPrimitive>();
        label2Primitive->setFont(labelFont);
        label2Primitive->setText(bottomLabel);
        label2Primitive->setAlignment(bottomFlags);
        label2Primitive->setPositionWindow(bottomPos);
        label2Primitive->setColor(textColor());
        label2Primitive->setTextFormat(Qt::AutoText);
        if(outlineEnabled())
            label2Primitive->setOutlineColor(outlineColor());
        boundingBox |= label2Primitive->computeBounds(frameGraph.visCache(), devicePixelRatio);
    }

    // Place the title label at the correct location based on color bar direction and position.
    int titleFlags = Qt::AlignBottom;
    QPointF titlePos;
    if(orientation() == Qt::Horizontal) {
        titleFlags = Qt::AlignHCenter | Qt::AlignBottom;
        titlePos.rx() = colorBarImageRect.left() + 0.5 * colorBarImageRect.width();
        titlePos.ry() = colorBarImageRect.top() - 0.5 * textMargin;
    }
    else { // bar orientation == Qt::Vertical
        if(!titleRotationEnabled()) { // title orientation == Qt::Horizontal
            titlePos.ry() = colorBarImageRect.top() - 0.5 * (textMargin + topLabelBoundingBox.height());
            if(alignment() & Qt::AlignLeft) {
                titleFlags = Qt::AlignLeft | Qt::AlignBottom;
                titlePos.rx() = colorBarImageRect.left();
            }
            else if(alignment() & Qt::AlignRight) {
                titleFlags = Qt::AlignRight | Qt::AlignBottom;
                titlePos.rx() = colorBarImageRect.right();
            }
            else {
                titleFlags = Qt::AlignHCenter | Qt::AlignBottom;
                titlePos.rx() = colorBarImageRect.left() + 0.5 * colorBarImageRect.width();
            }
        }
        else {
            titlePos.ry() = colorBarImageRect.top() + 0.5 * colorBarImageRect.height();
            if(alignment() & Qt::AlignRight) {
                titleFlags = Qt::AlignHCenter | Qt::AlignTop;
                titlePos.rx() = colorBarImageRect.right() + textMargin;
            }
            else {
                titleFlags = Qt::AlignHCenter | Qt::AlignBottom;
                titlePos.rx() = colorBarImageRect.left() - textMargin;
            }
        }
    }

    std::unique_ptr<TextPrimitive> titlePrimitive;
    if(!titleLabel.trimmed().isEmpty()) {
        // Prepare title label.
        titlePrimitive = std::make_unique<TextPrimitive>();
        QFont titleFont = this->font();
        TextPrimitive::setFontPixelSize(titleFont, fontSize / devicePixelRatio); // Font size is always in logical units.
        titlePrimitive->setFont(titleFont);
        titlePrimitive->setText(titleLabel);
        titlePrimitive->setColor(textColor());
        if(outlineEnabled())
            titlePrimitive->setOutlineColor(outlineColor());
        titlePrimitive->setAlignment(titleFlags);
        titlePrimitive->setPositionWindow(titlePos);
        titlePrimitive->setTextFormat(Qt::AutoText);
        if(titleRotationEnabled() && orientation() == Qt::Vertical)
            titlePrimitive->setRotation(qDegreesToRadians(270));
        boundingBox |= titlePrimitive->computeBounds(frameGraph.visCache(), devicePixelRatio);
    }

    std::vector<Box2> tickRects;
    std::vector<std::unique_ptr<TextPrimitive>> tickLabels;

    if(ticksEnabled() && std::isfinite(mapping.minValue()) && std::isfinite(mapping.maxValue())) {
        // The font metric needs to be calculated without device pixel ratio scaling of the font.
        // A devicePixelRatio of 3 leads to an intermediate 3x larger colorbarLength during supersampling.
        // However, the font metrics and labels need to be measured based on the original size of 1x to give
        // the correct label size after downsampling the image back to 1x.
        TextPrimitive::setFontPixelSize(labelFont, labelFontSize * devicePixelRatio);
        const QFontMetricsF fontMetrics{labelFont};
        const FloatType colorbarLength = (orientation() == Qt::Horizontal) ? colorBarImageRect.width() : colorBarImageRect.height();

        // Look up tick configuration in the cache
        const auto& [tickStart, tickStep] = frameGraph.visCache().lookup<std::tuple<FloatType, FloatType>>(
            RendererResourceKey<struct TickSpacingCache, QByteArray, FloatType, FloatType, FloatType, FloatType, FloatType, int>{
                format, labelFontSize, mapping.maxValue(), mapping.minValue(), tickSpacing(), colorbarLength, orientation()},
            [&](FloatType& tickStart, FloatType& tickStep) {
                // Calculate new tick configuration if it not found in the cache
                // tickSpacing() == 0 activates the automatic calculation
                // tickSpacing() != 0 uses the user defined settings
                if(tickSpacing() == 0) {
                    const auto [start, step]{
                        getAutomaticTickPositions(mapping.minValue(), mapping.maxValue(), colorbarLength, fontMetrics, format)};
                    tickStart = start;
                    tickStep = step;
                }
                else {
                    tickStart = getUserDefinedTickPositions(mapping.minValue(), mapping.maxValue(), tickSpacing());
                    tickStep = tickSpacing();
                }
            });

        int numTicks = get_number_of_ticks(mapping.minValue(), mapping.maxValue(), tickStep);
        // Check against the hard coded limit for the number of ticks. Prevents crash in the case of too many ticks
        {
            constexpr int maxTicks = 100;
            if(numTicks > maxTicks) {
                // Set warning status to be displayed in the GUI.
                setStatus(PipelineStatus(PipelineStatus::Warning, tr("Tried to generate %1 tick marks. Currently, no more than %2 "
                                                                     "ticks may be generated. Please increase the tick spacing.")
                                                                      .arg(numTicks)
                                                                      .arg(maxTicks)));

                // Escalate to an error state if in scripting mode.
                if(!this_task::isInteractive())
                    throw Exception(tr("Tried to generate %1 tick marks. Currently, no more than %2 "
                                       "ticks may be generated. Please increase the tick spacing.")
                                        .arg(numTicks)
                                        .arg(maxTicks));
                numTicks = 0;
            }
        }

        // Prepare tick marks and labels.
        TextPrimitive labelPrimitive;
        labelPrimitive.setColor(textColor());
        labelPrimitive.setTextFormat(Qt::AutoText);
        TextPrimitive::setFontPixelSize(labelFont, labelFontSize);
        labelPrimitive.setFont(labelFont);
        if(outlineEnabled())
            labelPrimitive.setOutlineColor(outlineColor());
        if(orientation() == Qt::Horizontal) {
            // label
            labelPrimitive.setAlignment(Qt::AlignHCenter | Qt::AlignTop);
            Point2 label_pos;
            label_pos.y() = colorBarImageRect.bottom() + outerTickHeight * colorBarImageRect.height() + fontMetrics.ascent() / 2;
            // ticks
            Point2 tickMin;
            Point2 tickMax;
            tickMin.y() = colorBarImageRect.top() + (1 - innerTickHeight) * colorBarImageRect.height();
            tickMax.y() = colorBarImageRect.top() + (1 + outerTickHeight) * colorBarImageRect.height() + borderWidth;
            boundingBox |= QRectF(QPointF(colorBarImageRect.left(), tickMin.y()), QPointF(colorBarImageRect.right(), tickMax.y()));

            // If the first tick is in the position of the minValue or maxValue it will be hidden.
            // Therefore we need to increase the num_ticks by 1 to get all required ticks drawn correctly.
            numTicks += ((tickStart == mapping.minValue()) || (tickStart == mapping.maxValue()));
            for(int i{0}; i <= numTicks; i++) {
                FloatType tickValue = tickStart + i * tickStep;
                // Fix tick values close to 0 being formatted as 5.5e-17 instead of 0 with the
                // default format specifier "%g".
                tickValue = (std::abs(tickValue) < 1e-12) ? 0.0 : tickValue;

                const FloatType tickPosition = (tickValue - mapping.minValue()) / (mapping.maxValue() - mapping.minValue());
                // omit labels to outside the range or too close to the color bar limit
                if((tickPosition <= 0) || (tickPosition >= 1)) {
                    continue;
                }
                // Label
                labelPrimitive.setText(QString::asprintf(format.constData(), tickValue));
                label_pos.x() = colorBarImageRect.left() + colorBarImageRect.width() * tickPosition;
                labelPrimitive.setPositionWindow(label_pos);
                boundingBox |= labelPrimitive.computeBounds(frameGraph.visCache(), devicePixelRatio);
                tickLabels.push_back(std::make_unique<TextPrimitive>(labelPrimitive));

                // Tick.
                tickMin.x() = colorBarImageRect.left() + tickPosition * colorBarImageRect.width() - (FloatType)tickWidth / 2.0;
                tickMax.x() = colorBarImageRect.left() + tickPosition * colorBarImageRect.width() + (FloatType)tickWidth / 2.0;
                tickRects.emplace_back(tickMin, tickMax);
            }
        }
        else { // orientation() == Qt::Vertical
            // labels
            Point2 labelPos;

            // ticks
            Point2 tickMin;
            Point2 tickMax;
            if((alignment() & Qt::AlignLeft) || (alignment() & Qt::AlignHCenter)) {
                // labels
                labelPrimitive.setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
                labelPos.x() = colorBarImageRect.right() + textMargin + outerTickHeight * colorBarImageRect.width();

                // ticks
                tickMin.x() = colorBarImageRect.left() + (1 - innerTickHeight) * colorBarImageRect.width();
                tickMax.x() = colorBarImageRect.left() + (1 + outerTickHeight) * colorBarImageRect.width() + borderWidth;
            }
            else {
                // labels
                labelPrimitive.setAlignment(Qt::AlignRight | Qt::AlignVCenter);
                labelPos.x() = colorBarImageRect.left() - textMargin - outerTickHeight * colorBarImageRect.width();

                // ticks
                tickMin.x() = colorBarImageRect.right() - (1 + outerTickHeight) * colorBarImageRect.width();
                tickMax.x() = colorBarImageRect.right() - (1 - innerTickHeight) * colorBarImageRect.width();
            }
            for(int i{0}; i < numTicks; i++) {
                FloatType tickValue = tickStart + i * tickStep;
                // Fix tick values close to 0 being formatted as 5.5e-17 instead of 0 with the
                // default format specifier "%g".
                tickValue = (std::abs(tickValue) < 1e-12) ? 0.0 : tickValue;

                FloatType tickPosition = (tickValue - mapping.minValue()) / (mapping.maxValue() - mapping.minValue());
                // omit labels to outside the range or too close to the color bar limit
                if((tickPosition <= minTickDistanceFromEdge) || (tickPosition >= (1 - minTickDistanceFromEdge))) {
                    continue;
                }
                // labels
                labelPrimitive.setText(QString::asprintf(format.constData(), tickValue));
                labelPos.y() = colorBarImageRect.bottom() - colorBarImageRect.height() * tickPosition;

                // Hide the first and last tick mark and label if they overlap with the limit labels
                if(((i == 0) || (i == (numTicks - 1))) &&
                   ((labelPos.y() > (colorBarImageRect.bottom() - tickOverlapFactor * fontMetrics.height())) ||
                    (labelPos.y() < (colorBarImageRect.top() + tickOverlapFactor * fontMetrics.height()))))
                    continue;
                labelPrimitive.setPositionWindow(labelPos);
                boundingBox |= labelPrimitive.computeBounds(frameGraph.visCache(), devicePixelRatio);
                tickLabels.push_back(std::make_unique<TextPrimitive>(labelPrimitive));

                // Tick
                tickMin.y() = colorBarImageRect.bottom() - tickPosition * colorBarImageRect.height() - (FloatType)tickWidth / 2.0;
                tickMax.y() = colorBarImageRect.bottom() - tickPosition * colorBarImageRect.height() + (FloatType)tickWidth / 2.0;
                tickRects.emplace_back(tickMin, tickMax);
                boundingBox |= QRectF(QPointF(tickMin.x(), tickMin.y()), QPointF(tickMax.x(), tickMax.y()));
            }

            // Manually add the tick marks at the ends of the color bar for the limit labels
            tickRects.emplace_back(tickMin.x(), colorBarImageRect.bottom() - tickWidth, tickMax.x(), colorBarImageRect.bottom());
            tickRects.emplace_back(tickMin.x(), colorBarImageRect.top(), tickMax.x(), colorBarImageRect.top() + tickWidth);
            boundingBox |= QRectF(QPointF(tickMin.x(), colorBarImageRect.bottom()), QPointF(tickMax.x(), colorBarImageRect.top()));
        }
    }

    // Render background rectangle.
    if(backgroundEnabled()) {
        // Look up tick image in the cache
        const QImage& backgroundImage = frameGraph.visCache().lookup<QImage>(
            RendererResourceKey<struct ColorBarBackgroundImageCache, Color>{backgroundColor()},
            [&](QImage& backgroundImage) {
                // Generate image if not found in the cache
                // 1 x 1 px texture of the right color which will be stretched to the desired rectangle dimensions.
                backgroundImage = QImage{QSize(1, 1), frameGraph.preferredImageFormat()};
                backgroundImage.fill(static_cast<QColor>(backgroundColor()));
            });

        boundingBox.adjust(-textMargin, -textMargin, textMargin, textMargin);
        commandGroup.addPrimitivePreprojected(std::make_unique<ImagePrimitive>(backgroundImage, boundingBox.toAlignedRect()));
    }

    // Render color bar.
    commandGroup.addPrimitivePreprojected(std::move(imagePrimitive));

    // Render title and limit labels.
    if(titlePrimitive)
        commandGroup.addPrimitivePreprojected(std::move(titlePrimitive));
    if(label1Primitive)
        commandGroup.addPrimitivePreprojected(std::move(label1Primitive));
    if(label2Primitive)
        commandGroup.addPrimitivePreprojected(std::move(label2Primitive));

    // Render ticks.
    if(!tickRects.empty()) {

        // Look up tick image in the cache.
        const QImage& tickImage = frameGraph.visCache().lookup<QImage>(
            RendererResourceKey<struct ColorBarTickImageCache, Color>{tickColor},
            [&](QImage& tickImage) {
                // Generate tick image primitive if not found in the cache.
                // 1x1 pixel texture of the right color which will be stretched to the desired tick dimensions
                tickImage = QImage{QSize(1, 1), frameGraph.preferredImageFormat()};
                tickImage.fill(static_cast<QColor>(tickColor));
            });

        // Render the series of tick images.
        for(const auto& rect : tickRects) {
            commandGroup.addPrimitivePreprojected(std::make_unique<ImagePrimitive>(tickImage, rect));
        }
    }

    // Render tick labels.
    for(auto& labelPrimitive : tickLabels) {
        commandGroup.addPrimitivePreprojected(std::move(labelPrimitive));
    }
}

/******************************************************************************
* Returns the enabled element types of a typed property, sorted by numeric ID.
* Types that are indistinguishable in the legend are reported only once.
******************************************************************************/
std::vector<const ElementType*> ColorLegendOverlay::getSortedElementTypes(const Property* property, const DataVis* visElement)
{
    OVITO_ASSERT(property->isTypedProperty());
    std::vector<const ElementType*> types;
    for(const ElementType* type : property->elementTypes()) {
        if(type && type->enabled())
            types.push_back(type);
    }
    std::ranges::sort(types, [](const ElementType* lhs, const ElementType* rhs) { return lhs->numericId() < rhs->numericId(); });

    // A typed property often contains several types that are indistinguishable in the legend. Molecular datasets,
    // for example, define one force field type per chemical environment of an atom, and all of them carry the name
    // and the appearance of the same chemical element. Listing such types more than once is just noise, so only the
    // first type of each group is kept whose members agree in every aspect the legend displays.
    struct LegendAppearance {
        QString name;
        Color color;
        ElementTypeSymbol symbol;
        bool operator==(const LegendAppearance& other) const = default;
    };
    std::vector<LegendAppearance> appearances;
    appearances.reserve(types.size());
    std::vector<const ElementType*> uniqueTypes;
    uniqueTypes.reserve(types.size());
    for(const ElementType* type : types) {
        LegendAppearance appearance{type->objectTitle(), type->color(), type->symbolGeometry(visElement)};
        if(std::ranges::find(appearances, appearance) != appearances.end())
            continue;
        appearances.push_back(std::move(appearance));
        uniqueTypes.push_back(type);
    }
    return uniqueTypes;
}

ColorLegendOverlay::DiscreteColorMapLabels ColorLegendOverlay::getDiscreteColorMapLabels(const Property* property, const DataVis* visElement)
{
    DiscreteColorMapLabels labels;
    for(const ElementType* type : getSortedElementTypes(property, visElement))
        labels.emplace_back(type->numericId(), type->objectTitle(), type->color());
    return labels;
}

/******************************************************************************
* Queries the 3d shapes representing the element types of a typed property.
******************************************************************************/
std::vector<ElementTypeSymbol> ColorLegendOverlay::getTypeSymbols(const Property* property, const DataVis* visElement)
{
    std::vector<ElementTypeSymbol> symbols;
    bool anyValid = false;
    for(const ElementType* type : getSortedElementTypes(property, visElement)) {
        ElementTypeSymbol symbol = type->symbolGeometry(visElement);
        if(symbol)
            anyValid = true;
        else
            symbol.color = type->color();
        symbols.push_back(std::move(symbol));
    }
    // If not a single type has a 3d representation, let the caller fall back to flat color boxes.
    if(!anyValid)
        symbols.clear();
    return symbols;
}

ColorLegendOverlay::DiscreteColorMapLabels ColorLegendOverlay::getDiscreteColorMapLabels(const ColorCodingGradient* gradient,
                                                                                         FloatType startValue, FloatType endValue,
                                                                                         int orientation) const
{
    const int numDiscreteColors = DiscreteColorMap::binCount(startValue, endValue);
    const int offset = (int)std::round(std::min(startValue, endValue));
    DiscreteColorMapLabels labels;
    labels.reserve(numDiscreteColors);

    // Format the numbers matching continuous color maps.
    QByteArray format = valueFormatString().toUtf8();
    if(format.contains("%s")) {
        format.clear();
    }

    const bool reverseMapping = startValue > endValue;
    const bool reverseLabelsOrder = (orientation == Qt::Vertical) ^ reverseMapping;

    for(int i = 0; i < numDiscreteColors; ++i) {
        FloatType t = DiscreteColorMap::mapValue(static_cast<FloatType>(i) / (numDiscreteColors - 1), numDiscreteColors);
        labels.emplace_back(i, QString::asprintf(format.constData(), FloatType(offset + i)),
                            gradient->valueToColor(reverseMapping ? (1.0 - t) : t));
    }

    if(reverseLabelsOrder) {
        std::ranges::reverse(labels);
    }

    return labels;
}

/******************************************************************************
* Returns the renderer to be used for rendering the 3d type symbols.
******************************************************************************/
SceneRenderer* ColorLegendOverlay::acquireSymbolRenderer(const OORef<SceneRenderer>& frameGraphRenderer)
{
    OvitoClassPtr sourceClass = frameGraphRenderer ? &frameGraphRenderer->getOOClass() : nullptr;

    // Reuse the existing instance as long as the frame graph is executed by the same kind of renderer as before.
    if(_symbolRenderer && _symbolRendererSourceClass == sourceClass)
        return _symbolRenderer.get();

    _symbolImageCache.clear();
    _symbolRenderer.reset();
    _symbolRendererSourceClass = sourceClass;

    if(frameGraphRenderer)
        _symbolRenderer = frameGraphRenderer->createPreviewRenderer();

    // Fall back to the standard renderer if the frame graph's renderer is unknown or does not
    // support the rendering of preview images.
    if(!_symbolRenderer) {
        NoninteractiveContext noninteractiveContext;
        _symbolRenderer = OORef<StandardRenderer>::create();
    }

    return _symbolRenderer.get();
}

/******************************************************************************
* Renders the 3d shapes of the given element types into a single offscreen image.
******************************************************************************/
Future<QImage> ColorLegendOverlay::renderTypeSymbolImage(std::vector<ElementTypeSymbol> symbols, QSize imageSize, int orientation,
                                                          OORef<SceneRenderer> renderer)
{
    OVITO_ASSERT(this_task::isMainThread());

    std::shared_ptr<UserInterface> ui = this_task::ui();
    if(!ui || !renderer || symbols.empty() || imageSize.isEmpty())
        co_return QImage{};

    const int numCells = symbols.size();
    const bool vertical = (orientation == Qt::Vertical);
    const FloatType imageWidth = imageSize.width();
    const FloatType imageHeight = imageSize.height();

    // Determine the extent of the largest of the shapes. It sets the common scale of all symbols.
    FloatType maxExtent = 0;
    for(const ElementTypeSymbol& symbol : symbols)
        maxExtent = std::max(maxExtent, symbol.boundingRadius());
    if(maxExtent <= 0)
        co_return QImage{};

    // Number of world units per image pixel, chosen such that the largest shape just fits into its cell.
    constexpr FloatType cellMargin = FloatType(0.12);
    const FloatType cellSize = vertical ? std::min(imageWidth, imageHeight / numCells)
                                        : std::min(imageWidth / numCells, imageHeight);
    const FloatType scale = maxExtent / (FloatType(0.5) * cellSize * (1 - cellMargin));

    // Set up a standardized orthographic camera. The viewing direction is slightly tilted, so that box-shaped
    // and polyhedral types are rendered as recognizable 3d bodies instead of flat silhouettes.
    const Vector3 viewDir = Vector3(FloatType(0.30), FloatType(1), FloatType(-0.35)).normalized();
    const FloatType cameraDistance = 4 * maxExtent;
    ViewProjectionParameters projParams;
    projParams.viewMatrix = AffineTransformation::lookAlong(Point3::Origin(), viewDir, Vector3(0,0,1));
    projParams.inverseViewMatrix = projParams.viewMatrix.inverse();
    projParams.isPerspective = false;
    projParams.aspectRatio = imageHeight / imageWidth;
    projParams.fieldOfView = FloatType(0.5) * imageHeight * scale;
    projParams.znear = cameraDistance - FloatType(1.5) * maxExtent;
    projParams.zfar = cameraDistance + FloatType(1.5) * maxExtent;
    projParams.projectionMatrix = Matrix4::ortho(-projParams.fieldOfView / projParams.aspectRatio, projParams.fieldOfView / projParams.aspectRatio,
                                                 -projParams.fieldOfView, projParams.fieldOfView,
                                                 projParams.znear, projParams.zfar);
    projParams.inverseProjectionMatrix = projParams.projectionMatrix.inverse();
    projParams.validityInterval = TimeInterval::infinite();

    // Create a frame graph that can be submitted to the RenderThread for offscreen rendering.
    OORef<FrameGraph> symbolFrameGraph = OORef<FrameGraph>::create(
        ui->datasetContainer().visCache()->acquireResourceFrame(),
        AnimationTime(0), projParams, imageSize, false, false, false, 1.0);
    // Render on a fully transparent background, so that the legend's own background shows through.
    symbolFrameGraph->setClearColor(ColorA(0,0,0,0));
    symbolFrameGraph->setRenderer(renderer);
    // Note: No outline settings are set, because the default OutlineSettings have the effect turned off.

    // World-space half-size of one legend cell.
    const FloatType cellHalfSize = FloatType(0.5) * cellSize * scale;

    FrameGraph::RenderingCommandGroup& group = symbolFrameGraph->addCommandGroup(FrameGraph::SceneLayer);
    for(int i = 0; i < numCells; i++) {
        ElementTypeSymbol symbol = symbols[i];

        // Types without a 3d representation, e.g. structure types, keep a flat color box filling their entire cell.
        if(!symbol) {
            symbol.shape = ElementTypeSymbol::Shape::Square;
            symbol.radius = cellHalfSize;
        }

        // Position of the cell's center in camera space, lifted into world space.
        // Note that only the translation is taken over into the model transformation. Applying the camera rotation
        // as well would cancel out the view rotation and make world-axis-aligned shapes, such as cubes and
        // polyhedral meshes, face the camera head-on instead of being seen from the standardized tilted angle.
        FloatType u = 0, v = 0;
        if(vertical)
            v = (FloatType(0.5) - (i + FloatType(0.5)) / numCells) * imageHeight * scale;
        else
            u = ((i + FloatType(0.5)) / numCells - FloatType(0.5)) * imageWidth * scale;
        const Point3 cellCenter = projParams.inverseViewMatrix * Point3(u, v, -cameraDistance);
        const AffineTransformation tm = AffineTransformation::translation(cellCenter - Point3::Origin());

        switch(symbol.shape) {
        case ElementTypeSymbol::Shape::Sphere:
        case ElementTypeSymbol::Shape::Box:
        case ElementTypeSymbol::Shape::Circle:
        case ElementTypeSymbol::Shape::Square: {
            const bool boxShaped = (symbol.shape == ElementTypeSymbol::Shape::Box || symbol.shape == ElementTypeSymbol::Shape::Square);
            const bool flatShaded = (symbol.shape == ElementTypeSymbol::Shape::Circle || symbol.shape == ElementTypeSymbol::Shape::Square);
            BufferFactory<Point3> positionBuffer(1);
            positionBuffer[0] = Point3::Origin();
            std::unique_ptr<ParticlePrimitive> primitive = std::make_unique<ParticlePrimitive>();
            primitive->setParticleShape(boxShaped ? ParticlePrimitive::SquareCubicShape : ParticlePrimitive::SphericalShape);
            primitive->setShadingMode(flatShaded ? ParticlePrimitive::FlatShading : ParticlePrimitive::NormalShading);
            primitive->setRenderingQuality(ParticlePrimitive::HighQuality);
            primitive->setUniformColor(symbol.color);
            primitive->setPositions(positionBuffer.take());
            primitive->setUniformRadius(symbol.radius);
            Box3 boundingBox = primitive->computeBoundingBox(symbolFrameGraph->visCache());
            group.addPrimitiveNonpickable(std::move(primitive), tm, boundingBox);
            break;
        }
        case ElementTypeSymbol::Shape::Cylinder:
        case ElementTypeSymbol::Shape::Spherocylinder: {
            BufferFactory<Point3G> vertexBuffer(2);
            vertexBuffer[0] = Point3G(0, 0, -static_cast<GraphicsFloatType>(symbol.length) / 2);
            vertexBuffer[1] = Point3G(0, 0,  static_cast<GraphicsFloatType>(symbol.length) / 2);
            std::unique_ptr<CylinderPrimitive> cylinderPrimitive = std::make_unique<CylinderPrimitive>();
            cylinderPrimitive->setShape(CylinderPrimitive::CylinderShape);
            cylinderPrimitive->setShadingMode(CylinderPrimitive::NormalShading);
            cylinderPrimitive->setUniformColor(symbol.color);
            cylinderPrimitive->setUniformWidth(2 * symbol.radius);
            cylinderPrimitive->setVertexPositions(vertexBuffer.take());
            if(symbol.shape == ElementTypeSymbol::Shape::Spherocylinder) {
                // Cap the cylinder with two hemispheres, as ParticlesVis does.
                std::unique_ptr<ParticlePrimitive> capPrimitive = std::make_unique<ParticlePrimitive>();
                capPrimitive->setParticleShape(ParticlePrimitive::SphericalShape);
                capPrimitive->setShadingMode(ParticlePrimitive::NormalShading);
                capPrimitive->setRenderingQuality(ParticlePrimitive::HighQuality);
                capPrimitive->setPositions(cylinderPrimitive->vertexPositions());
                capPrimitive->setUniformRadius(symbol.radius);
                capPrimitive->setUniformColor(symbol.color);
                Box3 boundingBox = capPrimitive->computeBoundingBox(symbolFrameGraph->visCache());
                group.addPrimitiveNonpickable(std::move(capPrimitive), tm, boundingBox);
            }
            Box3 boundingBox = cylinderPrimitive->computeBoundingBox(symbolFrameGraph->visCache());
            group.addPrimitiveNonpickable(std::move(cylinderPrimitive), tm, boundingBox);
            break;
        }
        case ElementTypeSymbol::Shape::Mesh: {
            std::unique_ptr<MeshPrimitive> primitive = std::make_unique<MeshPrimitive>();
            primitive->setMesh(symbol.mesh);
            primitive->setEmphasizeEdges(symbol.emphasizeEdges);
            primitive->setWireframeWidth(0.0);
            primitive->setCullFaces(symbol.cullFaces);
            if(!symbol.useMeshColor)
                primitive->setUniformColor(ColorA(symbol.color));
            Box3 boundingBox = primitive->computeBoundingBox(symbolFrameGraph->visCache());
            // The polyhedral shape is scaled by the type's radius, just like in ParticlesVis::renderMeshBasedParticles().
            group.addPrimitiveNonpickable(std::move(primitive), tm * AffineTransformation::scaling(symbol.radius), boundingBox);
            break;
        }
        default:
            break;
        }
    }
    symbolFrameGraph->computeSceneBoundingBox();

    // Render the frame graph into an offscreen buffer.
    // Note: The render target is deliberately not cached between frames, because it keeps the RenderThread alive.
    RenderTarget renderTarget = ui->renderThread()->createOffscreenTarget(imageSize * renderer->supersamplingFactor());
    std::shared_ptr<FrameBuffer> frameBuffer = std::make_shared<FrameBuffer>(imageSize.width(), imageSize.height());
    std::unique_ptr<SceneRenderer::Configuration> rendererConfig = renderer->createConfiguration(*symbolFrameGraph);
    co_await FutureAwaiter(ObjectExecutor(this),
        renderTarget.renderOffscreenFrame(std::move(symbolFrameGraph), std::move(rendererConfig), frameBuffer, TaskProgress::Ignore));

    co_return frameBuffer->image();
}

/******************************************************************************
* Returns the cached offscreen image showing the 3d shapes of the given element types.
******************************************************************************/
SharedFuture<QImage> ColorLegendOverlay::lookupOrRenderTypeSymbolImage(std::vector<ElementTypeSymbol> symbols, QSize imageSize,
                                                                       int orientation, SceneRenderer* renderer)
{
    OvitoClassPtr rendererClass = &renderer->getOOClass();

    // Look up an existing image. Note that the cache keys are value comparisons of the element type shapes.
    // This is a complete invalidation criterion, because data objects in OVITO are copy-on-write, i.e., any change
    // to a type's color, radius, shape, or mesh yields a different ElementTypeSymbol value.
    auto iter = std::find_if(_symbolImageCache.begin(), _symbolImageCache.end(), [&](const SymbolImageCacheEntry& entry) {
        return entry.imageSize == imageSize && entry.orientation == orientation && entry.rendererClass == rendererClass
            && entry.symbols == symbols;
    });
    if(iter != _symbolImageCache.end()) {
        // Move the entry to the front of the most-recently-used list.
        if(iter != _symbolImageCache.begin())
            std::rotate(_symbolImageCache.begin(), iter, std::next(iter));
        return _symbolImageCache.front().image;
    }

    // Start a new rendering operation. Storing the future instead of the finished image makes concurrent
    // requests from several viewports share a single rendering operation.
    SharedFuture<QImage> image = renderTypeSymbolImage(symbols, imageSize, orientation, renderer);
    _symbolImageCache.push_front(SymbolImageCacheEntry{std::move(symbols), imageSize, orientation, rendererClass, image});

    // Keep the cache bounded. Several viewports of different sizes may show the same legend.
    constexpr size_t maxCacheSize = 4;
    while(_symbolImageCache.size() > maxCacheSize)
        _symbolImageCache.pop_back();

    return image;
}

/******************************************************************************
* Discards all cached type symbol images.
******************************************************************************/
void ColorLegendOverlay::invalidateSymbolImageCache()
{
    _symbolImageCache.clear();
    _symbolRenderer.reset();
    _symbolRendererSourceClass = nullptr;
    _symbolRenderingUnavailable = false;
}

/******************************************************************************
* Draws the color legend for a typed property.
******************************************************************************/
void ColorLegendOverlay::drawDiscreteColorMap(FrameGraph& frameGraph, FrameGraph::RenderingCommandGroup& commandGroup,
                                              const QRectF& colorBarRect, FloatType legendSize,
                                              const DiscreteColorMapLabels& colorMapLabels, const QImage& symbolImage)
{
    const qreal devicePixelRatio = frameGraph.devicePixelRatio();

    // The border is only drawn around the flat color boxes. The 3d shapes of the element types stand on a
    // transparent background, which a border would turn into an opaque block behind the symbols.
    const bool drawBorder = borderEnabled() && symbolImage.isNull();

    // Compute bounding box of the entire legend to draw the background rectangle.
    QRectF boundingBox;

    // Look up the image primitive for the color bar in the cache.
    const auto& [image, offset] = frameGraph.visCache().lookup<std::tuple<QImage, QPointF>>(
        RendererResourceKey<struct TypeColorsImageCache, DiscreteColorMapLabels, FloatType, int, bool, Color, QSizeF, qint64>{
            colorMapLabels,
            devicePixelRatio,
            orientation(),
            drawBorder,
            borderColor(),
            colorBarRect.size(),
            symbolImage.cacheKey(),
        },
        [&](QImage& image, QPointF& offset) {
            // Render the color fields into an image texture.
            // Allocate the image buffer.
            QSize gradientSize = colorBarRect.size().toSize();
            int borderWidth = drawBorder ? (int)std::ceil(2.0 * devicePixelRatio) : 0;
            image = QImage(gradientSize.width() + 2*borderWidth, gradientSize.height() + 2*borderWidth, frameGraph.preferredImageFormat());
            if(drawBorder)
                image.fill((QColor)borderColor());
            else if(!symbolImage.isNull())
                image.fill(Qt::transparent);   // The type shapes do not cover the entire color bar area.

            // Create the color gradient image.
            if(!colorMapLabels.empty()) {
                QPainter painter(&image);
                // The color boxes are skipped when the 3d shapes of the types are displayed instead.
                if(symbolImage.isNull()) {
                    if(orientation() == Qt::Vertical) {
                        int effectiveSize = gradientSize.height() - borderWidth * (colorMapLabels.size() - 1);
                        for(size_t i = 0; i < colorMapLabels.size(); i++) {
                            QRect rect(borderWidth, borderWidth + (i * effectiveSize / colorMapLabels.size()) + i * borderWidth,
                                       gradientSize.width(), 0);
                            rect.setBottom(borderWidth + ((i + 1) * effectiveSize / colorMapLabels.size()) + i * borderWidth - 1);
                            painter.fillRect(rect, QColor(std::get<Color>(colorMapLabels[i])));
                        }
                    }
                    else {
                        int effectiveSize = gradientSize.width() - borderWidth * (colorMapLabels.size() - 1);
                        for(size_t i = 0; i < colorMapLabels.size(); i++) {
                            QRect rect(borderWidth + (i * effectiveSize / colorMapLabels.size()) + i * borderWidth, borderWidth, 0,
                                       gradientSize.height());
                            rect.setRight(borderWidth + ((i + 1) * effectiveSize / colorMapLabels.size()) + i * borderWidth - 1);
                            painter.fillRect(rect, QColor(std::get<Color>(colorMapLabels[i])));
                        }
                    }
                }
                else {
                    // Composite the offscreen rendering of the 3d type shapes on top of the color bar.
                    painter.drawImage(QRect(QPoint(borderWidth, borderWidth), gradientSize), symbolImage);
                }
            }
            offset = QPointF(-borderWidth,-borderWidth);
        });

    QPoint alignedPos = (colorBarRect.topLeft() + offset).toPoint();
    std::unique_ptr<ImagePrimitive> imagePrimitive = std::make_unique<ImagePrimitive>();
    imagePrimitive->setRectWindow(QRect(alignedPos, image.size()));
    imagePrimitive->setImage(image);

    // Actual bounding box of the rendered color bar including the border (if set).
    const QRectF colorBarImageRect{imagePrimitive->windowRect()};
    boundingBox |= colorBarImageRect;

    // Count the number of element types that are enabled.
    int numTypes = colorMapLabels.size();

    const qreal fontSize = legendSize * std::max(FloatType(0), this->fontSize());
    // The margin is derived from the thickness of the color bar rather than from the aspect ratio parameter,
    // because the bar geometry is determined by the number of types when the 3d type shapes are displayed.
    const qreal textMargin = 0.2 * ((orientation() == Qt::Vertical) ? colorBarRect.width() : colorBarRect.height());

    // Move the text path to the correct location based on color bar direction and position.
    int titleFlags = 0;
    QPointF titlePos;
    if(orientation() == Qt::Horizontal) {
        if((alignment() & Qt::AlignTop) || (alignment() & Qt::AlignVCenter)) {
            titleFlags = Qt::AlignHCenter | Qt::AlignBottom;
            titlePos.rx() = colorBarImageRect.left() + 0.5 * colorBarImageRect.width();
            titlePos.ry() = colorBarImageRect.top() - 0.5 * textMargin;
        }
        else {
            titleFlags = Qt::AlignHCenter | Qt::AlignTop;
            titlePos.rx() = colorBarImageRect.left() + 0.5 * colorBarImageRect.width();
            titlePos.ry() = colorBarImageRect.bottom() + 0.5 * textMargin;
        }
    }
    else { // bar orientation == Qt::Vertical
        if(!titleRotationEnabled()) { // title orientation == Qt::Horizontal
            titlePos.ry() = colorBarImageRect.top() - textMargin;
            if(alignment() & Qt::AlignLeft) {
                titleFlags = Qt::AlignLeft | Qt::AlignBottom;
                titlePos.rx() = colorBarImageRect.left();
            }
            else if(alignment() & Qt::AlignRight) {
                titleFlags = Qt::AlignRight | Qt::AlignBottom;
                titlePos.rx() = colorBarImageRect.right();
            }
            else {
                titleFlags = Qt::AlignHCenter | Qt::AlignBottom;
                titlePos.rx() = colorBarImageRect.left() + 0.5 * colorBarImageRect.width();
            }
        }
        else {
            titlePos.ry() = colorBarImageRect.top() + 0.5 * colorBarImageRect.height();
            if(alignment() & Qt::AlignRight) {
                titleFlags = Qt::AlignHCenter | Qt::AlignTop;
                titlePos.rx() = colorBarImageRect.right() + textMargin;
            }
            else {
                titleFlags = Qt::AlignHCenter | Qt::AlignBottom;
                titlePos.rx() = colorBarImageRect.left() - textMargin;
            }
        }
    }

    // Prepare title label.
    std::unique_ptr<TextPrimitive> titlePrimitive;
    const QString titleLabel = caption().isEmpty() ? _autoTitleText : caption();
    if(!titleLabel.trimmed().isEmpty()) {
        titlePrimitive = std::make_unique<TextPrimitive>();
        QFont titleFont = this->font();
        TextPrimitive::setFontPixelSize(titleFont, fontSize / devicePixelRatio); // Font size is always in logical units.
        titlePrimitive->setFont(titleFont);
        titlePrimitive->setText(titleLabel);
        titlePrimitive->setColor(textColor());
        if(outlineEnabled())
            titlePrimitive->setOutlineColor(outlineColor());
        titlePrimitive->setAlignment(titleFlags);
        titlePrimitive->setPositionWindow(titlePos);
        titlePrimitive->setTextFormat(Qt::AutoText);
        if(titleRotationEnabled() && orientation() == Qt::Vertical)
            titlePrimitive->setRotation(qDegreesToRadians(270));
        boundingBox |= titlePrimitive->computeBounds(frameGraph.visCache(), devicePixelRatio);
    }

    // Prepare type name labels.
    if(numTypes == 0)
        numTypes = 1; // Avoid division by 0 below.

    // Layout of the type labels.
    int labelFlags = 0;
    QPointF labelPos;
    if(orientation() == Qt::Vertical) {
        if((alignment() & Qt::AlignLeft) || (alignment() & Qt::AlignHCenter)) {
            labelFlags |= Qt::AlignLeft | Qt::AlignVCenter;
            labelPos.setX(colorBarRect.right() + textMargin);
        }
        else {
            labelFlags |= Qt::AlignRight | Qt::AlignVCenter;
            labelPos.setX(colorBarRect.left() - textMargin);
        }
        labelPos.setY(colorBarRect.top() + 0.5 * colorBarRect.height() / numTypes);
    }
    else {
        if((alignment() & Qt::AlignTop) || (alignment() & Qt::AlignVCenter)) {
            labelFlags |= Qt::AlignHCenter | Qt::AlignTop;
            labelPos.setY(colorBarRect.bottom() + 0.5 * textMargin);
        }
        else {
            labelFlags |= Qt::AlignHCenter | Qt::AlignBottom;
            labelPos.setY(colorBarRect.top() - textMargin);
        }
        labelPos.setX(colorBarRect.left() + 0.5 * colorBarRect.width() / numTypes);
    }

    FloatType labelFontSize{fontSize * relLabelFontSize() / devicePixelRatio};
    TextPrimitive labelPrimitive;
    QFont labelFont = this->font();
    TextPrimitive::setFontPixelSize(labelFont, labelFontSize);
    labelPrimitive.setFont(labelFont);
    labelPrimitive.setColor(textColor());
    if(outlineEnabled())
        labelPrimitive.setOutlineColor(outlineColor());
    labelPrimitive.setAlignment(labelFlags);
    labelPrimitive.setTextFormat(Qt::AutoText);

    std::vector<std::unique_ptr<TextPrimitive>> labels;
    for(const auto& colorLabel : colorMapLabels) {
        labelPrimitive.setText(std::get<QString>(colorLabel));
        labelPrimitive.setPositionWindow(labelPos);
        boundingBox |= labelPrimitive.computeBounds(frameGraph.visCache(), devicePixelRatio);
        labels.push_back(std::make_unique<TextPrimitive>(labelPrimitive));

        if(orientation() == Qt::Vertical)
            labelPos.ry() += colorBarRect.height() / numTypes;
        else
            labelPos.rx() += colorBarRect.width() / numTypes;
    }

    // Render background rectangle.
    if(backgroundEnabled()) {
        // Look up tick image in the cache
        const QImage& backgroundImage = frameGraph.visCache().lookup<QImage>(
            RendererResourceKey<struct ColorBarBackgroundImageCache, Color>{backgroundColor()},
            [&](QImage& backgroundImage) {
                // Generate image if not found in the cache
                // 1x1 pixel texture of the right color which will be stretched to the desired rectangle dimensions.
                backgroundImage = QImage{QSize(1, 1), frameGraph.preferredImageFormat()};
                backgroundImage.fill(static_cast<QColor>(backgroundColor()));
            });

        boundingBox.adjust(-textMargin, -textMargin, textMargin, textMargin);
        commandGroup.addPrimitivePreprojected(std::make_unique<ImagePrimitive>(backgroundImage, boundingBox.toAlignedRect()));
    }

    // Render title.
    if(titlePrimitive)
        commandGroup.addPrimitivePreprojected(std::move(titlePrimitive));

    // Render color bar.
    commandGroup.addPrimitivePreprojected(std::move(imagePrimitive));

    // Render type labels.
    for(auto& labelPrimitive : labels) {
        commandGroup.addPrimitivePreprojected(std::move(labelPrimitive));
    }

    // Notify the UI that the automatic label texts were recalculated during rendering.
    notifyDependents(ColorLegendOverlay::AutoLabelsUpdated);
}

/******************************************************************************
* This method is called once for this object after they have been completely loaded from a stream.
******************************************************************************/
void ColorLegendOverlay::loadFromStreamComplete(ObjectLoadStream& stream)
{
    ViewportOverlay::loadFromStreamComplete(stream);

    invalidateSymbolImageCache();

    // For backward compatibility with OVITO 3.16 and earlier:
    // Displaying the 3d shapes of the element types became the default behavior in OVITO 3.17.
    // Legends restored from older state files keep rendering flat color boxes, so that saved
    // visualizations continue to look the way their author created them.
    if(stream.formatVersion() < 30019)
        setUseTypeShapes(false);

    // For backward compatibility with OVITO 3.10.6:
    if(!pipeline()) {
        if(DataSet* dataset = stream.datasetBeingLoaded()) {
            // Automatically choose a scene pipeline for this overlay.
            if(Viewport* vp = dataset->viewportConfig()->activeViewport()) {
                if(Scene* scene = vp->scene()) {
                    Pipeline* selectedPipeline = nullptr;
                    scene->visitPipelines([&](SceneNode* sceneNode) {
                        Pipeline* pipeline = sceneNode->pipeline();
                        if(selectedPipeline == nullptr)
                            selectedPipeline = pipeline;
                        if(modifier()) {
                            ModificationNode* modNode = nullptr;
                            PipelineNode* node = pipeline->head();
                            for(;;) {
                                if((modNode = dynamic_object_cast<ModificationNode>(node))) {
                                    if(modNode->modifier() == modifier()) {
                                        selectedPipeline = pipeline;
                                        return false;
                                    }
                                    node = modNode->input();
                                }
                                else break;
                            }
                        }
                        else if(sourceProperty()) {
                            const PipelineFlowState& state = pipeline->getCachedPipelineOutput(scene->animationSettings()->currentTime());
                            if(state.getLeafObject(sourceProperty())) {
                                selectedPipeline = pipeline;
                                return false;
                            }
                        }
                        return true;
                    });
                    setPipeline(selectedPipeline);
                }
            }
        }
    }
}

}   // End of namespace
