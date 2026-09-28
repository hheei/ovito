// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/core/Core.h>
#include <ovito/core/utilities/concurrent/SharedFuture.h>
#include <ovito/core/dataset/scene/Scene.h>

namespace Ovito {

/**
 * \brief This object requests the next frames during animation playback in the interactive viewports.
 */
class OVITO_CORE_EXPORT SceneAnimationPlayback : public QObject, public RefMaker, public UserInterfaceComponent<UserInterface, false>
{
    OVITO_CLASS(SceneAnimationPlayback)
    Q_OBJECT

public:

    /// Constructor.
    void initializeObject(UserInterface& ui);

    /// Returns whether the animation is currently being played back in the viewports.
    bool isPlaybackActive() const { return _activePlaybackRate != 0; }

public Q_SLOTS:

    /// Starts playback of the animation.
    void startAnimationPlayback(Scene* scene, FloatType playbackRate = FloatType(1));

    /// Stops playback of the animation.
    void stopAnimationPlayback();

Q_SIGNALS:

    /// This signal is emitted when the animation playback is started or stopped.
    void playbackChanged(bool active);

    /// The signal is emitted when the current animation frame has changed during playback.
    void frameChanged(int frame);

private Q_SLOTS:

    /// Receives notification from a ViewportWindow that scene rendering is complete.
    void viewportWindowComplete();

    /// Starts a timer to show the next animation frame.
    void scheduleNextAnimationFrame();

protected:

    /// Handles timer events for this object.
    virtual void timerEvent(QTimerEvent* event) override;

private:

    /// Jumps to the given animation frame, then schedules the next frame as soon as the scene was completely shown.
    void continuePlaybackAtFrame(int frame);

    /// The scene being played back.
    DECLARE_MODIFIABLE_REFERENCE_FIELD(OORef<Scene>, scene, setScene);

    /// Indicates that the animation is currently being played back in the interactive viewports.
    FloatType _activePlaybackRate = 0;

    /// Measures how long it took to load, compute, and render the current animation frame.
    QElapsedTimer _frameRenderingTimer;

    /// Timer for scheduling the next animation frame.
    QBasicTimer _nextFrameTimer;

    /// Number of viewport windows we are waiting for to be rendered until the animation frame is complete.
    int _numPendingViewportWindows = 0;
};

}   // End of namespace
