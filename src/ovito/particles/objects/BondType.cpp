// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/particles/Particles.h>
#include <ovito/core/app/Application.h>
#include <ovito/core/utilities/units/UnitsManager.h>
#include "BondType.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(BondType);
OVITO_CLASSINFO(BondType, "DisplayName", "Bond type");
DEFINE_PROPERTY_FIELD(BondType, radius);
SET_PROPERTY_FIELD_LABEL(BondType, radius, "Radius");
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(BondType, radius, WorldParameterUnit, 0);

/******************************************************************************
* Returns a list of column names to be displayed in the data inspector for
* element types of this class.
******************************************************************************/
QStringList BondType::OOMetaClass::dataInspectorColumns() const
{
    QStringList columns = ElementTypeClass::dataInspectorColumns();
    columns << QStringLiteral("Radius");
    return columns;
}

/******************************************************************************
* Returns the Qt table model data for the given element type to be displayed in the data inspector.
******************************************************************************/
QVariant BondType::OOMetaClass::dataInspectorModelData(int columnIndex, const QString& columnName, const ElementType* elementType, int role) const
{
    if(role == Qt::DisplayRole) {
        if(const BondType* btype = dynamic_object_cast<BondType>(elementType)) {
            if(columnName == QStringLiteral("Radius")) {
                if(btype->radius() != 0)
                    return btype->radius();
                else
                    return {};
            }
        }
    }
    return ElementTypeClass::dataInspectorModelData(columnIndex, columnName, elementType, role);
}

}   // End of namespace
