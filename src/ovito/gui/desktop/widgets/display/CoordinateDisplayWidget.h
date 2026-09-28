// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/desktop/widgets/general/SpinnerWidget.h>

namespace Ovito {

/**
 * The coordinate display widget at the bottom of the main window,
 * which displays the current mouse coordinates and the transform of the selected object.
 */
class OVITO_GUI_EXPORT CoordinateDisplayWidget : public QFrame, public UserInterfaceComponent<MainWindowUI>
{
    Q_OBJECT

public:

    /// Constructor.
    CoordinateDisplayWidget(MainWindowUI& ui, QWidget* parent = nullptr);

    /// Shows the coordinate display widget.
    void activate(const QString& undoOperationName);

    /// Deactivates the coordinate display widget.
    void deactivate();

    /// Sets the values displayed by the coordinate display widget.
    void setValues(const Vector3& xyz) {
        if(!_spinners[0]->isDragging()) _spinners[0]->setFloatValue(xyz.x());
        if(!_spinners[1]->isDragging()) _spinners[1]->setFloatValue(xyz.y());
        if(!_spinners[2]->isDragging()) _spinners[2]->setFloatValue(xyz.z());
    }

    /// Returns the values displayed by the coordinate display widget.
    Vector3 getValues() const {
        return Vector3(
                _spinners[0]->floatValue(),
                _spinners[1]->floatValue(),
                _spinners[2]->floatValue());
    }

    /// Sets the units of the displayed values.
    void setUnit(ParameterUnit* unit) {
        _spinners[0]->setUnit(unit);
        _spinners[1]->setUnit(unit);
        _spinners[2]->setUnit(unit);
    }

Q_SIGNALS:

    /// This signal is emitted when the user has changed the value of one of the vector components.
    void valueEntered(int component, FloatType value);

    /// This signal is emitted when the user presses the "Animate transformation" button.
    void animatePressed();

protected Q_SLOT:

    /// Is called when a spinner value has been changed by the user.
    void onSpinnerValueChanged();

private:

    SpinnerWidget* _spinners[3];
};

}   // End of namespace
