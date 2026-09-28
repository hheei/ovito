// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/core/Core.h>
#include <ovito/core/oo/OvitoClass.h>
#include "OvitoObject.h"

namespace Ovito {

// The class descriptor instance for the OvitoObject class.
const OvitoClass OvitoObject::__OOClass_instance{QStringLiteral("OvitoObject"), nullptr, OVITO_PLUGIN_NAME, nullptr};

#ifdef OVITO_DEBUG
/******************************************************************************
* Destructor.
******************************************************************************/
OvitoObject::~OvitoObject()
{
    OVITO_CHECK_OBJECT_POINTER(this);
    OVITO_ASSERT(!isBeingConstructed());
    OVITO_ASSERT(isBeingDeleted());
    _magicAliveCode = 0xFEDCBA87;
}
#endif

/******************************************************************************
* Internal method that calls this object's aboutToBeDeleted() routine.
* It is automatically called when the object's reference counter reaches zero.
******************************************************************************/
void OvitoObject::deleteObjectInternal() noexcept
{
    OVITO_CHECK_OBJECT_POINTER(this);
    OVITO_ASSERT(!isBeingDeleted());
    OVITO_ASSERT(!isBeingConstructed());

    // Mark this object as being deleted.
    _flags.setFlag(BeingDeleted);
    aboutToBeDeleted();
}

/******************************************************************************
* Prints an object to Qt debug stream.
******************************************************************************/
QDebug operator<<(QDebug dbg, const OvitoObject* o)
{
    QDebugStateSaver saver(dbg);
    if(!o)
        return dbg << "OvitoObject(0x0)";
    dbg.nospace() << o->getOOClass().className() << '(' << (const void *)o << ')';
    return dbg;
}

}   // End of namespace
