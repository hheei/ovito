// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/stdobj/StdObj.h>
#include "Property.h"
#include "OwnerPropertyRef.h"
#include "PropertyContainer.h"

namespace Ovito {

/******************************************************************************
* Constructs a reference to a standard property.
******************************************************************************/
OwnerPropertyRef::OwnerPropertyRef(PropertyContainerClassPtr pclass, int typeId) :
    _containerClass(pclass),
    _name(pclass->standardPropertyName(typeId))
{
    OVITO_ASSERT(pclass);
    OVITO_ASSERT(!_name.isEmpty());
}

/******************************************************************************
* Constructs a reference to a user-defined property.
******************************************************************************/
OwnerPropertyRef::OwnerPropertyRef(PropertyContainerClassPtr pclass, const QString& name) :
    _containerClass(pclass),
    _name(name)
{
    OVITO_ASSERT(pclass);
    OVITO_ASSERT(!name.isEmpty());
}

/******************************************************************************
* Constructs a reference based on an existing Property.
******************************************************************************/
OwnerPropertyRef::OwnerPropertyRef(PropertyContainerClassPtr pclass, const Property* property) :
    _containerClass(pclass),
    _name(property->name())
{
    OVITO_ASSERT(pclass);
}

/******************************************************************************
* Strict ordering function.
******************************************************************************/
bool OwnerPropertyRef::operator<(const OwnerPropertyRef& other) const
{
    if(containerClass() == other.containerClass())
        return name() < other.name();
    else
        return containerClass() < other.containerClass();
}

/******************************************************************************
* Writes a OwnerPropertyRef to an output stream.
******************************************************************************/
SaveStream& operator<<(SaveStream& stream, const OwnerPropertyRef& r)
{
    stream.beginChunk(0x04);
    OvitoClass::serializeRTTI(stream, r.containerClass(), true);
    stream << r.name();
    stream.endChunk();
    return stream;
}

/******************************************************************************
* Reads a OwnerPropertyRef from an input stream.
******************************************************************************/
LoadStream& operator>>(LoadStream& stream, OwnerPropertyRef& r)
{
    int version = stream.expectChunkRange(0x02, 2);
    r._containerClass = static_cast<PropertyContainerClassPtr>(OvitoClass::deserializeRTTI(stream, true));
    if(version < 2) {
        int typeId;
        stream >> typeId;
    }
    stream >> r._name;
    if(version < 2) {
        int vectorComponentIndex;
        stream >> vectorComponentIndex;
    }
    if(version >= 1 && version < 2) {
        QString vectorComponentName;
        stream >> vectorComponentName;
    }
    if(!r._containerClass)
        r = OwnerPropertyRef();
    stream.closeChunk();
    return stream;
}

}   // End of namespace
