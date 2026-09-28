// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/gui/base/GUIBase.h>
#include <ovito/core/app/UserInterface.h>
#include <ovito/gui/base/viewport/ViewportInputManager.h>
#include "ViewportModeAction.h"

namespace Ovito {

/******************************************************************************
* Initializes the action object.
******************************************************************************/
ViewportModeAction::ViewportModeAction(UserInterface& ui, const QString& text, QObject* parent, OORef<ViewportInputMode> inputMode, const QColor& highlightColor) :
    QAction(text, parent),
    UserInterfaceComponent<UserInterface>(ui),
    _inputMode(std::move(inputMode)),
    _highlightColor(highlightColor)
{
    OVITO_CHECK_POINTER(ui.viewportInputManager());

    setCheckable(true);
    setChecked(_inputMode->isActive());

    connect(_inputMode.get(), &ViewportInputMode::statusChanged, this, &ViewportModeAction::setChecked);
    connect(this, &ViewportModeAction::toggled, this, &ViewportModeAction::onActionToggled);
    connect(this, &ViewportModeAction::triggered, this, &ViewportModeAction::onActionTriggered);
}

/******************************************************************************
* Is called when the user or the program have triggered the action's state.
******************************************************************************/
void ViewportModeAction::onActionToggled(bool checked)
{
    if(ViewportInputManager* inputManager = ui().viewportInputManager()) {
        // Activate/deactivate the input mode.
        if(checked && !_inputMode->isActive()) {
            inputManager->pushInputMode(_inputMode);
            // Give viewport windows the input focus.
            ui().setViewportInputFocus();
        }
        else if(!checked) {
            if(inputManager->activeMode() == _inputMode && _inputMode->modeType() == ViewportInputMode::ExclusiveMode) {
                // Make sure that an exclusive input mode cannot be deactivated by the user.
                setChecked(true);
            }
        }
    }
}

/******************************************************************************
* Is called when the user has triggered the action's state.
******************************************************************************/
void ViewportModeAction::onActionTriggered(bool checked)
{
    if(ViewportInputManager* inputManager = ui().viewportInputManager()) {
        if(!checked) {
            if(_inputMode->modeType() != ViewportInputMode::ExclusiveMode) {
                inputManager->removeInputMode(_inputMode);
            }
        }
    }
}

}   // End of namespace
