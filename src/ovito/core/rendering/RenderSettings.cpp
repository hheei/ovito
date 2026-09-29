// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/core/Core.h>
#include <ovito/core/rendering/SceneRenderer.h>
#include <ovito/core/rendering/FrameBuffer.h>
#include <ovito/core/rendering/FrameGraph.h>
#include <ovito/core/rendering/OffscreenRenderTarget.h>
#include <ovito/core/rendering/standard/StandardRenderer.h>
#include <ovito/core/utilities/units/UnitsManager.h>
#include <ovito/core/utilities/io/video/VideoEncoder.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/core/app/Application.h>
#include <ovito/core/app/PluginManager.h>
#include <ovito/core/app/UserInterface.h>
#include "RenderSettings.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(RenderSettings);
DEFINE_PROPERTY_FIELD(RenderSettings, imageInfo);
DEFINE_VIRTUAL_PROPERTY_FIELD(RenderSettings, imageFilename, setImageFilename);
DEFINE_REFERENCE_FIELD(RenderSettings, renderer);
DEFINE_REFERENCE_FIELD(RenderSettings, backgroundColorController);
DEFINE_PROPERTY_FIELD(RenderSettings, outputImageWidth);
DEFINE_PROPERTY_FIELD(RenderSettings, outputImageHeight);
DEFINE_PROPERTY_FIELD(RenderSettings, generateAlphaChannel);
DEFINE_PROPERTY_FIELD(RenderSettings, saveToFile);
DEFINE_PROPERTY_FIELD(RenderSettings, skipExistingImages);
DEFINE_PROPERTY_FIELD(RenderSettings, renderingRangeType);
DEFINE_PROPERTY_FIELD(RenderSettings, customRangeStart);
DEFINE_PROPERTY_FIELD(RenderSettings, customRangeEnd);
DEFINE_PROPERTY_FIELD(RenderSettings, customFrame);
DEFINE_PROPERTY_FIELD(RenderSettings, everyNthFrame);
DEFINE_PROPERTY_FIELD(RenderSettings, fileNumberBase);
DEFINE_PROPERTY_FIELD(RenderSettings, framesPerSecond);
DEFINE_PROPERTY_FIELD(RenderSettings, renderAllViewports);
DEFINE_PROPERTY_FIELD(RenderSettings, layoutSeparatorsEnabled);
DEFINE_PROPERTY_FIELD(RenderSettings, layoutSeparatorWidth);
DEFINE_PROPERTY_FIELD(RenderSettings, layoutSeparatorColor);
DEFINE_PROPERTY_FIELD(RenderSettings, outlinesEnabled);
DEFINE_PROPERTY_FIELD(RenderSettings, minDepthDiff);
DEFINE_PROPERTY_FIELD(RenderSettings, maxDepthDiff);
DEFINE_PROPERTY_FIELD(RenderSettings, minOutlineWidth);
DEFINE_PROPERTY_FIELD(RenderSettings, maxOutlineWidth);
DEFINE_PROPERTY_FIELD(RenderSettings, useCustomOutlineColor);
DEFINE_PROPERTY_FIELD(RenderSettings, outlineColor);
SET_PROPERTY_FIELD_LABEL(RenderSettings, imageInfo, "Image info");
SET_PROPERTY_FIELD_LABEL(RenderSettings, imageFilename, "Image output path");
SET_PROPERTY_FIELD_LABEL(RenderSettings, renderer, "Renderer");
SET_PROPERTY_FIELD_LABEL(RenderSettings, backgroundColorController, "Background color");
SET_PROPERTY_FIELD_LABEL(RenderSettings, outputImageWidth, "Width");
SET_PROPERTY_FIELD_LABEL(RenderSettings, outputImageHeight, "Height");
SET_PROPERTY_FIELD_LABEL(RenderSettings, generateAlphaChannel, "Transparent background");
SET_PROPERTY_FIELD_LABEL(RenderSettings, saveToFile, "Save to file");
SET_PROPERTY_FIELD_LABEL(RenderSettings, skipExistingImages, "Skip existing animation images");
SET_PROPERTY_FIELD_LABEL(RenderSettings, renderingRangeType, "Rendering range");
SET_PROPERTY_FIELD_LABEL(RenderSettings, customRangeStart, "Range start");
SET_PROPERTY_FIELD_LABEL(RenderSettings, customRangeEnd, "Range end");
SET_PROPERTY_FIELD_LABEL(RenderSettings, customFrame, "Frame");
SET_PROPERTY_FIELD_LABEL(RenderSettings, everyNthFrame, "Every Nth frame");
SET_PROPERTY_FIELD_LABEL(RenderSettings, fileNumberBase, "File number base");
SET_PROPERTY_FIELD_LABEL(RenderSettings, framesPerSecond, "Frames per second");
SET_PROPERTY_FIELD_LABEL(RenderSettings, renderAllViewports, "Render all viewports");
SET_PROPERTY_FIELD_LABEL(RenderSettings, layoutSeparatorsEnabled, "Layout separators");
SET_PROPERTY_FIELD_LABEL(RenderSettings, layoutSeparatorWidth, "Separator width");
SET_PROPERTY_FIELD_LABEL(RenderSettings, layoutSeparatorColor, "Separator color");
SET_PROPERTY_FIELD_LABEL(RenderSettings, outlinesEnabled, "Outlines");
SET_PROPERTY_FIELD_LABEL(RenderSettings, minDepthDiff, "Min. depth delta");
SET_PROPERTY_FIELD_LABEL(RenderSettings, maxDepthDiff, "Max. depth delta");
SET_PROPERTY_FIELD_LABEL(RenderSettings, minOutlineWidth, "Min. outline width");
SET_PROPERTY_FIELD_LABEL(RenderSettings, maxOutlineWidth, "Max. outline width");
SET_PROPERTY_FIELD_LABEL(RenderSettings, useCustomOutlineColor, "Custom color");
SET_PROPERTY_FIELD_LABEL(RenderSettings, outlineColor, "Outline color");
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(RenderSettings, minDepthDiff, FloatParameterUnit, 0);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(RenderSettings, maxDepthDiff, FloatParameterUnit, 0);
SET_PROPERTY_FIELD_UNITS_AND_RANGE(RenderSettings, minOutlineWidth, FloatParameterUnit, 0, 63);
SET_PROPERTY_FIELD_UNITS_AND_RANGE(RenderSettings, maxOutlineWidth, FloatParameterUnit, 0, 63);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(RenderSettings, outputImageWidth, IntegerParameterUnit, 1);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(RenderSettings, outputImageHeight, IntegerParameterUnit, 1);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(RenderSettings, everyNthFrame, IntegerParameterUnit, 1);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(RenderSettings, framesPerSecond, IntegerParameterUnit, 0);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(RenderSettings, layoutSeparatorWidth, IntegerParameterUnit, 1);
SET_PROPERTY_FIELD_ALIAS_IDENTIFIER(RenderSettings, layoutSeparatorsEnabled, "layoutSeperatorsEnabled"); // For backward compatibility with OVITO 3.10.6
SET_PROPERTY_FIELD_ALIAS_IDENTIFIER(RenderSettings, layoutSeparatorWidth, "layoutSeperatorWidth"); // For backward compatibility with OVITO 3.10.6
SET_PROPERTY_FIELD_ALIAS_IDENTIFIER(RenderSettings, layoutSeparatorColor, "layoutSeperatorColor"); // For backward compatibility with OVITO 3.10.6

/******************************************************************************
* Constructor.
******************************************************************************/
void RenderSettings::initializeObject(ObjectInitializationFlags flags)
{
    RefTarget::initializeObject(flags);

    if(!flags.testFlag(ObjectInitializationFlag::DontInitializeObject)) {
        // Set default background color.
        setBackgroundColorController(ControllerManager::createColorController());
        setBackgroundColor(Color(1,1,1));

        // Create an instance of the default renderer class.
        setRenderer(OORef<StandardRenderer>::create());
    }
}

/******************************************************************************
* Adopts the outline parameters of state files written by OVITO 3.15 or earlier,
* in which they were part of the renderer's own state.
******************************************************************************/
void RenderSettings::loadFromStreamComplete(ObjectLoadStream& stream)
{
    RefTarget::loadFromStreamComplete(stream);

    // Note that the settings must be *pulled* from the renderer here rather than pushed by the
    // renderer, because the relative order in which loadFromStreamComplete() is called on the
    // objects of the graph is unspecified. At this point all reference fields have been restored,
    // so renderer() is guaranteed to be valid if the file contained one.
    if(renderer()) {
        if(std::optional<OutlineSettings> legacySettings = renderer()->takeLegacyOutlineSettings()) {
            setOutlinesEnabled(legacySettings->enabled);
            setMinDepthDiff(legacySettings->minDepthDiff);
            setMaxDepthDiff(legacySettings->maxDepthDiff);
            setMinOutlineWidth(legacySettings->minOutlineWidth);
            setMaxOutlineWidth(legacySettings->maxOutlineWidth);
            setUseCustomOutlineColor(legacySettings->useCustomColor);
            setOutlineColor(legacySettings->customColor);
        }
    }
}

/******************************************************************************
* Assembles the depth-aware outline post-processing parameters into an
* OutlineSettings structure, which is handed to the renderer through the FrameGraph.
******************************************************************************/
OutlineSettings RenderSettings::outlineSettings() const
{
    OutlineSettings settings;
    settings.enabled = outlinesEnabled();
    settings.minDepthDiff = minDepthDiff();
    settings.maxDepthDiff = maxDepthDiff();
    settings.minOutlineWidth = minOutlineWidth();
    settings.maxOutlineWidth = maxOutlineWidth();
    settings.useCustomColor = useCustomOutlineColor();
    settings.customColor = outlineColor();
    return settings;
}

/******************************************************************************
* Sets the output filename of the rendered image.
******************************************************************************/
void RenderSettings::setImageFilename(const QString& filename)
{
    if(filename != imageFilename()) {
        ImageInfo newInfo = imageInfo();
        newInfo.setFilename(filename);
        setImageInfo(newInfo);
    }
}

/******************************************************************************
* This is the high-level rendering function, which invokes the renderer to
* generate one or more output images of the scene.
******************************************************************************/
Future<void> RenderSettings::render(const ViewportConfiguration& viewportConfiguration, OORef<const AnimationSettings> animationSettings, const std::shared_ptr<FrameBuffer>& frameBuffer)
{
    std::vector<std::pair<Viewport*, QRectF>> viewportLayout;
    if(renderAllViewports()) {
        // When rendering an entire viewport layout, determine the each viewport's target rectangle within the output framebuffer.
        QSizeF borderSize(0,0);
        if(layoutSeparatorsEnabled()) {
            // Convert separator width from pixels to reduced units, which are relative to the framebuffer width/height.
            borderSize.setWidth( 1.0 / outputImageWidth()  * layoutSeparatorWidth());
            borderSize.setHeight(1.0 / outputImageHeight() * layoutSeparatorWidth());
        }
        viewportLayout = viewportConfiguration.getViewportRectangles(QRectF(0,0,1,1), borderSize);
    }
    else if(viewportConfiguration.activeViewport()) {
        // When rendering just the active viewport, create an ad-hoc layout for the single viewport.
        viewportLayout.push_back({ viewportConfiguration.activeViewport(), QRectF(0,0,1,1) });
    }

    return render(std::move(viewportLayout), std::move(animationSettings), frameBuffer);
}
/******************************************************************************
 * Formats the image filename and replaces wildcards with the current frame number.
 ******************************************************************************/
QString RenderSettings::formatImageFilename(const QString& filename, int frameNumber)
{
    QFileInfo fileInfo{filename};
    if(fileInfo.completeBaseName().contains("*")) {
        // Wildcard - replace with frame number
        return fileInfo.path() + QChar('/') + fileInfo.completeBaseName().replace("*", QString("%1").arg(frameNumber, 4, 10, QChar('0'))) +
               QChar('.') + fileInfo.suffix();
    }
    else {
        // No wildcard - use old formatting
        return fileInfo.path() + QChar('/') + fileInfo.baseName() + QString("%1.").arg(frameNumber, 4, 10, QChar('0')) +
               fileInfo.completeSuffix();
    }
}

/******************************************************************************
* This is the high-level rendering function, which invokes the renderer to
* generate one or more output images of the scene.
******************************************************************************/
ScopedFuture<void> RenderSettings::render(const std::vector<std::pair<Viewport*, QRectF>> viewportLayout, OORef<const AnimationSettings> animationSettings, const std::shared_ptr<FrameBuffer> frameBuffer)
{
    OVITO_ASSERT(this_task::isMainThread());
    OVITO_ASSERT(this_task::get());
    // Note: This object is kept alive for the whole duration of the coroutine automatically, by the
    // self-guard installed in the coroutine promise; no manual guard needed.

    // Get the selected scene renderer.
    OORef<SceneRenderer> renderer = this->renderer();
    if(!renderer)
        throw Exception(tr("No rendering backend has been selected."));

    // Resize output frame buffer.
    if(frameBuffer->size() != QSize(outputImageWidth(), outputImageHeight())) {
        frameBuffer->setSize(QSize(outputImageWidth(), outputImageHeight()));
        frameBuffer->clear();
    }

    // Determine the range of frames to be rendered.
    int numberOfFrames = 1;
    int firstFrameNumber = 0;
    if(renderingRangeType() == RenderSettings::CURRENT_FRAME) {
        // Render a single frame.
        firstFrameNumber = animationSettings ? animationSettings->currentFrame() : 0;
    }
    else if(renderingRangeType() == RenderSettings::CUSTOM_FRAME) {
        // Render a specific frame.
        firstFrameNumber = customFrame();
    }
    else if(renderingRangeType() == RenderSettings::ANIMATION_INTERVAL || renderingRangeType() == RenderSettings::CUSTOM_INTERVAL) {
        // Render an animation interval.
        if(renderingRangeType() == RenderSettings::ANIMATION_INTERVAL) {
            firstFrameNumber = animationSettings ? animationSettings->firstFrame() : 0;
            numberOfFrames = animationSettings ? (animationSettings->lastFrame() - firstFrameNumber + 1) : 0;
        }
        else {
            firstFrameNumber = customRangeStart();
            numberOfFrames = (customRangeEnd() - firstFrameNumber + 1);
        }
        numberOfFrames = (numberOfFrames + everyNthFrame() - 1) / everyNthFrame();
        if(numberOfFrames < 1)
            throw Exception(tr("Invalid rendering range: frame %1 to %2").arg(customRangeStart()).arg(customRangeEnd()));
    }
    else {
        throw Exception(tr("Invalid rendering range type: %1").arg(renderingRangeType()));
    }

    // Per viewport data.
    struct ViewportRenderingData {
        OORef<Viewport> viewport;
        OffscreenRenderTarget renderTarget;
        QRect destinationRect;
    };

    // Create the rendering frame buffers, one for each viewport to be rendered.
    std::vector<ViewportRenderingData> viewportList;
    viewportList.reserve(viewportLayout.size());
    for(const auto& r : viewportLayout) {
        // Compute the rectangular area covered by the viewport in the output frame buffer.
        // For this, convert viewport layout rect from relative coordinates to frame buffer pixel coordinates and round to nearest integers.
        QRectF pixelRect(r.second.x() * frameBuffer->width(), r.second.y() * frameBuffer->height(), r.second.width() * frameBuffer->width(), r.second.height() * frameBuffer->height());
        QRect destinationRect = pixelRect.toRect();
        if(destinationRect.isEmpty())
            continue;
        // One offscreen render target per viewport, reused by all frames of the animation. Its GPU resources are
        // allocated on the first rendered frame, at the resolution that frame needs.
        viewportList.push_back(ViewportRenderingData{
            /* viewport */ r.first,
            /* renderTarget */ OffscreenRenderTarget(*this_task::ui(), OffscreenRenderTarget::Kind::Visual),
            /* destinationRect */ destinationRect });
    }

    std::unique_ptr<VideoEncoder> videoEncoder;
    // Initialize video encoder.
    if(saveToFile() && imageInfo().isMovie()) {
        if(imageFilename().isEmpty())
            throw Exception(tr("Cannot save rendered images to movie file. Output filename has not been specified."));

        videoEncoder = std::make_unique<VideoEncoder>();
        float fps = framesPerSecond();
        if(fps <= 0)
            fps = animationSettings ? animationSettings->framesPerSecond() : 1.0f;
        videoEncoder->openFile(imageFilename(), outputImageWidth(), outputImageHeight(), fps);
    }

    // The visualization data cache used for building the frame graph.
    const std::shared_ptr<RendererResourceCache> visCache = this_task::ui()->datasetContainer().visCache();

    // Progress reporting when rendering an entire animation.
    std::optional<TaskProgress> animationProgress;
    if(numberOfFrames > 1) {
        animationProgress.emplace(this_task::ui());
        animationProgress->setMaximum(numberOfFrames);
    }

    // Render the animation frames, one by one.
    for(int frameIndex = 0; frameIndex < numberOfFrames && !this_task::isCanceled(); frameIndex++) {
        int frameNumber = firstFrameNumber + frameIndex * everyNthFrame() + fileNumberBase();
        AnimationTime renderTime = AnimationTime::fromFrame(frameNumber);

        if(animationProgress) {
            animationProgress->setValue(frameIndex);
            animationProgress->setText(tr("Rendering animation (frame %1 of %2)").arg(frameIndex+1).arg(numberOfFrames));
        }

        // Progress reporting for the current frame.
        TaskProgress frameProgress(this_task::ui());
        if(numberOfFrames == 1)
            frameProgress.setText(tr("Rendering frame %1").arg(frameNumber));

        // Determine output filename for this frame.
        QString outputFilename;
        if(saveToFile() && !videoEncoder) {
            outputFilename = imageFilename();
            if(outputFilename.isEmpty())
                throw Exception(tr("Cannot save rendered image to file, because no output filename has been specified."));

            // Append frame number to filename when rendering an animation.
            if(renderingRangeType() != RenderSettings::CURRENT_FRAME && renderingRangeType() != RenderSettings::CUSTOM_FRAME) {
                // Format output filename.
                outputFilename = formatImageFilename(outputFilename, frameNumber);

                // Check for existing image file and skip frame.
                if(skipExistingImages() && QFileInfo(outputFilename).isFile())
                    continue;
            }
        }

        // Subdivide progress range into sub-steps for each viewport when rendering a multi-viewport layout.
        frameProgress.beginSubSteps(viewportList.size());

        // Render each viewport of the layout one after the other.
        for(ViewportRenderingData& vpData : viewportList) {

            // Set up preliminary projection.
            const FloatType viewportAspectRatio = (FloatType)vpData.destinationRect.height() / vpData.destinationRect.width();
            ViewProjectionParameters projParams = vpData.viewport->computeProjectionParameters(renderTime, viewportAspectRatio);
            this_task::throwIfCanceled();

            // Create a new frame graph.
            OORef<FrameGraph> frameGraph = OORef<FrameGraph>::create(
                visCache->acquireResourceFrame(),
                renderTime,
                projParams,
                vpData.destinationRect.size(),
                false,
                false,
                stopOnPipelineError(),
                renderer->supersamplingFactor());

            // Let the frame graph know which renderer is going to execute it.
            frameGraph->setRenderer(renderer);

            // Set background color.
            frameGraph->setClearColor(generateAlphaChannel() ? ColorA(0,0,0,0) : ColorA(backgroundColorAt(renderTime)));

            // Set the depth-aware outline post-processing parameters, which are independent of the selected renderer.
            frameGraph->setOutlineSettings(outlineSettings());

            // Target rectangles for overlay/underlay rendering.
            const QRect logicalOverlayRect(QPoint(0,0), vpData.destinationRect.size());
            const QRect physicalOverlayRect(QPoint(0,0), vpData.destinationRect.size() * renderer->supersamplingFactor());

            // Let the FrameGraph class do the heavy lifting and generate the drawing commands for the current scene.
            co_await FutureAwaiter(ObjectExecutor(this), frameGraph->buildFromScene(vpData.viewport->scene(), vpData.viewport, logicalOverlayRect, physicalOverlayRect, projParams));

            // Compute final projection based on the now known bounding box.
            frameGraph->setProjectionParams(vpData.viewport->computeProjectionParameters(renderTime, viewportAspectRatio, frameGraph->sceneBoundingBox()));

            // Let the offscreen render target produce the rendering in the framebuffer, which covers this viewport's
            // destination rectangle within the whole output image.
            frameBuffer->setViewportRect(vpData.destinationRect);
            co_await FutureAwaiter(ObjectExecutor(this), vpData.renderTarget.renderImage(std::move(frameGraph), *renderer,
                frameBuffer, frameProgress, renderer->supersamplingFactor()));

            frameProgress.nextSubStep();
        }
        frameProgress.endSubSteps();

        // Write rendered image or video frame to disk.
        if(saveToFile()) {
            if(!videoEncoder) {
                OVITO_ASSERT(!outputFilename.isEmpty());

                // The QImage.save() function requires a Qt application object in order to load the Qt file format plugins.
                Application::instance()->createQtApplication(false);

                // Use the QImage.save() function to save the rendered image to disk.
                if(!frameBuffer->image().save(outputFilename, imageInfo().format()))
                    throw Exception(tr("Failed to save rendered image to output file '%1'.").arg(outputFilename));
            }
            else {
                videoEncoder->writeFrame(frameBuffer->image());
            }
        }
    }

    // Finalize movie file.
    if(videoEncoder) {
        videoEncoder->closeFile();
        videoEncoder.reset();
    }
}

/******************************************************************************
* Computes a viewport's area in the framebuffer to be rendered.
******************************************************************************/
QRect RenderSettings::viewportFramebufferArea(const Viewport* viewport, const ViewportConfiguration* viewportConfig) const
{
    QRect frameBufferRect(0, 0, outputImageWidth(), outputImageHeight());

    // Aspect ratio of the viewport rectangle in the rendered output image.
    if(renderAllViewports() && viewportConfig && viewport) {

        // Compute target rectangles of all viewports of the current layout.
        // TODO: This should be optimized. Computing the full layout every time seems unnecessary.
        std::vector<std::pair<Viewport*, QRectF>> viewportRects = viewportConfig->getViewportRectangles(frameBufferRect);

        // Find the viewport among the list of all viewports to look up its target rectangle in the output image.
        for(const std::pair<Viewport*, QRectF>& rect : viewportRects) {
            if(rect.first == viewport)
                return rect.second.toRect();
        }
    }

    return frameBufferRect;
}

}   // End of namespace
