// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import Ovito.Qml

// The workbench window of the Qt Quick frontend: a header with the window title and the Import command, the viewport
// area whose panes come from the data set's viewport layout, a right-hand command panel, a status line with the
// progress of the running operations, and the dialogs of the shell.
//
// The pipeline view, the property inspector and the timeline are not implemented yet; see docs/design/UI_PLAN.md for
// the phase they belong to.
Rectangle {
    id: workbench
    color: theme.surfaceWorkbench

    Theme { id: theme }

    // Commands of the shell. The command layer of Phase 3 replaces them with the regular action system; until then these
    // are the commands the shell owes the user, and the ones the design specifies as keyboard shortcuts.
    Shortcut {
        sequences: [StandardKey.Open]
        onActivated: workbenchController.showImportDialog()
    }

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

    // ---------------------------------------------------------------------------------------------
    // Header: the window title and the commands that do not need a data set.
    // ---------------------------------------------------------------------------------------------
    Rectangle {
        id: header
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: theme.headerHeight
        color: theme.surfaceHeader

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 1
            color: theme.borderSubtle
        }

        Text {
            id: titleText
            anchors.left: parent.left
            anchors.leftMargin: theme.spacing * 2
            anchors.verticalCenter: parent.verticalCenter
            anchors.right: importButton.left
            anchors.rightMargin: theme.spacing * 2
            text: workbenchController.windowTitle
            color: theme.textPrimary
            font.pixelSize: theme.fontSize
            font.bold: true
            elide: Text.ElideRight
        }

        Button {
            id: importButton
            anchors.right: parent.right
            anchors.rightMargin: theme.spacing
            anchors.verticalCenter: parent.verticalCenter
            // The first stop of the keyboard focus chain: the command that brings data into the workbench.
            focus: true
            text: qsTr("Import Data…")
            Accessible.name: qsTr("Import data files into the current scene")
            onClicked: workbenchController.showImportDialog()

            palette.button: theme.controlBackground
            palette.buttonText: theme.textPrimary
        }
    }

    // ---------------------------------------------------------------------------------------------
    // Status line at the bottom of the window.
    // ---------------------------------------------------------------------------------------------
    WorkbenchStatusBar {
        id: statusBar
        controller: workbenchController
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
    }

    // ---------------------------------------------------------------------------------------------
    // Central area hosting the viewport panes. The arrangement of the panes - including the handles the user can drag
    // to resize them - follows the layout tree of the current dataset, i.e. the same tree the classic frontend lays out
    // with widgets (see QmlViewportLayout).
    // ---------------------------------------------------------------------------------------------
    Item {
        id: viewportHost
        objectName: "viewportHost"
        anchors.left: parent.left
        anchors.top: header.bottom
        anchors.bottom: statusBar.top
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

            delegate: WorkbenchPane {
                controller: viewportController
                layout: viewportLayout
            }
        }

        // The handles between the panes. They are placed on top of the panes, so that a drag reaches the handle
        // instead of the viewport below it.
        Repeater {
            model: viewportLayout.splitters

            delegate: WorkbenchSplitter {
                layout: viewportLayout
            }
        }

        // Empty state: while the scene has no object to display, the viewports only show the construction grid, so the
        // shell offers the one command that can change that.
        Rectangle {
            id: emptyState
            anchors.fill: parent
            visible: !workbenchController.hasData && !dropArea.containsDrag
            color: theme.surfaceOverlay
            Accessible.role: Accessible.Grouping

            Column {
                anchors.centerIn: parent
                spacing: theme.spacing * 2

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("No data loaded")
                    color: theme.textPrimary
                    font.pixelSize: theme.fontSize + 4
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("Import a simulation file, or drop one onto this window.")
                    color: theme.textSecondary
                    font.pixelSize: theme.fontSize
                }

                Button {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("Import Data…")
                    Accessible.name: qsTr("Import data files into the current scene")
                    onClicked: workbenchController.showImportDialog()

                    palette.button: theme.controlBackground
                    palette.buttonText: theme.textPrimary
                }
            }
        }
    }

    // ---------------------------------------------------------------------------------------------
    // Right-hand command panel.
    // ---------------------------------------------------------------------------------------------
    Rectangle {
        id: commandPanel
        anchors.right: parent.right
        anchors.top: header.bottom
        anchors.bottom: statusBar.top
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

    // ---------------------------------------------------------------------------------------------
    // Files can be dropped onto the window, which imports them the same way the file dialog and the command line do.
    // ---------------------------------------------------------------------------------------------
    DropArea {
        id: dropArea
        anchors.fill: parent
        onDropped: (drop) => {
            if(drop.hasUrls)
                workbenchController.importFiles(drop.urls)
            drop.acceptProposedAction()
        }

        Rectangle {
            anchors.fill: parent
            visible: dropArea.containsDrag
            color: theme.surfaceOverlay
            border.color: theme.accentPrimary
            border.width: 2

            Text {
                anchors.centerIn: parent
                text: qsTr("Drop the files to import them")
                color: theme.textPrimary
                font.pixelSize: theme.fontSize + 2
            }
        }
    }

    // ---------------------------------------------------------------------------------------------
    // Dialogs. They are opened by the frontend through the workbench controller, so that the file dialog and the
    // message dialog are the only places that know how this frontend asks the user a question.
    // ---------------------------------------------------------------------------------------------
    FileDialog {
        id: importDialog
        title: qsTr("Import Data")
        fileMode: FileDialog.OpenFiles
        onAccepted: workbenchController.importFiles(selectedFiles)
    }

    WorkbenchMessageBox {
        id: messageBox
        controller: workbenchController
    }

    Connections {
        target: workbenchController

        function onImportDialogRequested(directoryUrl) {
            if(directoryUrl.toString().length > 0)
                importDialog.currentFolder = directoryUrl
            importDialog.open()
        }
    }
}
