// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

import QtQuick
import QtQuick.Controls
import Ovito.Qml

// The status line at the bottom of the workbench window: the message the frontend last reported, and the progress of
// the operations that are running. The progress of a task comes from the workbench's task progress model, which is the
// same data the status bar of the classic frontend displays. The Cancel command cancels the operation the shell started on behalf of the user
// (currently the import), which is the same operation the classic frontend's progress dialog offers to cancel.
Item {
    id: statusBar

    required property WorkbenchController controller

    Theme { id: theme }

    implicitHeight: Math.max(statusText.implicitHeight, taskArea.implicitHeight, 24) + theme.spacing

    Text {
        id: statusText
        anchors.left: parent.left
        anchors.leftMargin: theme.spacing
        anchors.verticalCenter: parent.verticalCenter
        anchors.right: taskArea.left
        anchors.rightMargin: theme.spacing
        text: statusBar.controller.statusMessage.length > 0 ? statusBar.controller.statusMessage : qsTr("Ready")
        color: theme.textSecondary
        font.pixelSize: theme.fontSize
        elide: Text.ElideRight
    }

    Row {
        id: taskArea
        anchors.right: parent.right
        anchors.rightMargin: theme.spacing
        anchors.verticalCenter: parent.verticalCenter
        spacing: theme.spacing
        visible: taskProgress.busy

        Text {
            anchors.verticalCenter: parent.verticalCenter
            visible: text.length > 0
            text: taskProgress.text
            color: theme.textSecondary
            font.pixelSize: theme.fontSize
            elide: Text.ElideRight
            width: Math.min(implicitWidth, 320)
        }

        ProgressBar {
            anchors.verticalCenter: parent.verticalCenter
            // An operation that does not know its extent shows an indeterminate bar.
            indeterminate: taskProgress.maximum <= 0
            from: 0
            to: Math.max(1, taskProgress.maximum)
            value: taskProgress.value
            Accessible.name: qsTr("Import progress")

            palette.highlight: theme.accentPrimary
            palette.base: theme.controlDisabledBackground
            palette.text: theme.textSecondary
        }

        Button {
            anchors.verticalCenter: parent.verticalCenter
            visible: statusBar.controller.cancellable
            // While the operation is winding down, its cancellation cannot be requested a second time.
            enabled: !statusBar.controller.cancelling
            text: statusBar.controller.cancelling ? qsTr("Cancelling…") : qsTr("Cancel")
            Accessible.name: qsTr("Cancel the running operation")
            onClicked: statusBar.controller.cancelCurrentOperation()

            palette.button: theme.controlBackground
            palette.buttonText: enabled ? theme.textPrimary : theme.textDisabled
        }
    }
}
