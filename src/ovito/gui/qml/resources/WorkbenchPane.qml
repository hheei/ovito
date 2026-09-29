// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

import QtQuick
import Ovito.Qml

// One pane of the workbench's viewport area (the view of a ViewportPane object of the layout model). The layout model supplies the geometry and the state of the pane
// (see QmlViewportPane); the pane itself holds the viewport item the frontend creates inside it, which renders the
// OVITO scene of one viewport.
Item {
    id: paneItem

    /// The pane object the layout model's list supplies for this delegate.
    required property var modelData

    /// The pane of the viewport layout this item displays.
    readonly property ViewportPane pane: modelData

    /// The workbench controller that creates and owns the viewport items.
    required property ViewportController controller

    /// The layout model, which the maximize button talks to.
    required property ViewportLayout layout

    Theme { id: theme }

    x: pane.x
    y: pane.y
    width: Math.max(pane.width, 0)
    height: Math.max(pane.height, 0)

    // The pane is visible whenever the layout says so; a pane of a hidden viewport stays in the scene so that its
    // viewport item survives maximizing and releasing the keyboard focus, but it does not show anything.
    visible: pane.visible

    /// The viewport item rendering the scene of this pane, if it has been created already.
    property var viewportItem: null

    // A viewport item belongs to the viewport of one dataset, so the item is created from the viewport index the layout
    // model reports for this pane.
    //
    // The item is created while the workbench window is being loaded, i.e. before a data set is imported. That is when
    // it matters: the importer asks the viewports to zoom to the scene extents, and that request only reaches viewport
    // windows that exist at that moment. The layout model therefore reports the panes as soon as it knows the layout,
    // even before the QML scene has reported the size of the viewport area; the item starts out with the size the pane
    // has at that point and is resized as soon as the pane geometry arrives.
    function bindViewport(recreate) {
        if(viewportItem && !recreate)
            return
        viewportItem = controller.createViewportItem(paneItem, pane.viewportIndex)
    }

    Component.onCompleted: bindViewport(false)
    onWidthChanged: bindViewport(false)
    onHeightChanged: bindViewport(false)

    Connections {
        target: paneItem.controller
        function onViewportConfigurationChanged() { paneItem.bindViewport(true) }
    }

    Connections {
        target: paneItem.pane
        function onViewportIndexChanged() { paneItem.bindViewport(true) }
    }

    // Marks the active viewport, i.e. the pane the viewport commands and the keyboard navigation apply to.
    Rectangle {
        z: 2
        anchors.fill: parent
        color: "transparent"
        border.width: 1
        border.color: paneItem.pane.active ? theme.borderFocus : "transparent"
    }

    // Maximize/restore button. It fades in while the pointer is over the pane; a maximized pane keeps it visible,
    // because that is the way back to the full layout. The button stays hoverable while it is transparent, which is why
    // it is hidden through the opacity rather than through the visible flag.
    Rectangle {
        id: maximizeButton

        z: 3
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: 4
        width: 20
        height: 20
        radius: 3
        visible: paneItem.pane.maximizable
        opacity: (maximizeArea.containsMouse || paneItem.pane.maximized) ? 1 : 0
        color: maximizeArea.containsMouse ? theme.surfaceSelected : Qt.rgba(0, 0, 0, 0.4)
        border.width: 1
        border.color: theme.borderSubtle

        Behavior on opacity { NumberAnimation { duration: 100 } }

        // The icons come from the shared icon set of the two frontends (Icons, see IconTheme in gui/base), in the theme
        // that matches the current color scheme - the same arrow-icon pair the classic frontend uses for its viewport
        // maximize command, plus the counterpart the shell needs for the way back.
        Image {
            anchors.centerIn: parent
            source: Icons.url(paneItem.pane.maximized ? "viewport_restore" : "viewport_maximize")
            sourceSize.width: 16
            sourceSize.height: 16
            fillMode: Image.PreserveAspectFit
            // The image provider of the icons has the same lifetime as the engine, so the icon is there; a missing one
            // is reported instead of silently leaving an empty button.
            onStatusChanged: if(status === Image.Error)
                console.warn("The maximize button could not load its icon from the shared icon set")
            Accessible.ignored: true
        }

        MouseArea {
            id: maximizeArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: paneItem.layout.toggleMaximize(paneItem.pane.viewportIndex)

            Accessible.role: Accessible.Button
            Accessible.name: paneItem.pane.maximized ? qsTr("Restore viewport size") : qsTr("Maximize viewport")
        }
    }
}
