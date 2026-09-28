// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/particles/Particles.h>
#include <ovito/stdmod/modifiers/AffineTransformationModifier.h>

namespace Ovito {

/**
 * \brief Delegate for the AffineTransformationModifier that operates on particles.
 */
class ParticlesAffineTransformationModifierDelegate : public AffineTransformationModifierDelegate
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
        virtual const DataObject::OOMetaClass& getApplicableObjectClass() const override { return Particles::OOClass(); }

        /// The name by which Python scripts can refer to this modifier delegate.
        virtual QString pythonDataName() const override { return QStringLiteral("particles"); }
    };

    OVITO_CLASS_META(ParticlesAffineTransformationModifierDelegate, OOMetaClass)

public:

    /// Applies this modifier delegate to the data.
    virtual Future<PipelineFlowState> apply(const ModifierEvaluationRequest& request, PipelineFlowState&& state, const PipelineFlowState& originalState, const std::vector<PipelineFlowState>& additionalInputs) override;
};

/**
 * \brief Delegate for the AffineTransformationModifier that operates on any kind of vector property.
 */
class VectorParticlePropertiesAffineTransformationModifierDelegate : public AffineTransformationModifierDelegate
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
        virtual const DataObject::OOMetaClass& getApplicableObjectClass() const override { return Property::OOClass(); }

        /// The name by which Python scripts can refer to this modifier delegate.
        virtual QString pythonDataName() const override { return QStringLiteral("vector_properties"); }
    };

    OVITO_CLASS_META(VectorParticlePropertiesAffineTransformationModifierDelegate, OOMetaClass)

public:

    /// Applies this modifier delegate to the data.
    virtual Future<PipelineFlowState> apply(const ModifierEvaluationRequest& request, PipelineFlowState&& state, const PipelineFlowState& originalState, const std::vector<PipelineFlowState>& additionalInputs) override;

private:

    /// Decides if the given particle property is one that should be transformed.
    static bool isTransformableProperty(const Property* property);
};

}   // End of namespace
