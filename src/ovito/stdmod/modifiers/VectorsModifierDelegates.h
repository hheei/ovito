// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once

#include <ovito/stdmod/StdMod.h>
#include <ovito/stdobj/vectors/Vectors.h>
#include "ExpressionSelectionModifier.h"
#include "DeleteSelectedModifier.h"
#include "ComputePropertyModifier.h"

namespace Ovito {

/**
 * \brief Delegate for the ExpressionSelectionModifier that operates on vectors.
 */
class VectorsExpressionSelectionModifierDelegate : public ExpressionSelectionModifierDelegate
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
        virtual const DataObject::OOMetaClass& getApplicableObjectClass() const override { return Vectors::OOClass(); }

        /// The name by which Python scripts can refer to this modifier delegate.
        virtual QString pythonDataName() const override { return QStringLiteral("vectors"); }
    };

    OVITO_CLASS_META(VectorsExpressionSelectionModifierDelegate, OOMetaClass)
};

/**
 * \brief Delegate for the DeleteSelectedModifier that operates on lines.
 */
class VectorsDeleteSelectedModifierDelegate : public DeleteSelectedModifierDelegate
{
    /// Give the modifier delegate its own metaclass.
    class OOMetaClass : public DeleteSelectedModifierDelegate::OOMetaClass
    {
    public:
        /// Inherit constructor from base class.
        using DeleteSelectedModifierDelegate::OOMetaClass::OOMetaClass;

        /// Indicates which data objects in the given input data collection the modifier delegate is able to operate on.
        virtual QVector<DataObjectReference> getApplicableObjects(const DataCollection& input) const override;

        /// Indicates which class of data objects the modifier delegate is able to operate on.
        virtual const DataObject::OOMetaClass& getApplicableObjectClass() const override { return Vectors::OOClass(); }

        /// The name by which Python scripts can refer to this modifier delegate.
        virtual QString pythonDataName() const override { return QStringLiteral("vectors"); }
    };

    OVITO_CLASS_META(VectorsDeleteSelectedModifierDelegate, OOMetaClass)

public:
    /// Applies this modifier delegate to the data.
    virtual Future<PipelineFlowState> apply(const ModifierEvaluationRequest& request, PipelineFlowState&& state, const PipelineFlowState& originalState, const std::vector<PipelineFlowState>& additionalInputs) override;
};

/**
 * \brief Delegate plugin for the ComputePropertyModifier that operates on lines.
 */
class VectorsComputePropertyModifierDelegate : public ComputePropertyModifierDelegate
{
    /// Give the modifier delegate its own metaclass.
    class OOMetaClass : public ComputePropertyModifierDelegate::OOMetaClass
    {
    public:
        /// Inherit constructor from base class.
        using ComputePropertyModifierDelegate::OOMetaClass::OOMetaClass;

        /// Indicates which data objects in the given input data collection the modifier delegate is able to operate on.
        virtual QVector<DataObjectReference> getApplicableObjects(const DataCollection& input) const override;

        /// Indicates which class of data objects the modifier delegate is able to operate on.
        virtual const DataObject::OOMetaClass& getApplicableObjectClass() const override { return Vectors::OOClass(); }

        /// The name by which Python scripts can refer to this modifier delegate.
        virtual QString pythonDataName() const override { return QStringLiteral("vectors"); }
    };

    OVITO_CLASS_META(VectorsComputePropertyModifierDelegate, OOMetaClass)
};

}  // namespace Ovito
