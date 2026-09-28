// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/desktop/GUI.h>
#include <ovito/core/rendering/FrameBuffer.h>

namespace Ovito {

/**
 * This widget displays the contents of a FrameBuffer.
 */
class OVITO_GUI_EXPORT FrameBufferWidget : public QAbstractScrollArea
{
    Q_OBJECT
    Q_PROPERTY(qreal zoomFactor READ zoomFactor WRITE setZoomFactor)

public:

    /// Constructor.
    explicit FrameBufferWidget(QWidget* parent = nullptr);

    /// Return the FrameBuffer that is currently shown in the widget (can be NULL).
    const std::shared_ptr<FrameBuffer>& frameBuffer() const { return _frameBuffer; }

    /// Sets the FrameBuffer that is currently shown in the widget.
    void setFrameBuffer(const std::shared_ptr<FrameBuffer>& frameBuffer);

    /// Returns the current zoom factor.
    qreal zoomFactor() const { return _zoomFactor; }

    /// Zooms in or out of the image.
    void setZoomFactor(qreal zoom);

    /// Returns the preferred size of the window.
    virtual QSize sizeHint() const override;

    /// Returns a brush used for the background of (semi)transparent framebuffer images.
    static const QBrush& backgroundBrush();

public Q_SLOTS:

    /// Scales the image up.
    void zoomIn();

    /// Scales the image down.
    void zoomOut();

    /// Smoothly adjusts the zoom factor.
    void zoomTo(qreal newZoomFactor);

protected:

    /// This is called by the system to paint the viewport area.
    virtual void paintEvent(QPaintEvent* event) override;

    /// Handles viewport resize events.
    virtual void resizeEvent(QResizeEvent* event) override;

    /// Handles mouse wheel events.
    virtual void wheelEvent(QWheelEvent* event) override;

    /// Handles mouse press events.
    virtual void mousePressEvent(QMouseEvent* event) override;

    /// Handles mouse move events.
    virtual void mouseMoveEvent(QMouseEvent* event) override;

    /// Handles events of the viewport.
    virtual bool viewportEvent(QEvent* event) override;

    /// Returns the preferred size of the viewport widget.
    virtual QSize viewportSizeHint() const override;

    /// Updates the scroll bars of the widget.
    void updateScrollBarRange();

    /// Calculates the drawing rectangle for the framebuffer image within the viewport.
    QRect calculateViewportRect() const;

private Q_SLOTS:

    /// This handles contentChanged() signals from the frame buffer.
    void onFrameBufferContentChanged(const QRect& changedRegion);

    /// This handles bufferResized() signals from the frame buffer.
    void onFrameBufferResize();

    /// Updates the transparency of the zoom value indicator.
    void zoomLabelAnimationChanged(const QVariant& value);

private:

    /// The FrameBuffer that is shown in the widget.
    std::shared_ptr<FrameBuffer> _frameBuffer;

    /// The current zoom factor.
    qreal _zoomFactor = 1;

    /// For smoothly interpolating the zoom factor.
    QPropertyAnimation _zoomAnimation;
    QPropertyAnimation _horizontalScrollAnimation;
    QPropertyAnimation _verticalScrollAnimation;

    /// Stores the mouse cursor position from the last mouse move event.
    QPointF _mouseLastPosition;

    /// A label that is shown to indicate the current image zoom factor.
    QLabel* _zoomFactorDisplay;

    /// For animating the visibility of the zoom factor indicator.
    QVariantAnimation _zoomLabelAnimation;

    static constexpr qreal ZoomIncrement = 1.15;
    static constexpr qreal ZoomIncrementPow5 = ZoomIncrement*ZoomIncrement*ZoomIncrement*ZoomIncrement*ZoomIncrement;
    static constexpr qreal ZoomFactorMax = ZoomIncrementPow5 * ZoomIncrementPow5;
    static constexpr qreal ZoomFactorMin = 1.0 / (ZoomIncrementPow5 * ZoomIncrementPow5);
    static constexpr int ScrollBarScale = 10;
};

}   // End of namespace
