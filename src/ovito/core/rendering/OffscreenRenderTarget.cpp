// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/core/rendering/OffscreenRenderTarget.h>
#include <ovito/core/rendering/RenderThread.h>
#include <ovito/core/rendering/FrameGraph.h>
#include <ovito/core/rendering/FrameBuffer.h>
#include <ovito/core/rendering/SceneRenderer.h>
#include <ovito/core/app/UserInterface.h>
#include <ovito/core/utilities/concurrent/TaskProgress.h>

namespace Ovito {

/******************************************************************************
* Constructor.
******************************************************************************/
OffscreenRenderTarget::OffscreenRenderTarget(UserInterface& userInterface, Kind kind)
	: _userInterface(&userInterface)
	, _kind(kind)
{
}

/******************************************************************************
* Destructor. Releases the GPU target, which waits for the render thread.
******************************************************************************/
OffscreenRenderTarget::~OffscreenRenderTarget() = default;
OffscreenRenderTarget::OffscreenRenderTarget(OffscreenRenderTarget&& other) noexcept = default;
OffscreenRenderTarget& OffscreenRenderTarget::operator=(OffscreenRenderTarget&& other) noexcept = default;

/******************************************************************************
* Returns the user interface this target renders for.
******************************************************************************/
UserInterface* OffscreenRenderTarget::userInterface() const
{
	return _userInterface.lock().get();
}

/******************************************************************************
* Allocates the GPU resources of the target, or recreates them when the size changed.
******************************************************************************/
void OffscreenRenderTarget::prepare(const QSize& pixelSize)
{
	ensureTarget(pixelSize);
}

/******************************************************************************
* Returns the shared render thread of the user interface, creating it if needed.
******************************************************************************/
std::shared_ptr<RenderThread> OffscreenRenderTarget::renderThread() const
{
	if(OORef<UserInterface> userInterface = _userInterface.lock()) {
		// Creates the shared render thread (and with it the graphics device) on first use.
		if(std::shared_ptr<RenderThread> thread = userInterface->renderThread())
			return thread;
	}
	throw Exception(QCoreApplication::translate("OffscreenRenderTarget",
		"Cannot render offscreen: the user interface that requested the rendering is gone."));
}

/******************************************************************************
* Creates the GPU target, or recreates it when the requested pixel size changed.
******************************************************************************/
void OffscreenRenderTarget::ensureTarget(const QSize& pixelSize)
{
	// A pass without a region to render into would leave a target of an undefined size behind; that is a programming
	// error of the caller rather than something the render thread could make sense of.
	if(pixelSize.isEmpty()) {
		OVITO_ASSERT_MSG(false, "OffscreenRenderTarget::ensureTarget()", "The offscreen render target needs a non-empty resolution.");
		throw Exception(QCoreApplication::translate("OffscreenRenderTarget",
			"Cannot render offscreen: the area to render is empty."));
	}

	if(_target && _pixelSize == pixelSize)
		return;

	// Allocating the target needs the render thread, which is created on demand and only by the main thread. A pass
	// that is submitted from a worker thread therefore has to find the target prepared already (see prepare()).
	if(!this_task::isMainThread()) {
		OVITO_ASSERT_MSG(false, "OffscreenRenderTarget::ensureTarget()",
			"An offscreen render target can only be allocated by the main thread.");
		throw Exception(QCoreApplication::translate("OffscreenRenderTarget",
			"Cannot render offscreen: the target has to be prepared by the main thread before a pass is submitted for it."));
	}

	// The old target is released before the new one is allocated, so that the peak GPU memory stays at one target's
	// worth: a full-resolution render output holds textures large enough for the double allocation to fail.
	_target.reset();

	// Creating a target allocates the GPU resources and blocks until the render thread is done.
	_target = std::make_unique<RenderTarget>(renderThread()->createOffscreenTarget(pixelSize, _kind == Kind::PickingOnly));
	_pixelSize = pixelSize;
}

/******************************************************************************
* Renders a frame graph into a frame buffer and reads the resulting image back.
******************************************************************************/
ScopedFuture<void> OffscreenRenderTarget::renderImage(OORef<FrameGraph> frameGraph, const SceneRenderer& renderer,
	std::shared_ptr<FrameBuffer> frameBuffer, TaskProgress& progress, int supersamplingFactor)
{
	OVITO_ASSERT_MSG(_kind == Kind::Visual, "OffscreenRenderTarget::renderImage()",
		"Only a visual offscreen target has a color texture to render an image into.");
	if(_kind != Kind::Visual) {
		throw Exception(QCoreApplication::translate("OffscreenRenderTarget",
			"Cannot render a color image: this offscreen target holds only the object picking buffers."));
	}
	OVITO_ASSERT(frameGraph && frameBuffer);
	OVITO_ASSERT(supersamplingFactor >= 1);

	ensureTarget(frameBuffer->viewportRect().size() * supersamplingFactor);

	// The pass writes the whole viewport rectangle of the frame buffer, so a delayed clear of that area queued by the
	// caller would be committed after and would overwrite the rendered image again.
	frameBuffer->discardChanges();

	// Note: the renderer's configuration has to be created before the frame graph is moved into the call below.
	std::unique_ptr<SceneRenderer::Configuration> rendererConfig = renderer.createConfiguration(*frameGraph);
	return _target->renderOffscreenFrame(std::move(frameGraph), std::move(rendererConfig), std::move(frameBuffer), progress);
}

/******************************************************************************
* Renders a picking pass and returns the picking buffer.
******************************************************************************/
ScopedFuture<ObjectPickingBuffer> OffscreenRenderTarget::renderPicking(OORef<FrameGraph> frameGraph,
	const SceneRenderer& renderer, const QSize& pixelSize)
{
	OVITO_ASSERT_MSG(_kind == Kind::PickingOnly, "OffscreenRenderTarget::renderPicking()",
		"Picking passes need an offscreen target that allocates the object and primitive ID buffers.");
	if(_kind != Kind::PickingOnly) {
		throw Exception(QCoreApplication::translate("OffscreenRenderTarget",
			"Cannot render a picking pass: this offscreen target was created for color images."));
	}
	OVITO_ASSERT(frameGraph);

	ensureTarget(pixelSize);

	// Note: the renderer's configuration has to be created before the frame graph is moved into the call below.
	std::unique_ptr<SceneRenderer::Configuration> rendererConfig = renderer.createConfiguration(*frameGraph);
	return _target->renderPickingFrame(std::move(frameGraph), std::move(rendererConfig));
}

/******************************************************************************
* Renders an ambient-occlusion sampling pass and returns the raw ID buffers.
******************************************************************************/
ScopedFuture<std::pair<QByteArray, QByteArray>> OffscreenRenderTarget::renderAmbientOcclusion(OORef<FrameGraph> frameGraph,
	const QSize& pixelSize)
{
	OVITO_ASSERT_MSG(_kind == Kind::PickingOnly, "OffscreenRenderTarget::renderAmbientOcclusion()",
		"Ambient-occlusion sampling needs an offscreen target that allocates the object and primitive ID buffers.");
	if(_kind != Kind::PickingOnly) {
		throw Exception(QCoreApplication::translate("OffscreenRenderTarget",
			"Cannot render an ambient-occlusion sampling pass: this offscreen target was created for color images."));
	}
	OVITO_ASSERT(frameGraph);

	ensureTarget(pixelSize);

	return _target->renderAOFrame(std::move(frameGraph));
}

}	// End of namespace
