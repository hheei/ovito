// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/crystalanalysis/CrystalAnalysis.h>
#include <ovito/stdmod/modifiers/AffineTransformationModifier.h>

namespace Ovito {

/**
 * \brief Delegate for the AffineTransformationModifier that operates on dislocation lines.
 */
class DislocationAffineTransformationModifierDelegate : public AffineTransformationModifierDelegate
{
    /// Give the modifier delegate its own metaclass.
    class OOMetaClass : public AffineTransformationModifierDelegate::OOMetaClass
    {
    public:

        /// Inherit constructor from base class.
        using AffineTransformationModifierDelegate::OOMetaClass::OOMetaClass;

        /// Indicates which data objects in the given input data collection the modifier delegate is able to operate on.
        virtual QVector<DataObjectReference> getApplicableObjects(const DataCollection& input) const override;

        /// Indicates which class of data objects the modifier delegate is able to operate on.
        virtual const DataObject::OOMetaClass& getApplicableObjectClass() const override { return DislocationNetwork::OOClass(); }

        /// The name by which Python scripts can refer to this modifier delegate.
        virtual QString pythonDataName() const override { return QStringLiteral("dislocations"); }
    };

    OVITO_CLASS_META(DislocationAffineTransformationModifierDelegate, OOMetaClass)

public:

    /// Applies this modifier delegate to the data.
    virtual Future<PipelineFlowState> apply(const ModifierEvaluationRequest& request, PipelineFlowState&& state, const PipelineFlowState& originalState, const std::vector<PipelineFlowState>& additionalInputs) override;
};

}   // End of namespace
