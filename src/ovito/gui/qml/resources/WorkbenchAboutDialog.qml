// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

import QtQuick
import QtQuick.Controls

import Ovito.Qml

// The About dialog of the workbench, which the Help menu opens through the shared About command.
//
// The classic frontend shows a message box with the application name, the version and the copyright notice
// (WidgetActionManager::on_HelpAbout_triggered). This dialog carries the same information; on macOS it is also the only
// way to reach About, because Qt draws the menu bar of this shell inside the window instead of in the global menu.
Dialog {
    id: aboutDialog
    objectName: "aboutDialog"

    anchors.centerIn: parent
    modal: true
    title: qsTr("About %1").arg(workbenchController.applicationName)
    standardButtons: Dialog.Ok

    onOpened: {
        if(detailsText) {
            detailsText.forceActiveFocus()
            detailsText.cursorPosition = 0
        }
    }

    Column {
        spacing: theme.spacing * 2

        // The build version and the copyright notice are long single lines; the dialog must stay inside the workbench
        // window (whose minimum width is 640 device-independent pixels) instead of growing past its edges.
        readonly property real preferredWidth: Math.max(360, headerText.implicitWidth, detailsText.implicitWidth)
        width: Math.min(preferredWidth, 560)

        Theme { id: theme }

        Text {
            id: headerText
            width: parent.width
            wrapMode: Text.WordWrap
            textFormat: Text.RichText
            color: theme.textPrimary
            text: qsTr("<h3>%1 (Open Visualization Tool)</h3><p>Version %2</p><p>%3, Qt Quick user interface</p>")
                    .arg(workbenchController.applicationName)
                    .arg(workbenchController.applicationVersion)
                    .arg(workbenchController.buildType)
        }

        TextEdit {
            id: detailsText
            width: parent.width
            readOnly: true
            wrapMode: TextEdit.WordWrap
            textFormat: TextEdit.RichText
            color: theme.textSecondary
            font.pixelSize: theme.fontSize
            text: workbenchController.copyrightNotice
                    + qsTr("<p><a href=\"https://www.ovito.org/\">https://www.ovito.org/</a></p>")
            onLinkActivated: (link) => Qt.openUrlExternally(link)
        }
    }
}
