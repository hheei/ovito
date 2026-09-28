// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/core/Core.h>

namespace Ovito {

/// Formats an integer number for display in the user interface, inserting digit grouping separators, e.g. "1,024".
/// Note: The application installs the C locale as the process-wide default (see Application::initialize()),
/// which has digit grouping turned off. This helper therefore uses its own locale instance.
inline QString formatNumberForUI(qulonglong value)
{
    const static QLocale groupSeparatorLocale = []() {
        QLocale loc(QLocale::C);
        loc.setNumberOptions(QLocale::DefaultNumberOptions); // Enable grouping separator.
        return loc;
    }();
    return groupSeparatorLocale.toString(value);
}

}   // End of namespace
