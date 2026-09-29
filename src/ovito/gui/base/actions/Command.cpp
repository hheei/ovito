// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/gui/base/GUIBase.h>
#include <ovito/core/app/UserInterface.h>
#include <ovito/gui/base/viewport/ViewportInputManager.h>
#include <ovito/gui/base/viewport/ViewportInputMode.h>
#include "Command.h"

namespace Ovito {

/******************************************************************************
* Constructs a new command.
******************************************************************************/
Command::Command(const QString& id, const QString& text, const QString& iconPath, const QString& statusTip, const QKeySequence& shortcut, QObject* parent) :
    QObject(parent),
    _id(id),
    _text(text),
    _statusTip(statusTip),
    _iconPath(iconPath),
    _shortcut(shortcut)
{
    OVITO_ASSERT_MSG(!id.isEmpty(), "Command::Command()", "A command must have a non-empty identifier.");
}

/******************************************************************************
* Returns the tool tip of this command, which is its title plus its shortcut.
******************************************************************************/
QString Command::toolTip() const
{
    if(!_toolTip.isEmpty())
        return _toolTip;
    if(_shortcut.isEmpty())
        return _text;
    return QStringLiteral("%1 [%2]").arg(_text).arg(_shortcut.toString(QKeySequence::NativeText));
}

/******************************************************************************
* Overrides the tool tip of this command.
******************************************************************************/
void Command::setToolTip(const QString& toolTip)
{
    if(_toolTip == toolTip)
        return;
    _toolTip = toolTip;
    Q_EMIT changed();
}

/******************************************************************************
* Sets the title of this command.
******************************************************************************/
void Command::setText(const QString& text)
{
    if(_text == text)
        return;
    _text = text;
    Q_EMIT changed();
}

/******************************************************************************
* Sets the status-bar text of this command.
******************************************************************************/
void Command::setStatusTip(const QString& statusTip)
{
    if(_statusTip == statusTip)
        return;
    _statusTip = statusTip;
    Q_EMIT changed();
}

/******************************************************************************
* Sets the icon path of this command.
******************************************************************************/
void Command::setIconPath(const QString& iconPath)
{
    if(_iconPath == iconPath)
        return;
    _iconPath = iconPath;
    Q_EMIT changed();
}

/******************************************************************************
* Sets the default shortcut of this command.
******************************************************************************/
void Command::setShortcut(const QKeySequence& shortcut)
{
    if(_shortcut == shortcut)
        return;
    _shortcut = shortcut;
    Q_EMIT changed();
}

/******************************************************************************
* Sets the accent color of the control that presents this command.
******************************************************************************/
void Command::setHighlightColor(const QColor& color)
{
    if(_highlightColor == color)
        return;
    _highlightColor = color;
    Q_EMIT changed();
}

/******************************************************************************
* Turns this command into a checkable one.
******************************************************************************/
void Command::setCheckable(bool checkable)
{
    if(_checkable == checkable)
        return;
    _checkable = checkable;
    Q_EMIT changed();
}

/******************************************************************************
* Sets the check state of this command.
******************************************************************************/
void Command::setChecked(bool checked)
{
    if(!_checkable)
        checked = false;
    if(_checked == checked)
        return;
    _checked = checked;
    Q_EMIT changed();
    Q_EMIT toggled(_checked);
}

/******************************************************************************
* Enables or disables this command.
******************************************************************************/
void Command::setEnabled(bool enabled)
{
    if(_enabled == enabled)
        return;
    _enabled = enabled;
    Q_EMIT changed();
}

/******************************************************************************
* Controls whether this command is presented to the user.
******************************************************************************/
void Command::setVisible(bool visible)
{
    if(_visible == visible)
        return;
    _visible = visible;
    Q_EMIT changed();
}

/******************************************************************************
* Invokes this command, i.e. runs the operation the user has asked for.
******************************************************************************/
void Command::trigger()
{
    if(!_enabled)
        return;
    Q_EMIT triggered();
}

/******************************************************************************
* Constructs a new command that activates a viewport input mode.
******************************************************************************/
ViewportModeCommand::ViewportModeCommand(UserInterface& ui, const QString& id, const QString& text, OORef<ViewportInputMode> inputMode, const QColor& highlightColor, const QString& iconPath, const QString& statusTip, const QKeySequence& shortcut, QObject* parent) :
    Command(id, text, iconPath, statusTip, shortcut, parent),
    UserInterfaceComponent<UserInterface>(ui),
    _inputMode(std::move(inputMode))
{
    OVITO_CHECK_POINTER(_inputMode);
    OVITO_CHECK_POINTER(ui.viewportInputManager());

    setHighlightColor(highlightColor);
    setCheckable(true);
    setChecked(_inputMode->isActive());

    // The input mode is the source of truth for the check state of this command.
    connect(_inputMode.get(), &ViewportInputMode::statusChanged, this, &Command::setChecked);
}

/******************************************************************************
* Invokes the command by activating or deactivating the input mode.
******************************************************************************/
void ViewportModeCommand::trigger()
{
    setModeActive(!_inputMode->isActive());
    Command::trigger();
}

/******************************************************************************
* Activates or deactivates the input mode.
******************************************************************************/
void ViewportModeCommand::setModeActive(bool active)
{
    ViewportInputManager* inputManager = ui().viewportInputManager();
    if(!inputManager)
        return;

    if(active) {
        if(!_inputMode->isActive()) {
            inputManager->pushInputMode(_inputMode);
            // Give viewport windows the input focus.
            ui().setViewportInputFocus();
        }
    }
    else if(_inputMode->isActive() && _inputMode->modeType() != ViewportInputMode::ExclusiveMode) {
        // An exclusive input mode cannot be turned off by the user.
        inputManager->removeInputMode(_inputMode);
    }

    // The input mode is the source of truth for the check state, but it does not report every no-op
    // (for example when the user tries to deactivate an exclusive mode), so re-assert the state here.
    setChecked(_inputMode->isActive());
}

}   // End of namespace
