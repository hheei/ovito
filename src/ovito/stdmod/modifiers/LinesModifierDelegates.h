// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once

#include <ovito/stdmod/StdMod.h>
#include <ovito/stdobj/lines/Lines.h>
#include <ovito/stdmod/modifiers/ExpressionSelectionModifier.h>
#include <ovito/stdmod/modifiers/DeleteSelectedModifier.h>
#include <ovito/stdmod/modifiers/ComputePropertyModifier.h>
#include <ovito/stdmod/modifiers/AssignColorModifier.h>

namespace Ovito {

/**
 * \brief Delegate plugin for the AssignColorModifier that operates on lines.
 */
class LinesAssignColorModifierDelegate : public AssignColorModifierDelegate
{
    /// Give the modifier delegate its own metaclass.
    class LinesAssignColorModifierDelegateClass : public AssignColorModifierDelegate::OOMetaClass
    {
    public:
        /// Inherit constructor from base class.
        using AssignColorModifierDelegate::OOMetaClass::OOMetaClass;

        /// Indicates which data objects in the given input data collection the modifier delegate is able to operate on.
        [[nodiscard]] virtual QVector<DataObjectReference> getApplicableObjects(const DataCollection& input) const override;

        /// Indicates which class of data objects the modifier delegate is able to operate on.
        [[nodiscard]] virtual const DataObject::OOMetaClass& getApplicableObjectClass() const override { return Lines::OOClass(); }

        /// The name by which Python scripts can refer to this modifier delegate.
        [[nodiscard]] virtual QString pythonDataName() const override { return QStringLiteral("lines"); }
    };

    OVITO_CLASS_META(LinesAssignColorModifierDelegate, LinesAssignColorModifierDelegateClass)

protected:
    /// \brief returns the ID of the standard property that will receive the assigned colors.
    virtual int outputColorPropertyId() const override { return Lines::ColorProperty; }
};

/**
 * \brief Delegate for the ExpressionSelectionModifier that operates on lines.
 */
class LinesExpressionSelectionModifierDelegate : public ExpressionSelectionModifierDelegate
{
    /// Give the modifier delegate its own metaclass.
    class OOMetaClass : public ExpressionSelectionModifierDelegate::OOMetaClass
    {
    public:
        /// Inherit constructor from base class.
        using ExpressionSelectionModifierDelegate::OOMetaClass::OOMetaClass;

        /// Indicates which data objects in the given input data collection the modifier delegate is able to operate on.
        [[nodiscard]] virtual QVector<DataObjectReference> getApplicableObjects(const DataCollection& input) const override;

        /// Indicates which class of data objects the modifier delegate is able to operate on.
        [[nodiscard]] virtual const DataObject::OOMetaClass& getApplicableObjectClass() const override { return Lines::OOClass(); }

        /// The name by which Python scripts can refer to this modifier delegate.
        [[nodiscard]] virtual QString pythonDataName() const override { return QStringLiteral("lines"); }
    };

    OVITO_CLASS_META(LinesExpressionSelectionModifierDelegate, OOMetaClass)
};

/**
 * \brief Delegate for the DeleteSelectedModifier that operates on lines.
 */
class LinesDeleteSelectedModifierDelegate : public DeleteSelectedModifierDelegate
{
    /// Give the modifier delegate its own metaclass.
    class OOMetaClass : public DeleteSelectedModifierDelegate::OOMetaClass
    {
    public:
        /// Inherit constructor from base class.
        using DeleteSelectedModifierDelegate::OOMetaClass::OOMetaClass;

        /// Indicates which data objects in the given input data collection the modifier delegate is able to operate on.
        [[nodiscard]] virtual QVector<DataObjectReference> getApplicableObjects(const DataCollection& input) const override;

        /// Indicates which class of data objects the modifier delegate is able to operate on.
        [[nodiscard]] virtual const DataObject::OOMetaClass& getApplicableObjectClass() const override { return Lines::OOClass(); }

        /// The name by which Python scripts can refer to this modifier delegate.
        [[nodiscard]] virtual QString pythonDataName() const override { return QStringLiteral("lines"); }
    };

    OVITO_CLASS_META(LinesDeleteSelectedModifierDelegate, OOMetaClass)

public:
    /// Applies this modifier delegate to the data.
    virtual Future<PipelineFlowState> apply(const ModifierEvaluationRequest& request, PipelineFlowState&& state,
                                            const PipelineFlowState& originalState,
                                            const std::vector<PipelineFlowState>& additionalInputs) override;
};

/**
 * \brief Delegate plugin for the ComputePropertyModifier that operates on lines.
 */
class LinesComputePropertyModifierDelegate : public ComputePropertyModifierDelegate
{
    /// Give the modifier delegate its own metaclass.
    class OOMetaClass : public ComputePropertyModifierDelegate::OOMetaClass
    {
    public:
        /// Inherit constructor from base class.
        using ComputePropertyModifierDelegate::OOMetaClass::OOMetaClass;

        /// Indicates which data objects in the given input data collection the modifier delegate is able to operate on.
        [[nodiscard]] virtual QVector<DataObjectReference> getApplicableObjects(const DataCollection& input) const override;

        /// Indicates which class of data objects the modifier delegate is able to operate on.
        [[nodiscard]] virtual const DataObject::OOMetaClass& getApplicableObjectClass() const override { return Lines::OOClass(); }

        /// The name by which Python scripts can refer to this modifier delegate.
        [[nodiscard]] virtual QString pythonDataName() const override { return QStringLiteral("lines"); }
    };

    OVITO_CLASS_META(LinesComputePropertyModifierDelegate, OOMetaClass)
};

}  // namespace Ovito
