// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/stdobj/StdObj.h>
#include <ovito/stdobj/properties/PropertyContainer.h>
#include <ovito/core/app/PluginManager.h>
#include "GenericPropertyModifier.h"

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(GenericPropertyModifier);
DEFINE_PROPERTY_FIELD(GenericPropertyModifier, subject);

/******************************************************************************
* Sets the subject property container.
******************************************************************************/
void GenericPropertyModifier::setDefaultSubject(const QString& pluginId, const QString& containerClassName)
{
    if(OvitoClassPtr containerClass = PluginManager::instance().findClass(pluginId, containerClassName)) {
        OVITO_ASSERT(containerClass->isDerivedFrom(PropertyContainer::OOClass()));
        setSubject(static_cast<PropertyContainerClassPtr>(containerClass));
    }
}

/******************************************************************************
* Asks the modifier whether it can be applied to the given input data.
******************************************************************************/
bool GenericPropertyModifier::OOMetaClass::isApplicableTo(const DataCollection& input) const
{
    if(!ModifierClass::isApplicableTo(input))
        return false;

    // Modifier is applicable if there is at least one property container in the input data.
    // Subclasses can override this behavior.
    return input.containsObjectRecursive(PropertyContainer::OOClass());
}

}   // End of namespace
