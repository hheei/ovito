// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/grid/Grid.h>
#include <ovito/grid/objects/VoxelGrid.h>
#include <ovito/stdmod/modifiers/ComputePropertyModifier.h>

namespace Ovito {

/**
 * \brief Delegate plugin for the ComputePropertyModifier that operates on voxel grids.
 */
class OVITO_GRID_EXPORT VoxelGridComputePropertyModifierDelegate : public ComputePropertyModifierDelegate
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
        virtual const DataObject::OOMetaClass& getApplicableObjectClass() const override { return VoxelGrid::OOClass(); }

        /// The name by which Python scripts can refer to this modifier delegate.
        virtual QString pythonDataName() const override { return QStringLiteral("voxels"); }
    };

    OVITO_CLASS_META(VoxelGridComputePropertyModifierDelegate, OOMetaClass)

protected:

    /// Initializes an expression evaluator.
    virtual std::unique_ptr<PropertyExpressionEvaluator> initializeExpressionEvaluator(const ComputePropertyModifier* modifier, const PipelineFlowState& originalState, int frame) const override;
};

}   // End of namespace
