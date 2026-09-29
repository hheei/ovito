// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

import QtQuick

// Design tokens of the QML workbench (see docs/design/UI_DESIGN.md section 7).
//
// The palette follows the color scheme of the operating system: dark on a dark desktop, light on a light one. Each
// component of the shell declares its own Theme instance, so that a component can be looked at in isolation - the tokens
// are constants, not shared state.
QtObject {
    id: theme

    // Whether the shell draws itself dark is decided by the shared settings facade and not here, so that the Qt Quick
    // shell and the classic frontend follow the same color-scheme policy (including the case of a platform that reports
    // no color scheme at all, which counts as light in both).
    readonly property bool dark: guiSettings.usingDarkTheme

    // Spacing and sizing.
    readonly property int spacing: 6
    readonly property int panelWidth: 264
    readonly property int headerHeight: 34
    readonly property int fontSize: 12

    // Surfaces.
    readonly property color surfaceWorkbench: dark ? "#181818" : "#f3f3f3"
    readonly property color surfacePanel: dark ? "#1f1f1f" : "#ffffff"
    readonly property color surfaceHeader: dark ? "#252526" : "#e8e8e8"
    readonly property color surfaceHover: dark ? "#2a2d2e" : "#e0e0e0"
    readonly property color surfaceSelected: dark ? "#04395e" : "#cce4f7"
    readonly property color surfaceOverlay: dark ? "#000000cc" : "#ffffffcc"

    // Lines.
    readonly property color borderSubtle: dark ? "#2b2b2b" : "#d4d4d4"
    readonly property color borderFocus: dark ? "#0078d4" : "#005a9e"
    readonly property color accentPrimary: dark ? "#0078d4" : "#007acc"
    readonly property color accentWarning: dark ? "#cca700" : "#b8860b"
    readonly property color accentError: dark ? "#f14c4c" : "#c62828"

    // Text.
    readonly property color textPrimary: dark ? "#cccccc" : "#333333"
    readonly property color textSecondary: dark ? "#858585" : "#6b6b6b"
    readonly property color textDisabled: dark ? "#5a5a5a" : "#a0a0a0"
    readonly property color textOnAccent: "#ffffff"

    // Controls. Qt Quick Controls draws itself from the palette of the application; the shell overrides the entries it
    // uses with the tokens above, so that the Controls look like the rest of the shell on every platform.
    readonly property color controlBackground: dark ? "#333333" : "#fdfdfd"
    readonly property color controlDisabledBackground: dark ? "#2a2a2a" : "#e8e8e8"
    readonly property color controlBorder: dark ? "#3c3c3c" : "#c8c8c8"
}
