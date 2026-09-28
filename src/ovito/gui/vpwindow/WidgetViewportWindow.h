////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2026 OVITO GmbH, Germany
//
//  This file is part of OVITO (Open Visualization Tool).
//
//  OVITO is free software; you can redistribute it and/or modify it either under the
//  terms of the GNU General Public License version 3 as published by the Free Software
//  Foundation (the "GPL") or, at your option, under the terms of the MIT License.
//  If you do not alter this notice, a recipient may use your version of this
//  file under either the GPL or the MIT License.
//
//  You should have received a copy of the GPL along with this program in a
//  file LICENSE.GPL.txt.  You should have received a copy of the MIT License along
//  with this program in a file LICENSE.MIT.txt
//
//  This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND,
//  either express or implied. See the GPL or the MIT License for the specific language
//  governing rights and limitations.
//
////////////////////////////////////////////////////////////////////////////////////////

#pragma once


#include <ovito/gui/base/GUIBase.h>
#include <ovito/gui/base/viewport/BaseViewportWindow.h>
#include <ovito/core/rendering/RenderThread.h>

#include <QWidget>

namespace Ovito {

/**
 * \brief QWidget-based viewport window implementation.
 *
 * Each WidgetViewportWindow renders into its own QWindow via a shared QRhi instance
 * managed by a RenderThread. All viewport windows within the same top-level
 * main window share the same render thread and QRhi, allowing them to share GPU resources.
 */
class OVITO_VIEWPORTWINDOW_EXPORT WidgetViewportWindow : public BaseViewportWindow
{
	Q_OBJECT
	OVITO_CLASS(WidgetViewportWindow)

public:

	/// Associates this window with a viewport and creates the UI widget.
	void initializeWindow(Viewport* vp, UserInterface& userInterface, QWidget* parent);

	/// Releases the render target (while the QWindow's surface is still valid) and then
	/// destroys the embedded widget/QWindow, in that order. Safe to call more than once.
	/// Performing the teardown in this order avoids a deadlock between the GUI thread and
	/// the RenderThread during window destruction.
	void releaseWindow();

	/// Renders the current window contents offscreen and returns them as a QImage.
	/// Reuses the frame graph of the last displayed frame, so the image matches the
	/// on-screen view (including tripod, caption and gizmos). Returns a null QImage if
	/// nothing has been rendered yet. Intended for occasional use to create screenshots.
	Future<QImage> grabViewportImage();

	/// Returns the internal QWindow that serves as the rendering surface for this viewport window.
	QWindow* window() const { return _window; }

	/// Returns Qt widget associated with this viewport window.
	QWidget* widget() const { return _widget; }

	/// Returns the icon image used for displaying non-fatal rendering warnings.
	virtual QImage warningIcon() const override;

	/// Determines the object located under the given mouse cursor position.
	virtual std::optional<PickResult> pick(const QPointF& pos) override;

	/// Indicates whether the window is currently shown or not.
	virtual bool isVisible() const override {
		return widget() && widget()->isVisible() && window() && window()->isExposed();
	}

	/// Sets the mouse cursor shape for the window.
	virtual void setCursor(const QCursor& cursor) override {
		if(window())
			window()->setCursor(cursor);
	}

	/// Returns the current position of the mouse cursor relative to the viewport window.
	virtual QPoint getCurrentMousePos() const override {
		return window() ? window()->mapFromGlobal(QCursor::pos()) : QPoint();
	}

	/// Returns the current size of the viewport window (in device pixels).
	virtual QSize viewportWindowDeviceSize() const override {
		return window() ? window()->size() * devicePixelRatio() : QSize();
	}

	/// Returns the current size of the viewport window (in device-independent pixels).
	virtual QSize viewportWindowDeviceIndependentSize() const override {
		return window() ? window()->size() : QSize();
	}

	/// Returns the device pixel ratio of the viewport window's canvas.
	virtual qreal devicePixelRatio() const override {
        return window() ? window()->devicePixelRatio() : 1.0;
	}

protected:

	/// This method is called after the reference counter of this object has reached zero
	/// and before the object is being finally deleted.
	virtual void aboutToBeDeleted() override;

	/// Renders the window contents after the frame graph was generated.
	virtual void renderFrameGraph(OORef<FrameGraph> frameGraph) override;

	/// Filters events sent to the widget.
	bool eventFilter(QObject* obj, QEvent* event) override;

private:

	/// The internal QWindow that serves as the rendering surface for this viewport window.
	QPointer<QWindow> _window;

	/// Tracks whether the QWindow currently has an exposed (renderable) surface.
	/// Driven exclusively by QExposeEvent and QEvent::Hide transitions.
	bool _isExposed = false;

	/// The QWidget that wraps the internal QWindow and makes it usable in the Qt widget hierarchy.
	QPointer<QWidget> _widget;

	/// RAII guard for the onscreen render target. Automatically destroys the target
	/// and keeps the shared render thread alive while this viewport window exists.
	RenderTarget _renderTarget;

	/// The frame graph of the most recently displayed (non-preliminary) frame,
	/// retained so the window contents can be grabbed as an image on demand.
	OORef<FrameGraph> _lastFrameGraph;
};

}   // End of namespace
