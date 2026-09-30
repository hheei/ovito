// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Ovito.Qml

// The dialog that asks which pipeline of a session file to keep.
//
// OVITO Basic displays one file source pipeline at a time, so a session that holds several of them (written by OVITO Pro)
// can only be loaded after the user picked one. The classic frontend asks the same question with a QDialog
// (MainWindowUI::checkLoadedDataset); this is the QML counterpart of that dialog, including its answer: closing the
// dialog without picking a pipeline aborts the loading of the session.
Dialog {
    id: pipelineChooser
    objectName: "pipelineChooser"

    required property WorkbenchController controller

    Theme { id: theme }

    /// The pipeline the user picked, or -1 while nothing was picked.
    property int pickedIndex: -1

    title: qsTr("Multiple pipelines found")
    modal: true
    closePolicy: Popup.CloseOnEscape
    anchors.centerIn: Overlay.overlay
    width: Math.min(560, Overlay.overlay ? Overlay.overlay.width - 4 * theme.spacing : 560)
    height: Math.min(implicitHeight, Overlay.overlay ? Overlay.overlay.height - 4 * theme.spacing : implicitHeight)

    palette.window: theme.surfacePanel
    palette.windowText: theme.textPrimary
    palette.text: theme.textPrimary
    palette.button: theme.controlBackground
    palette.buttonText: theme.textPrimary

    // Answering the question is what the blocked frontend waits for, so it happens here and not when the dialog closes:
    // dismissing the dialog (Escape or the close button) means 'load nothing'.
    onClosed: controller.answerPipelineChoice(accepted ? pickedIndex : -1)

    // The frontend can answer the question itself (a verification check does), and then the dialog never goes through
    // its own close path. Following the controller's state here is what closes it in that case; binding `visible` to
    // the controller instead would not work, because a popup writes `visible` itself and destroys such a binding.
    Connections {
        target: controller
        function onPipelineChoiceChanged() {
            if(!controller.pipelineChoiceVisible && pipelineChooser.visible)
                pipelineChooser.close()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: theme.spacing * 2

        Text {
            Layout.fillWidth: true
            text: qsTr("The session file contains %1 pipelines. OVITO Pro is required to work with several pipelines in "
                       + "the same scene; please pick one of the pipelines below to load only that one now.")
                  .arg(controller.pipelineChoiceItems.length)
            color: theme.textPrimary
            font.pixelSize: theme.fontSize
            wrapMode: Text.WordWrap
        }

        Text {
            Layout.fillWidth: true
            text: qsTr("Available pipelines:")
            color: theme.textSecondary
            font.pixelSize: theme.fontSize
        }

        Frame {
            Layout.fillWidth: true
            Layout.preferredHeight: 160
            palette.window: theme.controlBackground
            padding: 0

            ListView {
                id: pipelineList
                anchors.fill: parent
                clip: true
                model: controller.pipelineChoiceItems
                currentIndex: count > 0 ? 0 : -1

                delegate: ItemDelegate {
                    required property var modelData
                    required property int index

                    width: ListView.view.width
                    text: modelData
                    highlighted: ListView.isCurrentItem
                    onClicked: pipelineList.currentIndex = index

                    palette.text: theme.textPrimary
                    palette.highlightedText: theme.textOnAccent
                }
            }
        }

        RowLayout {
            Layout.alignment: Qt.AlignRight

            Button {
                text: qsTr("OK")
                enabled: pipelineList.currentIndex >= 0
                highlighted: enabled
                Accessible.name: text
                onClicked: {
                    pipelineChooser.pickedIndex = pipelineList.currentIndex
                    pipelineChooser.accept()
                }

                palette.button: theme.accentPrimary
                palette.buttonText: theme.textOnAccent
            }

            Button {
                text: qsTr("Cancel")
                Accessible.name: text
                onClicked: pipelineChooser.reject()

                palette.button: theme.controlBackground
                palette.buttonText: theme.textPrimary
            }
        }
    }
}
