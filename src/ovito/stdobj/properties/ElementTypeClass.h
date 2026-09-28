// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/stdobj/StdObj.h>
#include <ovito/core/dataset/data/DataObject.h>

namespace Ovito {

/**
 * \brief A meta-class for classes derived from the ElementType base class, e.g. particle types, bond types, etc.
 */
 class OVITO_STDOBJ_EXPORT ElementTypeClass : public DataObject::OOMetaClass
{
public:

    /// Inherit standard constructor from base meta class.
    using DataObject::OOMetaClass::OOMetaClass;

    /// Returns a list of column names to be displayed in the data inspector for element types of this class.
    virtual QStringList dataInspectorColumns() const;

    /// Returns the Qt table model data for the given element type to be displayed in the data inspector.
    virtual QVariant dataInspectorModelData(int columnIndex, const QString& columnName, const ElementType* elementType, int role) const;
};

}   // End of namespace

Q_DECLARE_METATYPE(Ovito::ElementTypeClassPtr);
