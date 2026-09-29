// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

import QtQuick
import QtQuick.Controls

import Ovito.Qml

// A menu entry that presents one command of the shared command layer.
//
// The command owns the text, the enabled state and the checked state, so the entry is only a view of it - exactly like
// the QAction the classic frontend offers the same command through. Two consequences are worth knowing:
//
//  * the entry does not install the command's shortcut. The shell installs every shortcut once (see WorkbenchWindow.qml),
//    and a second owner of the same QKeySequence would carry out the command twice.
//  * a MenuItem toggles its own 'checked' state when it is clicked, which would replace the binding to the command with a
//    stale value. The state is therefore re-read from the command whenever the command reports a change.
MenuItem {
    id: menuItem

    /// The command this entry presents, or null if the frontend does not provide it.
    property Command command

    text: command ? command.text : ""
    // The icon of the command comes from the shared icon set, the same one the QAction of the classic frontend shows;
    // commands without an icon simply leave the entry without one. The theme is passed in so that this binding is
    // re-evaluated when the shell switches between the light and the dark icon theme.
    icon.source: (command && command.iconPath.length) ? Icons.url(command.iconPath, Icons.themeName) : ""
    enabled: command ? command.enabled : false
    visible: command ? command.visible : false
    checkable: command ? command.checkable : false
    checked: command ? command.checked : false

    onTriggered: {
        if(command)
            commandManager.triggerCommand(command.id)
        // The item has just toggled its own 'checked' state, which replaces the binding to the command. Not every
        // command changes its state when it is carried out - an active exclusive viewport mode stays active - so the
        // state is read back from the command in any case.
        syncChecked()
    }

    function syncChecked() {
        if(command)
            checked = command.checked
    }

    Component.onCompleted: syncChecked()

    Connections {
        target: menuItem.command
        function onChanged() { menuItem.syncChecked() }
    }
}
