// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/particles/Particles.h>
#include <ovito/particles/objects/Bonds.h>
#include <ovito/stdobj/properties/ElementType.h>
#include <ovito/stdobj/properties/Property.h>

namespace Ovito {

/**
 * \brief Stores the properties of a bond type, e.g. name, color, and radius.
 */
class OVITO_PARTICLES_EXPORT BondType : public ElementType
{
    /// Define a new metaclass.
    class BondTypeClass : public ElementTypeClass
    {
    public:
        /// Inherit constructor from base class.
        using ElementTypeClass::ElementTypeClass;

        /// Returns a list of column names to be displayed in the data inspector for element types of this class.
        virtual QStringList dataInspectorColumns() const override;

        /// Returns the Qt table model data for the given element type to be displayed in the data inspector.
        virtual QVariant dataInspectorModelData(int columnIndex, const QString& columnName, const ElementType* elementType, int role) const override;
    };
    OVITO_CLASS_META(BondType, BondTypeClass);

public:

    //////////////////////////////////// Utility methods ////////////////////////////////

    /// Builds a map from type identifiers to bond radii.
    /// Types which have a zero radius are not included in the map.
    static boost::container::flat_map<int, GraphicsFloatType> typeRadiusMap(const Property* typeProperty) {
        boost::container::flat_map<int, GraphicsFloatType> m;
        for(const ElementType* type : typeProperty->elementTypes())
            if(const BondType* bondType = dynamic_object_cast<BondType>(type))
                if(bondType->radius() > 0)
                    m.emplace(type->numericId(), static_cast<GraphicsFloatType>(bondType->radius()));
        return m;
    }

private:

    /// Stores the radius of the bond type.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(FloatType{0}, radius, setRadius);
};

}   // End of namespace
