// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

import QtQuick
import QtQuick.Controls
import Ovito.Qml

// The message dialog of the workbench shell. The frontend blocks in a nested event loop while this dialog is open (like
// the modal dialogs of the desktop frontend), so it has to be presentable for every message the shared workbench code
// reports, including the buttons the caller asked for.
Dialog {
    id: messageBox

    required property WorkbenchController controller

    Theme { id: theme }

    title: controller.messageBoxTitle
    modal: true
    closePolicy: Popup.CloseOnEscape
    padding: theme.spacing * 2
    anchors.centerIn: Overlay.overlay
    width: Math.min(560, Overlay.overlay ? Overlay.overlay.width - 4 * theme.spacing : 560)
    height: Math.min(implicitHeight, Overlay.overlay ? Overlay.overlay.height - 4 * theme.spacing : implicitHeight)
    visible: controller.messageBoxVisible

    palette.window: theme.surfacePanel
    palette.windowText: theme.textPrimary
    palette.text: theme.textPrimary
    palette.button: theme.controlBackground
    palette.buttonText: theme.textPrimary

    // Answering with a button closes the dialog; dismissing it (Escape or the close button) means the default answer.
    // Both paths go through the controller, which is what the blocked frontend waits for.
    onClosed: controller.answerMessageBox(controller.defaultMessageBoxButton)

    Column {
        anchors.fill: parent
        spacing: theme.spacing * 2

        Row {
            width: parent.width
            spacing: theme.spacing * 2

            // The icon of the message, drawn as a colored box with a glyph: the shell ships no icon assets yet.
            Rectangle {
                id: iconBox
                visible: controller.messageBoxIcon !== 0
                width: 24
                height: 24
                radius: 3
                color: controller.messageBoxIcon === 3 ? theme.accentError
                     : controller.messageBoxIcon === 2 ? theme.accentWarning
                     : theme.accentPrimary
                Text {
                    anchors.centerIn: parent
                    text: controller.messageBoxIcon === 1 ? "i" : "!"
                    color: theme.textOnAccent
                    font.pixelSize: theme.fontSize + 2
                    font.bold: true
                }
            }

            Text {
                id: messageText
                width: parent.width - (iconBox.visible ? iconBox.width + parent.spacing : 0)
                text: controller.messageBoxText
                color: theme.textPrimary
                font.pixelSize: theme.fontSize
                wrapMode: Text.WordWrap
                // Long error texts (with a traceback) must not push the buttons off the screen.
                maximumLineCount: 20
                elide: Text.ElideRight
            }
        }

        Row {
            id: buttons
            anchors.right: parent.right
            spacing: theme.spacing

            Repeater {
                model: controller.messageBoxButtons

                delegate: Button {
                    required property var modelData
                    text: modelData.text
                    focus: modelData.isDefault
                    highlighted: modelData.isDefault
                    Accessible.name: modelData.text
                    onClicked: controller.answerMessageBox(modelData.button)

                    palette.button: highlighted ? theme.accentPrimary : theme.controlBackground
                    palette.buttonText: highlighted ? theme.textOnAccent : theme.textPrimary
                }
            }
        }
    }
}
