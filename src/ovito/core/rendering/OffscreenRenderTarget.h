// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once

#include <ovito/core/Core.h>
#include <ovito/core/rendering/ObjectPickingBuffer.h>

namespace Ovito {

class FrameBuffer;
class FrameGraph;
class RenderTarget;
class SceneRenderer;
class RenderThread;
class TaskProgress;
class UserInterface;

/**
 * A reusable offscreen render target of the shared render thread, and the rules of the passes that render into it.
 *
 * Every consumer of the render thread's offscreen entry points used to own its own RenderTarget, had to know which
 * buffers such a target allocates, computed the supersampled pixel size itself and repeated the readback and reuse
 * rules; nothing in the types or the API said which of them may be called from which thread, so misuse was caught by
 * an assertion deep inside the render thread rather than at the call site. This class owns those decisions. Its three
 * entry points behave alike in every respect but the buffers they need:
 *
 * - renderImage() renders a color image (render output, viewport grabs, symbol images) into a visual target.
 * - renderPicking() renders a picking pass into a target that holds only the ID buffers.
 * - renderAmbientOcclusion() renders an ID-only sampling pass of the same kind of target.
 *
 * All three may be called from any thread except the render thread itself, they all deliver their completion to the
 * executor the caller awaits them with, and they all reuse the GPU target while its resolution does not change. The
 * rules they share in detail:
 *
 * - **Threading.** Submitting a pass and creating the GPU target may be done from any thread; both block until the
 *   render thread has processed the request. Only the render thread's own renderer implementations ever touch QRhi,
 *   and this class never holds a QRhi pointer. The completion of a pass is delivered to whichever executor the caller
 *   awaits the returned future with (see FutureAwaiter), never to the render thread.
 * - **Ownership.** This class owns the GPU target for its whole lifetime and destroys it in the destructor, which
 *   waits for the render thread to release the resources. While an instance exists, the render thread - and with it
 *   the graphics device - is kept alive, so an instance that outlives a frame has to be intentional (the picking
 *   target of a viewport window, the render output of an animation, the sampling loop of the ambient occlusion
 *   modifier); a one-shot use keeps the instance a local object.
 * - **Reuse.** The GPU target is created on the first render request, not in the constructor, and is reused as long as
 *   the requested pixel size and the kind stay the same. A size change destroys the old target (which waits for the
 *   render thread) and allocates a new one. Requests are serialized by the render thread's event queue, and destroying
 *   a target while a pass is in flight waits for that pass instead of freeing the buffers while it reads them.
 * - **Failure.** A pass that does not match the kind of the target (a color image from a picking-only target, or a
 *   picking pass from a visual one) is refused before any GPU work is submitted, with an assertion in debug builds and
 *   an exception in all builds.
 */
class OVITO_CORE_EXPORT OffscreenRenderTarget
{
public:

	/// What an offscreen target is used for. It decides which buffers the GPU target allocates and which passes may be
	/// submitted to it.
	enum class Kind {
		/// A target with a color texture, for image output: render output, viewport grabs and symbol images.
		Visual,
		/// A target without a color texture, holding only the object/primitive ID and depth buffers, for offscreen
		/// picking and for ambient-occlusion sampling.
		PickingOnly,
	};

	/// Constructor. The GPU target is created on the first render request, not here.
	OffscreenRenderTarget(UserInterface& userInterface, Kind kind);

	/// Destructor. Destroys the GPU target, waiting for the render thread to release its resources.
	~OffscreenRenderTarget();

	/// Move constructor.
	OffscreenRenderTarget(OffscreenRenderTarget&& other) noexcept;

	/// Move assignment. Destroys the target held by this instance (if any).
	OffscreenRenderTarget& operator=(OffscreenRenderTarget&& other) noexcept;

	/// Non-copyable: the class owns a GPU resource.
	OffscreenRenderTarget(const OffscreenRenderTarget&) = delete;
	OffscreenRenderTarget& operator=(const OffscreenRenderTarget&) = delete;

	/// Returns the user interface this target renders for, or null if it has been destroyed already.
	UserInterface* userInterface() const;

	/// Indicates whether offscreen rendering is still possible, i.e. whether the user interface exists. Requesting a
	/// render creates the user interface's shared render thread (and thereby the graphics device) if it does not exist.
	bool isAvailable() const { return userInterface() != nullptr; }

	/// Returns the kind this target was created for.
	[[nodiscard]] Kind kind() const { return _kind; }

	/// Returns the device-pixel resolution of the GPU target. Empty while no target has been created yet.
	[[nodiscard]] const QSize& pixelSize() const { return _pixelSize; }

	/// Indicates whether the GPU resources of the target have been allocated already.
	[[nodiscard]] bool hasGpuResources() const;

	/**
	 * Allocates the GPU resources of the target for a given device-pixel resolution, recreating them if the size
	 * changed since the last call. Calling it is optional: a pass allocates the target implicitly.
	 *
	 * This must run on the program's main thread: the shared render thread - and with it the graphics device - comes
	 * into existence on demand, and only the main thread may create it. ef renderAmbientOcclusion() calls this from
	 * the main thread before the sampling moves to a worker thread, because the sampling loop is the one pass that is
	 * submitted from a thread other than the main one.
	 */
	void prepare(const QSize& pixelSize);

	/**
	 * Renders \a frameGraph into \a frameBuffer and reads the resulting color image back.
	 *
	 * The GPU target is created or reused at `frameBuffer->viewportRect().size() * supersamplingFactor` device pixels,
	 * which is the resolution the frame graph is rendered at; the readback scales the image down to the viewport
	 * rectangle of the frame buffer. The frame buffer receives the rendered image; a delayed clear that the caller
	 * queued before is dropped, because the pass writes the whole viewport rectangle. The frame graph is consumed by
	 * the pass and may be rendered only once (see FrameGraph::finalizeForRendering()).
	 */
	[[nodiscard]] ScopedFuture<void> renderImage(OORef<FrameGraph> frameGraph, const SceneRenderer& renderer,
		std::shared_ptr<FrameBuffer> frameBuffer, TaskProgress& progress, int supersamplingFactor = 1);

	/**
	 * Renders a picking pass of \a frameGraph at a resolution of \a pixelSize and returns the picking buffer, from
	 * which the GUI thread resolves pick results without touching the render thread (see ObjectPickingBuffer).
	 */
	[[nodiscard]] ScopedFuture<ObjectPickingBuffer> renderPicking(OORef<FrameGraph> frameGraph,
		const SceneRenderer& renderer, const QSize& pixelSize);

	/**
	 * Renders an ambient-occlusion sampling pass of \a frameGraph and returns the raw object and primitive ID buffers.
	 * This is the only pass that reuses one frame graph for many samples, which is why it does not take a renderer:
	 * the IDs do not depend on the renderer's configuration.
	 */
	[[nodiscard]] ScopedFuture<std::pair<QByteArray, QByteArray>> renderAmbientOcclusion(OORef<FrameGraph> frameGraph,
		const QSize& pixelSize);

private:

	/// Creates the GPU target, or recreates it when the requested pixel size changed.
	void ensureTarget(const QSize& pixelSize);

	/// Returns the shared render thread of the user interface, creating it if needed. Throws if the interface is gone.
	[[nodiscard]] std::shared_ptr<RenderThread> renderThread() const;

	/// The user interface whose render thread performs the rendering. Weak, because the interface outlives the target.
	OOWeakRef<UserInterface> _userInterface;

	/// The kind of passes this target accepts.
	Kind _kind;

	/// The GPU target, created on the first render request. Owns the render thread's resources for this target.
	std::unique_ptr<RenderTarget> _target;

	/// The device-pixel resolution the current GPU target was created for.
	QSize _pixelSize;
};

}	// End of namespace
