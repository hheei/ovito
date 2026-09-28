// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/core/Core.h>
#include <ovito/core/oo/PropertyFieldDescriptor.h>
#include <ovito/core/oo/RefMaker.h>
#include <ovito/core/oo/OvitoObject.h>
#include <ovito/core/dataset/DataSet.h>
#include "RefMakerClass.h"

namespace Ovito {

/******************************************************************************
* Is called by the system after construction of the meta-class instance.
******************************************************************************/
void RefMakerClass::initialize()
{
    OvitoClass::initialize();

    // Collect all property fields of the class hierarchy in one array.
    for(const RefMakerClass* clazz = this; clazz != &RefMaker::OOClass(); clazz = static_cast<const RefMakerClass*>(clazz->superClass())) {
        for(const PropertyFieldDescriptor* field = clazz->firstPropertyField(); field != nullptr; field = field->next()) {
            _propertyFields.push_back(field);
        }
    }
}

/******************************************************************************
* Searches for a property field defined in this class or one of its super classes.
******************************************************************************/
const PropertyFieldDescriptor* RefMakerClass::findPropertyField(const char* identifier, bool searchSuperClasses) const
{
    OVITO_ASSERT(identifier != nullptr);

    if(!searchSuperClasses) {
        for(const PropertyFieldDescriptor* field = firstPropertyField(); field; field = field->next()) {
            if(qstrcmp(field->identifier(), identifier) == 0 || qstrcmp(field->identifierAlias(), identifier) == 0)
                return field;
        }
    }
    else {
        for(const PropertyFieldDescriptor* field : _propertyFields) {
            if(qstrcmp(field->identifier(), identifier) == 0 || qstrcmp(field->identifierAlias(), identifier) == 0)
                return field;
        }
    }

    return nullptr;
}

}   // End of namespace
