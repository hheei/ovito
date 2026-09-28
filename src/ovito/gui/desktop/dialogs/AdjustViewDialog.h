// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/desktop/GUI.h>
#include <ovito/core/viewport/Viewport.h>
#include <ovito/core/oo/RefTargetListener.h>

namespace Ovito {

/**
 * This dialog box lets the user adjust the camera settings of the current viewport.
 */
class AdjustViewDialog : public QDockWidget, public UserInterfaceComponent<MainWindowUI>
{
    Q_OBJECT

public:

    /// Constructor.
    AdjustViewDialog(MainWindowUI& ui, Viewport* viewport);

private Q_SLOTS:

    /// Event handler for the Cancel button.
    void onCancel();

    /// Is called when the user has changed the camera settings.
    void onAdjustCamera();

    /// Updates the values displayed in the dialog.
    void updateGUI();

private:

    bool _isUpdatingGUI = false;

    QRadioButton* _camPerspective;
    QRadioButton* _camParallel;

    SpinnerWidget* _camPosXSpinner;
    SpinnerWidget* _camPosYSpinner;
    SpinnerWidget* _camPosZSpinner;

    SpinnerWidget* _camDirXSpinner;
    SpinnerWidget* _camDirYSpinner;
    SpinnerWidget* _camDirZSpinner;

    QRadioButton* _constrainRotationBtn;
    QRadioButton* _rollAngleBtn;
    SpinnerWidget* _rollAngleSpinner;
    SpinnerWidget* _upDirXSpinner;
    SpinnerWidget* _upDirYSpinner;
    SpinnerWidget* _upDirZSpinner;

    SpinnerWidget* _camFOVAngleSpinner;
    SpinnerWidget* _camFOVSpinner;

    RefTargetListener<Viewport> _viewportListener;
    Viewport::ViewType _oldViewType;
    AffineTransformation _oldCameraTM;
    FloatType _oldFOV;
};

}   // End of namespace
