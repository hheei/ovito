// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/stdobj/StdObj.h>
#include <ovito/core/dataset/data/DataVis.h>

namespace Ovito {

/**
 * \brief A visual element that renders a SimulationCell as a wireframe box.
 */
class OVITO_STDOBJ_EXPORT SimulationCellVis : public DataVis
{
    OVITO_CLASS(SimulationCellVis)

public:

    /// Lets the visualization element render the data object.
    virtual PipelineStatus renderSynchronous(const ConstDataObjectPath& path, const PipelineFlowState& flowState, FrameGraph& frameGraph, const SceneNode* sceneNode) override;

    /// Computes the bounding box of the object.
    virtual Box3 boundingBoxImmediate(AnimationTime time, const ConstDataObjectPath& path, const Pipeline* pipeline, const PipelineFlowState& flowState, TimeInterval& validityInterval) override;

    /// Indicates whether this object should be surrounded by a selection marker in the viewports when it is selected.
    virtual bool showSelectionMarker() override { return false; }

protected:

    /// Renders the given simulation using wireframe mode.
    void renderWireframe(const SimulationCell* cell, const PipelineFlowState& flowState, FrameGraph& frameGraph, const SceneNode* sceneNode);

    /// Renders the given simulation using solid shading mode.
    void renderSolid(const SimulationCell* cell, const PipelineFlowState& flowState, FrameGraph& frameGraph, const SceneNode* sceneNode);

protected:

    /// Controls the line width used to render the simulation cell.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(FloatType{0}, cellLineWidth, setCellLineWidth);
    DECLARE_SNAPSHOT_PROPERTY_FIELD(cellLineWidth);

    /// Controls whether the simulation cell is visible.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{true}, renderCellEnabled, setRenderCellEnabled);

    /// Controls the rendering color of the simulation cell.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS((Color{0,0,0}), cellColor, setCellColor, PROPERTY_FIELD_MEMORIZE);
};

}   // End of namespace
