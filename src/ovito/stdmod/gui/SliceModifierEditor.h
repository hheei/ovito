// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/stdmod/gui/StdModGui.h>
#include <ovito/gui/desktop/properties/PropertiesEditor.h>
#include <ovito/gui/base/viewport/ViewportInputMode.h>
#include <ovito/gui/base/viewport/ViewportInputManager.h>
#include <ovito/stdmod/modifiers/SliceModifier.h>
#include <ovito/core/utilities/units/PrescribedScaleUnit.h>

namespace Ovito {

class PickPlanePointsInputMode; // Defined below

/**
 * A properties editor for the SliceModifier class.
 */
class SliceModifierEditor : public PropertiesEditor
{
    OVITO_CLASS(SliceModifierEditor)
    Q_OBJECT

protected:

    /// Creates the user interface controls for the editor.
    virtual void createUI(const RolloutInsertionParameters& rolloutParams) override;

protected Q_SLOTS:

    /// Aligns the slicing plane to the viewing direction.
    void onAlignPlaneToView();

    /// Aligns the current viewing direction to the slicing plane.
    void onAlignViewToPlane();

    /// Aligns the normal of the slicing plane with one of the coordinate axes.
    void onAlignNormalWithAxis(const QString& link);

    /// Moves the plane to the center of the simulation box.
    void onCenterOfBox();

    /// Is called when the user switches between Cartesian and reduced cell coordinates.
    void onCoordinateTypeChanged();

    /// Is called when the selected type of plane normal coordinates have changed.
    void updateCoordinateLabels();

    /// Auto-adjusts the increment steps of the numeric parameter spinner widgets.
    void updateParameterUnitScales();

private:

    ViewportModeAction* _pickPlanePointsInputModeAction;
    BooleanRadioButtonParameterUI* _reducedCoordinatesPUI;
    VectorParameterUI* _normalPUI[3];
    FloatParameterUI* _distancePUI;
    QPushButton* _centerPlaneBtn;
    std::optional<PrescribedScaleUnit> _distanceUnit;
    std::optional<PrescribedScaleUnit> _slabWidthUnit;
    std::optional<PrescribedScaleUnit> _normalVectorUnit;
};

/******************************************************************************
* The viewport input mode that lets the user select three points in space
* to define the slicing plane.
******************************************************************************/
class PickPlanePointsInputMode : public ViewportInputMode, public ViewportGizmo
{
public:

    /// Constructor.
    void initializeObject(SliceModifierEditor* editor) {
        ViewportInputMode::initializeObject();
        _editor = editor;
    }

    /// Handles the mouse events for a Viewport.
    virtual void mouseReleaseEvent(ViewportWindow* vpwin, QMouseEvent* event) override;

    /// Handles mouse move events for a Viewport.
    virtual void mouseMoveEvent(ViewportWindow* vpwin, QMouseEvent* event) override;

    /// Lets the input mode render its overlay content in a viewport.
    virtual void renderOverlay(Viewport* vp, ViewportWindow* vpWin, FrameGraph& frameGraph, DataSet* dataset) override;

protected:

    /// This is called by the system after the input handler has become the active handler.
    virtual void activated(bool temporary) override;

    /// This is called by the system after the input handler is no longer the active handler.
    virtual void deactivated(bool temporary) override;

private:

    /// Aligns the modifier's slicing plane to the three selected points.
    void alignPlane(SliceModifier* mod);

    /// The list of spatial points picked by the user so far.
    Point3 _pickedPoints[3];

    /// The number of points picked so far.
    int _numPickedPoints = 0;

    /// Indicates whether a preliminary point is available.
    bool _hasPreliminaryPoint = false;

    /// The properties editor of the SliceModifier.
    SliceModifierEditor* _editor = nullptr;
};

}   // End of namespace
