////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2026 OVITO GmbH, Germany
//
//  This file is part of OVITO (Open Visualization Tool).
//
//  OVITO is free software; you can redistribute it and/or modify it either under the
//  terms of the GNU General Public License version 3 as published by the Free Software
//  Foundation (the "GPL") or, at your option, under the terms of the MIT License.
//  If you do not alter this notice, a recipient may use your version of this
//  file under either the GPL or the MIT License.
//
//  You should have received a copy of the GPL along with this program in a
//  file LICENSE.GPL.txt.  You should have received a copy of the MIT License along
//  with this program in a file LICENSE.MIT.txt
//
//  This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND,
//  either express or implied. See the GPL or the MIT License for the specific language
//  governing rights and limitations.
//
////////////////////////////////////////////////////////////////////////////////////////

#pragma once


#include <ovito/core/Core.h>
#include <ovito/core/oo/ReferenceEvent.h>
#include <ovito/core/utilities/units/UnitsManager.h>
#include "PropertyFieldDescriptor.h"

namespace Ovito {

/// Compile-time helper that decomposes a pointer-to-data-member type into its
/// owning class type and the type of the member it points to.
template<typename T> struct member_ptr_traits;
template<typename C, typename M> struct member_ptr_traits<M C::*> {
    using class_type = C;
    using member_type = M;
};

/// Empty tag types, each carrying one or two pointer-to-member non-type template parameters.
/// A tag is passed as the first argument of the corresponding generic NativePropertyFieldDescriptor
/// constructor to select it and to convey, at compile time, which storage member(s) of the owning
/// class the field uses. (C++ has no syntax for supplying explicit template arguments to a
/// constructor, so the member pointer is smuggled in through this deduced tag argument.) From the
/// member pointer the constructor recovers the owning class type and the storage type, and from those
/// it synthesizes the descriptor's accessor thunks -- replacing the per-field lambdas that the
/// DEFINE_*_FIELD macros used to spell out by hand.
template<auto MemberPtr> struct PlainFieldGen {};
template<auto MemberPtr> struct SingleReferenceFieldGen {};
template<auto MemberPtr> struct VectorReferenceFieldGen {};
template<auto SnapshotPtr, auto FieldPtr> struct SnapshotFieldGen {};

/******************************************************************************
* Property field descriptor for fields declared by native (compiled) C++ classes.
*
* Specialization of PropertyFieldDescriptor used for every field of a RefMaker-derived C++ class.
* It exists to construct the base descriptor conveniently: in addition to the low-level base
* constructors (which take the accessor thunks as explicit function pointers), it provides a set of
* generic, templated constructors that derive the complete thunk table automatically from a pointer
* to the field's storage member. These are the constructors invoked by the DEFINE_*_FIELD macros.
*
* The class also hosts the small helper structs (PropertyFieldUnitsSetter, PropertyFieldDisplayNameSetter,
* ...) that the SET_PROPERTY_FIELD_* macros use to attach optional metadata to a descriptor after it
* has been constructed.
******************************************************************************/
class OVITO_CORE_EXPORT NativePropertyFieldDescriptor : public PropertyFieldDescriptor
{
public:

    /// Inherit the low-level base constructors (taking explicit accessor-thunk function pointers).
    /// Used directly only where the thunks cannot be derived from a storage member, e.g. virtual
    /// property fields (DEFINE_VIRTUAL_PROPERTY_FIELD). All other field kinds use the generic
    /// constructors below.
    using PropertyFieldDescriptor::PropertyFieldDescriptor;

    /// Generic constructor for a plain-value property field. MemberPtr must point to the field's
    /// PropertyField<> storage member of the owning RefMaker-derived class; from it this constructor
    /// synthesizes the complete copy/read/write/compare/save/load thunk table, so the call site needs
    /// no hand-written accessor lambdas. Selected by passing a PlainFieldGen tag (see DEFINE_PROPERTY_FIELD).
    template<auto MemberPtr>
    NativePropertyFieldDescriptor(PlainFieldGen<MemberPtr>, RefMakerClass* definingClass, const char* identifier, PropertyFieldFlags flags)
        : PropertyFieldDescriptor(definingClass, identifier, flags,
            // propertyStorageCopyFunc
            [](RefMaker* obj, const PropertyFieldDescriptor* descriptor, const RefMaker* other) {
                using Owner = typename member_ptr_traits<decltype(MemberPtr)>::class_type;
                (static_cast<Owner*>(obj)->*MemberPtr).set(obj, descriptor, (static_cast<const Owner*>(other)->*MemberPtr).get());
            },
            // propertyStorageReadFunc
            [](const RefMaker* obj, const PropertyFieldDescriptor*) -> QVariant {
                using Owner = typename member_ptr_traits<decltype(MemberPtr)>::class_type;
                return (static_cast<const Owner*>(obj)->*MemberPtr).getQVariant();
            },
            // propertyStorageWriteFunc
            [](RefMaker* obj, const PropertyFieldDescriptor* descriptor, const QVariant& newValue) {
                using Owner = typename member_ptr_traits<decltype(MemberPtr)>::class_type;
                (static_cast<Owner*>(obj)->*MemberPtr).setQVariant(obj, descriptor, newValue);
            },
            // propertyStorageCompareFunc
            [](const RefMaker* obj, const PropertyFieldDescriptor*, const RefMaker* other) -> bool {
                using Owner = typename member_ptr_traits<decltype(MemberPtr)>::class_type;
                return (static_cast<const Owner*>(obj)->*MemberPtr).equals(static_cast<const Owner*>(other)->*MemberPtr);
            },
            // propertyStorageSaveFunc
            [](const RefMaker* obj, const PropertyFieldDescriptor*, SaveStream& stream) {
                using Owner = typename member_ptr_traits<decltype(MemberPtr)>::class_type;
                (static_cast<const Owner*>(obj)->*MemberPtr).saveToStream(stream);
            },
            // propertyStorageLoadFunc
            [](RefMaker* obj, const PropertyFieldDescriptor*, LoadStream& stream) {
                using Owner = typename member_ptr_traits<decltype(MemberPtr)>::class_type;
                (static_cast<Owner*>(obj)->*MemberPtr).loadFromStream(stream);
            })
    {}

    /// Generic constructor for a single-reference field, synthesizing the reference accessor
    /// thunks from a pointer to the field's ReferenceField<> storage member. The referenced
    /// target class is deduced from the storage member's target_object_type.
    template<auto MemberPtr,
        typename Owner = typename member_ptr_traits<decltype(MemberPtr)>::class_type,
        typename Storage = typename member_ptr_traits<decltype(MemberPtr)>::member_type,
        typename Target = typename Storage::target_object_type>
    NativePropertyFieldDescriptor(SingleReferenceFieldGen<MemberPtr>, RefMakerClass* definingClass, const char* identifier, PropertyFieldFlags flags)
        : PropertyFieldDescriptor(definingClass, &Target::OOClass(), identifier, flags,
            // singleReferenceReadFunc
            [](const RefMaker* obj, const PropertyFieldDescriptor*) -> RefTarget* {
                return const_cast<Target*>((static_cast<const Owner*>(obj)->*MemberPtr).get());
            },
            // singleReferenceWriteFuncRef
            [](RefMaker* obj, const PropertyFieldDescriptor* descriptor, OORef<const RefTarget> newTarget) {
                (static_cast<Owner*>(obj)->*MemberPtr).set(obj, descriptor,
                    static_object_cast<Target>(const_pointer_cast<RefTarget>(std::move(newTarget))));
            })
    {}

    /// Generic constructor for a vector-reference field, synthesizing the count/get/set/
    /// remove/insert thunks from a pointer to the field's VectorReferenceField<> storage member.
    template<auto MemberPtr,
        typename Owner = typename member_ptr_traits<decltype(MemberPtr)>::class_type,
        typename Storage = typename member_ptr_traits<decltype(MemberPtr)>::member_type,
        typename Target = typename Storage::target_object_type>
    NativePropertyFieldDescriptor(VectorReferenceFieldGen<MemberPtr>, RefMakerClass* definingClass, const char* identifier, PropertyFieldFlags flags)
        : PropertyFieldDescriptor(definingClass, &Target::OOClass(), identifier, flags,
            // vectorReferenceCountFunc
            [](const RefMaker* obj, const PropertyFieldDescriptor*) -> int {
                return (static_cast<const Owner*>(obj)->*MemberPtr).size();
            },
            // vectorReferenceGetFunc
            [](const RefMaker* obj, const PropertyFieldDescriptor*, int index) -> RefTarget* {
                return const_cast<Target*>((static_cast<const Owner*>(obj)->*MemberPtr).get(index));
            },
            // vectorReferenceSetFunc
            [](RefMaker* obj, const PropertyFieldDescriptor* descriptor, int index, const RefTarget* newTarget) {
                (static_cast<Owner*>(obj)->*MemberPtr).set(obj, descriptor, index,
                    static_object_cast<Target>(const_cast<RefTarget*>(newTarget)));
            },
            // vectorReferenceRemoveFunc
            [](RefMaker* obj, const PropertyFieldDescriptor* descriptor, int index) {
                (static_cast<Owner*>(obj)->*MemberPtr).remove(obj, descriptor, index);
            },
            // vectorReferenceInsertFunc
            [](RefMaker* obj, const PropertyFieldDescriptor* descriptor, int index, OORef<RefTarget> newTarget) {
                (static_cast<Owner*>(obj)->*MemberPtr).insert(obj, descriptor, index,
                    static_object_cast<Target>(std::move(newTarget)));
            })
    {}

    /// Generic constructor for a snapshot property field, synthesizing all accessor and snapshot
    /// thunks from pointers to the snapshot storage member and the original field's storage member.
    /// The descriptor of the original (captured) field is supplied so that restoring a snapshot
    /// generates notifications/undo against the original field, not the snapshot field.
    template<auto SnapshotPtr, auto FieldPtr,
        typename Owner = typename member_ptr_traits<decltype(SnapshotPtr)>::class_type>
    NativePropertyFieldDescriptor(SnapshotFieldGen<SnapshotPtr, FieldPtr>, RefMakerClass* definingClass, const char* identifier,
                                  PropertyFieldFlags flags, const PropertyFieldDescriptor* sourceField)
        : PropertyFieldDescriptor(definingClass, identifier, flags,
            // propertyStorageCopyFunc
            [](RefMaker* obj, const PropertyFieldDescriptor*, const RefMaker* other) {
                if((static_cast<const Owner*>(other)->*SnapshotPtr).hasSnapshot())
                    (static_cast<Owner*>(obj)->*SnapshotPtr).takeSnapshot((static_cast<const Owner*>(other)->*SnapshotPtr).get());
            },
            // propertyStorageReadFunc
            [](const RefMaker* obj, const PropertyFieldDescriptor*) -> QVariant {
                return (static_cast<const Owner*>(obj)->*SnapshotPtr).getQVariant();
            },
            // propertyStorageWriteFunc
            [](RefMaker* obj, const PropertyFieldDescriptor* descriptor, const QVariant& newValue) {
                (static_cast<Owner*>(obj)->*SnapshotPtr).setQVariant(obj, descriptor, newValue);
            },
            // propertyStorageCompareFunc
            [](const RefMaker* obj, const PropertyFieldDescriptor*, const RefMaker* other) -> bool {
                return (static_cast<const Owner*>(obj)->*SnapshotPtr).equals(static_cast<const Owner*>(other)->*SnapshotPtr);
            },
            // propertyStorageSaveFunc
            [](const RefMaker* obj, const PropertyFieldDescriptor*, SaveStream& stream) {
                (static_cast<const Owner*>(obj)->*SnapshotPtr).saveToStream(stream);
            },
            // propertyStorageLoadFunc
            [](RefMaker* obj, const PropertyFieldDescriptor*, LoadStream& stream) {
                (static_cast<Owner*>(obj)->*SnapshotPtr).loadFromStream(stream);
            },
            // propertyStorageTakeSnapshotFunc
            [](RefMaker* obj, const PropertyFieldDescriptor*) {
                (static_cast<Owner*>(obj)->*SnapshotPtr).takeSnapshot((static_cast<const Owner*>(obj)->*FieldPtr).get());
            },
            // propertyStorageRestoreSnapshotFunc
            [](const RefMaker* source, const PropertyFieldDescriptor* snapshotDescriptor, RefMaker* target) {
                if((static_cast<const Owner*>(source)->*SnapshotPtr).hasSnapshot())
                    (static_cast<Owner*>(target)->*FieldPtr).set(target,
                        static_cast<const NativePropertyFieldDescriptor*>(snapshotDescriptor)->_snapshotSourceField,
                        (static_cast<const Owner*>(source)->*SnapshotPtr).get());
            })
    {
        _snapshotSourceField = sourceField;
    }

public:

    // Internal helper class that is used to specify the units for a controller
    // property field. Do not use this class directly but use the
    // SET_PROPERTY_FIELD_UNITS macro instead.
    struct PropertyFieldUnitsSetter : public NumericalParameterDescriptor {
        PropertyFieldUnitsSetter(NativePropertyFieldDescriptor* propfield, const QMetaObject* parameterUnitType, FloatType minValue = FLOATTYPE_MIN, FloatType maxValue = FLOATTYPE_MAX)
            : NumericalParameterDescriptor({ parameterUnitType, minValue, maxValue })
        {
            propfield->setNumericalParameterInfo(this);
        }
    };

    // Internal helper class that is used to specify the label text for a
    // property field. Do not use this class directly but use the
    // SET_PROPERTY_FIELD_LABEL macro instead.
    struct PropertyFieldDisplayNameSetter {
        PropertyFieldDisplayNameSetter(NativePropertyFieldDescriptor* propfield, const QString& label) {
            propfield->setDisplayName(label);
        }
    };

    // Internal helper class that is used to set the reference event type to generate
    // for a property field every time its value changes. Do not use this class directly but use the
    // SET_PROPERTY_FIELD_CHANGE_EVENT macro instead.
    struct PropertyFieldChangeEventSetter {
        PropertyFieldChangeEventSetter(NativePropertyFieldDescriptor* propfield, int eventType) {
            OVITO_ASSERT(propfield->_extraChangeEventType == 0);
            propfield->_extraChangeEventType = eventType;
        }
    };

    // Internal helper class that is used to specify the alias for a property field's identifier. Do not use this class directly but use the
    // SET_PROPERTY_FIELD_ALIAS_IDENTIFIER macro instead.
    struct PropertyFieldAliasIdentifierSetter {
        PropertyFieldAliasIdentifierSetter(NativePropertyFieldDescriptor* propfield, const char* identifierAlias) {
            OVITO_ASSERT(!propfield->_identifierAlias);
            propfield->_identifierAlias = identifierAlias;
        }
    };

private:

    /// For snapshot property fields only: the descriptor of the original property field whose value
    /// is being captured. Used when restoring a snapshot so that the change is attributed to the
    /// original field. Null for all non-snapshot fields.
    const PropertyFieldDescriptor* _snapshotSourceField = nullptr;
};

/*** Macros for declaring property and reference fields in RefMaker-derived classes ***
 *
 * Each persistent field of a RefMaker-derived class is declared with a pair of macros:
 *
 *   - A DECLARE_*_FIELD macro placed inside the class definition (header). It generates: the field's
 *     storage member (a PropertyField<>, ReferenceField<> or VectorReferenceField<>); a static
 *     NativePropertyFieldDescriptor for the field; a typed getter method; and, for the MODIFIABLE
 *     variants, a typed setter method. A PropertyFieldFlags value selects the field's behavior
 *     (serialization, undo, change notification, cloning, ...).
 *
 *   - A matching DEFINE_*_FIELD macro placed in the class's .cpp file. It defines and constructs the
 *     static descriptor declared above, selecting the appropriate generic NativePropertyFieldDescriptor
 *     constructor via a *FieldGen tag, so the expansion contains no hand-written accessor lambdas.
 *
 * Constructing the descriptor registers the field with its defining class's meta-object (see
 * PropertyFieldDescriptor). Because the descriptor's constructor writes into that meta-object, the
 * DEFINE_*_FIELD invocation must appear in the same translation unit as -- and textually after -- the
 * class's IMPLEMENT_*_OVITO_CLASS meta-class definition, which guarantees the meta-object is fully
 * constructed first during static initialization.
 *
 * Optional per-field metadata is attached afterwards with the SET_PROPERTY_FIELD_* macros (UI label,
 * physical unit and value range, extra change-event type, backward-compatible identifier alias).
 */

/// Expands to a pointer to the NativePropertyFieldDescriptor of a named property or reference field.
/// Spelled PROPERTY_FIELD(ClassName::fieldName) at a use site, or PROPERTY_FIELD(fieldName) inside the
/// declaring class. The accessor it refers to is generated by the DECLARE_*_FIELD macros.
#define PROPERTY_FIELD(RefMakerClassPlusStorageFieldName) \
        RefMakerClassPlusStorageFieldName##__propdescr()

/// Defines (in the .cpp file) the descriptor for a single-reference field declared with
/// DECLARE_REFERENCE_FIELD*. The reference accessor thunks are synthesized generically from a
/// pointer to the field's ReferenceField<> storage member (see the SingleReferenceFieldGen
/// constructor of NativePropertyFieldDescriptor).
#define DEFINE_REFERENCE_FIELD(classname, name) \
    Ovito::NativePropertyFieldDescriptor classname::name##__propdescr_instance( \
            Ovito::SingleReferenceFieldGen<&classname::_##name>{}, \
            const_cast<classname::OOMetaClass*>(&classname::OOClass()), \
            #name, \
            static_cast<Ovito::PropertyFieldFlags>(classname::__##name##_flags));

/// Defines (in the .cpp file) the descriptor for a vector-reference field declared with
/// DECLARE_VECTOR_REFERENCE_FIELD*. The reference accessor thunks are synthesized generically from
/// a pointer to the field's VectorReferenceField<> storage member (see the VectorReferenceFieldGen
/// constructor of NativePropertyFieldDescriptor).
#define DEFINE_VECTOR_REFERENCE_FIELD(classname, name) \
    Ovito::NativePropertyFieldDescriptor classname::name##__propdescr_instance( \
            Ovito::VectorReferenceFieldGen<&classname::_##name>{}, \
            const_cast<classname::OOMetaClass*>(&classname::OOClass()), \
            #name, \
            static_cast<Ovito::PropertyFieldFlags>(classname::__##name##_flags));

/// Declares a single-reference field inside a class definition. Generates the ReferenceField<> storage
/// member, the field's static descriptor, and a getter returning the referenced object. \a type is the
/// fancy-pointer type of the reference (e.g. OORef<Foo>), \a name the field's unique identifier within
/// the class, and \a flags its PropertyFieldFlags. Pair with DEFINE_REFERENCE_FIELD in the .cpp file.
#define DECLARE_REFERENCE_FIELD_FLAGS(type, name, flags) \
    private: \
        enum { __##name##_flags = flags | PROPERTY_FIELD_STANDARD_FLAGS }; \
        using __##name##_target_object_type = Ovito::ReferenceField<type>::target_object_type; \
        static Ovito::NativePropertyFieldDescriptor name##__propdescr_instance; \
    public: \
        static inline Ovito::NativePropertyFieldDescriptor* PROPERTY_FIELD(name) { return &name##__propdescr_instance; } \
        Ovito::ReferenceField<type> _##name; \
        inline typename std::pointer_traits<type>::element_type* name() const { OVITO_CHECK_OBJECT_POINTER(this); return _##name.get(); } \
    private:

/// Convenience form of DECLARE_REFERENCE_FIELD_FLAGS with no extra flags.
#define DECLARE_REFERENCE_FIELD(type, name) \
    DECLARE_REFERENCE_FIELD_FLAGS(type, name, PROPERTY_FIELD_NO_FLAGS)

/// Like DECLARE_REFERENCE_FIELD_FLAGS, but additionally generates a public setter method \a setterName
/// (plus a non-template overload for the Python bindings) that replaces the referenced object.
#define DECLARE_MODIFIABLE_REFERENCE_FIELD_FLAGS(type, name, setterName, flags) \
    DECLARE_REFERENCE_FIELD_FLAGS(type, name, flags) \
    public: \
        template<typename U> inline void setterName(U&& newValue) { OVITO_CHECK_OBJECT_POINTER(this); _##name.set(this, PROPERTY_FIELD(name), std::forward<U>(newValue)); } \
        inline void setterName##PYTHON(typename std::pointer_traits<type>::element_type* newValue) { OVITO_CHECK_OBJECT_POINTER(this); _##name.set(this, PROPERTY_FIELD(name), newValue); } \
    private:

/// Convenience form of DECLARE_MODIFIABLE_REFERENCE_FIELD_FLAGS with no extra flags.
#define DECLARE_MODIFIABLE_REFERENCE_FIELD(type, name, setterName) \
    DECLARE_MODIFIABLE_REFERENCE_FIELD_FLAGS(type, name, setterName, PROPERTY_FIELD_NO_FLAGS)

/// Declares a vector-reference field (an ordered list of references) inside a class definition.
/// Generates the VectorReferenceField<> storage member, the field's static descriptor, and a getter
/// returning the list of referenced objects. \a type is the per-element fancy-pointer type, \a name the
/// field's unique identifier, \a flags its PropertyFieldFlags. Pair with DEFINE_VECTOR_REFERENCE_FIELD.
#define DECLARE_VECTOR_REFERENCE_FIELD_FLAGS(type, name, flags) \
    private: \
        enum { __##name##_flags = flags | PROPERTY_FIELD_VECTOR | PROPERTY_FIELD_STANDARD_FLAGS }; \
        using __##name##_target_object_type = Ovito::VectorReferenceField<type>::target_object_type; \
        static Ovito::NativePropertyFieldDescriptor name##__propdescr_instance; \
    public: \
        static inline Ovito::NativePropertyFieldDescriptor* PROPERTY_FIELD(name) { return &name##__propdescr_instance; } \
        Ovito::VectorReferenceField<type> _##name; \
        inline decltype(std::declval<Ovito::VectorReferenceField<type>>().targets()) name() const { OVITO_CHECK_OBJECT_POINTER(this); return _##name.targets(); } \
    private:

/// Convenience form of DECLARE_VECTOR_REFERENCE_FIELD_FLAGS with no extra flags.
#define DECLARE_VECTOR_REFERENCE_FIELD(type, name) \
    DECLARE_VECTOR_REFERENCE_FIELD_FLAGS(type, name, PROPERTY_FIELD_NO_FLAGS)

/// Like DECLARE_VECTOR_REFERENCE_FIELD_FLAGS, but additionally generates a public setter method
/// \a setterName that replaces the entire list of referenced objects.
#define DECLARE_MODIFIABLE_VECTOR_REFERENCE_FIELD_FLAGS(type, name, setterName, flags) \
    DECLARE_VECTOR_REFERENCE_FIELD_FLAGS(type, name, flags) \
    public: \
        template<typename U> inline void setterName(U&& newList) { OVITO_CHECK_OBJECT_POINTER(this); _##name.setTargets(this, PROPERTY_FIELD(name), std::forward<U>(newList)); } \
        inline void setterName(std::initializer_list<type> newList) { OVITO_CHECK_OBJECT_POINTER(this); _##name.setTargets(this, PROPERTY_FIELD(name), newList); } \
    private:

/// Convenience form of DECLARE_MODIFIABLE_VECTOR_REFERENCE_FIELD_FLAGS with no extra flags.
#define DECLARE_MODIFIABLE_VECTOR_REFERENCE_FIELD(type, name, setterName) \
    DECLARE_MODIFIABLE_VECTOR_REFERENCE_FIELD_FLAGS(type, name, setterName, PROPERTY_FIELD_NO_FLAGS)

/// Assigns a unit class to an animation controller reference or numeric property field.
/// The unit class will automatically be assigned to the numeric input field for this parameter in the user interface.
#define SET_PROPERTY_FIELD_UNITS(DefiningClass, name, ParameterUnitClass)                               \
    Q_DECL_UNUSED static Ovito::NativePropertyFieldDescriptor::PropertyFieldUnitsSetter __unitsSetter##DefiningClass##name(PROPERTY_FIELD(DefiningClass::name), &ParameterUnitClass::staticMetaObject);

/// Assigns a unit class and a minimum value limit to an animation controller reference or numeric property field.
/// The unit class and the value limit will automatically be assigned to the numeric input field for this parameter in the user interface.
#define SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(DefiningClass, name, ParameterUnitClass, minValue) \
    Q_DECL_UNUSED static Ovito::NativePropertyFieldDescriptor::PropertyFieldUnitsSetter __unitsSetter##DefiningClass##name(PROPERTY_FIELD(DefiningClass::name), &ParameterUnitClass::staticMetaObject, minValue);

/// Assigns a unit class and a minimum and maximum value limit to an animation controller reference or numeric property field.
/// The unit class and the value limits will automatically be assigned to the numeric input field for this parameter in the user interface.
#define SET_PROPERTY_FIELD_UNITS_AND_RANGE(DefiningClass, name, ParameterUnitClass, minValue, maxValue) \
    Q_DECL_UNUSED static Ovito::NativePropertyFieldDescriptor::PropertyFieldUnitsSetter __unitsSetter##DefiningClass##name(PROPERTY_FIELD(DefiningClass::name), &ParameterUnitClass::staticMetaObject, minValue, maxValue);

/// Assigns a label string to the given reference or property field.
/// This string will be used in the user interface.
#define SET_PROPERTY_FIELD_LABEL(DefiningClass, name, labelText)                                        \
    Q_DECL_UNUSED static Ovito::NativePropertyFieldDescriptor::PropertyFieldDisplayNameSetter __displayNameSetter##DefiningClass##name(PROPERTY_FIELD(DefiningClass::name), labelText);

/// Use this macro to let the system automatically generate an event of the
/// given type every time the given property field changes its value.
#define SET_PROPERTY_FIELD_CHANGE_EVENT(DefiningClass, name, eventType)                                     \
    Q_DECL_UNUSED static Ovito::NativePropertyFieldDescriptor::PropertyFieldChangeEventSetter __changeEventSetter##DefiningClass##name(PROPERTY_FIELD(DefiningClass::name), eventType);

/// Use this macro to give a property field an alias identifier.
#define SET_PROPERTY_FIELD_ALIAS_IDENTIFIER(DefiningClass, name, aliasName)                                     \
    Q_DECL_UNUSED static Ovito::NativePropertyFieldDescriptor::PropertyFieldAliasIdentifierSetter __aliasSetter##DefiningClass##name(PROPERTY_FIELD(DefiningClass::name), aliasName); \


/// Declares a plain-value property field inside a class definition. Generates the PropertyField<>
/// storage member (initialized to \a init_value, whose type also fixes the field's value type), the
/// field's static descriptor, and a const getter. \a fieldname is the field's unique identifier within
/// the class and \a flags its PropertyFieldFlags. Pair with DEFINE_PROPERTY_FIELD in the .cpp file.
#define DECLARE_PROPERTY_FIELD_FLAGS(init_value, fieldname, flags) \
    private: \
        static Ovito::NativePropertyFieldDescriptor fieldname##__propdescr_instance; \
    public: \
        static inline Ovito::NativePropertyFieldDescriptor* PROPERTY_FIELD(fieldname) { \
            return &fieldname##__propdescr_instance; \
        } \
        Ovito::PropertyField<std::remove_reference_t<decltype(init_value)>, flags> _##fieldname{init_value}; \
        inline std::add_const_t<std::remove_reference_t<decltype(init_value)>>& fieldname() const noexcept { OVITO_CHECK_OBJECT_POINTER(this); return _##fieldname; } \
    private:

/// Defines (in the .cpp file) the descriptor for a non-animatable property field declared
/// with DECLARE_PROPERTY_FIELD*. The descriptor's storage accessor thunks are synthesized
/// generically from a pointer to the field's storage member, so no per-accessor lambdas are
/// emitted at the call site (see the PlainFieldGen constructor of NativePropertyFieldDescriptor).
#define DEFINE_PROPERTY_FIELD(classname, fieldname) \
    Ovito::NativePropertyFieldDescriptor classname::fieldname##__propdescr_instance( \
            Ovito::PlainFieldGen<&classname::_##fieldname>{}, \
            const_cast<classname::OOMetaClass*>(&classname::OOClass()), \
            #fieldname, \
            static_cast<Ovito::PropertyFieldFlags>(static_cast<Ovito::PropertyFieldFlag>(decltype(classname::_##fieldname)::property_field_flags)));

/// Convenience form of DECLARE_PROPERTY_FIELD_FLAGS with no extra flags.
#define DECLARE_PROPERTY_FIELD(init_value, fieldname) \
    DECLARE_PROPERTY_FIELD_FLAGS(init_value, fieldname, Ovito::PROPERTY_FIELD_NO_FLAGS)

/// Like DECLARE_PROPERTY_FIELD_FLAGS, but additionally generates a public setter method \a setterName
/// that assigns a new value to the field (with automatic undo and change notification).
#define DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(init_value, fieldname, setterName, flags) \
    public: \
        void setterName(std::add_const_t<std::remove_reference_t<decltype(init_value)>>& value) { OVITO_CHECK_OBJECT_POINTER(this); _##fieldname.set(this, PROPERTY_FIELD(fieldname), value); } \
        DECLARE_PROPERTY_FIELD_FLAGS(init_value, fieldname, flags)

/// Convenience form of DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS with no extra flags.
#define DECLARE_MODIFIABLE_PROPERTY_FIELD(init_value, fieldname, setterName) \
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(init_value, fieldname, setterName, Ovito::PROPERTY_FIELD_NO_FLAGS)

/***************** Virtual property fields *******************
 *
 * A virtual property field has no backing storage member of its own: its value is computed and stored
 * by a hand-written getter/setter pair of the class. The descriptor still makes the value visible to
 * the reflection system (UI, scripting) via QVariant read/write, but the field is not serialized and
 * not copied when cloning. Because there is no storage member to point at, the descriptor is built with
 * the low-level base constructor and explicit thunks rather than a *FieldGen tag.
 */

/// Declares a virtual property field. \a type is the value type; \a fieldname its identifier. The class
/// must provide the corresponding getter (and a setter, named in DEFINE_VIRTUAL_PROPERTY_FIELD) itself.
#define DECLARE_VIRTUAL_PROPERTY_FIELD(type, fieldname) \
    private: \
        static Ovito::NativePropertyFieldDescriptor fieldname##__propdescr_instance; \
    public: \
        using _##fieldname##__prop_type = type; \
        static inline Ovito::NativePropertyFieldDescriptor* PROPERTY_FIELD(fieldname) { \
            return &fieldname##__propdescr_instance; \
        } \
    private:

/// Defines (in the .cpp file) the descriptor for a virtual property field declared with
/// DECLARE_VIRTUAL_PROPERTY_FIELD. \a fieldname names the getter method and \a setterName the setter
/// method that the read/write thunks call. The field is marked PROPERTY_FIELD_DONT_SERIALIZE.
#define DEFINE_VIRTUAL_PROPERTY_FIELD(classname, fieldname, setterName) \
    Ovito::NativePropertyFieldDescriptor classname::fieldname##__propdescr_instance( \
            const_cast<classname::OOMetaClass*>(&classname::OOClass()), \
            #fieldname, \
            Ovito::PROPERTY_FIELD_DONT_SERIALIZE, \
            [](Ovito::RefMaker* obj, const Ovito::PropertyFieldDescriptor*, const Ovito::RefMaker* other) {}, /* propertyStorageCopyFunc */ \
            [](const Ovito::RefMaker* obj, const Ovito::PropertyFieldDescriptor*) -> QVariant { /* propertyStorageReadFunc */ \
                return QVariant::fromValue(static_cast<const classname*>(obj)->fieldname()); \
            }, \
            [](Ovito::RefMaker* obj, const Ovito::PropertyFieldDescriptor*, const QVariant& newValue) { /* propertyStorageWriteFunc */ \
                static_cast<classname*>(obj)->setterName(newValue.value<classname::_##fieldname##__prop_type>()); \
            }, \
            [](const Ovito::RefMaker* obj, const Ovito::PropertyFieldDescriptor*, const Ovito::RefMaker* other) -> bool { /* propertyStorageCompareFunc */ \
                return static_cast<const classname*>(obj)->fieldname() == static_cast<const classname*>(other)->fieldname(); \
            }, \
            nullptr, /* propertyStorageSaveFunc */ \
            nullptr /* propertyStorageLoadFunc */ \
        );

/***************** Snapshot property fields *******************
 *
 * A snapshot property field augments an existing property field with a second, hidden storage slot that
 * can hold a snapshot of the original field's value. OVITO uses this to remember the value a parameter
 * had right after a file was imported (its "as-loaded default"), so that the UI can later tell which
 * parameters the user has since changed, and offer to reset them. RefMaker::freezeInitialParameterValues()
 * fills the snapshot; RefMaker::copyInitialParametersToObject() restores it into the original field.
 * The snapshot descriptor carries its own take/restore-snapshot thunks in addition to the normal ones.
 *
 * Note: for backward compatibility with existing state files, the snapshot field's *serialized*
 * identifier still ends in the historical suffix "__shadow" (see DEFINE_SNAPSHOT_PROPERTY_FIELD); only
 * the C++ symbols use the clearer "snapshot" naming.
 */

/// Expands to a pointer to the descriptor of a snapshot property field (the snapshot counterpart of
/// PROPERTY_FIELD).
#define SNAPSHOT_PROPERTY_FIELD(RefMakerClassPlusStorageFieldName) \
        RefMakerClassPlusStorageFieldName##__snapshot__propdescr()

/// Declares a snapshot field for the existing property field \a fieldname. Generates the hidden snapshot
/// storage member and its static descriptor. Pair with DEFINE_SNAPSHOT_PROPERTY_FIELD in the .cpp file.
#define DECLARE_SNAPSHOT_PROPERTY_FIELD(fieldname) \
    private: \
        static Ovito::NativePropertyFieldDescriptor fieldname##__snapshot__propdescr_instance; \
    public: \
        static inline Ovito::NativePropertyFieldDescriptor* SNAPSHOT_PROPERTY_FIELD(fieldname) { \
            return &fieldname##__snapshot__propdescr_instance; \
        } \
        Ovito::SnapshotPropertyField<decltype(_##fieldname)::property_type> _##fieldname##__snapshot; \
    private:

/// Defines (in the .cpp file) the descriptor for a snapshot property field declared with
/// DECLARE_SNAPSHOT_PROPERTY_FIELD. The accessor and snapshot thunks are synthesized generically from
/// pointers to the snapshot storage member and the original field's storage member (see the
/// SnapshotFieldGen constructor of NativePropertyFieldDescriptor). The original field's descriptor is
/// passed so that restoring a snapshot is attributed to the original field. The serialized identifier
/// keeps the historical "__shadow" suffix so that state files written by older OVITO versions still load.
#define DEFINE_SNAPSHOT_PROPERTY_FIELD(classname, fieldname) \
    Ovito::NativePropertyFieldDescriptor classname::fieldname##__snapshot__propdescr_instance( \
            Ovito::SnapshotFieldGen<&classname::_##fieldname##__snapshot, &classname::_##fieldname>{}, \
            const_cast<classname::OOMetaClass*>(&classname::OOClass()), \
            #fieldname "__shadow", \
            static_cast<Ovito::PropertyFieldFlags>(decltype(classname::_##fieldname##__snapshot)::property_field_flags), \
            PROPERTY_FIELD(classname::fieldname));

}   // End of namespace
