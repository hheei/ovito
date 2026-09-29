// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

import QtQuick
import QtQuick.Controls

// The entry of a command that this shell does not offer yet.
//
// The rule of the migration plan is that such an entry is disabled and names the phase that brings it, so that the menu
// is an honest picture of the shell instead of a list of entries that do nothing. A QML menu item cannot carry a
// tooltip, so the phase is part of the text - the verification harness reads it from there and fails the build if an
// entry is disabled without naming a phase (see the parity check of OvitoQmlSpike).
MenuItem {
    id: menuItem

    /// The title of the command, as it will read once the phase below delivers it.
    required property string itemText

    /// The phase of the migration plan that delivers this command, for example "Phase 4".
    required property string ownerPhase

    text: itemText + " (" + ownerPhase + ")"
    enabled: false

    Accessible.description: qsTr("Not implemented yet. This command arrives with %1 of the migration plan.").arg(ownerPhase)
}
