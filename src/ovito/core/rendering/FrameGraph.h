// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once

#include <ovito/core/Core.h>
#include <ovito/core/viewport/ViewProjectionParameters.h>
#include "RendererResourceCache.h"
#include "OutlineSettings.h"
#include "SceneRenderer.h"
#include "LinePrimitive.h"
#include "ParticlePrimitive.h"
#include "TextPrimitive.h"
#include "TextBillboardPrimitive.h"
#include "ImagePrimitive.h"
#include "CylinderPrimitive.h"
#include "MeshPrimitive.h"
#include "MarkerPrimitive.h"
#include "VolumePrimitive.h"

namespace Ovito {

/**
 * Abstract base class for per-object information used by the object picking system.
 */
class OVITO_CORE_EXPORT ObjectPickInfo : public OvitoObject
{
    OVITO_CLASS(ObjectPickInfo)

public:
    /// Returns a human-readable string describing the picked object, which will be displayed in the status bar by OVITO.
    virtual QString infoString(const Pipeline* pipeline, uint32_t subobjectId) { return {}; }
};

/**
 * The FrameGraph class represents a sequence of rendering commands that are used to generate
 * a single frame of a 3D scene. The recorded rendering commands can be executed by a SceneRenderer to produce an image.
 */
class OVITO_CORE_EXPORT FrameGraph : public OvitoObject
{
    OVITO_CLASS(FrameGraph)

public:
    /// The type of layers that get rendered on top of each other. Each rendering command belongs to one of these layers.
    enum RenderLayerType
    {
        UnderLayer,
        SceneLayer,
        HighlightLayer,
        OverLayer
    };

    /// Describes a single primitive to be rendered.
    class OVITO_CORE_EXPORT RenderingCommand
    {
    public:
        /// Bit-wise flags for rendering commands.
        enum Flag
        {
            NoFlags = 0,
            ExcludeFromVisual = (1 << 0),    // Skip the primitive in the visual rendering pass
            ExcludeFromPicking = (1 << 1),   // Skip the primitive in the object picking rendering pass
            ExcludeFromLighting = (1 << 2),  // Exclude this primitive from global illumination and shadows
            ExcludeFromOutline = (1 << 3),   // Do not draw an outline around the primitive (only for renderers that support outlines)
        };
        Q_DECLARE_FLAGS(Flags, Flag);

        /// Constructor.
        explicit RenderingCommand(Flags flags, std::unique_ptr<RenderingPrimitive> primitive, const AffineTransformation& tm,
                                  OORef<const SceneNode> sceneNode = {}, OORef<ObjectPickInfo> pickInfo = {}, uint32_t pickElementOffset = 0)
            : _primitive(std::move(primitive)),
              _tm(tm),
              _sceneNode(std::move(sceneNode)),
              _pickInfo(std::move(pickInfo)),
              _pickElementOffset(pickElementOffset),
              _flags(flags)
        {
        }

        /// Returns the graphics primitive rendered by this command.
        RenderingPrimitive* primitive() const { return _primitive.get(); }

        /// Replaces the graphics primitive with a new one.
        void setPrimitive(std::unique_ptr<RenderingPrimitive> primitive) { _primitive = std::move(primitive); }

        /// Returns the model-to-world transformation matrix to be applied to the graphics primitive.
        const AffineTransformation& modelWorldTM() const { return _tm; }

        /// The pipeline scene node to which this rendering command belongs.
        const OORef<const SceneNode>& sceneNode() const { return _sceneNode; }

        /// An optional object that knows more about what is being rendered and which sub-elements it consists of.
        const OORef<ObjectPickInfo>& pickInfo() const { return _pickInfo; }

        /// If this rendering command is part of a composite object that requires multiple rendering commands,
        /// then this offset indicates where this command's primitive elements start in the composite range.
        uint32_t pickElementOffset() const { return _pickElementOffset; }

        /// Determines whether this command should be skipped in object picking render mode.
        bool skipInPickingPass() const { return _flags.testFlag(ExcludeFromPicking); }

        /// Determines whether this command should be skipped in visual render mode.
        bool skipInVisualPass() const { return _flags.testFlag(ExcludeFromVisual); }

        /// Determines whether this the primitive drawn by this command should be excluded from global illumination and shadow calculations.
        bool excludeFromLighting() const { return _flags.testFlag(ExcludeFromLighting); }

        /// Controls whether this the primitive drawn by this command should be excluded from global illumination and shadow calculations.
        void setExcludeFromLighting(bool exclude) { _flags.setFlag(ExcludeFromLighting, exclude); }

        /// Exclude this drawing command from the outline rendering pass.
        bool excludeFromOutline() const { return _flags.testFlag(ExcludeFromOutline); }

        /// Controls whether this drawing command is excluded from the outline rendering pass.
        void setExcludeFromOutline(bool exclude) { _flags.setFlag(ExcludeFromOutline, exclude); }

    private:

        /// The graphics primitive to be rendered.
        std::unique_ptr<RenderingPrimitive> _primitive;

        /// The model-to-world transformation matrix to be applied to the primitive.
        /// May be a null matrix to indicate that the primitive contains pre-projected coordinates.
        AffineTransformation _tm = AffineTransformation::Zero();

        /// The pipeline scene node to which this rendering command belongs.
        /// Note: may be null in rare cases, e.g., when the AmbientOcclusionModifier renders particles using false colors.
        OORef<const SceneNode> _sceneNode;

        /// An optional object that knows what high-level data is being represented by this render command and which sub-elements it
        /// consists of.
        OORef<ObjectPickInfo> _pickInfo;

        /// If this rendering command is part of a composite object that requires multiple rendering commands,
        /// then this offset indicates where this command's primitive elements start in the composite range.
        uint32_t _pickElementOffset;

        /// Bit-wise flags of this rendering command.
        Flags _flags = NoFlags;
    };

    /// A group of rendering commands.
    class OVITO_CORE_EXPORT RenderingCommandGroup
    {
        Q_DISABLE_COPY(RenderingCommandGroup)

    public:

        /// Constructor.
        explicit RenderingCommandGroup(RenderLayerType layerType) : _layerType(layerType) {}

        /// Returns the type of layer this group belongs to.
        RenderLayerType layerType() const { return _layerType; }

        /// Returns the world-space bounding box of the command group.
        const Box3& boundingBox() const { return _boundingBox; }

        /// Returns the sequence of rendering commands in this group.
        const auto& commands() const { return _commands; }

        /// Returns the mutable sequence of rendering commands in this group.
        auto& commands() { return _commands; }

        /// Appends a rendering command to the group.
        template<typename... Args>
        RenderingCommand& addCommand(Args&&... args) {
            return _commands.emplace_back(std::forward<Args>(args)...);
        }

        /// Add a 3d rendering primitive to the current layer of the frame graph with a pre-computed bounding box.
        /// Automatically computes the bounding box of the primitive and the model-to-world transformation.
        /// Optional: A FrameGraph::RenderingCommand::Flag can be give, default is "NoFlags"
        RenderingCommand& addPrimitive(std::unique_ptr<RenderingPrimitive> primitive, const AffineTransformation& tm, const Box3& box,
                                       OORef<const SceneNode> pickableSceneNode, OORef<ObjectPickInfo> pickInfo = {},
                                       uint32_t pickElementOffset = 0, RenderingCommand::Flags flags = RenderingCommand::NoFlags);

        /// Add a 3d rendering primitive to the current layer of the frame graph with a pre-computed bounding box.
        /// Automatically computes the bounding box of the primitive and the model-to-world transformation.
        RenderingCommand& addPrimitiveNonpickable(std::unique_ptr<RenderingPrimitive> primitive,
                                                  const AffineTransformation& tm,
                                                  const Box3& box,
                                                  RenderingCommand::Flags flags = RenderingCommand::NoFlags);

        /// Adds a primitive to the frame graph containing pre-projected coordinates.
        RenderingCommand& addPrimitivePreprojected(std::unique_ptr<RenderingPrimitive> primitive);

        /// Renders a 2d polyline or polygon into an interactive viewport.
        void render2DPolyline(const Point2* points, int count, const ColorA& color, bool closed, const QSize& logicalViewportSize);

    private:

        /// The list of rendering commands.
        QVarLengthArray<RenderingCommand, 2> _commands;

        /// The world-space bounding box of the command group.
        Box3 _boundingBox;

        /// The kind of layer this group belongs to.
        RenderLayerType _layerType;
    };

public:

    /// Constructor.
    void initializeObject(RendererResourceCache::ResourceFrame visCache, AnimationTime time,
                          const ViewProjectionParameters& projectionParams, QSize viewportDeviceIndependentSize, bool isInteractive,
                          bool isPreviewMode, bool stopOnPipelineError, qreal devicePixelRatio)
    {
        OvitoObject::initializeObject();

        _visCache = std::move(visCache);
        _time = time;
        _projectionParams = projectionParams;
        _isInteractive = isInteractive;
        _isPreviewMode = isPreviewMode;
        _stopOnPipelineError = stopOnPipelineError;
        _devicePixelRatio = devicePixelRatio;
        _viewportDeviceIndependentSize = viewportDeviceIndependentSize;
    }

    /// Returns the data cache to be used by visualization elements while building the frame graph.
    const RendererResourceCache::ResourceFrame& visCache() const { OVITO_ASSERT(_visCache); return _visCache; }

    /// Returns the animation time being rendered.
    AnimationTime time() const { return _time; }

    /// Returns whether we are rendering an interactive viewport or not.
    bool isInteractive() const { return _isInteractive; }

    /// Returns whether preview mode is active in the interactive viewport being rendered.
    bool isPreviewMode() const { return _isPreviewMode; }

    /// Returns whether the rendering should be stopped when an error occurs in a data pipeline.
    bool stopOnPipelineError() const { return _stopOnPipelineError; }

    /// Returns whether the rendered scene represents a preliminary pipeline state, i.e., a partial output
    /// of pipelines that have not been fully evaluated yet.
    bool isPreliminaryState() const { return _isPreliminaryState; }

    /// Specifies whether the rendered scene represents a preliminary pipeline state, i.e., a partial output
    /// of pipelines that have not been fully evaluated yet.
    void setIsPreliminaryState(bool isPreliminary) { _isPreliminaryState = isPreliminary; }

    /// Returns the best format for QImage to be used when creating an ImagePrimitive.
    QImage::Format preferredImageFormat() const { return _preferredImageFormat; }

    /// Returns the device pixel ratio of the output device we are rendering to.
    qreal devicePixelRatio() const { return _devicePixelRatio; }

    /// Returns the 3d projection parameters to be used for rendering.
    const ViewProjectionParameters& projectionParams() const { return _projectionParams; }

    /// Changes the 3d projection parameters to be used for rendering.
    void setProjectionParams(const ViewProjectionParameters& params) { _projectionParams = params; }

    /// Sets the color to clear the framebuffer with.
    void setClearColor(const ColorA& c) { _clearColor = c; }

    /// Returns The color to clear the framebuffer with.
    const ColorA& clearColor() const { return _clearColor; }

    /// Sets the depth-aware outline post-processing parameters, which are configured by the user in the
    /// RenderSettings and apply to every rendering backend alike. The raw parameter values are normalized
    /// here, because this class is the one that knows the device pixel ratio the outline widths refer to.
    void setOutlineSettings(OutlineSettings settings) {
        settings.minOutlineWidth *= devicePixelRatio();
        settings.maxOutlineWidth *= devicePixelRatio();
        // If the user has set the maximum depth difference to a value smaller than the minimum depth difference,
        // then we fall back to uniform-width mode.
        if(settings.maxDepthDiff < settings.minDepthDiff) {
            settings.maxDepthDiff = std::numeric_limits<FloatType>::infinity();
            settings.minOutlineWidth = std::max(settings.minOutlineWidth, settings.maxOutlineWidth);
        }
        _outlineSettings = settings;
    }

    /// Returns the normalized depth-aware outline post-processing parameters.
    const OutlineSettings& outlineSettings() const { return _outlineSettings; }

    /// Returns the scene renderer that is going to execute this frame graph, or null if it is not known,
    /// e.g., when the frame graph is built for a purpose other than producing an image.
    const OORef<SceneRenderer>& renderer() const { return _renderer; }

    /// Specifies the scene renderer that is going to execute this frame graph. It is set by the code that
    /// creates the frame graph. Visual elements and viewport layers may consult it in order to produce
    /// output that matches the appearance of the selected rendering backend.
    void setRenderer(OORef<SceneRenderer> renderer) { _renderer = std::move(renderer); }

    /// Returns the world-space bounding box of the 3d scene.
    const Box3& sceneBoundingBox() const { return _sceneBoundingBox; }

    /// Computes the combined scene bounding box from all command groups.
    void computeSceneBoundingBox();

    /// Returns the line rendering width to use in object picking mode.
    FloatType defaultLinePickingWidth() const;

    /// Returns const reference to the list of command groups.
    const std::deque<RenderingCommandGroup>& commandGroups() const { return _commandGroups; }

    /// Returns the mutable list of command groups.
    std::deque<RenderingCommandGroup>& commandGroups() { return _commandGroups; }

    /// Adds a new rendering command group to the graph.
    RenderingCommandGroup& addCommandGroup(RenderLayerType layerType, bool prepend = false) {
        OVITO_ASSERT_MSG(this_task::isMainThread(), "FrameGraph::addCommandGroup", "This method is not thread-safe and may only be called from the main thread.");
        if(prepend)
            return _commandGroups.emplace_front(layerType);
        else
            return _commandGroups.emplace_back(layerType);
    }

    /// Add a 3d rendering primitive to the current layer of the frame graph.
    /// Automatically computes the bounding box of the primitive and the model-to-world transformation.
    /// Optional: A FrameGraph::RenderingCommand::Flag can be give, default is "NoFlags"
    RenderingCommand& addPrimitive(RenderingCommandGroup& group, std::unique_ptr<RenderingPrimitive> primitive,
                                   OORef<const SceneNode> sceneNode, OORef<ObjectPickInfo> pickInfo = {}, uint32_t pickElementOffset = 0,
                                   RenderingCommand::Flags flags = RenderingCommand::NoFlags);

    /// Add a 3d rendering primitive to the current layer of the frame graph.
    /// Automatically computes the bounding box of the primitive and the model-to-world transformation.
    RenderingCommand& addPrimitiveNonpickable(RenderingCommandGroup& group,
                                              std::unique_ptr<RenderingPrimitive> primitive,
                                              const SceneNode* sceneNode,
                                              RenderingCommand::Flags flags = RenderingCommand::NoFlags);

    /// Replaces all text primitives with (cached) image primitives.
    void renderTextAsImagePrimitives();

    /// Adjust wireframe line widths to match device pixel ratio.
    void adjustWireframeLineWidths();

    /// Returns the size of the rendering viewport in device-independent pixels.
    const QSize& viewportDeviceIndependentSize() const { return _viewportDeviceIndependentSize; }

    /// Computes the world size of an object that should appear always in the same size on the screen.
    FloatType nonScalingSize(const Point3& worldPosition) const {
        return projectionParams().nonScalingSize(worldPosition, viewportDeviceIndependentSize());
    }

	/// Generates the frame graph contents for a scene.
	Future<void> buildFromScene(OORef<Scene> scene, OORef<Viewport> viewport, const QRect& logicalViewportRect = {}, const QRect& physicalViewportRect = {}, const ViewProjectionParameters& noninteractiveProjParams = {});

    /// Finalizes the frame graph for rendering, e.g., resolves text primitives into image primitives.
    /// This method is called by the RenderThread before the frame graph gets rendered for the first time.
    /// After this method has been called, no more rendering commands should be added to the frame graph.
    /// A task context must be active when calling this method.
    void finalizeForRendering();

private:

    /// The data cache to be used by visualization elements while building the frame graph.
    RendererResourceCache::ResourceFrame _visCache;

    /// The animation time being rendered.
    AnimationTime _time;

    /// The 3d projection parameters to be used for rendering.
    ViewProjectionParameters _projectionParams;

    /// Indicates whether we are rendering an interactive viewport or not.
    bool _isInteractive;

    /// Indicates that preview mode is active in the interactive viewport being rendered.
    bool _isPreviewMode;

    /// Indicates whether the rendering should stop when an error occurs in a data pipeline.
    bool _stopOnPipelineError;

    /// The best pixel format for QImage to be used when creating an ImagePrimitive.
    /// This is chosen such that it is compatible with the QRhiTexture::RGBA8 texture format.
    QImage::Format _preferredImageFormat = QImage::Format_RGBA8888;

    /// The device pixel ratio of the output device we are rendering to.
    qreal _devicePixelRatio;

    /// The size of the viewport in device-independent pixels.
    QSize _viewportDeviceIndependentSize;

    /// The color to clear the framebuffer with.
    ColorA _clearColor = ColorA(0, 0, 0, 0);

    /// The normalized depth-aware outline post-processing parameters. See setOutlineSettings().
    OutlineSettings _outlineSettings;

    /// The scene renderer that is going to execute this frame graph. May be null.
    OORef<SceneRenderer> _renderer;

    /// The list of recorded rendering commands groups.
    /// Ordering is important, as the groups are rendered in sequence within each RenderLayerType.
    /// Using a deque instead of a vector, because addresses must be stable.
    std::deque<RenderingCommandGroup> _commandGroups;

    /// The world-space bounding box of the 3d scene.
    Box3 _sceneBoundingBox;

    /// Indicates whether the rendered scene represents a preliminary or the fully evaluated pipeline state.
    bool _isPreliminaryState = false;

    /// Indicates whether the frame graph has been finalized, i.e., whether no more rendering commands will be added to it
    /// and text primitives have been resolved into image primitives.
    bool _isFinalizedForRendering = false;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(FrameGraph::RenderingCommand::Flags);

}  // namespace Ovito

#include <ovito/core/dataset/scene/Pipeline.h>
