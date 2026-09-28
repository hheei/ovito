////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2026 OVITO GmbH, Germany
//
//  This file is part of OVITO (Open Visualization Tool).
//
//  OVITO is free software; you can redistribute it and/or modify it either under the
//  terms of the GNU General Public License version 3 as published by the Free Software
//  Foundation (the "GPL") or, at your option, under the terms of the MIT License.
//  If you do not alter this notice, a recipient may use your version of this
//  file under either the GPL or the MIT License.
//
//  You should have received a copy of the GPL along with this program in a
//  file LICENSE.GPL.txt.  You should have received a copy of the MIT License along
//  with this program in a file LICENSE.MIT.txt
//
//  This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND,
//  either express or implied. See the GPL or the MIT License for the specific language
//  governing rights and limitations.
//
////////////////////////////////////////////////////////////////////////////////////////

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
