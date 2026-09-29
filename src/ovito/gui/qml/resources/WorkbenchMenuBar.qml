// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

import QtQuick
import QtQuick.Controls

// The menu bar of the workbench.
//
// It mirrors the menu of the classic frontend (MainWindow::createMainMenu) as far as this shell can carry it: an entry is
// only in the menu if the shared command layer or this shell provides its handler, and everything else is a disabled
// placeholder that names the phase which brings it (WorkbenchPlaceholderMenuItem). The parity check of OvitoQmlSpike
// walks this menu and fails if an entry is enabled without a command or disabled without a phase.
//
// The View menu has no counterpart in the classic frontend, which carries its viewport commands in a toolbar; the shell
// has no toolbar yet, so the commands live here (see UI_PARITY_MATRIX.md).
//
// Note that Qt Quick draws this menu bar inside the window on every platform. The native global menu bar of macOS would
// require Qt Widgets (the Qt Labs Platform implementation falls back to QMenuBar when no native menu is available), which
// this frontend deliberately does not link; the native macOS application menu still provides Quit.
MenuBar {
    id: menuBar
    objectName: "workbenchMenuBar"

    Menu {
        title: qsTr("&File")

        WorkbenchMenuItem { command: commandManager.command("FileImport") }

        WorkbenchPlaceholderMenuItem {
            itemText: qsTr("Load Remote File…")
            ownerPhase: "Phase 7"
        }

        WorkbenchPlaceholderMenuItem {
            itemText: qsTr("Export Data…")
            ownerPhase: "Phase 7"
        }

        MenuSeparator {}

        WorkbenchPlaceholderMenuItem {
            itemText: qsTr("Open Session State…")
            ownerPhase: "Phase 3"
        }

        WorkbenchPlaceholderMenuItem {
            itemText: qsTr("Save Session State")
            ownerPhase: "Phase 3"
        }

        WorkbenchPlaceholderMenuItem {
            itemText: qsTr("Save Session State As…")
            ownerPhase: "Phase 3"
        }

        MenuSeparator {}

        WorkbenchPlaceholderMenuItem {
            itemText: qsTr("New Window")
            ownerPhase: "Phase 7"
        }

        MenuSeparator {}

        // The About and Quit commands exist in the shared command layer, but their handlers belong to the frontend (the
        // desktop one opens a widget dialog). This shell provides them, so these entries are functional.
        WorkbenchMenuItem { command: commandManager.command("Quit") }
    }

    Menu {
        title: qsTr("&Edit")

        WorkbenchMenuItem { command: commandManager.command("EditUndo") }
        WorkbenchMenuItem { command: commandManager.command("EditRedo") }

        MenuSeparator {}

        // Deleting the selected object is the edit command of the pipeline view, which arrives with Phase 4; the command
        // and its handler exist already, so the entry works as soon as the panel can make a selection.
        WorkbenchMenuItem { command: commandManager.command("EditDelete") }

        MenuSeparator {}

        WorkbenchPlaceholderMenuItem {
            itemText: qsTr("Application Settings…")
            ownerPhase: "Phase 7"
        }
    }

    Menu {
        title: qsTr("&View")

        WorkbenchMenuItem { command: commandManager.command("SelectionMode") }
        WorkbenchMenuItem { command: commandManager.command("ViewportZoom") }
        WorkbenchMenuItem { command: commandManager.command("ViewportPan") }
        WorkbenchMenuItem { command: commandManager.command("ViewportOrbit") }
        WorkbenchMenuItem { command: commandManager.command("ViewportFOV") }
        WorkbenchMenuItem { command: commandManager.command("ViewportOrbitPickCenter") }

        MenuSeparator {}

        WorkbenchMenuItem { command: commandManager.command("ViewportMaximize") }
        WorkbenchMenuItem { command: commandManager.command("ViewportZoomSceneExtents") }
        WorkbenchMenuItem { command: commandManager.command("ViewportZoomSceneExtentsAll") }
        WorkbenchMenuItem { command: commandManager.command("ViewportZoomSelectionExtents") }
        WorkbenchMenuItem { command: commandManager.command("ViewportZoomSelectionExtentsAll") }

        MenuSeparator {}

        WorkbenchPlaceholderMenuItem {
            itemText: qsTr("Configure Viewport Graphics…")
            ownerPhase: "Phase 7"
        }
    }

    Menu {
        title: qsTr("&Help")

        WorkbenchMenuItem { command: commandManager.command("HelpAbout") }

        MenuSeparator {}

        WorkbenchPlaceholderMenuItem {
            itemText: qsTr("Show Online Help")
            ownerPhase: "Phase 7"
        }

        WorkbenchPlaceholderMenuItem {
            itemText: qsTr("Show Scripting Reference")
            ownerPhase: "Phase 7"
        }

        MenuSeparator {}

        WorkbenchPlaceholderMenuItem {
            itemText: qsTr("System Information")
            ownerPhase: "Phase 7"
        }

        WorkbenchPlaceholderMenuItem {
            itemText: qsTr("Request a Feature")
            ownerPhase: "Phase 7"
        }
    }
}
