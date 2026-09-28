// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/particles/Particles.h>
#include <ovito/stdmod/modifiers/ExpressionSelectionModifier.h>

namespace Ovito {

/**
 * \brief Delegate for the ExpressionSelectionModifier that operates on particles.
 */
class ParticlesExpressionSelectionModifierDelegate : public ExpressionSelectionModifierDelegate
{
    /// Give the modifier delegate its own metaclass.
    class OOMetaClass : public ExpressionSelectionModifierDelegate::OOMetaClass
    {
    public:

        /// Inherit constructor from base class.
        using ExpressionSelectionModifierDelegate::OOMetaClass::OOMetaClass;

        /// Indicates which data objects in the given input data collection the modifier delegate is able to operate on.
        virtual QVector<DataObjectReference> getApplicableObjects(const DataCollection& input) const override;

        /// Indicates which class of data objects the modifier delegate is able to operate on.
        virtual const DataObject::OOMetaClass& getApplicableObjectClass() const override { return Particles::OOClass(); }

        /// The name by which Python scripts can refer to this modifier delegate.
        virtual QString pythonDataName() const override { return QStringLiteral("particles"); }
    };

    OVITO_CLASS_META(ParticlesExpressionSelectionModifierDelegate, OOMetaClass)

public:

    /// Constructor.
    using ExpressionSelectionModifierDelegate::ExpressionSelectionModifierDelegate;

    /// Creates and initializes the expression evaluator object.
    virtual std::unique_ptr<PropertyExpressionEvaluator> initializeExpressionEvaluator(const QStringList& expressions, const PipelineFlowState& inputState, const ConstDataObjectPath& containerPath, int animationFrame) override;
};

/**
 * \brief Delegate for the ExpressionSelectionModifier that operates on bonds.
 */
class BondsExpressionSelectionModifierDelegate : public ExpressionSelectionModifierDelegate
{
    /// Give the modifier delegate its own metaclass.
    class OOMetaClass : public ExpressionSelectionModifierDelegate::OOMetaClass
    {
    public:

        /// Inherit constructor from base class.
        using ExpressionSelectionModifierDelegate::OOMetaClass::OOMetaClass;

        /// Indicates which data objects in the given input data collection the modifier delegate is able to operate on.
        virtual QVector<DataObjectReference> getApplicableObjects(const DataCollection& input) const override;

        /// Indicates which class of data objects the modifier delegate is able to operate on.
        virtual const DataObject::OOMetaClass& getApplicableObjectClass() const override { return Bonds::OOClass(); }

        /// The name by which Python scripts can refer to this modifier delegate.
        virtual QString pythonDataName() const override { return QStringLiteral("bonds"); }
    };

    OVITO_CLASS_META(BondsExpressionSelectionModifierDelegate, OOMetaClass)

public:

    /// Constructor.
    using ExpressionSelectionModifierDelegate::ExpressionSelectionModifierDelegate;

    /// Creates and initializes the expression evaluator object.
    virtual std::unique_ptr<PropertyExpressionEvaluator> initializeExpressionEvaluator(const QStringList& expressions, const PipelineFlowState& inputState, const ConstDataObjectPath& containerPath, int animationFrame) override;
};

}   // End of namespace
