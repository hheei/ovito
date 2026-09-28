// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/gui/base/GUIBase.h>
#include <ovito/core/app/UserInterface.h>
#include <ovito/core/viewport/Viewport.h>
#include <ovito/core/viewport/ViewportConfiguration.h>
#include <ovito/core/rendering/FrameBuffer.h>
#include <ovito/core/rendering/SceneRenderer.h>
#include <ovito/core/utilities/concurrent/ObjectExecutor.h>
#include "WidgetViewportWindow.h"

#include <QToolTip>
#include <QAccessible>
#include <QAccessibleWidget>

namespace Ovito {

namespace {

/// Name of the dynamic QWidget property used to look up the WidgetViewportWindow instance
/// owning a given viewport widget, regardless of the concrete widget type created by the
/// active rendering backend (e.g. the QWindow-container widget used here, or a QOpenGLWidget
/// on other branches).
constexpr const char* kViewportWindowProperty = "_ovito_viewportWindow";

/**
 * Exposes a viewport's Qt widget to assistive technologies and UI automation tools as a
 * named pane. Without this, the widget has no accessible name at all, so a screen reader
 * user tabbing through the main window cannot tell which viewport ("Top", "Front", a camera
 * name, etc.) currently has the focus. The interactive 3D contents of the viewport (gizmos,
 * scene objects) are not exposed as accessible children; only the viewport's identity is.
 */
class ViewportWidgetAccessible : public QAccessibleWidget
{
public:
	explicit ViewportWidgetAccessible(QWidget* widget) : QAccessibleWidget(widget, QAccessible::Grouping) {}

	QString text(QAccessible::Text t) const override
	{
		if(WidgetViewportWindow* vpWindow = qobject_cast<WidgetViewportWindow*>(object()->property(kViewportWindowProperty).value<QObject*>())) {
			if(t == QAccessible::Name)
				return QAccessibleWidget::tr("%1 viewport").arg(vpWindow->viewport()->viewportTitle());
		}
		return QAccessibleWidget::text(t);
	}

	// Advertises the standard "show context menu" action to assistive technologies (e.g.
	// VoiceOver's "show menu" gesture on macOS), in addition to Qt's own default action list.
	// Qt only does this automatically for widgets using Qt::ActionsContextMenu, which does not
	// apply here since the viewport's context menu is built and shown on demand (see
	// BaseViewportWindow::contextMenuEvent()).
	QStringList actionNames() const override
	{
		return QAccessibleWidget::actionNames() + QStringList(showMenuAction());
	}

	void doAction(const QString& actionName) override
	{
		if(actionName == showMenuAction()) {
			if(QWidget* w = qobject_cast<QWidget*>(object())) {
				QContextMenuEvent e(QContextMenuEvent::Other, QPoint(), w->mapToGlobal(QPoint()));
				QCoreApplication::sendEvent(w, &e);
			}
			return;
		}
		QAccessibleWidget::doAction(actionName);
	}
};

QAccessibleInterface* viewportWidgetAccessibleFactory(const QString&, QObject* object)
{
	if(QWidget* widget = qobject_cast<QWidget*>(object)) {
		if(widget->property(kViewportWindowProperty).isValid())
			return new ViewportWidgetAccessible(widget);
	}
	return nullptr;
}

} // End of anonymous namespace

IMPLEMENT_CREATABLE_OVITO_CLASS(WidgetViewportWindow);

/******************************************************************************
* This method is called after the reference counter of this object has reached zero
* and before the object is being finally deleted.
******************************************************************************/
void WidgetViewportWindow::aboutToBeDeleted()
{
	releaseWindow();

	BaseViewportWindow::aboutToBeDeleted();
}

/******************************************************************************
* Releases the render target and destroys the embedded widget/QWindow.
******************************************************************************/
void WidgetViewportWindow::releaseWindow()
{
	// On Linux/Vulkan, QXcbVulkanWindow::~QXcbVulkanWindow() (called via delete widget()
	// below) invokes vkDestroySurfaceKHR, which requires the QVulkanInstance owned by the
	// RenderThread to still be alive. Without this guard, RenderTarget::reset() may drop
	// the last shared_ptr<RenderThread>, destroying the QVulkanInstance before the QWindow.
	auto keepRenderThreadAlive = _renderTarget.thread();

	// Release the retained frame graph so its visualization-cache resource frame is freed
	// together with the rest of the window's GPU resources.
	_lastFrameGraph.reset();

	// Destroy the render target (blocks until GPU resources are released).
	// The swap chain must be released while the QWindow's surface is still valid,
	// otherwise the RenderThread could try to recreate the native window (deadlocking
	// the GUI thread, which is parked during window destruction).
	_renderTarget.reset();

	// Remove the event filter before nulling _window. Destroying the QWindowContainer below
	// causes AppKit to synchronously deliver hide/expose events to the QWindow, which would
	// re-enter eventFilter() and dereference _window while it is already null.
	if(_window)
		_window->removeEventFilter(this);
	_window = nullptr; // Will be destroyed when the container widget is deleted.

	// Release the UI widget (also destroys the QWindow). The QVulkanInstance in keepRenderThreadAlive
	// remains valid throughout this call, allowing vkDestroySurfaceKHR to complete safely.
	delete widget();
	_widget = nullptr;
}

/******************************************************************************
* Returns the icon image used for displaying non-fatal rendering warnings.
******************************************************************************/
QImage WidgetViewportWindow::warningIcon() const
{
	return QIcon(QStringLiteral(":/guibase/mainwin/status/status_error.svg")).pixmap(22, 22).toImage();
}

/******************************************************************************
* Associates this window with a viewport and creates the UI widget.
******************************************************************************/
void WidgetViewportWindow::initializeWindow(Viewport* vp, UserInterface& userInterface, QWidget* parent)
{
	OVITO_ASSERT(!_widget && !_window && !_renderTarget);

	setViewport(vp, userInterface);

	// Obtain the shared render thread and create an onscreen render target.
	// The RenderThread creates the QWindow with the correct surface type.
	auto [renderTarget, window] = userInterface.renderThread()->createOnscreenTarget(this);
	_renderTarget = std::move(renderTarget);
	_window = window;

	// Install an event filter for input events (mouse, keyboard, etc.).
	window->installEventFilter(this);

	// Wrap the QWindow in a QWidget to make it usable in the Qt widget hierarchy.
	_widget = QWidget::createWindowContainer(window, parent);

	static const bool accessibilityFactoryRegistered = []() {
		QAccessible::installFactory(viewportWidgetAccessibleFactory);
		return true;
	}();
	Q_UNUSED(accessibilityFactoryRegistered);
	widget()->setProperty(kViewportWindowProperty, QVariant::fromValue(static_cast<QObject*>(this)));

	widget()->setMouseTracking(true);
	widget()->setFocusPolicy(Qt::StrongFocus);

	// Make sure the viewport window releases its resources before the application shuts down, e.g. due to a Python script error.
	connect(QCoreApplication::instance(), &QObject::destroyed, this, &BaseViewportWindow::releaseResources);
}

/******************************************************************************
* Renders the window contents after the frame graph was generated.
******************************************************************************/
void WidgetViewportWindow::renderFrameGraph(OORef<FrameGraph> frameGraph)
{
	// Submit frame graph to render thread for onscreen rendering.
	// frameCompleted will be emitted by the render thread after QRhi::beginFrame() succeeds.
	if(_renderTarget && sceneRenderer()) {
		auto rendererConfig = sceneRenderer()->createConfiguration(*frameGraph);
		// Retain the last fully-rendered frame graph so the window contents can be grabbed
		// as an image later (see grabViewportImage()). Preliminary frames are skipped.
		if(!frameGraph->isPreliminaryState())
			_lastFrameGraph = frameGraph;
		_renderTarget.renderOnscreenFrame(std::move(frameGraph), std::move(rendererConfig));
	}
	else if(frameGraph && !frameGraph->isPreliminaryState()) {
		// No render target available; emit immediately so animation playback is not blocked.
		Q_EMIT frameCompleted();
	}
}

/******************************************************************************
* Renders the current window contents offscreen and returns them as a QImage.
******************************************************************************/
Future<QImage> WidgetViewportWindow::grabViewportImage()
{
	// Establish the object's task/thread context (as generateFrameGraph() does).
	co_await ExecutorAwaiter(DeferredObjectExecutor(this));

	// Grab the frame graph of the last displayed frame. Keep a copy in _lastFrameGraph so
	// repeated grabs remain possible (renderOffscreenFrame() consumes the OORef we pass it).
	OORef<FrameGraph> frameGraph = _lastFrameGraph;
	if(!frameGraph || !sceneRenderer() || !ui().renderThread())
		co_return QImage();

	this_task::get()->setUserInterface(ui().shared_from_this());
	this_task::get()->setIsInteractive();

	// Render at the exact device-pixel resolution the retained frame was displayed at.
	QSize size = frameGraph->viewportDeviceIndependentSize() * frameGraph->devicePixelRatio();
	if(size.isEmpty())
		co_return QImage();

	// Reuse OVITO's offscreen render-to-image infrastructure.
	RenderTarget renderTarget = ui().renderThread()->createOffscreenTarget(size);
	auto frameBuffer = std::make_shared<FrameBuffer>(size);

	auto rendererConfig = sceneRenderer()->createConfiguration(*frameGraph);
	co_await FutureAwaiter(ObjectExecutor(this), renderTarget.renderOffscreenFrame(
		std::move(frameGraph), std::move(rendererConfig), frameBuffer, TaskProgress::Ignore));

	co_return frameBuffer->image();
}

/******************************************************************************
* Filters events sent to the widget.
******************************************************************************/
bool WidgetViewportWindow::eventFilter(QObject* obj, QEvent* event)
{
	switch(event->type()) {
	case QEvent::Show:
		// The window entered the widget hierarchy. Surface may not be exposed yet.
		// showEvent() sets ScenePreparation::setAutoRestart(true) and calls handleUpdateRequests(), which
		// guards with isVisible() → isExposed(), so it is a safe no-op until exposed.
		showEvent(static_cast<QShowEvent*>(event));
		break;
	case QEvent::Hide: {
		// The window left the widget hierarchy (e.g., viewport maximization).
		// Use exchange to detect whether we were exposed: if QExposeEvent(false) already fired
		// before this Hide (e.g., on minimize on some platforms), _isExposed is already false
		// and suspend() must not be called a second time.
		bool wasExposed = std::exchange(_isExposed, false);
		hideEvent(static_cast<QHideEvent*>(event));  // Stops pipeline + cancels frame graph future.
		if(wasExposed && _renderTarget)
			_renderTarget.suspend();  // Release GPU resources (swap chain, renderer state, picking buffers).
		break;
	}
	case QEvent::Leave:
		leaveEvent(event);
		break;
	case QEvent::MouseButtonDblClick:
		if(widget()->isEnabled())
			mouseDoubleClickEvent(static_cast<QMouseEvent*>(event));
		break;
	case QEvent::MouseButtonPress:
		if(widget()->isEnabled())
			mousePressEvent(static_cast<QMouseEvent*>(event));
		break;
	case QEvent::MouseButtonRelease:
		if(widget()->isEnabled())
			mouseReleaseEvent(static_cast<QMouseEvent*>(event));
		break;
	case QEvent::MouseMove: {
		auto* me = static_cast<QMouseEvent*>(event);
		// Show tooltip if cursor is over the warning indicator icon.
		QRectF iconArea = warningIconArea();
		if(!iconArea.isEmpty() && iconArea.contains(me->position())) {
			QStringList msgs = currentWarnings();
			if(!msgs.isEmpty())
				QToolTip::showText(me->globalPosition().toPoint(),
				                   msgs.mid(0, 3).join(QLatin1Char('\n')), widget());
		}
		else {
			QToolTip::hideText();
		}
		if(widget()->isEnabled())
			mouseMoveEvent(me);
		break;
	}
	case QEvent::Wheel:
		if(widget()->isEnabled())
			wheelEvent(static_cast<QWheelEvent*>(event));
		break;
	case QEvent::FocusOut:
		focusOutEvent(static_cast<QFocusEvent*>(event));
		break;
	case QEvent::Resize:
		resizeEvent(static_cast<QResizeEvent*>(event));
		break;
	case QEvent::KeyPress:
		if(widget()->isEnabled())
			keyPressEvent(static_cast<QKeyEvent*>(event));
		break;
	case QEvent::ContextMenu:
		if(widget()->isEnabled())
			contextMenuEvent(static_cast<QContextMenuEvent*>(event));
		break;
	case QEvent::Expose: {
		bool nowExposed = window()->isExposed();
		if(nowExposed && !_isExposed) {
			// Transition: surface became available for rendering.
			_isExposed = true;
			// Resume pipeline evaluation (may have been stopped by a prior hide or un-expose).
			scenePreparation().setAutoRestart(true);
			// Schedule a full re-render. An earlier attempt may have been dropped because
			// the surface wasn't available yet, or the swap chain needs recreation.
			requestRerender(false);
		}
		else if(!nowExposed && _isExposed) {
			// Transition: surface became unavailable (e.g., main window minimized).
			_isExposed = false;
			QHideEvent fakeHide;
			hideEvent(&fakeHide);  // Stop pipeline evaluation + cancel frame graph future.
			if(_renderTarget)
				_renderTarget.suspend();  // Release GPU resources.
		}
		else if(nowExposed && _isExposed) {
			// Already exposed — this expose event comes from a display cycle (e.g. during
			// a live resize). Qt's Metal backend may have dropped the previously rendered
			// frame because the drawable's texture size no longer matches the surface size
			// (see QRhiMetal::endFrame's presentsWithTransaction path). Schedule a re-render
			// so the viewport doesn't remain stuck showing a stale frame.
			requestRerender(false);
		}
		break;
	}
	default:
		break;
	}
	return false;
}

/******************************************************************************
* Determines the object that is visible under the given mouse cursor position.
******************************************************************************/
std::optional<ViewportWindow::PickResult> WidgetViewportWindow::pick(const QPointF& pos)
{
	if(!_renderTarget || !window() || !window()->isExposed())
		return std::nullopt;

	// Convert cursor position from logical coordinated to render buffer coordinates.
	QPointF devicePixelPos = pos * devicePixelRatio();

	// Request the render thread to perform a picking operation at the given position.
	// This may entail a separate rendering pass. The GUI thread will block until the result is available.
	std::optional<ViewportWindow::PickResult> result;
	handleExceptions([&] {
		result = _renderTarget.requestPick(devicePixelPos);
	});
	return result;
}

}   // End of namespace
