// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/grid/Grid.h>
#include <ovito/grid/objects/VoxelGrid.h>
#include <ovito/stdmod/modifiers/SliceModifier.h>

namespace Ovito {

/**
 * \brief Slice function that operates on voxel grids.
 */
class OVITO_GRID_EXPORT VoxelGridSliceModifierDelegate : public SliceModifierDelegate
{
    /// Give the modifier delegate its own metaclass.
    class VoxelGridSliceModifierDelegateClass : public SliceModifierDelegate::OOMetaClass
    {
    public:

        /// Inherit constructor from base class.
        using SliceModifierDelegate::OOMetaClass::OOMetaClass;

        /// Indicates which data objects in the given input data collection the modifier delegate is able to operate on.
        virtual QVector<DataObjectReference> getApplicableObjects(const DataCollection& input) const override;

        /// Indicates which class of data objects the modifier delegate is able to operate on.
        virtual const DataObject::OOMetaClass& getApplicableObjectClass() const override { return VoxelGrid::OOClass(); }

        /// The name by which Python scripts can refer to this modifier delegate.
        virtual QString pythonDataName() const override { return QStringLiteral("voxels"); }
    };

    OVITO_CLASS_META(VoxelGridSliceModifierDelegate, VoxelGridSliceModifierDelegateClass)

public:

    /// Constructor.
    void initializeObject(ObjectInitializationFlags flags);

    /// Applies this modifier delegate to the data.
    virtual Future<PipelineFlowState> apply(const ModifierEvaluationRequest& request, PipelineFlowState&& state, const PipelineFlowState& originalState, const std::vector<PipelineFlowState>& additionalInputs) override;

private:

    /// The vis element for rendering the generated mesh.
    DECLARE_MODIFIABLE_REFERENCE_FIELD_FLAGS(OORef<SurfaceMeshVis>, surfaceMeshVis, setSurfaceMeshVis, PROPERTY_FIELD_DONT_PROPAGATE_MESSAGES | PROPERTY_FIELD_MEMORIZE | PROPERTY_FIELD_OPEN_SUBEDITOR);
};

}   // End of namespace
