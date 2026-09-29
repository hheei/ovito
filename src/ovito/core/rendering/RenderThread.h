// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once

#include <ovito/core/Core.h>
#include <ovito/core/rendering/FrameGraph.h>
#include <ovito/core/rendering/SceneRenderer.h>
#include <ovito/core/rendering/RendererService.h>
#include <ovito/core/rendering/FrameGraphRenderPass.h>
#include <ovito/core/rendering/ObjectPickingMap.h>
#include <ovito/core/rendering/ObjectPickingBuffer.h>
#include <ovito/core/rendering/ObjectIdAllocator.h>
#include <ovito/core/rendering/RendererResourceCache.h>
#include <ovito/core/rendering/WarningIndicatorRenderer.h>
#ifdef OVITO_BUILD_BASIC
#include <ovito/core/rendering/WatermarkRenderer.h>
#endif
#include <ovito/core/viewport/ViewportWindow.h>

namespace Ovito {

class FrameBuffer;
class RenderTarget;

/**
 * Render thread that owns a shared QRhi instance and manages render targets
 * for all viewport windows and offscreen rendering within the same UserInterface.
 *
 * Render targets are created via createOnscreenTarget() / createOffscreenTarget(),
 * which return a RenderTarget that automatically destroys the target when it
 * goes out of scope. Rendering and picking operations are performed from the
 * application's main thread through the RenderTarget helper class.
 */
class OVITO_CORE_EXPORT RenderThread
	: public QThread
	, public std::enable_shared_from_this<RenderThread>
	, public RendererService
	, private UserInterfaceComponent<UserInterface, false>
{
	Q_OBJECT

	friend class RenderTarget;

public:

	/// Opaque handle identifying a render target (onscreen or offscreen).
	using RenderTargetHandle = int;

	/// Constructor. Determines the graphics API to use but does not create the QRhi yet.
	RenderThread(UserInterface& ui, QRhi::Implementation graphicsApi);

	/// Destructor. Shuts down the render thread and releases all resources.
	~RenderThread() override;

	/// Creates an onscreen render target backed by a new QWindow.
	/// The QWindow is created on the calling (GUI) thread with the correct surface type.
	/// Blocks the calling thread until the swap chain has been set up on the render thread.
	/// RenderThread installs itself as an event filter on the QWindow to
	/// handle PlatformSurface (surface destruction) events internally.
	/// Ownership of the QWindow transfers to the container widget when the caller
	/// passes it to QWidget::createWindowContainer().
	/// \param viewportWindow   The ViewportWindow that owns this target. Warning messages and fatal
	///                          rendering errors are reported back to the GUI thread via the viewport window.
	/// Returns a pair of (RenderTarget, QWindow*).
	/// Throws Exception if the RHI resources could not be created.
	[[nodiscard]] std::pair<RenderTarget, QWindow*> createOnscreenTarget(ViewportWindow* viewportWindow = nullptr);

	/// Creates a reusable offscreen render target at the given pixel resolution.
	/// Creates a new offscreen render target of the given size.
	/// \param forPickingOnly  If true, the target provides only the buffers a picking pass renders into,
	///                       i.e. no color texture is allocated and no visual frame may be submitted to it.
	///                       Used for ambient occlusion sampling and for offscreen picking.
	/// Blocks the calling thread until the GPU resources have been allocated.
	/// Throws Exception if the RHI resources could not be created.
	[[nodiscard]] RenderTarget createOffscreenTarget(const QSize& size, bool forPickingOnly = false);

	/// Returns the QRhi instance owned by this render thread. May only be called from a renderer implementation on the render thread.
	QRhi* rhi() const override { return _rhi.get(); }

	/// Returns the object ID allocator for this render thread.
	ObjectIdAllocator& objectIdAllocator() override { return _objectIdAllocator; }

	/// Returns the shared cache for QRhi resources such as vertex buffers and textures.
	RendererResourceCache& rhiResourceCache() override { return *_rhiResourceCache; }

	/// Indicates whether the calling thread is the render thread that owns this service's GPU resources.
	bool isRendererThread() const override { return QThread::currentThread() == this; }

	/// Returns the graphics API used by this render thread.
	QRhi::Implementation graphicsApi() const override { return _graphicsApi; }

#if QT_CONFIG(vulkan) && defined(Q_OS_LINUX)
	/// Returns the Vulkan instance used by this render thread (if using Vulkan).
	/// Must be called on the GUI thread to set on QWindows before they are shown.
	QVulkanInstance* vulkanInstance() const { return _vulkanInstance.get(); }
#endif

	/// Loads a compiled .qsb shader from the Qt resource system.
	/// May only be called from a renderer implementation on the render thread.
	[[nodiscard]] QShader loadShader(const QString& resourcePath) override;

	/// Records a non-fatal warning encountered during rendering.
	/// May be called from the renderFrame() method of a renderer implementation on the render thread.
	void reportWarning(const QString& message) override;

protected:

	/// The render thread's main loop.
	void run() override;

	/// Handles events on the GUI thread (installed as event filter on QWindows).
	bool eventFilter(QObject* obj, QEvent* event) override;

private:

	/// Destroys a render target (onscreen or offscreen) and releases its GPU resources.
	/// Blocks the calling thread until all GPU resources have been released.
	/// For onscreen targets, does NOT destroy the QWindow (owned by the container widget).
	void destroyTarget(RenderTargetHandle handle);

	/// Submits a frame graph for rendering to the specified onscreen target.
	/// Renders and presents to the swap chain.
	void renderOnscreenFrame(RenderTargetHandle handle, OORef<FrameGraph> frameGraph,
							 std::unique_ptr<SceneRenderer::Configuration> rendererConfig);

	/// Renders and reads back the image into the provided FrameBuffer using the given offscreen target.
	/// The returned ScopedFuture<void> is fulfilled when the readback is complete and the FrameBuffer has been updated.
	[[nodiscard]] ScopedFuture<void> renderOffscreenFrame(RenderTargetHandle handle, OORef<FrameGraph> frameGraph,
							                     std::unique_ptr<SceneRenderer::Configuration> rendererConfig,
		 					                     std::shared_ptr<FrameBuffer> frameBuffer, TaskProgress& progress);

	/// Submits a frame graph for ambient occlusion sampling to the specified target.
	/// This method is only used by the implementation of the AmbientOcclusionModifier.
	/// It returns the contents of the object ID and primitive ID buffers as byte arrays, which are used to determine the AO contribution for each particle.
	ScopedFuture<std::pair<QByteArray, QByteArray>> renderAOFrame(RenderTargetHandle handle, OORef<FrameGraph> frameGraph);

	/// Renders a picking pass for the given frame graph into the target's offscreen picking buffers.
	/// The target must have been created with createOffscreenTarget(..., forPickingOnly = true).
	/// May be called from any thread; the returned future is fulfilled once the readback is complete.
	[[nodiscard]] ScopedFuture<ObjectPickingBuffer> renderPickingFrame(RenderTargetHandle handle, OORef<FrameGraph> frameGraph,
		std::unique_ptr<SceneRenderer::Configuration> rendererConfig);

	/// Requests a picking operation at the given position.
	/// Blocks the calling (GUI) thread until the result is available.
	std::optional<ViewportWindow::PickResult> requestPick(RenderTargetHandle handle,
	                                                      const QPointF& pos, int pickRadius = 4);

	/// Types of events sent from the GUI thread to the render thread.
	enum class EventType : int {
		CreateOnscreenTarget = QEvent::User + 1,
		CreateOffscreenTarget,
		DestroyTarget,
		RenderOnscreenFrame,
		RenderOffscreenFrame,
		RenderAOFrame,
		RenderPickingFrame,
		RequestPick,
		SurfaceGoingAway,
		SuspendTarget,
		Shutdown,
	};

	/// Base class for events sent to the render thread.
	struct RenderEvent : public QEvent {
		RenderEvent(EventType type) : QEvent(static_cast<QEvent::Type>(type)) {}
	};

	/// Event to set up the swap chain for a new onscreen target.
	struct CreateOnscreenTargetEvent : public RenderEvent {
		CreateOnscreenTargetEvent(RenderTargetHandle h, QWindow* w,
		                          QImage icon, ViewportWindow* vw = nullptr)
			: RenderEvent(EventType::CreateOnscreenTarget), handle(h), window(w)
			, warningIconImage(std::move(icon)), viewportWindow(vw) {}
		RenderTargetHandle handle;
		QWindow* window;
		QImage warningIconImage;
		ViewportWindow* viewportWindow = nullptr;
	};

	/// Event to create an offscreen render target.
	struct CreateOffscreenTargetEvent : public RenderEvent {
		CreateOffscreenTargetEvent(RenderTargetHandle h, QSize s, bool pickingOnly)
			: RenderEvent(EventType::CreateOffscreenTarget), handle(h), size(s), forPickingOnly(pickingOnly) {}
		RenderTargetHandle handle;
		QSize size;
		bool forPickingOnly; ///< Whether the offscreen target is used only for picking passes (e.g. ambient occlusion sampling).
	};

	/// Event to destroy a render target (onscreen or offscreen).
	struct DestroyTargetEvent : public RenderEvent {
		DestroyTargetEvent(RenderTargetHandle h)
			: RenderEvent(EventType::DestroyTarget), handle(h) {}
		RenderTargetHandle handle;
	};

	/// Event to submit a frame graph for onscreen rendering.
	struct RenderOnscreenFrameEvent : public RenderEvent {
		RenderOnscreenFrameEvent(RenderTargetHandle h, OORef<FrameGraph> fg, std::unique_ptr<SceneRenderer::Configuration> rc)
			: RenderEvent(EventType::RenderOnscreenFrame), handle(h), frameGraph(std::move(fg)), rendererConfig(std::move(rc)) {}
		RenderTargetHandle handle;
		OORef<FrameGraph> frameGraph;
		std::unique_ptr<SceneRenderer::Configuration> rendererConfig;
	};

	/// Event to submit a frame graph for offscreen rendering.
	struct RenderOffscreenFrameEvent : public RenderEvent {
		RenderOffscreenFrameEvent(RenderTargetHandle h, OORef<FrameGraph> fg, std::unique_ptr<SceneRenderer::Configuration> rc, std::shared_ptr<FrameBuffer> fb, TaskProgress& pr, Promise<void> p)
			: RenderEvent(EventType::RenderOffscreenFrame), handle(h), frameGraph(std::move(fg)), rendererConfig(std::move(rc)), frameBuffer(std::move(fb)), progress(pr), promise(std::move(p)) {}
		RenderTargetHandle handle;
		OORef<FrameGraph> frameGraph;
		std::unique_ptr<SceneRenderer::Configuration> rendererConfig;
		std::shared_ptr<FrameBuffer> frameBuffer;
		TaskProgress& progress;
		Promise<void> promise;
	};

	/// Event to submit a frame graph for ambient occlusion sampling.
	struct RenderAOFrameEvent : public RenderEvent {
		RenderAOFrameEvent(RenderTargetHandle h, OORef<FrameGraph> fg, Promise<std::pair<QByteArray, QByteArray>> p)
			: RenderEvent(EventType::RenderAOFrame), handle(h), frameGraph(std::move(fg)), promise(std::move(p)) {}
		RenderTargetHandle handle;
		OORef<FrameGraph> frameGraph;
		Promise<std::pair<QByteArray, QByteArray>> promise;
	};

	/// Event to submit a frame graph for an offscreen picking pass.
	struct RenderPickingFrameEvent : public RenderEvent {
		RenderPickingFrameEvent(RenderTargetHandle h, OORef<FrameGraph> fg, std::unique_ptr<SceneRenderer::Configuration> rc, Promise<ObjectPickingBuffer> p)
			: RenderEvent(EventType::RenderPickingFrame), handle(h), frameGraph(std::move(fg)), rendererConfig(std::move(rc)), promise(std::move(p)) {}
		RenderTargetHandle handle;
		OORef<FrameGraph> frameGraph;
		std::unique_ptr<SceneRenderer::Configuration> rendererConfig;
		Promise<ObjectPickingBuffer> promise;
	};

	/// Event to request a picking operation at a given position.
	struct RequestPickEvent : public RenderEvent {
		RequestPickEvent(RenderTargetHandle h, QPointF pos, int radius, Task* t)
			: RenderEvent(EventType::RequestPick), handle(h), position(pos), pickRadius(radius), task(t) {}
		RenderTargetHandle handle;
		QPointF position;
		int pickRadius;
		Task* task;
	};

	/// Event indicating that an onscreen target's platform surface is about to be destroyed.
	struct SurfaceGoingAwayEvent : public RenderEvent {
		SurfaceGoingAwayEvent(RenderTargetHandle h)
			: RenderEvent(EventType::SurfaceGoingAway), handle(h) {}
		RenderTargetHandle handle;
	};

	/// Event to temporarily suspend a render target's GPU resources while its window
	/// is hidden (but the platform surface remains valid). The swap chain can be
	/// automatically recreated by renderOnscreen() when rendering resumes.
	struct SuspendTargetEvent : public RenderEvent {
		SuspendTargetEvent(RenderTargetHandle h)
			: RenderEvent(EventType::SuspendTarget), handle(h) {}
		RenderTargetHandle handle;
	};

	/// Event to shut down the render thread.
	struct ShutdownEvent : public RenderEvent {
		ShutdownEvent() : RenderEvent(EventType::Shutdown) {}
	};

	/// Thread-safe event queue for communication from GUI thread to render thread.
	/// Modeled after Qt Quick's scenegraph render thread event queue.
	class EventQueue : public QQueue<QEvent*>
	{
	public:
		void addEvent(QEvent* e) {
			QMutexLocker locker(&_mutex);
			enqueue(e);
			if(_waiting)
				_condition.wakeOne();
		}

		QEvent* takeEvent(bool wait) {
			QMutexLocker locker(&_mutex);
			if(isEmpty() && wait) {
				_waiting = true;
				_condition.wait(&_mutex);
				_waiting = false;
			}
			return isEmpty() ? nullptr : dequeue();
		}

		template<typename Predicate>
		QEvent* takeParticularEvent(Predicate&& pred) {
			QMutexLocker locker(&_mutex);
			for(auto iter = begin(); iter != end(); ++iter) {
				if(pred(*iter)) {
					QEvent* e = *iter;
					erase(iter);
					return e;
				}
			}
			return nullptr;
		}

		bool hasMoreEvents() {
			QMutexLocker locker(&_mutex);
			return !isEmpty();
		}

	private:
		QMutex _mutex;
		QWaitCondition _condition;
		bool _waiting = false;
	};

	/// Unified per-target rendering state, managed on the render thread.
	/// Certain fields are only used for onscreen targets (viewport windows), others only for offscreen targets,
	/// but they share the same struct for simplicity.
	struct TargetState
	{
		std::unique_ptr<SceneRenderer::Configuration> rendererConfig; ///< The scene renderer's configuration passed form the GUI thread.

		// --- Onscreen-specific (window != nullptr means onscreen target) ---
		QWindow* window = nullptr;
		std::unique_ptr<QRhiSwapChain> swapChain;
		std::unique_ptr<QRhiRenderBuffer> swapChainDepthStencil;
		std::unique_ptr<QRhiRenderPassDescriptor> swapChainRenderPassDesc;
		OORef<FrameGraph> pendingFrameGraph;
		bool hasSwapChain = false;
		bool needsRender = false; ///< Set to true when a render is requested (even without a FrameGraph).
		OOWeakRef<ViewportWindow> viewportWindow; ///< ViewportWindow owning this target; used for warning reporting and fatal error notification (onscreen targets only).
		std::unique_ptr<WarningIndicatorRenderer> warningIndicator;    ///< Draws the warning icon quad; nullptr when no icon image was provided.
#ifdef OVITO_BUILD_BASIC
		std::unique_ptr<WatermarkRenderer> watermarkRenderer;         ///< Tiles the "OVITO Pro Demo" watermark; non-null when the active renderer returns isWatermarked() == true.
#endif

		// --- Progressive refinement (onscreen only) ---
		int refinementIteration = 0;                                 ///< The current refinement iteration for the next onscreen render.
		OORef<FrameGraph> lastRenderedFrameGraph;                    ///< FrameGraph from the last visual render, used for the picking pass, but also kept around to keep cached resources alive.

		// --- Picking (onscreen only, lazily allocated) ---
		QByteArray pickObjectIdData;                                 ///< CPU readback of objectId texture.
		QByteArray pickPrimitiveIdData;                              ///< CPU readback of primitiveId texture.
		QByteArray pickDepthData;                                    ///< CPU readback of depth texture (D32F, one float per pixel).
		QSize pickBufferSize;                                        ///< Pixel dimensions the picking buffer was rendered at.
		bool pickBufferValid = false;                                ///< Whether pick data matches the current lastRenderedFrameGraph.
		ObjectPickingMap pickingMap;                                 ///< Maps (objectId, primitiveId) to PickResult; built during picking pass rendering.
		bool forPickingOnly = false;									 ///< Whether this target exists only for picking passes and cannot display a visual image.

		// --- Offscreen-specific ---
		QSize offscreenSize;
		std::unique_ptr<QRhiTexture> colorTexture;
		std::unique_ptr<QRhiRenderBuffer> offscreenDepthStencil;
		std::unique_ptr<QRhiTextureRenderTarget> offscreenRenderTarget;
		std::unique_ptr<QRhiRenderPassDescriptor> offscreenRenderPassDesc;

		/// The renderer implementations of this target, created by its SceneRenderer::Configuration: one for
		/// visual rendering and, if the renderer supports it, one for picking. Two implementations are needed to
		/// avoid interference between progressive refinement rendering (visual) and picking passes, which may be
		/// requested at any time.
		FrameGraphRenderPass::Implementations renderers;
	};

	/// Creates the QRhi instance on the render thread.
	void createRhi();

	/// Processes a single event on the render thread.
	void processEvent(QEvent* e);

	/// Event handlers called from processEvent(), one per event type.
	void handleCreateOnscreenTarget(CreateOnscreenTargetEvent* event);
	void handleCreateOffscreenTarget(CreateOffscreenTargetEvent* event);
	void handleDestroyTarget(DestroyTargetEvent* event);
	void handleRenderOnscreenFrame(RenderOnscreenFrameEvent* event);
	void handleRenderOffscreenFrame(RenderOffscreenFrameEvent* event);
	void handleRenderAOFrame(RenderAOFrameEvent* event);
	void handleRenderPickingFrame(RenderPickingFrameEvent* event);
	void handleRequestPick(RequestPickEvent* event);
	void handleSurfaceGoingAway(SurfaceGoingAwayEvent* event);
	void suspendTarget(RenderTargetHandle handle);
	void handleSuspendTarget(SuspendTargetEvent* event);
	void handleShutdown();

	/// Renders a single onscreen target's frame graph to the swap chain.
	void renderOnscreen(TargetState& state);

	/// Renders a frame graph to a single offscreen target.
	void renderOffscreen(TargetState& state, RenderOffscreenFrameEvent* event);

	/// Ensures the picking GPU resources are allocated and match the given size.
	/// Creates or recreates them as needed. Called on the render thread.
	void ensurePickingResources(const QSize& size);

	/// Renders the picking pass using the target's lastRenderedFrameGraph.
	/// Reads back the entire picking texture to pickBufferData.
	/// Called on the render thread.
	void renderPickingPass(TargetState& state, const QSize& size);

	/// Looks up the nearest non-zero object ID in the pick buffer around the given position.
	/// Then returns the corresponding PickResult with scene node and pick info. Returns std::nullopt if no object was hit.
	[[nodiscard]] std::optional<ViewportWindow::PickResult> lookupPickBuffer(const TargetState& state, const QPointF& pos, int radius) const;

	/// Executes the resource upload and render pass for a frame graph into the given render target.
	/// This is the shared rendering code path used by visual and picking buffer rendering as well as offscreen rendering.
	/// Called on the render thread.
	void renderFrameGraph(QRhiCommandBuffer* cb, QRhiRenderTarget* renderTarget,
	                      TargetState& state, FrameGraph* frameGraph, const SceneRenderer::Configuration& config,
	                      TaskProgress& progress, bool isPickingPass,
	                      int refinementIteration = 0);

	/// Composites the warning icon into the active render pass and pushes the current warning
	/// list to the owning ViewportWindow. Must be called inside a render pass. Visual passes only.
	void updateWarningStore(QRhiCommandBuffer* cb, QRhiRenderTarget* renderTarget, TargetState& state);

	/// Finds the handle for a given QWindow (linear scan, typically 1-4 viewports).
	/// Returns 0 if not found.
	RenderTargetHandle findHandleForWindow(QWindow* window) const;

	/// Reports a fatal rendering error to the owning ViewportWindow (for onscreen targets).
	void reportFatalError(TargetState& state, const Exception& exception);

	/// The graphics API to use.
	QRhi::Implementation _graphicsApi;

	/// The name of the user-selected GPU adapter (empty string means default).
	QByteArray _selectedAdapterName;

	/// Whether the render thread is active.
	bool _active = true;

	/// Indicates whether the render thread can respond to object picking requests.
	////Set to true after the first onscreen render has completed.
	std::atomic<bool> _canServePickRequests = false;

	/// The event queue.
	EventQueue _eventQueue;

	/// Mutex and condition variable for blocking the GUI thread during synchronous operations.
	QMutex _syncMutex;
	QWaitCondition _syncCondition;

	/// Exception from the render thread, used to propagate errors back to the calling thread.
	/// Written by the render thread under _syncMutex, read by the calling thread after sync.
	std::exception_ptr _syncError;

	/// Result of the last pick operation. Written by the render thread, read by the GUI thread after sync.
	std::optional<ViewportWindow::PickResult> _pickResult;

	/// The shared QRhi instance, created and used only on the render thread.
	std::unique_ptr<QRhi> _rhi;

#if QT_CONFIG(opengl)
	/// Fallback surface for OpenGL (must outlive the QRhi).
	std::unique_ptr<QOffscreenSurface> _fallbackSurface;
#endif

#if QT_CONFIG(vulkan) && defined(Q_OS_LINUX)
	/// The Vulkan instance, created on the GUI thread in the constructor.
	/// Shared by all QWindows and the QRhi that use this render thread.
	std::unique_ptr<QVulkanInstance> _vulkanInstance;
#endif

	/// Counter for generating unique render target handles.
	RenderTargetHandle _nextHandle = 1;

	/// Per-target rendering state, keyed by handle.
	/// Accessed only from the render thread (except via events from the GUI thread).
	/// Allocator for unique object IDs used in rendering and object picking.
	/// Must be declared before the render targets and the resource cache, because both may hold ObjectHandle instances
	/// (the picking map of a render target keeps the IDs of its last pass reserved until it is reset): members declared
	/// after this one are destroyed before it, returning all IDs while the allocator is still alive.
	ObjectIdAllocator _objectIdAllocator;

	/// Uses std::unordered_map because TargetState is move-only (contains unique_ptr).
	std::unordered_map<RenderTargetHandle, TargetState> _targets;

	// QRhi resources for object picking passes (lazily allocated)
	std::unique_ptr<QRhiTexture> _pickObjectIdTexture;            ///< R32U — per-pixel object ID.
	std::unique_ptr<QRhiTexture> _pickPrimitiveIdTexture;         ///< R32U — per-pixel primitive ID (sub-object ID within an object).
	std::unique_ptr<QRhiTexture> _pickDepthTexture;               ///< D32F — per-pixel depth value (readable, for world-position reconstruction).
	std::unique_ptr<QRhiTextureRenderTarget> _pickTarget;         ///< Render target for the picking pass (two color attachments + depth texture).
	std::unique_ptr<QRhiRenderPassDescriptor> _pickRenderPassDesc;///< Render pass descriptor for the picking target.

	/// Cache for QRhi resources such as vertex buffers and textures. Shared by all active render targets.
    std::shared_ptr<RendererResourceCache> _rhiResourceCache = std::make_shared<RendererResourceCache>();

	/// Warning messages from the renderer implementation collected during the last render pass
	/// and to be displayed as an overlay in the viewport.
	QStringList _warnings;
};

/**
 * Represents an onscreen or offscreen render target managed by a RenderThread.
 * Provides methods for submitting frame graphs and performing picking operations.
 * Automatically destroys the underlying GPU resources when it goes out of scope
 * and keeps the owning RenderThread alive via a std::shared_ptr.
 *
 * Movable but not copyable. Default-constructed state is null (no target).
 */
class OVITO_CORE_EXPORT RenderTarget
{
public:

	/// Default constructor. Creates a null guard with no associated render target.
	RenderTarget() noexcept = default;

	/// Takes ownership of the given render target handle and render thread.
	RenderTarget(RenderThread::RenderTargetHandle handle, std::shared_ptr<RenderThread> thread) noexcept
		: _handle(handle), _thread(std::move(thread)) {}

	/// Destructor. Destroys the render target if this guard is non-null.
	~RenderTarget() { reset(); }

	/// Move constructor. Transfers ownership from \a other, leaving it null.
	RenderTarget(RenderTarget&& other) noexcept
		: _handle(std::exchange(other._handle, 0))
		, _thread(std::exchange(other._thread, {})) {}

	/// Move assignment. Destroys the currently held target (if any) and transfers ownership from \a other.
	RenderTarget& operator=(RenderTarget&& other) noexcept
	{
		if(this != &other) {
			reset();
			_handle = std::exchange(other._handle, 0);
			_thread = std::exchange(other._thread, {});
		}
		return *this;
	}

	/// Non-copyable.
	RenderTarget(const RenderTarget&) = delete;
	RenderTarget& operator=(const RenderTarget&) = delete;

	/// Returns true if this guard holds a valid render target.
	explicit operator bool() const noexcept { return _handle != 0; }

	/// Submits a frame graph for rendering and presenting on an onscreen target.
	void renderOnscreenFrame(OORef<FrameGraph> frameGraph, std::unique_ptr<SceneRenderer::Configuration> rendererConfig)
	{
		OVITO_ASSERT(_handle != 0 && _thread);
		return _thread->renderOnscreenFrame(_handle, std::move(frameGraph), std::move(rendererConfig));
	}

	/// Renders and reads back the image into the provided offscreen FrameBuffer.
	/// The returned ScopedFuture<void> is fulfilled when the readback is complete and the FrameBuffer has been updated.
	[[nodiscard]] ScopedFuture<void> renderOffscreenFrame(OORef<FrameGraph> frameGraph, std::unique_ptr<SceneRenderer::Configuration> rendererConfig, std::shared_ptr<FrameBuffer> frameBuffer, TaskProgress& progress)
	{
		OVITO_ASSERT(_handle != 0 && _thread);
		return _thread->renderOffscreenFrame(_handle, std::move(frameGraph), std::move(rendererConfig), std::move(frameBuffer), progress);
	}

	/// Requests a picking operation at the given position.
	/// Blocks the calling (GUI) thread until the result is available.
	[[nodiscard]] std::optional<ViewportWindow::PickResult> requestPick(const QPointF& pos, int pickRadius = 4)
	{
		OVITO_ASSERT(_handle != 0 && _thread);
		return _thread->requestPick(_handle, pos, pickRadius);
	}

	/// Submits a frame graph for ambient occlusion sampling to the target.
	/// This method is only used by the implementation of the AmbientOcclusionModifier.
	/// It returns the contents of the object ID and primitive ID buffers as byte arrays, which are used to determine the AO contribution for each particle.
	/// The method may be called from a worker thread.
	[[nodiscard]] ScopedFuture<std::pair<QByteArray, QByteArray>> renderAOFrame(OORef<FrameGraph> frameGraph)
	{
		OVITO_ASSERT(_handle != 0 && _thread);
		return _thread->renderAOFrame(_handle, std::move(frameGraph));
	}

	/// Renders a picking pass for the frame graph into the target's offscreen picking buffers
	/// and returns the resulting picking buffer.
	[[nodiscard]] ScopedFuture<ObjectPickingBuffer> renderPickingFrame(OORef<FrameGraph> frameGraph,
		std::unique_ptr<SceneRenderer::Configuration> rendererConfig)
	{
		OVITO_ASSERT(_handle != 0 && _thread);
		return _thread->renderPickingFrame(_handle, std::move(frameGraph), std::move(rendererConfig));
	}

	/// Releases the GPU resources (swap chain, renderer state, picking buffers) of this
	/// render target while the QWindow remains valid. This is a fire-and-forget operation.
	/// The swap chain will be automatically recreated by the render thread when the next
	/// frame is submitted.
	void suspend()
	{
		OVITO_ASSERT(_handle != 0 && _thread);
		_thread->suspendTarget(_handle);
	}

	/// Destroys the held render target (if any) and resets this guard to null.
	void reset() noexcept;

	/// Returns the shared_ptr to the owning RenderThread without releasing the target.
	/// Useful for keeping the thread alive independently of this target's lifetime.
	std::shared_ptr<RenderThread> thread() const noexcept { return _thread; }

private:

	/// The owned render target handle. 0 means null/empty.
	RenderThread::RenderTargetHandle _handle = 0;

	/// Keeps the render thread alive while this target exists.
	std::shared_ptr<RenderThread> _thread;
};

}   // End of namespace
