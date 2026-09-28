// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/core/dataset/animation/AnimationSettings.h>
#include <ovito/core/app/undo/UndoableOperation.h>
#include <ovito/gui/desktop/widgets/general/SpinnerWidget.h>

namespace Ovito {

/**
 * This dialog box lets the user manage the animation settings.
 */
class OVITO_GUI_EXPORT AnimationSettingsDialog : public QDialog, public UserInterfaceComponent<MainWindowUI>, private UndoableTransaction
{
    Q_OBJECT

public:

    /// Constructor.
    AnimationSettingsDialog(MainWindowUI& ui, QWidget* parentWindow = nullptr);

private Q_SLOTS:

    /// Event handler for the Ok button.
    void onOk();

    /// Is called when the user has selected a new value for the frames per seconds.
    void onFramesPerSecondChanged(int index);

    /// Is called when the user has selected a new value for the playback speed.
    void onPlaybackSpeedChanged(int index);

    /// Is called when the user changes the start/end values of the animation interval.
    void onAnimationIntervalChanged();

private:

    /// Updates the values shown in the dialog.
    void updateUI();

    /// The animation settings being edited.
    OORef<AnimationSettings> _animSettings;

    QComboBox* fpsBox;
    SpinnerWidget* animStartSpinner;
    SpinnerWidget* animEndSpinner;
    SpinnerWidget* everyNthFrameSpinner;
    QComboBox* playbackSpeedBox;
    QCheckBox* loopPlaybackBox;
    QGroupBox* animIntervalBox;
    QButtonGroup* preferSimulationTimeGroup;

    bool framesPerSecondModified = false;
    bool playbackSpeedModified = false;
    bool loopPlaybackModified = false;
    bool preferSimulationTimeModified = false;
};

}   // End of namespace
