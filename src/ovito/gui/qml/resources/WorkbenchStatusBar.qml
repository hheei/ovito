// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

import QtQuick
import QtQuick.Controls
import Ovito.Qml

// The status line at the bottom of the workbench window: the message the frontend last reported, and the progress of the
// operations that are running.
//
// One row is shown per running task, which comes from the workbench's shared task progress model - the same model the
// classic frontend's status bar reads. Presenting one row per task is new here: the classic status bar shows a single
// aggregate bar. Cancelling stays at the level of the operation the shell started on behalf of the user (currently the
// import), because a task's progress record carries no handle to the task itself.
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
        text: statusBar.controller.statusMessage.length > 0 ? statusBar.controller.statusMessage
            : (statusBar.controller.importNotice.length > 0 ? statusBar.controller.importNotice : qsTr("Ready"))
        color: theme.textSecondary
        font.pixelSize: theme.fontSize
        elide: Text.ElideRight
    }

    Column {
        id: taskArea
        anchors.right: parent.right
        anchors.rightMargin: theme.spacing
        anchors.verticalCenter: parent.verticalCenter
        spacing: 2
        visible: taskProgress.count > 0

        Repeater {
            id: taskRows
            objectName: "taskProgressRows"
            model: taskProgress

            delegate: Row {
                required property string text
                required property int value
                required property int maximum

                spacing: theme.spacing

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    visible: parent.text.length > 0
                    text: parent.text
                    color: theme.textSecondary
                    font.pixelSize: theme.fontSize
                    elide: Text.ElideRight
                    width: Math.min(implicitWidth, 320)
                }

                ProgressBar {
                    anchors.verticalCenter: parent.verticalCenter
                    // An operation that does not know its extent shows an indeterminate bar.
                    indeterminate: parent.maximum <= 0
                    from: 0
                    to: Math.max(1, parent.maximum)
                    value: parent.value
                    Accessible.name: qsTr("Progress of the running operation")

                    palette.highlight: theme.accentPrimary
                    palette.base: theme.controlDisabledBackground
                    palette.text: theme.textSecondary
                }
            }
        }

        Button {
            anchors.right: parent.right
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
