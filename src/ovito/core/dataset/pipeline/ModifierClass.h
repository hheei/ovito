// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/core/Core.h>
#include <ovito/core/oo/RefTarget.h>

namespace Ovito {

/**
 * \brief A meta-class for modifiers (i.e. classes derived from Modifier).
 */
class OVITO_CORE_EXPORT ModifierClass : public RefTarget::OOMetaClass
{
public:

    /// Inherit standard constructor from base meta class.
    using RefTarget::OOMetaClass::OOMetaClass;

    /// \brief Asks the modifier metaclass whether the modifier class can be applied to the given input data.
    /// \param input The data collection to operate on.
    /// \return true if the modifier can operate on the provided input data; false otherwise.
    ///
    /// This method is used to filter the list of available modifiers. The default implementation returns true.
    virtual bool isApplicableTo(const DataCollection& input) const { return true; }

    /// \brief Returns the category under which the modifier will be displayed in the modifier list box.
    virtual QString modifierCategory() const { return classMetadata("ModifierCategory"); }
};

}   // End of namespace

Q_DECLARE_METATYPE(Ovito::ModifierClassPtr);
