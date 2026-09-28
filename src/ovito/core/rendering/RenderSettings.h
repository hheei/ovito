// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/core/Core.h>
#include <ovito/core/oo/RefTarget.h>
#include <ovito/core/dataset/animation/TimeInterval.h>
#include <ovito/core/dataset/animation/controller/Controller.h>
#include <ovito/core/dataset/animation/AnimationSettings.h>
#include "FrameBuffer.h"
#include "SceneRenderer.h"

namespace Ovito {

/**
 * Stores general settings for rendering pictures and movies.
 */
class OVITO_CORE_EXPORT RenderSettings : public RefTarget
{
    OVITO_CLASS(RenderSettings)

public:

    /// Assembles the depth-aware outline post-processing parameters into an OutlineSettings
    /// structure, which is handed to the renderer through the FrameGraph.
    OutlineSettings outlineSettings() const;

    /// This enumeration specifies the animation range that should be rendered.
    enum RenderingRangeType {
        CURRENT_FRAME,      ///< Renders the current animation frame.
        ANIMATION_INTERVAL, ///< Renders the complete animation interval.
        CUSTOM_INTERVAL,    ///< Renders a user-defined time interval.
        CUSTOM_FRAME,       ///< Renders a specific animation frame.
    };
    Q_ENUM(RenderingRangeType);

public:

    /// Constructor.
    void initializeObject(ObjectInitializationFlags flags);

    /// Returns the aspect ratio (height/width) of the rendered image.
    FloatType outputImageAspectRatio() const { return (FloatType)outputImageHeight() / (FloatType)outputImageWidth(); }

    /// Returns the background color of the rendered image.
    Color backgroundColor() const { return backgroundColorAt(AnimationTime(0)); }
    /// Returns the background color of the rendered image at the given animation time.
    Color backgroundColorAt(AnimationTime time) const { return backgroundColorController() ? backgroundColorController()->getColorValue(time) : Color(0,0,0); }
    /// Sets the background color of the rendered image.
    void setBackgroundColor(const Color& color) { if(backgroundColorController()) backgroundColorController()->setColorValue(AnimationTime(0), color); }

    /// Returns the output filename of the rendered image.
    const QString& imageFilename() const { return imageInfo().filename(); }
    /// Sets the output filename of the rendered image.
    void setImageFilename(const QString& filename);

    /// Formats the image filename and replaces wildcards with the current frame number.
    static QString formatImageFilename(const QString& filename, int frameNumber);

    /// Returns whether errors that occur within a data pipeline lead to an abortion of the rendering process.
    bool stopOnPipelineError() const { return _stopOnPipelineError; }
    /// Sets whether errors that occur within a data pipeline lead to an abortion of the rendering process.
    void setStopOnPipelineError(bool stopOnPipelineError) { _stopOnPipelineError = stopOnPipelineError; }

    /// High-level rendering function that invokes the renderer to generate one or more output images of the scene.
    [[nodiscard]] Future<void> render(const ViewportConfiguration& viewportConfiguration, OORef<const AnimationSettings> animationSettings, const std::shared_ptr<FrameBuffer>& outputFrameBuffer);

    /// High-level rendering function that invokes the renderer to generate one or more output images of the scene.
    [[nodiscard]] ScopedFuture<void> render(const std::vector<std::pair<Viewport*, QRectF>> viewportLayout, OORef<const AnimationSettings> animationSettings, const std::shared_ptr<FrameBuffer> outputFrameBuffer);

    /// Computes a viewport's area in the rendered output image.
    QRect viewportFramebufferArea(const Viewport* viewport, const ViewportConfiguration* viewportConfig) const;

protected:

    /// Adopts the outline parameters of state files written by OVITO 3.15 or earlier, in which they
    /// were part of the renderer's own state. See SceneRenderer::takeLegacyOutlineSettings().
    virtual void loadFromStreamComplete(ObjectLoadStream& stream) override;

private:

    /// Contains the output filename and format of the image to be rendered.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(ImageInfo{}, imageInfo, setImageInfo);

    /// The filename of the output image.
    DECLARE_VIRTUAL_PROPERTY_FIELD(QString, imageFilename);

    /// The instance of the plugin renderer class.
    DECLARE_MODIFIABLE_REFERENCE_FIELD_FLAGS(OORef<SceneRenderer>, renderer, setRenderer, PROPERTY_FIELD_MEMORIZE);

    /// Controls the background color of the rendered image.
    DECLARE_MODIFIABLE_REFERENCE_FIELD_FLAGS(OORef<Controller>, backgroundColorController, setBackgroundColorController, PROPERTY_FIELD_MEMORIZE);

    /// The width of the output image in pixels.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(int{640}, outputImageWidth, setOutputImageWidth, PROPERTY_FIELD_MEMORIZE);

    /// The height of the output image in pixels.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(int{480}, outputImageHeight, setOutputImageHeight, PROPERTY_FIELD_MEMORIZE);

    /// Controls whether the alpha channel will be included in the output image.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool{false}, generateAlphaChannel, setGenerateAlphaChannel, PROPERTY_FIELD_MEMORIZE);

    /// Controls whether the rendered image is saved to the output file.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{false}, saveToFile, setSaveToFile);

    /// Controls whether already rendered frames are skipped.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{false}, skipExistingImages, setSkipExistingImages);

    /// Specifies which part of the animation should be rendered.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(RenderingRangeType{CURRENT_FRAME}, renderingRangeType, setRenderingRangeType);

    /// The first frame to render when rendering range is set to CUSTOM_INTERVAL.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(int{0}, customRangeStart, setCustomRangeStart);

    /// The last frame to render when rendering range is set to CUSTOM_INTERVAL.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(int{100}, customRangeEnd, setCustomRangeEnd);

    /// The frame to render when rendering range is set to CUSTOM_FRAME.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(int{0}, customFrame, setCustomFrame);

    /// Specifies the number of frames to skip when rendering an animation.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(int{1}, everyNthFrame, setEveryNthFrame);

    /// Specifies the base number for filename generation when rendering an animation.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(int{0}, fileNumberBase, setFileNumberBase);

    /// The frames per second for encoding videos.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(int{0}, framesPerSecond, setFramesPerSecond);

    /// Controls whether all viewports of the current viewport layout are rendered (or just the active viewport).
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{false}, renderAllViewports, setRenderAllViewports);

    /// Controls the visibility of separators between viewports when rendering an entire viewport layout.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool{false}, layoutSeparatorsEnabled, setLayoutSeparatorsEnabled, PROPERTY_FIELD_MEMORIZE);

    /// Controls the width (in pixels) of the separators between viewports when rendering an entire viewport layout.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(int{2}, layoutSeparatorWidth, setLayoutSeparatorWidth, PROPERTY_FIELD_MEMORIZE);

    /// Controls the color of the separator lines between viewports when rendering an entire viewport layout.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS((Color{0.5, 0.5, 0.5}), layoutSeparatorColor, setLayoutSeparatorColor, PROPERTY_FIELD_MEMORIZE);

    // Depth-aware outline post-processing effect. These parameters used to be part of each
    // renderer's own state. They are renderer-independent and are kept here so that the
    // settings are preserved when the user switches to a different rendering backend.
    // See SceneRenderer::OOMetaClass::overrideFieldDeserialization() for the migration of
    // outline settings stored in state files written by OVITO 3.15 or earlier.

    /// Enables the depth-aware outline post-processing effect.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool{false}, outlinesEnabled, setOutlinesEnabled, PROPERTY_FIELD_MEMORIZE);

    /// Minimum depth difference (world units) at which an outline of minimum width is drawn.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{0.5}, minDepthDiff, setMinDepthDiff,
                                            PROPERTY_FIELD_MEMORIZE | PROPERTY_FIELD_RESETTABLE);

    /// Depth difference (world units) at which the outline reaches maximum width. Set to +inf for uniform width.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{std::numeric_limits<FloatType>::infinity()}, maxDepthDiff, setMaxDepthDiff,
                                            PROPERTY_FIELD_MEMORIZE | PROPERTY_FIELD_RESETTABLE);

    /// Minimum outline width in pixels.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{1}, minOutlineWidth, setMinOutlineWidth,
                                            PROPERTY_FIELD_MEMORIZE | PROPERTY_FIELD_RESETTABLE);

    /// Maximum outline width in pixels.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{4}, maxOutlineWidth, setMaxOutlineWidth,
                                            PROPERTY_FIELD_MEMORIZE | PROPERTY_FIELD_RESETTABLE);

    /// Enables a user-defined outline color instead of the automatic color.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool{false}, useCustomOutlineColor, setUseCustomOutlineColor,
                                            PROPERTY_FIELD_MEMORIZE | PROPERTY_FIELD_RESETTABLE);

    /// The custom outline color (used only when useCustomOutlineColor is true).
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS((Color{0.0, 0.0, 0.0}), outlineColor, setOutlineColor,
                                            PROPERTY_FIELD_MEMORIZE | PROPERTY_FIELD_RESETTABLE);

    /// Controls whether errors that occur within a data pipeline lead to an abortion of the rendering process.
    bool _stopOnPipelineError = false;

    friend class RenderSettingsEditor;
};

}   // End of namespace
