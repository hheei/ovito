// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/base/GUIBase.h>

namespace Ovito {

/**
 * Abstract base class for utility objects, which represent utility applets
 * shown in the command panel of OVITO. Utility objects encapsulate the state (data)
 * associated with a utility applet and get stored as part of a state file to
 * restore their settings when the state file is loaded.
 *
 * The actual GUI component of a utility applet is implemented in a PropertiesEditor
 * subclass that is associated with the utility object.
 */
class OVITO_GUIBASE_EXPORT UtilityObject : public RefTarget
{
public:
    /// A meta-class for utility objects (i.e. classes derived from UtilityObject).
    class OVITO_GUIBASE_EXPORT OOMetaClass : public RefTarget::OOMetaClass
    {
    public:
        /// Inherit constructor from base meta class.
        using RefTarget::OOMetaClass::OOMetaClass;

        /// Returns the category under which the utility will be displayed in the drop-down list box.
        virtual QString utilityCategory() const;

        /// Determines whether the given UtilityObject instance is an instance of this class.
        /// The default implementation checks the C++ class hierarchy. Subclasses may override this method
        /// to customize the membership test.
        virtual bool isMemberUtility(const UtilityObject* utility) const {
            return isMember(utility);
        }
    };

    OVITO_CLASS_META(UtilityObject, OOMetaClass);
};

}   // End of namespace
