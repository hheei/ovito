// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/stdobj/StdObj.h>
#include <ovito/stdobj/properties/PropertyContainerClass.h>

namespace Ovito {

/**
 * A simple data structure that denotes which property an ElementType instance belongs to.
 *
 * It consists of two parts:
 *
 *   1. the property's name, e.g. ("Particle Type").
 *   2. the type of the container the property belongs to, e.g. Particles.
 *
 * Each ElementType (and derived type) stores a OwnerPropertyRef to keep track of the property
 * it is associated with. This is useful for loading the right presets for the type's attribute,
 * e.g. color and radius.
 */
class OVITO_STDOBJ_EXPORT OwnerPropertyRef
{
public:

    /// \brief Default constructor.
    OwnerPropertyRef() = default;

    /// \brief Constructs a reference to a standard property.
    OwnerPropertyRef(PropertyContainerClassPtr pclass, int typeId);

    /// \brief Constructs a reference to a user-defined property.
    OwnerPropertyRef(PropertyContainerClassPtr pclass, const QString& name);

    /// \brief Constructs a reference based on an existing Property.
    OwnerPropertyRef(PropertyContainerClassPtr pclass, const Property* property);

    /// \brief Gets the human-readable name of the referenced property.
    const QString& name() const { return _name; }

    /// \brief Return the class of the referenced property.
    PropertyContainerClassPtr containerClass() const { return _containerClass; }

    /// \brief If the referenced property is a standard property, returns its numeric ID.
    int typeId() const { return containerClass() ? containerClass()->standardPropertyTypeId(name()) : 0; }

    /// \brief Compares two references for equality.
    bool operator==(const OwnerPropertyRef& other) const {
        if(containerClass() != other.containerClass()) return false;
        return name() == other.name();
    }

    /// \brief Compares two references for inequality.
    bool operator!=(const OwnerPropertyRef& other) const { return !(*this == other); }

    /// \brief Strict ordering function.
    bool operator<(const OwnerPropertyRef& other) const;

    /// \brief Returns whether this reference refers to any property.
    /// \return false if this is a default-constructed OwnerPropertyRef.
    explicit operator bool() const { return containerClass() && !name().isEmpty(); }

private:

    /// The class of property container.
    PropertyContainerClassPtr _containerClass = nullptr;

    /// The name of the property being referenced.
    QString _name;

    friend OVITO_STDOBJ_EXPORT SaveStream& operator<<(SaveStream& stream, const OwnerPropertyRef& r);
    friend OVITO_STDOBJ_EXPORT LoadStream& operator>>(LoadStream& stream, OwnerPropertyRef& r);
};

/// Writes a OwnerPropertyRef to an output stream.
/// \relates OwnerPropertyRef
extern OVITO_STDOBJ_EXPORT SaveStream& operator<<(SaveStream& stream, const OwnerPropertyRef& r);

/// Reads a OwnerPropertyRef from an input stream.
/// \relates OwnerPropertyRef
extern OVITO_STDOBJ_EXPORT LoadStream& operator>>(LoadStream& stream, OwnerPropertyRef& r);

}   // End of namespace

Q_DECLARE_METATYPE(Ovito::OwnerPropertyRef);
