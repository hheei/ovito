// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

/**
 * \file SceneRenderer.h
 * \brief Contains the definition of the Ovito::SceneRenderer class.
 */

#pragma once


#include <ovito/core/Core.h>
#include <ovito/core/oo/RefTarget.h>
#include <ovito/core/rendering/OutlineSettings.h>

namespace Ovito {

class RenderThread; // Forward declaration to avoid circular include

/**
 * Abstract base class for scene renderers, which produce a picture of the three-dimensional scene.
 */
class OVITO_CORE_EXPORT SceneRenderer : public RefTarget
{
public:

	/// Defines a metaclass specialization for this renderer class.
	class OVITO_CORE_EXPORT OOMetaClass : public RefTarget::OOMetaClass
	{
	public:
		/// Inherit standard constructor from base meta class.
		using RefTarget::OOMetaClass::OOMetaClass;

		/// Provides a custom function that takes care of the deserialization of a serialized property field
		/// whose data layout has changed. This is needed for backward compatibility with OVITO 3.15 and earlier,
		/// where the outline post-processing parameters were part of each renderer's own state instead of
		/// belonging to the RenderSettings object. See takeLegacyOutlineSettings().
		virtual SerializedPropertyField::CustomDeserializationFunctionPtr overrideFieldDeserialization(LoadStream& stream, const SerializedPropertyField& field) const override;
	};
	OVITO_CLASS_META(SceneRenderer, OOMetaClass)

public:

	class Configuration; // Forward declaration

	/// Returns the mutable outline parameters recovered from a state file written by OVITO 3.15 or earlier,
	/// creating the record on first access. Only used by the deserialization code path.
	OutlineSettings& legacyOutlineSettings() {
		if(!_legacyOutlineSettings) _legacyOutlineSettings.emplace();
		return *_legacyOutlineSettings;
	}

	/// Returns and clears the outline parameters recovered from a state file written by OVITO 3.15 or earlier,
	/// or std::nullopt if the file did not contain any. RenderSettings calls this from loadFromStreamComplete()
	/// to adopt the settings, which are no longer part of the renderer's own state.
	std::optional<OutlineSettings> takeLegacyOutlineSettings() {
		return std::exchange(_legacyOutlineSettings, std::nullopt);
	}

	/**
	 * Base class for rendering devices, which encapsulate the graphics API-specific device objects and resources
	 * used by a SceneRenderer implementation.
	 *
	 * Typically, rendering devices are shared by multiple renderer implementations running in the same RenderThread and using the same graphics API,
	 * to allow sharing of graphics resources between them.
	 */
	class OVITO_CORE_EXPORT Device
	{
	public:
		/// Constructor.
		explicit Device(RendererService* service) : _service(service) {}

		/// Destructor.
		virtual ~Device() = default;

		/// Returns the renderer service this device is associated with.
		RendererService* rendererService() const { return _service; }

	private:
		/// The renderer service this device is associated with.
		RendererService* _service;
	};

	/**
	 * Base class for renderer implementation objects, which perform the actual rendering process.
	 *
	 * The RenderThread create on Implementation instance per render target (onscreen viewport or offscreen framebuffer)
	 * to let the renderer produce an image that can be composited into the output framebuffer.
	 *
	 * All methods of the class are called from the RenderThread.
	 */
	class OVITO_CORE_EXPORT Implementation
	{
	public:

		/// Constructor.
		explicit Implementation(RendererService* service);

		/// Destructor.
		virtual ~Implementation() = default;

		/// Returns the renderer service that owns this implementation and supplies its GPU resources.
		RendererService* service() const { return _service; }

		/// Returns the QRhi instance provided by the renderer service.
		QRhi* rhi() const { return _rhi; }

		/// Called BEFORE beginPass().
		/// Visual:  renders frame with color + depth channels.
		/// Picking: renders frame with instanceId + primitiveId + depth channels.
		/// \param pickingMap  Non-null for picking passes; the implementation registers objectIds here.
		virtual void renderFrame(const FrameGraph& frameGraph, const SceneRenderer::Configuration& config, QSize renderSize, TaskProgress& progress, bool isPickingPass, int refinementIteration, ObjectPickingMap* pickingMap = nullptr) = 0;

		/// Called BEFORE beginPass(), after renderFrame(), in a resource update batch.
		/// Ensures intermediate QRhiTextures exist at the right size and enqueues
		/// texture upload commands into \a batch (which is passed to beginPass()).
		virtual void prepareResourceUpdates(QRhiRenderTarget* renderTarget, QRhiResourceUpdateBatch* batch, QSize renderSize, bool isPickingPass, const FrameGraph& frameGraph) = 0;

		/// Called AFTER prepareResourceUpdates() but BEFORE beginPass().
		/// Allows recording additional compute or graphics passes that must complete before the main render pass,
		/// e.g. GPU sort dispatches or OIT accumulation render passes.
		/// \param pendingBatch  The resource update batch filled by prepareResourceUpdates().
		///   The implementation may commit it early (by passing it to beginComputePass or beginPass) and
		///   set the pointer to null, allowing subsequent passes to use the uploaded data.
		///   Any uncommitted batch is passed on to the main render pass's beginPass().
		virtual void performPrePasses(QRhiCommandBuffer* cb, QSize renderSize, bool isPickingPass, QRhiResourceUpdateBatch*& pendingBatch) {}

		/// Called BEFORE prepareResourceUpdates() when a post-processing effect (e.g. outlines) is active.
		/// If the implementation needs the scene to be rendered into an intermediate color+depth target
		/// rather than the final target (so a post-process pass can composite the result with effects),
		/// it should create/resize the intermediate resources here and return the intermediate render target.
		/// The implementation owns the intermediate target's lifetime. Returns nullptr to keep the
		/// existing single-pass, direct-to-final-target path.
		/// \param finalRpd  Render pass descriptor of the final target; used for post-process pipeline creation.
		/// \param size      Pixel size of the final target.
		virtual QRhiRenderTarget* prepareIntermediateTarget(const QRhiRenderPassDescriptor* finalRpd, QSize size) { return nullptr; }

		/// Called INSIDE the render pass (after beginPass(), before endPass()).
		/// Issues draw calls only (no resource uploads — those are in prepareResourceUpdates).
		/// Visual:  draws compositing fullscreen quad sampling color+depth textures.
		/// Picking: draws compositing fullscreen quad writing objectId+primitiveId+depth into the picking target.
		/// \param skipOverLayer  When true (used by the intermediate-target path), the OverLayer is omitted so it
		///                       can be drawn separately into the final target after post-processing.
		virtual void compositeInPass(QRhiCommandBuffer* cb, QRhiRenderTarget* renderTarget, bool isPickingPass, bool skipOverLayer = false) = 0;

		/// Called BEFORE the final render pass begins; builds and returns a resource-update batch
		/// (caller passes it to beginPass) so that UBO uploads are done outside any active pass,
		/// as required by D3D11. Returns nullptr if there is nothing to upload.
		/// The default implementation returns nullptr.
		virtual QRhiResourceUpdateBatch* preparePostProcess(QRhiCommandBuffer* cb) { return nullptr; }

		/// Runs the post-processing pass (outlines, highlight) from the intermediate textures into the final target.
		/// Called INSIDE the final target's render pass, after the scene was rendered into the intermediate target.
		/// The default implementation does nothing (used by renderers that return nullptr from prepareIntermediateTarget).
		virtual void runPostProcess(QRhiCommandBuffer* cb, QRhiRenderTarget* finalTarget) {}

		/// Renders only the OverLayer commands into the currently active render pass.
		/// Called INSIDE the final target's render pass, after runPostProcess(), so that OverLayer
		/// elements appear on top of all post-processing effects but below OS chrome.
		/// The default implementation does nothing.
		virtual void renderOverLayerOnly(QRhiCommandBuffer* cb, QRhiRenderTarget* target) {}

#ifdef OVITO_BUILD_BASIC
		/// Returns true if the output of this renderer should receive the "OVITO Pro Demo" watermark.
		/// Overridden to return true by external (Tachyon, OSPRay, ANARI) renderer implementations.
		virtual bool isWatermarked() const { return false; }
#endif

		/// Indicates whether the renderer wants to perform further refinement iterations for the current frame.
		virtual bool refinementIterationNeeded(const FrameGraph& frameGraph, const SceneRenderer::Configuration& config, int numIterationsSoFar) const { return false; }

		/// Optionally returns the total number of refinement iterations the renderer is going to perform.
		/// This is used to provide a more accurate progress indication in the GUI.
		virtual int totalRefinementIterations(const FrameGraph& frameGraph, const SceneRenderer::Configuration& config) const { return 0; }

		/// Records a non-fatal warning encountered during rendering.
		void reportWarning(const QString& message);

	private:

		/// Non-owning pointer to the renderer service that created this implementation.
		RendererService* _service;

		/// The QRhi instance to use for rendering. Provided by the renderer service when creating the implementation.
		QRhi* _rhi;
	};

	/**
	 * Base class for renderer configuration objects, which encapsulate the current configuration of
	 * a scene renderer (e.g. rendering mode, quality settings, light options, etc.).
	 *
	 * The data structure is used to pass the renderer's configuration from the GUI thread to the RenderThread as part of a FrameGraph.
	 */
	class OVITO_CORE_EXPORT Configuration
	{
		Q_DISABLE_COPY_MOVE(Configuration)

	public:

		/// Constructor.
		Configuration() = default;

		/// Destructor.
		virtual ~Configuration() = default;

		/// Creates an implementation object that can be used to render a visual image.
		virtual std::unique_ptr<Implementation> createImplementationForVisual(RendererService* service, std::unique_ptr<Implementation> existingImpl) const = 0;

		/// Creates an implementation object that can be used to render a object picking image (if supported by the renderer).
		virtual std::unique_ptr<Implementation> createImplementationForPicking(RendererService* service, std::unique_ptr<Implementation> existingImpl) const { return {}; }

		/// Depth-aware outline post-processing settings. Populated by the renderer's createConfiguration() method.
		OutlineSettings outlineSettings;
	};

public:

	/// Constructor.
	using RefTarget::RefTarget;

	/// Creates a new configuration object encapsulating the current renderer configuration.
	virtual std::unique_ptr<Configuration> createConfiguration(const FrameGraph& frameGraph) const = 0;

	/// Supersampling factor to apply to the rendered image size. Default is 1 (no supersampling).
	/// This value is used by the RenderThread to determine the size of the offscreen render target to create for this renderer.
	virtual int supersamplingFactor() const { return 1; }

	/// Creates a renderer instance with standardized settings for rendering small preview images of individual
	/// objects, for example the 3d type symbols displayed by the ColorLegendOverlay. The returned instance is
	/// always initialized to fixed factory values, never the parameter values memorized by the user, so that
	/// preview images have a consistent appearance and a predictable, low rendering cost.
	/// May return null if this renderer class is not suitable for rendering preview images. The caller should
	/// then fall back to the StandardRenderer.
	/// This method must be called from the main thread while a task context is active.
	virtual OORef<SceneRenderer> createPreviewRenderer() const;

#ifdef OVITO_BUILD_BASIC
	/// Returns an image mask serving as watermark for demo versions of scene renderers.
    static const QImage& watermark();
#endif

private:

	/// Transient storage for the outline post-processing parameters found in a state file written by
	/// OVITO 3.15 or earlier, where they were part of the renderer's own state. They are handed over to
	/// the RenderSettings object once the whole object graph has been loaded. Never serialized.
	std::optional<OutlineSettings> _legacyOutlineSettings;
};

}	// End of namespace
