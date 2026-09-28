// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once

#include <ovito/grid/Grid.h>
#include <ovito/grid/objects/VoxelGrid.h>
#include <ovito/mesh/surface/SurfaceMeshVis.h>
#include <ovito/mesh/surface/SurfaceMeshBuilder.h>
#include <ovito/stdobj/table/DataTable.h>
#include <ovito/core/dataset/pipeline/Modifier.h>

namespace Ovito {

/*
 * Constructs an isosurface from a data grid.
 */
class OVITO_GRID_EXPORT CreateIsosurfaceModifier : public Modifier
{
    /// Give this modifier class its own metaclass.
    class CreateIsosurfaceModifierClass : public ModifierClass
    {
    public:
        /// Inherit constructor from base class.
        using ModifierClass::ModifierClass;

        /// Asks the metaclass whether the modifier can be applied to the given input data.
        virtual bool isApplicableTo(const DataCollection& input) const override;
    };

    OVITO_CLASS_META(CreateIsosurfaceModifier, CreateIsosurfaceModifierClass)

public:

    /// Constructor.
    void initializeObject(ObjectInitializationFlags flags);

    /// This method is called by the system after the modifier has been inserted into a data pipeline.
    virtual void initializeModifier(const ModifierInitializationRequest& request) override;

    /// Modifies the input data.
    virtual Future<PipelineFlowState> evaluateModifier(const ModifierEvaluationRequest& request, PipelineFlowState&& state) override;

    /// Is called by the pipeline system before a new modifier evaluation begins.
    virtual void preevaluateModifier(const ModifierEvaluationRequest& request, PipelineEvaluationResult::EvaluationTypes& evaluationTypes, TimeInterval& validityInterval) const override;

    /// Returns the level at which to create the isosurface.
    FloatType isolevel() const { return isolevelController() ? isolevelController()->getFloatValue(AnimationTime(0)) : 0; }

    /// Sets the level at which to create the isosurface.
    void setIsolevel(FloatType value)
    {
        if(isolevelController()) isolevelController()->setFloatValue(AnimationTime(0), value);
    }

    /// Returns a short piece of information (typically a string or color) to be displayed next to the modifier's title in the pipeline editor.
    virtual QVariant getPipelineEditorShortInfo(Scene* scene, ModificationNode* node) const override {
        // Note: Whenever the source property or the isolevel changes, we trigger a ReferenceEvent::ObjectStatusChanged event in propertyChanged() and referenceEvent(), respectively.
        if(sourceProperty())
            return QStringLiteral("%1=%2").arg(sourceProperty().nameWithComponent()).arg(isolevel());
        return {};
    }

    /// Transfers voxel grid properties to the vertices of a surfaces mesh.
    static void transferPropertiesFromGridToMesh(SurfaceMeshBuilder& mesh, const std::vector<ConstPropertyPtr>& fieldProperties,
                                                 const SimulationCell& gridDomain, VoxelGrid::GridDimensions gridShape,
                                                 VoxelGrid::GridType gridType, TaskProgress& progress);

protected:

    /// Is called when the value of a property of this object has changed.
    virtual void propertyChanged(const PropertyFieldDescriptor* field) override;

    /// Is called when a RefTarget referenced by this object generated an event.
    virtual bool referenceEvent(RefTarget* source, const ReferenceEvent& event) override;

private:

    /// Specifies the voxel grid this modifier should operate on.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(TypedDataObjectReference<VoxelGrid>{}, inputGrid, setInputGrid);

    /// The voxel property that serves as input.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(PropertyReference{}, sourceProperty, setSourceProperty);

    /// This controller stores the level at which to create the isosurface.
    DECLARE_MODIFIABLE_REFERENCE_FIELD_FLAGS(OORef<Controller>, isolevelController, setIsolevelController, PROPERTY_FIELD_MEMORIZE);

    /// Controls whether auxiliary field values should be copied over from the grid to the generated isosurface vertices.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool{false}, transferFieldValues, setTransferFieldValues, PROPERTY_FIELD_MEMORIZE);

    /// Controls whether the algorithm should identify disconnected spatial regions.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{false}, identifyRegions, setIdentifyRegions);

    // Controls the amount of smoothing.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(int{0}, smoothingLevel, setSmoothingLevel, PROPERTY_FIELD_MEMORIZE);

    /// The vis element for rendering the surface.
    DECLARE_MODIFIABLE_REFERENCE_FIELD_FLAGS(OORef<SurfaceMeshVis>, surfaceMeshVis, setSurfaceMeshVis, PROPERTY_FIELD_DONT_PROPAGATE_MESSAGES | PROPERTY_FIELD_MEMORIZE | PROPERTY_FIELD_OPEN_SUBEDITOR);
};

}  // namespace Ovito
