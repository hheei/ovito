// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/stdobj/StdObj.h>
#include <ovito/stdobj/simcell/SimulationCell.h>
#include "PeriodicDomainObject.h"

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(PeriodicDomainObject);
OVITO_CLASSINFO(PeriodicDomainObject, "ClassNameAlias", "PeriodicDomainDataObject");  // For backward compatibility with OVITO 3.9.2
DEFINE_REFERENCE_FIELD(PeriodicDomainObject, domain);
DEFINE_PROPERTY_FIELD(PeriodicDomainObject, cuttingPlanes);
DEFINE_PROPERTY_FIELD(PeriodicDomainObject, title);
SET_PROPERTY_FIELD_LABEL(PeriodicDomainObject, domain, "Domain");
SET_PROPERTY_FIELD_LABEL(PeriodicDomainObject, cuttingPlanes, "Cutting planes");
SET_PROPERTY_FIELD_LABEL(PeriodicDomainObject, title, "Title");
SET_PROPERTY_FIELD_CHANGE_EVENT(PeriodicDomainObject, title, ReferenceEvent::TitleChanged);

/******************************************************************************
* Constructor.
******************************************************************************/
void PeriodicDomainObject::initializeObject(ObjectInitializationFlags flags, const QString& title)
{
    DataObject::initializeObject(flags);

    setTitle(title);
}

/******************************************************************************
* Returns the display title of this object.
******************************************************************************/
QString PeriodicDomainObject::objectTitle() const
{
    if(!title().isEmpty()) return title();
    return DataObject::objectTitle();
}

}   // End of namespace
