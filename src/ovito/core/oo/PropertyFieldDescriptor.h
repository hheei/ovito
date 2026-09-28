// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/core/Core.h>
#include <ovito/core/oo/OvitoObject.h>
#include <ovito/core/oo/RefMakerClass.h>
#include <ovito/core/oo/PropertyFieldFlags.h>

namespace Ovito {

/**
 * \brief Optional extra metadata attached to a numeric property or animation controller reference field.
 *
 * Specifies the physical unit the parameter is expressed in (a ParameterUnit subclass, given by its
 * Qt meta-object) together with the admissible value range. The desktop application uses this
 * information to configure the spinner widget for the parameter and to convert between the internal
 * value and the unit-dependent representation shown to the user. Attached to a field with the
 * SET_PROPERTY_FIELD_UNITS* macros.
 */
struct NumericalParameterDescriptor
{
    /// The ParameterUnit-derived class which describes the units of the numerical parameter.
    const QMetaObject* unitType = nullptr;

    /// The minimum value permitted for the parameter.
    FloatType minValue = FLOATTYPE_MIN;

    /// The maximum value permitted for the parameter.
    FloatType maxValue = FLOATTYPE_MAX;
};

/**
 * \brief Run-time descriptor for a single parameter or reference field of a RefMaker-derived class.
 *
 * OVITO maintains a lightweight reflection system for the persistent fields of its object classes.
 * Every RefMaker-derived class declares its fields using the DECLARE_*_FIELD / DEFINE_*_FIELD macro
 * pairs (see NativePropertyFieldDescriptor.h). Each field is backed by exactly one static
 * PropertyFieldDescriptor instance, which provides type-erased, run-time access to the field without
 * the caller needing to know its concrete C++ type. This reflection layer powers all the generic
 * services that must operate uniformly across every object type in OVITO:
 *
 *   - serialization of objects to and from state files (ObjectSaveStream / ObjectLoadStream),
 *   - deep and shallow cloning of object graphs (CloneHelper),
 *   - the parameter editor UI that the desktop application builds automatically from the descriptors,
 *   - read/write access to object parameters from Python scripts,
 *   - undo recording and the generation of ReferenceEvents when a field's value changes.
 *
 * Two kinds of fields are distinguished:
 *
 *   - A *property field* stores a plain value (number, string, enum, color, ...). Its value is exposed
 *     generically as a QVariant through the read/write accessor functions. isReferenceField() is false.
 *   - A *reference field* stores either one (single) or several (vector, isVector() == true) OORef
 *     references to other RefTarget objects. targetClass() returns the common base class of the
 *     permissible reference targets.
 *
 * The descriptor does not store the field's value itself: the value lives in a PropertyField<>,
 * ReferenceField<> or VectorReferenceField<> data member of the owning object. The descriptor only
 * holds the field's metadata (identifier, flags, target class, display name, units, ...) plus a small
 * table of function pointers ("accessor thunks") that translate a type-erased RefMaker* back into the
 * concrete storage member and forward the requested operation to it. For native C++ classes these
 * thunks are synthesized generically from a pointer-to-member by NativePropertyFieldDescriptor.
 *
 * Descriptors come into existence during static initialization, before main() runs. On construction
 * each descriptor links itself into a singly-linked list rooted in the RefMakerClass meta-class of its
 * defining class (unless flagged PROPERTY_FIELD_DONT_REGISTER_IN_CLASS); RefMakerClass::initialize(),
 * invoked once at plugin-registration time, then flattens these per-class lists -- including the fields
 * inherited from base classes -- into the array returned by RefMakerClass::propertyFields(). A field is
 * identified within its class by a stable string identifier(); identifierAlias() provides an optional
 * second name so that fields can be renamed in the source code without breaking older state files.
 */
class OVITO_CORE_EXPORT PropertyFieldDescriptor
{
public:

    /// Low-level constructor for a property field that stores a plain value.
    /// Takes the field's defining meta-class, its unique identifier, behavior flags, and the set of
    /// accessor thunks giving generic read/write/copy/compare/serialize access to the value, plus the
    /// optional take/restore-snapshot thunks used by snapshot fields. The thunks are normally not written
    /// by hand: NativePropertyFieldDescriptor generates them from a pointer-to-member.
    PropertyFieldDescriptor(RefMakerClass* definingClass, const char* identifier, PropertyFieldFlags flags,
            void (*propertyStorageCopyFunc)(RefMaker*, const PropertyFieldDescriptor*, const RefMaker*),
            QVariant (*propertyStorageReadFunc)(const RefMaker*, const PropertyFieldDescriptor*),
            void (*propertyStorageWriteFunc)(RefMaker*, const PropertyFieldDescriptor*, const QVariant&),
            bool (*propertyStorageCompareFunc)(const RefMaker*, const PropertyFieldDescriptor*, const RefMaker*),
            void (*propertyStorageSaveFunc)(const RefMaker*, const PropertyFieldDescriptor*, SaveStream&),
            void (*propertyStorageLoadFunc)(RefMaker*, const PropertyFieldDescriptor*, LoadStream&),
            void (*propertyStorageTakeSnapshotFunc)(RefMaker*, const PropertyFieldDescriptor*) = nullptr,
            void (*propertyStorageRestoreSnapshotFunc)(const RefMaker*, const PropertyFieldDescriptor*, RefMaker*) = nullptr);

    /// Low-level constructor for a reference field that stores a single OORef reference to a RefTarget.
    /// targetClass is the common base class of the permissible reference targets. The two thunks read
    /// and replace the referenced object. Generated by NativePropertyFieldDescriptor from a pointer to
    /// the field's ReferenceField<> storage member.
    PropertyFieldDescriptor(RefMakerClass* definingClass, OvitoClassPtr targetClass, const char* identifier, PropertyFieldFlags flags,
        RefTarget* (*singleReferenceReadFunc)(const RefMaker*, const PropertyFieldDescriptor*),
        void (*singleReferenceWriteFuncRef)(RefMaker*, const PropertyFieldDescriptor*, OORef<const RefTarget>));

    /// Low-level constructor for a reference field that stores a vector of OORef references to RefTargets.
    /// targetClass is the common base class of the permissible reference targets. The thunks query the
    /// list size and get/replace/remove/insert individual elements. Generated by
    /// NativePropertyFieldDescriptor from a pointer to the field's VectorReferenceField<> storage member.
    PropertyFieldDescriptor(RefMakerClass* definingClass, OvitoClassPtr targetClass, const char* identifier, PropertyFieldFlags flags,
        int (*vectorReferenceCountFunc)(const RefMaker*, const PropertyFieldDescriptor*),
        RefTarget* (*vectorReferenceGetFunc)(const RefMaker*, const PropertyFieldDescriptor*, int),
        void (*vectorReferenceSetFunc)(RefMaker*, const PropertyFieldDescriptor*, int, const RefTarget*),
        void (*vectorReferenceRemoveFunc)(RefMaker*, const PropertyFieldDescriptor*, int),
        void (*vectorReferenceInsertFunc)(RefMaker*, const PropertyFieldDescriptor*, int, OORef<RefTarget>));

    /// Returns the unique identifier of the reference field.
    const char* identifier() const { return _identifier; }

    /// Returns the alias identifier of the reference field (used for backward compatibility) if defined.
    const char* identifierAlias() const { return _identifierAlias; }

    /// Returns the RefMaker derived class that owns the reference.
    const RefMakerClass* definingClass() const { return _definingClassDescriptor; }

    /// Returns the base type of the objects stored in this property field if it is a reference field; otherwise returns nullptr.
    OvitoClassPtr targetClass() const { return _targetClassDescriptor; }

    /// Returns whether this is a reference field that stores a pointer to a RefTarget derived class.
    bool isReferenceField() const { return _targetClassDescriptor != nullptr; }

    /// Returns true if this reference field stores a vector of objects.
    bool isVector() const { return _flags.testFlag(PROPERTY_FIELD_VECTOR); }

    /// Returns true if referenced target or the property field's value should not be saved to a scene file.
    bool dontSerialize() const { return _flags.testFlag(PROPERTY_FIELD_DONT_SERIALIZE); }

    /// Returns true if referenced objects should not save their recomputable data to a scene file.
    bool dontSaveRecomputableData() const { return _flags.testFlag(PROPERTY_FIELD_DONT_SAVE_RECOMPUTABLE_DATA); }

    /// Indicates that automatic undo-handling for this property field is enabled.
    /// This is the default.
    bool automaticUndo() const { return !_flags.testFlag(PROPERTY_FIELD_NO_UNDO); }

    /// Returns true if a TargetChanged event should be generated each time the property's value changes.
    bool shouldGenerateChangeEvent() const { return !_flags.testFlag(PROPERTY_FIELD_NO_CHANGE_MESSAGE); }

    /// Return the type of reference event to generate each time this property field's value changes
    /// (in addition to the TargetChanged event, which is generated by default).
    int extraChangeEventType() const { return _extraChangeEventType; }

    /// Returns the human-readable and localized name of the property field.
    /// It will be used as label text in the user interface.
    QString displayName() const;

    /// Sets the human-readable name of this property field. It will be used as label in the user interface.
    void setDisplayName(const QString& name) { OVITO_ASSERT(_displayName.isNull() && !name.isNull()); _displayName = name; }

    /// Returns the next field in the singly-linked registration list of the defining class, or null at
    /// the end. This list contains only the fields declared directly by definingClass(), not those
    /// inherited from base classes; use RefMakerClass::propertyFields() for the full, inherited set.
    const PropertyFieldDescriptor* next() const { return _next; }

    /// Returns a descriptor structure that provides additional settings for a numerical parameter.
    const NumericalParameterDescriptor* numericalParameterInfo() const { return _numericalParameterInfo; }

    /// Sets a descriptor structure that provides additional settings for a numerical parameter.
    void setNumericalParameterInfo(const NumericalParameterDescriptor* numericalInfo) { OVITO_ASSERT(!_numericalParameterInfo && numericalInfo); _numericalParameterInfo = numericalInfo; }

    /// Returns the flags that control the behavior of the property field.
    PropertyFieldFlags flags() const { return _flags; }

    /// Writes the field's current value to the application's persistent settings store as the new
    /// user-defined default. Only meaningful for fields carrying the PROPERTY_FIELD_MEMORIZE flag.
    void memorizeDefaultValue(RefMaker* object) const;

    /// Initializes the field of a newly created object with the user-defined default value previously
    /// stored via memorizeDefaultValue(). Returns true if a stored default was found and applied.
    bool loadDefaultValue(RefMaker* object) const;

protected:

    /// The unique identifier of the reference field. This must be unique within
    /// a RefMaker derived class.
    const char* _identifier;

    /// The base type of the objects stored in this field if this is a reference field.
    OvitoClassPtr _targetClassDescriptor = nullptr;

    /// The RefMaker derived class that owns the property.
    const RefMakerClass* _definingClassDescriptor;

    /// The next property field in the linked list (of the RefMaker derived class defining this property field).
    const PropertyFieldDescriptor* _next;

    /// The flags that control the behavior of the property field.
    PropertyFieldFlags _flags;

    // The following function pointers form this descriptor's type-erased accessor table -- the heart of
    // the reflection mechanism. Each receives the owning object as a RefMaker* (which the thunk casts
    // back to the concrete defining class) and forwards the operation to the field's storage member.
    // For native C++ classes the thunks are generated from a pointer-to-member by the constructors of
    // NativePropertyFieldDescriptor. Property fields populate the *propertyStorage* group; single and
    // vector reference fields populate the *singleReference* / *vectorReference* groups respectively;
    // unused entries remain null.

    /// Copies the value of a plain property field from one object instance to another (used when cloning).
    void (*_propertyStorageCopyFunc)(RefMaker*, const PropertyFieldDescriptor*, const RefMaker*) = nullptr;

    /// Reads the current value of a plain property field, boxed as a QVariant.
    QVariant (*_propertyStorageReadFunc)(const RefMaker*, const PropertyFieldDescriptor*) = nullptr;

    /// Assigns a new value to a plain property field, unboxed from a QVariant.
    void (*_propertyStorageWriteFunc)(RefMaker*, const PropertyFieldDescriptor*, const QVariant&) = nullptr;

    /// Serializes the value of a plain property field to an output stream.
    void (*_propertyStorageSaveFunc)(const RefMaker*, const PropertyFieldDescriptor*, SaveStream&) = nullptr;

    /// Deserializes the value of a plain property field from an input stream.
    void (*_propertyStorageLoadFunc)(RefMaker*, const PropertyFieldDescriptor*, LoadStream&) = nullptr;

    /// Tests the values of a plain property field in two object instances for equality.
    bool (*_propertyStorageCompareFunc)(const RefMaker*, const PropertyFieldDescriptor*, const RefMaker*) = nullptr;

    /// Snapshot fields only: copies the current value of the original field into this field's snapshot slot.
    void (*_propertyStorageTakeSnapshotFunc)(RefMaker*, const PropertyFieldDescriptor*) = nullptr;

    /// Snapshot fields only: writes a previously taken snapshot back into the original field of another instance.
    void (*_propertyStorageRestoreSnapshotFunc)(const RefMaker*, const PropertyFieldDescriptor*, RefMaker*) = nullptr;

    /// Single reference fields only: returns the currently referenced target object.
    RefTarget* (*_singleReferenceReadFunc)(const RefMaker*, const PropertyFieldDescriptor*) = nullptr;

    /// Single reference fields only: replaces the referenced target object.
    void (*_singleReferenceWriteFuncRef)(RefMaker*, const PropertyFieldDescriptor*, OORef<const RefTarget>) = nullptr;

    /// Vector reference fields only: returns the number of referenced target objects.
    int (*_vectorReferenceCountFunc)(const RefMaker*, const PropertyFieldDescriptor*) = nullptr;

    /// Vector reference fields only: returns the i-th referenced target object.
    RefTarget* (*_vectorReferenceGetFunc)(const RefMaker*, const PropertyFieldDescriptor*, int) = nullptr;

    /// Vector reference fields only: replaces the i-th referenced target object.
    void (*_vectorReferenceSetFunc)(RefMaker*, const PropertyFieldDescriptor*, int, const RefTarget*) = nullptr;

    /// Vector reference fields only: erases the i-th referenced target object.
    void (*_vectorReferenceRemoveFunc)(RefMaker*, const PropertyFieldDescriptor*, int) = nullptr;

    /// Vector reference fields only: inserts a target object at the given position.
    void (*_vectorReferenceInsertFunc)(RefMaker*, const PropertyFieldDescriptor*, int, OORef<RefTarget>) = nullptr;

    /// The human-readable name of this property field. It is used as label in the user interface.
    QString _displayName;

    /// Provides further settings for numerical object parameters.
    const NumericalParameterDescriptor* _numericalParameterInfo = nullptr;

    /// The type of reference event to generate each time this property field's value changes.
    int _extraChangeEventType = 0;

    /// The alias identifier of the reference field. This can be set for backward compatibility with older OVITO versions.
    const char* _identifierAlias = nullptr;

    friend class RefMaker;
    friend class RefTarget;
    friend class ObjectSaveStream; // Needs direct access to _propertyStorageSaveFunc
    friend class ObjectLoadStream; // Needs direct access to _propertyStorageLoadFunc
};

}   // End of namespace
