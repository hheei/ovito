// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/particles/Particles.h>
#include <ovito/particles/util/ParticleExpressionEvaluator.h>
#include <ovito/particles/objects/Particles.h>
#include <ovito/particles/objects/Bonds.h>
#include "ParticlesExpressionSelectionModifierDelegate.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(ParticlesExpressionSelectionModifierDelegate);
OVITO_CLASSINFO(ParticlesExpressionSelectionModifierDelegate, "DisplayName", "Particles");
IMPLEMENT_CREATABLE_OVITO_CLASS(BondsExpressionSelectionModifierDelegate);
OVITO_CLASSINFO(BondsExpressionSelectionModifierDelegate, "DisplayName", "Bonds");

/******************************************************************************
* Indicates which data objects in the given input data collection the modifier
* delegate is able to operate on.
******************************************************************************/
QVector<DataObjectReference> ParticlesExpressionSelectionModifierDelegate::OOMetaClass::getApplicableObjects(const DataCollection& input) const
{
    if(input.containsObject<Particles>())
        return { DataObjectReference(&Particles::OOClass()) };
    return {};
}

/******************************************************************************
* Creates and initializes the expression evaluator object.
******************************************************************************/
std::unique_ptr<PropertyExpressionEvaluator> ParticlesExpressionSelectionModifierDelegate::initializeExpressionEvaluator(const QStringList& expressions, const PipelineFlowState& inputState, const ConstDataObjectPath& containerPath, int animationFrame)
{
    std::unique_ptr<ParticleExpressionEvaluator> evaluator = std::make_unique<ParticleExpressionEvaluator>();
    evaluator->initializeInputs(inputState, containerPath, animationFrame);
    evaluator->initializeExpressions(expressions);
    return evaluator;
}

/******************************************************************************
* Indicates which data objects in the given input data collection the modifier
* delegate is able to operate on.
******************************************************************************/
QVector<DataObjectReference> BondsExpressionSelectionModifierDelegate::OOMetaClass::getApplicableObjects(const DataCollection& input) const
{
    if(const Particles* particles = input.getObject<Particles>()) {
        if(particles->bonds())
            return { DataObjectReference(ConstDataObjectPath({particles, particles->bonds()})) };
    }
    return {};
}

/******************************************************************************
* Creates and initializes the expression evaluator object.
******************************************************************************/
std::unique_ptr<PropertyExpressionEvaluator> BondsExpressionSelectionModifierDelegate::initializeExpressionEvaluator(const QStringList& expressions, const PipelineFlowState& inputState, const ConstDataObjectPath& containerPath, int animationFrame)
{
    std::unique_ptr<BondExpressionEvaluator> evaluator = std::make_unique<BondExpressionEvaluator>();
    evaluator->initializeInputs(inputState, containerPath, animationFrame);
    evaluator->initializeExpressions(expressions);
    return evaluator;
}

}   // End of namespace
