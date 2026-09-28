// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/mesh/Mesh.h>
#include <ovito/mesh/surface/SurfaceMesh.h>
#include <ovito/stdmod/modifiers/AffineTransformationModifier.h>

namespace Ovito {

/**
 * \brief Delegate for the AffineTransformationModifier that operates on surface meshes.
 */
class OVITO_MESHMOD_EXPORT SurfaceMeshAffineTransformationModifierDelegate : public AffineTransformationModifierDelegate
{
    /// Give the modifier delegate its own metaclass.
    class SurfaceMeshAffineTransformationModifierDelegateClass : public AffineTransformationModifierDelegate::OOMetaClass
    {
    public:

        /// Inherit constructor from base class.
        using AffineTransformationModifierDelegate::OOMetaClass::OOMetaClass;

        /// Indicates which data objects in the given input data collection the modifier delegate is able to operate on.
        virtual QVector<DataObjectReference> getApplicableObjects(const DataCollection& input) const override;

        /// Indicates which class of data objects the modifier delegate is able to operate on.
        virtual const DataObject::OOMetaClass& getApplicableObjectClass() const override { return SurfaceMesh::OOClass(); }

        /// The name by which Python scripts can refer to this modifier delegate.
        virtual QString pythonDataName() const override { return QStringLiteral("surfaces"); }
    };

    OVITO_CLASS_META(SurfaceMeshAffineTransformationModifierDelegate, SurfaceMeshAffineTransformationModifierDelegateClass)

public:

    /// Applies this modifier delegate to the data.
    virtual Future<PipelineFlowState> apply(const ModifierEvaluationRequest& request, PipelineFlowState&& state, const PipelineFlowState& originalState, const std::vector<PipelineFlowState>& additionalInputs) override;
};

}   // End of namespace
