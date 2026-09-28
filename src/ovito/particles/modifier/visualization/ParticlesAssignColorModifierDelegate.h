// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/particles/Particles.h>
#include <ovito/particles/objects/Particles.h>
#include <ovito/particles/objects/Bonds.h>
#include <ovito/stdmod/modifiers/AssignColorModifier.h>

namespace Ovito {

/**
 * \brief Function for the AssignColorModifier that operates on particles.
 */
class ParticlesAssignColorModifierDelegate : public AssignColorModifierDelegate
{
    /// Give the modifier delegate its own metaclass.
    class ParticlesAssignColorModifierDelegateClass : public AssignColorModifierDelegate::OOMetaClass
    {
    public:

        /// Inherit constructor from base class.
        using AssignColorModifierDelegate::OOMetaClass::OOMetaClass;

        /// Indicates which data objects in the given input data collection the modifier delegate is able to operate on.
        virtual QVector<DataObjectReference> getApplicableObjects(const DataCollection& input) const override;

        /// Indicates which class of data objects the modifier delegate is able to operate on.
        virtual const DataObject::OOMetaClass& getApplicableObjectClass() const override { return Particles::OOClass(); }

        /// The name by which Python scripts can refer to this modifier delegate.
        virtual QString pythonDataName() const override { return QStringLiteral("particles"); }
    };

    OVITO_CLASS_META(ParticlesAssignColorModifierDelegate, ParticlesAssignColorModifierDelegateClass)

protected:

    /// \brief returns the ID of the standard property that will receive the assigned colors.
    virtual int outputColorPropertyId() const override { return Particles::ColorProperty; }
};

/**
 * \brief Function for the AssignColorModifier that operates on particle vectors.
 */
class ParticleVectorsAssignColorModifierDelegate : public AssignColorModifierDelegate
{
    /// Give the modifier delegate its own metaclass.
    class OOMetaClass : public AssignColorModifierDelegate::OOMetaClass
    {
    public:

        /// Inherit constructor from base class.
        using AssignColorModifierDelegate::OOMetaClass::OOMetaClass;

        /// Indicates which data objects in the given input data collection the modifier delegate is able to operate on.
        virtual QVector<DataObjectReference> getApplicableObjects(const DataCollection& input) const override;

        /// Indicates which class of data objects the modifier delegate is able to operate on.
        virtual const DataObject::OOMetaClass& getApplicableObjectClass() const override { return Particles::OOClass(); }

        /// The name by which Python scripts can refer to this modifier delegate.
        virtual QString pythonDataName() const override { return QStringLiteral("vectors"); }
    };

    OVITO_CLASS_META(ParticleVectorsAssignColorModifierDelegate, OOMetaClass)

protected:

    /// \brief returns the ID of the standard property that will receive the assigned colors.
    virtual int outputColorPropertyId() const override { return Particles::VectorColorProperty; }
};

/**
 * \brief Function for the AssignColorModifier that operates on bonds.
 */
class BondsAssignColorModifierDelegate : public AssignColorModifierDelegate
{
    /// Give the modifier delegate its own metaclass.
    class BondsAssignColorModifierDelegateClass : public AssignColorModifierDelegate::OOMetaClass
    {
    public:

        /// Inherit constructor from base class.
        using AssignColorModifierDelegate::OOMetaClass::OOMetaClass;

        /// Indicates which data objects in the given input data collection the modifier delegate is able to operate on.
        virtual QVector<DataObjectReference> getApplicableObjects(const DataCollection& input) const override;

        /// Indicates which class of data objects the modifier delegate is able to operate on.
        virtual const DataObject::OOMetaClass& getApplicableObjectClass() const override { return Bonds::OOClass(); }

        /// The name by which Python scripts can refer to this modifier delegate.
        virtual QString pythonDataName() const override { return QStringLiteral("bonds"); }
    };

    OVITO_CLASS_META(BondsAssignColorModifierDelegate, BondsAssignColorModifierDelegateClass)

protected:

    /// \brief returns the ID of the standard property that will receive the computed colors.
    virtual int outputColorPropertyId() const override { return Bonds::ColorProperty; }
};

}   // End of namespace
