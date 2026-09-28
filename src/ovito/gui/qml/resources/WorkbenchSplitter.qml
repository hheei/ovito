// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

import QtQuick
import Ovito.Qml

// The handle between two panes of the workbench's viewport area. Dragging it changes the relative sizes of the two
// panes; the layout model records one undoable operation per drag, so a single Undo restores the sizes from before
// the drag. Double-clicking the handle distributes the panes evenly again.
Item {
    id: handleItem

    /// The handle object the layout model's list supplies for this delegate.
    required property var modelData

    /// The handle of the viewport layout this item displays.
    readonly property ViewportSplitter splitter: modelData

    /// The layout model, which performs the drag and knows the pane sizes.
    required property ViewportLayout layout

    /// Position of the handle in the layout model's list of handles.
    required property int index

    Theme { id: theme }

    x: splitter.x
    y: splitter.y
    width: Math.max(splitter.width, 0)
    height: Math.max(splitter.height, 0)

    // The visible divider line, centered in the gap the layout reserves between two panes.
    Rectangle {
        anchors.centerIn: parent
        width: handleItem.splitter.horizontal ? 1 : handleItem.width
        height: handleItem.splitter.horizontal ? handleItem.height : 1
        color: (handleArea.containsMouse || handleArea.pressed) ? theme.accentPrimary : theme.borderSubtle
    }

    MouseArea {
        id: handleArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: handleItem.splitter.horizontal ? Qt.SplitHCursor : Qt.SplitVCursor

        // The pointer position is reported in the coordinate system of the viewport area: the handle sits exactly where
        // the layout model placed it, so its position plus the local pointer position is the position the model expects.
        // While a drag is in progress both the handle and the pointer move, which keeps the sum equal to the pointer
        // position - even when the pointer leaves the handle, because the mouse area keeps tracking it.
        function panelPosition(mouse) {
            return Qt.point(handleItem.splitter.x + mouse.x, handleItem.splitter.y + mouse.y)
        }

        onPressed: (mouse) => {
            const position = panelPosition(mouse)
            handleItem.layout.beginSplitterDrag(handleItem.index, position.x, position.y)
        }
        onPositionChanged: (mouse) => {
            if(handleArea.pressed) {
                const position = panelPosition(mouse)
                handleItem.layout.dragSplitter(position.x, position.y)
            }
        }
        onReleased: handleItem.layout.endSplitterDrag()
        onCanceled: handleItem.layout.cancelSplitterDrag()
        onDoubleClicked: handleItem.layout.resetSplitter(handleItem.index)
    }
}
