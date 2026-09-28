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

    // Workbench-wide keyboard commands. The command layer of Phase 3 replaces them with the regular action system;
    // until then, undo/redo and maximizing the active viewport are the commands the shell owes the user.
    Shortcut {
        sequences: [StandardKey.Undo]
        enabled: viewportController.canUndo
        onActivated: viewportController.undo()
    }

    Shortcut {
        sequences: [StandardKey.Redo]
        enabled: viewportController.canRedo
        onActivated: viewportController.redo()
    }

    Shortcut {
        sequence: "Ctrl+Shift+M"
        enabled: viewportLayout.maximizable
        onActivated: viewportLayout.toggleMaximize(viewportLayout.activeViewportIndex)
    }

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

    // Central area hosting the viewport panes. The arrangement of the panes - including the handles the user can drag
    // to resize them - follows the layout tree of the current dataset, i.e. the same tree the classic frontend lays out
    // with widgets (see QmlViewportLayout).
    Item {
        id: viewportHost
        objectName: "viewportHost"
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: statusLine.top
        anchors.right: commandPanel.left
        anchors.margins: theme.spacing
        anchors.bottomMargin: theme.spacing

        // The layout model can only place the panes once it knows the size of the area they live in.
        onWidthChanged: viewportLayout.setPanelSize(width, height)
        onHeightChanged: viewportLayout.setPanelSize(width, height)
        Component.onCompleted: viewportLayout.setPanelSize(width, height)

        // One pane per viewport of the current dataset. A viewport item belongs to the viewport of one dataset, so the
        // panes are created anew whenever the frontend reports a different set of viewports.
        Repeater {
            model: viewportLayout.panes

            delegate: ViewportPane {
                controller: viewportController
                layout: viewportLayout
            }
        }

        // The handles between the panes. They are placed on top of the panes, so that a drag reaches the handle
        // instead of the viewport below it.
        Repeater {
            model: viewportLayout.splitters

            delegate: ViewportSplitter {
                layout: viewportLayout
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
