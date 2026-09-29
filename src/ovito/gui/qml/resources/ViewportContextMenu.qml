// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

import QtQuick
import QtQuick.Controls
import Ovito.Qml

// The context menu of a viewport, shown when the user clicks the caption of a pane.
//
// The classic frontend's counterpart is src/ovito/gui/desktop/viewport/ViewportMenu.cpp; the entries it can already
// perform go through the viewport API and the shared commands (the model is QmlViewportMenu), and the entries whose
// feature has not been migrated yet are placeholders that name their phase.
//
// The check marks of the entries belong to the viewport, not to the menu: a MenuItem toggles its own 'checked' state
// when it is clicked, which would leave a stale mark behind, so every checkable entry re-reads the state from the model
// whenever the model reports a change.
Menu {
    id: contextMenu
    objectName: "viewportContextMenu"
    title: qsTr("Viewport")

    // The menu belongs to the viewport that was clicked, so it closes when the model is reset (for example when the
    // data set, and with it the viewport, is replaced).
    onClosed: viewportMenu.close()

    Connections {
        target: viewportMenu

        function onOpened(item, x, y) {
            contextMenu.popup(item, x, y)
        }

        // The model closes itself when its viewport goes away (the items are rebuilt when the data set or the layout
        // changes); a menu that outlives its viewport would offer entries that act on nothing.
        function onChanged() {
            if(!viewportMenu.open)
                contextMenu.close()
        }
    }

    WorkbenchPlaceholderMenuItem {
        itemText: qsTr("Preview Mode")
        ownerPhase: "Phase 5"
    }

    MenuItem {
        id: gridItem
        text: qsTr("Show Grid")
        checkable: true
        onTriggered: viewportMenu.setGridVisible(!viewportMenu.gridVisible)

        function syncChecked() { checked = viewportMenu.gridVisible }
        Component.onCompleted: syncChecked()
        Connections {
            target: viewportMenu
            function onChanged() { gridItem.syncChecked() }
        }
    }

    MenuItem {
        id: constrainItem
        text: qsTr("Constrain Rotation")
        checkable: true
        onTriggered: viewportMenu.setConstrainRotation(!viewportMenu.constrainRotation)

        function syncChecked() { checked = viewportMenu.constrainRotation }
        Component.onCompleted: syncChecked()
        Connections {
            target: viewportMenu
            function onChanged() { constrainItem.syncChecked() }
        }
    }

    MenuSeparator {}

    Menu {
        id: viewTypeMenu
        title: qsTr("View Type")

        Repeater {
            model: viewportMenu.viewTypes

            delegate: MenuItem {
                id: viewTypeItem
                required property var modelData

                text: modelData.text
                checkable: true
                onTriggered: viewportMenu.setViewType(modelData.value)

                function syncChecked() { checked = (viewportMenu.viewType === modelData.value) }
                Component.onCompleted: syncChecked()
                Connections {
                    target: viewportMenu
                    function onChanged() { viewTypeItem.syncChecked() }
                }
            }
        }

        MenuSeparator {}

        WorkbenchPlaceholderMenuItem {
            objectName: "createCameraItem"
            itemText: qsTr("Create Camera")
            ownerPhase: "Phase 4"
        }
    }

    WorkbenchPlaceholderMenuItem {
        itemText: qsTr("Adjust View…")
        ownerPhase: "Phase 5"
    }

    MenuSeparator {}

    // Maximizing is not part of the classic viewport menu (it has a button on the viewport) but belongs here as well:
    // the command is shared, so the entry and the pane's button are the same command.
    MenuItem {
        id: maximizeItem
        text: viewportMenu.maximized ? qsTr("Restore Viewport") : qsTr("Maximize Viewport")
        enabled: viewportLayout.maximizable
        onTriggered: viewportMenu.toggleMaximize()
    }

    MenuSeparator {}

    WorkbenchPlaceholderMenuItem {
        itemText: qsTr("Window Layout")
        ownerPhase: "Phase 4"
    }

    WorkbenchPlaceholderMenuItem {
        itemText: qsTr("Pipeline Visibility")
        ownerPhase: "Phase 4"
    }

    MenuSeparator {}

    WorkbenchPlaceholderMenuItem {
        itemText: qsTr("Configure Viewport Graphics…")
        ownerPhase: "Phase 7"
    }
}
