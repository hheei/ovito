// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/desktop/mainwin/MainWindowUI.h>
#include <ovito/core/dataset/animation/TimeInterval.h>
#include <ovito/core/dataset/DataSetContainer.h>

namespace Ovito {

/**
 * A slider widget that lets the user control the current animation time.
 */
class AnimationTimeSlider : public QFrame, public UserInterfaceComponent<MainWindowUI>
{
    Q_OBJECT

public:

    /// Constructor.
    AnimationTimeSlider(MainWindowUI& ui, QWidget* parentWindow = nullptr);

    /// Computes the x position within the widget corresponding to the given animation frame.
    int frameToPos(int frame);

    /// Computes the x position within the widget corresponding to the given animation time.
    int timeToPos(AnimationTime time) { return frameToPos(time.frame()); }

    /// Converts a distance in pixels to a time difference.
    int distanceToFrameDifference(int distance);

    /// Computes the current position of the slider thumb widget.
    QRect thumbRectangle();

    /// Computes the width of the thumb widget.
    int thumbWidth() const;

    /// Computes the text to display on the thumb widget.
    QString thumbText(int frame) const;

    /// Computes the text to display next to a timeline tick.
    QString tickLabel(int frame) const;

    /// Computes the optimal distribution of tick marks along the timeline.
    std::tuple<int,int,int> tickRange(int minTickSeparation);

    /// Computes the maximum width of a frame tick label.
    int maxTickLabelWidth() const;

    /// Returns the recommended size of the widget.
    virtual QSize sizeHint() const override;

    /// Returns the minimum size of the widget.
    virtual QSize minimumSizeHint() const override { return sizeHint(); }

protected:

    /// Handles paint events.
    virtual void paintEvent(QPaintEvent* event) override;

    /// Handles mouse down events.
    virtual void mousePressEvent(QMouseEvent* event) override;

    /// Handles mouse up events.
    virtual void mouseReleaseEvent(QMouseEvent* event) override;

    /// Handles mouse move events.
    virtual void mouseMoveEvent(QMouseEvent* event) override;

    /// Is called when the widgets looses the input focus.
    virtual void focusOutEvent(QFocusEvent* event) override;

    /// Handles widget state changes.
    virtual void changeEvent(QEvent* event) override;

private:

    /// Creates the color palettes used by the widget.
    void updateColorPalettes();

protected Q_SLOTS:

    /// Is called whenever the Auto Key mode is activated or deactivated.
    void onAutoKeyModeChanged(bool active);

private:

    /// The dragging start position.
    int _dragPos = -1;

    /// The default palette used to the draw the time slide background.
    QPalette _normalPalette;

    /// The color palette used to the draw the time slide background when auto-key animation mode is active.
    QPalette _autoKeyModePalette;

    /// The palette used to the draw the slider.
    QPalette _sliderPalette;
};

}   // End of namespace
