// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/core/Core.h>
#include "AttributeDataObject.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(AttributeDataObject);
OVITO_CLASSINFO(AttributeDataObject, "DisplayName", "Attribute");
DEFINE_PROPERTY_FIELD(AttributeDataObject, value);
SET_PROPERTY_FIELD_LABEL(AttributeDataObject, value, "Value");

/******************************************************************************
* Loads the class' contents from the given stream.
******************************************************************************/
void AttributeDataObject::loadFromStream(ObjectLoadStream& stream)
{
    DataObject::loadFromStream(stream);

    if(stream.formatVersion() < 30016) {
        stream.expectChunk(0x01);
        stream >> _value.mutableValue();
        stream.closeChunk();
    }
}

}   // End of namespace
