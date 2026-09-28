// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/particles/Particles.h>
#include <ovito/stdmod/modifiers/CombineDatasetsModifier.h>

namespace Ovito {

/**
 * \brief Combines two particle datasets into one.
 */
class OVITO_PARTICLES_EXPORT ParticlesCombineDatasetsModifierDelegate : public CombineDatasetsModifierDelegate
{
    /// Give this modifier class its own metaclass.
    class OOMetaClass : public CombineDatasetsModifierDelegate::OOMetaClass
    {
    public:

        /// Inherit constructor from base metaclass.
        using CombineDatasetsModifierDelegate::OOMetaClass::OOMetaClass;

        /// Indicates which data objects in the given input data collection the modifier delegate is able to operate on.
        virtual QVector<DataObjectReference> getApplicableObjects(const DataCollection& input) const override;

        /// The name by which Python scripts can refer to this modifier delegate.
        virtual QString pythonDataName() const override { return QStringLiteral("particles"); }
    };

    OVITO_CLASS_META(ParticlesCombineDatasetsModifierDelegate, OOMetaClass)

public:

    /// Applies this modifier delegate to the data.
    virtual Future<PipelineFlowState> apply(const ModifierEvaluationRequest& request, PipelineFlowState&& state, const PipelineFlowState& originalState, const std::vector<PipelineFlowState>& additionalInputs) override;
};

}   // End of namespace
