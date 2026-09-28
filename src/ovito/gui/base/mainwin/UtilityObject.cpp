// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/gui/base/GUIBase.h>
#include "UtilityObject.h"

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(UtilityObject);

/******************************************************************************
* Returns the category under which the utility will be displayed in the drop-down list box.
******************************************************************************/
QString UtilityObject::OOMetaClass::utilityCategory() const
{
    return {};
}

}   // End of namespace
