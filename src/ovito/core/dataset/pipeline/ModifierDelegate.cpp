// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/core/Core.h>
#include "ModifierDelegate.h"
#include "DelegatingModifier.h"

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(ModifierDelegate);
OVITO_CLASSINFO(ModifierDelegate, "ClassNameAlias", "AsynchronousModifierDelegate");  // For backward compatibility with OVITO 3.2.1
DEFINE_PROPERTY_FIELD(ModifierDelegate, isEnabled);
DEFINE_PROPERTY_FIELD(ModifierDelegate, inputDataObject);
SET_PROPERTY_FIELD_LABEL(ModifierDelegate, isEnabled, "Enabled");
SET_PROPERTY_FIELD_LABEL(ModifierDelegate, inputDataObject, "Data object");

/******************************************************************************
* Returns the modifier to which this delegate belongs.
******************************************************************************/
Modifier* ModifierDelegate::modifier() const
{
    Modifier* result = nullptr;
    visitDependents([&](RefMaker* dependent) {
        if(DelegatingModifier* modifier = dynamic_object_cast<DelegatingModifier>(dependent)) {
            if(modifier->delegate() == this) result = modifier;
        }
        else if(MultiDelegatingModifier* modifier = dynamic_object_cast<MultiDelegatingModifier>(dependent)) {
            if(modifier->delegates().contains(const_cast<ModifierDelegate*>(this))) result = modifier;
        }
    });
    return result;
}

}   // End of namespace
