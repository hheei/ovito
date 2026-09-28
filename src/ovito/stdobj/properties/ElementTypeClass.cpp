// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/stdobj/StdObj.h>
#include "ElementTypeClass.h"
#include "ElementType.h"

namespace Ovito {

/******************************************************************************
* Returns a list of column names to be displayed in the data inspector for
* element types of this class.
******************************************************************************/
QStringList ElementTypeClass::dataInspectorColumns() const
{
    return {
        QStringLiteral("ID"),
        QStringLiteral("Type Name"),
    };
}

/******************************************************************************
* Returns the Qt table model data for the given element type to be displayed in the data inspector.
******************************************************************************/
QVariant ElementTypeClass::dataInspectorModelData(int columnIndex, const QString& columnName, const ElementType* elementType, int role) const
{
    if(role == Qt::DisplayRole) {
        if(columnIndex == 0)
            return elementType->numericId();
        else if(columnIndex == 1)
            return elementType->nameOrNumericId();
    }
    else if(role == Qt::DecorationRole) {
        if(columnIndex == 1)
            return static_cast<QColor>(elementType->color());
    }
    return {};
}

}   // End of namespace
