// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

import QtQuick

// Design tokens of the modern workbench UI.
// See docs/design/UI_DESIGN.md section 7 for the rationale behind the individual values.
QtObject {
    // Layout
    readonly property int spacing: 6
    readonly property int panelWidth: 264
    readonly property int fontSize: 12

    // Surfaces
    readonly property color surfaceWorkbench: "#181818"
    readonly property color surfacePanel: "#1f1f1f"
    readonly property color surfaceSelected: "#04395e"
    readonly property color borderSubtle: "#2b2b2b"
    readonly property color borderFocus: "#0078d4"

    // Content
    readonly property color accentPrimary: "#0078d4"
    readonly property color textPrimary: "#cccccc"
    readonly property color textSecondary: "#858585"
}
