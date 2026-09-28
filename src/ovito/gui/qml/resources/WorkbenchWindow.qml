// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

import QtQuick

// Minimal prototype of the OVITO modern workbench: a central viewport area, a right-hand command panel
// and a status line. The pipeline view, the property inspector and the timeline are not implemented yet;
// see docs/design/UI_PLAN.md for the phase they belong to.
Rectangle {
    id: workbench
    color: theme.surfaceWorkbench

    Theme { id: theme }

    // Status line at the bottom of the window.
    Text {
        id: statusLine
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: theme.spacing
        anchors.rightMargin: theme.spacing
        anchors.bottomMargin: theme.spacing
        height: implicitHeight
        text: viewportController.statusMessage.length > 0 ? viewportController.statusMessage : qsTr("Ready")
        color: theme.textSecondary
        font.pixelSize: theme.fontSize
        elide: Text.ElideRight
    }

    // Central area hosting the 3D viewport items created by the C++ frontend.
    // The prototype uses a 2x2 grid, mirroring the four default viewports of a new OVITO dataset.
    Item {
        id: viewportHost
        objectName: "viewportHost"
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: statusLine.top
        anchors.right: commandPanel.left
        anchors.margins: theme.spacing
        anchors.bottomMargin: theme.spacing

        Grid {
            id: viewportGrid
            anchors.fill: parent
            columns: 2
            spacing: theme.spacing

            Repeater {
                model: 4

                Item {
                    width: Math.round((viewportGrid.width - viewportGrid.spacing) / 2)
                    height: Math.round((viewportGrid.height - viewportGrid.spacing) / 2)
                    Component.onCompleted: viewportController.createViewportItem(this)
                }
            }
        }
    }

    // Right-hand command panel.
    Rectangle {
        id: commandPanel
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: statusLine.top
        anchors.margins: theme.spacing
        width: theme.panelWidth
        color: theme.surfacePanel
        border.color: theme.borderSubtle
        border.width: 1

        Column {
            anchors.fill: parent
            anchors.margins: theme.spacing
            spacing: theme.spacing

            Text {
                text: qsTr("Pipeline")
                color: theme.textPrimary
                font.pixelSize: theme.fontSize + 1
                font.bold: true
            }

            Text {
                width: parent.width
                wrapMode: Text.WordWrap
                text: qsTr("The pipeline view comes with Phase 4 of the migration plan.")
                color: theme.textSecondary
                font.pixelSize: theme.fontSize
            }

            Rectangle {
                width: parent.width
                height: 1
                color: theme.borderSubtle
            }

            Text {
                text: qsTr("Properties")
                color: theme.textPrimary
                font.pixelSize: theme.fontSize + 1
                font.bold: true
            }

            Text {
                width: parent.width
                wrapMode: Text.WordWrap
                text: qsTr("The parameter inspector comes with Phases 4 and 6 of the migration plan.")
                color: theme.textSecondary
                font.pixelSize: theme.fontSize
            }
        }
    }
}
