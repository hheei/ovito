// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/stdobj/StdObj.h>
#include <ovito/core/dataset/pipeline/Modifier.h>
#include <ovito/stdobj/properties/PropertyContainer.h>

namespace Ovito {

/**
 * \brief Base class for modifiers that operate on properties and which have no
 *        specific behavior that depends on the type of property it is (e.g. particle property, bond property, etc).
 */
class OVITO_STDOBJ_EXPORT GenericPropertyModifier : public Modifier
{
    /// Give this modifier class its own metaclass.
    class OVITO_STDOBJ_EXPORT GenericPropertyModifierClass : public ModifierClass
    {
    public:

        /// Inherit constructor from base class.
        using ModifierClass::ModifierClass;

        /// Asks the metaclass whether the modifier can be applied to the given input data.
        virtual bool isApplicableTo(const DataCollection& input) const override;
    };

    OVITO_CLASS_META(GenericPropertyModifier, GenericPropertyModifierClass)

protected:

    /// Sets the subject property container.
    void setDefaultSubject(const QString& pluginId, const QString& containerClassName);

private:

    /// The property container the modifier will operate on.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(PropertyContainerReference{}, subject, setSubject);
};

}   // End of namespace
