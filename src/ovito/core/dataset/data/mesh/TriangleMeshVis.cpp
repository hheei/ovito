// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/core/Core.h>
#include <ovito/core/dataset/data/mesh/TriangleMesh.h>
#include <ovito/core/rendering/FrameGraph.h>
#include <ovito/core/rendering/MeshPrimitive.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/utilities/units/UnitsManager.h>
#include "TriangleMeshVis.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(TriangleMeshVis);
OVITO_CLASSINFO(TriangleMeshVis, "DisplayName", "Triangle mesh");
OVITO_CLASSINFO(TriangleMeshVis, "ClassNameAlias", "TriMeshVis");  // For backward compatibility with OVITO 3.9.2
DEFINE_PROPERTY_FIELD(TriangleMeshVis, color);
DEFINE_REFERENCE_FIELD(TriangleMeshVis, transparencyController);
DEFINE_PROPERTY_FIELD(TriangleMeshVis, highlightEdges);
DEFINE_PROPERTY_FIELD(TriangleMeshVis, wireframeColor);
DEFINE_PROPERTY_FIELD(TriangleMeshVis, wireframeWidth);
DEFINE_PROPERTY_FIELD(TriangleMeshVis, wireframeFullyOpaque);
DEFINE_PROPERTY_FIELD(TriangleMeshVis, backfaceCulling);
SET_PROPERTY_FIELD_LABEL(TriangleMeshVis, color, "Display color");
SET_PROPERTY_FIELD_LABEL(TriangleMeshVis, transparencyController, "Transparency");
SET_PROPERTY_FIELD_LABEL(TriangleMeshVis, highlightEdges, "Highlight edges");
SET_PROPERTY_FIELD_LABEL(TriangleMeshVis, wireframeColor, "Line color");
SET_PROPERTY_FIELD_LABEL(TriangleMeshVis, wireframeWidth, "Line width (px)");
SET_PROPERTY_FIELD_LABEL(TriangleMeshVis, wireframeFullyOpaque, "Always fully opaque");
SET_PROPERTY_FIELD_LABEL(TriangleMeshVis, backfaceCulling, "Back-face culling");
SET_PROPERTY_FIELD_UNITS_AND_RANGE(TriangleMeshVis, wireframeWidth, FloatParameterUnit, 0.0, 8.0);
SET_PROPERTY_FIELD_UNITS_AND_RANGE(TriangleMeshVis, transparencyController, PercentParameterUnit, 0, 1);

/******************************************************************************
* Constructor.
******************************************************************************/
void TriangleMeshVis::initializeObject(ObjectInitializationFlags flags)
{
    DataVis::initializeObject(flags);

    if(!flags.testFlag(ObjectInitializationFlag::DontInitializeObject)) {
        setTransparencyController(ControllerManager::createFloatController());
    }
}

/******************************************************************************
* Computes the bounding box of the object.
******************************************************************************/
Box3 TriangleMeshVis::boundingBoxImmediate(AnimationTime time, const ConstDataObjectPath& path, const Pipeline* pipeline, const PipelineFlowState& flowState, TimeInterval& validityInterval)
{
    // Let the triangle mesh do the computing of the bounding box.
    if(const TriangleMesh* triMeshObj = path.lastAs<TriangleMesh>()) {
        return triMeshObj->boundingBox();
    }
    return {};
}

/******************************************************************************
* Lets the vis element produce a visual representation of a data object.
******************************************************************************/
PipelineStatus TriangleMeshVis::renderSynchronous(const ConstDataObjectPath& path, const PipelineFlowState& flowState, FrameGraph& frameGraph, const SceneNode* sceneNode)
{
    // Obtain transparency parameter value and display color value.
    FloatType transp = 0;
    TimeInterval iv;
    if(transparencyController())
        transp = transparencyController()->getFloatValue(frameGraph.time(), iv);

    // Prepare the mesh rendering primitive.
    auto primitive = std::make_unique<MeshPrimitive>();
    primitive->setEmphasizeEdges(highlightEdges());
    primitive->setUniformColor(ColorA(color(), FloatType(1) - transp));
    primitive->setWireframeColor(ColorA(wireframeColor(), wireframeFullyOpaque() ? 1 : (1 - transp)));
    primitive->setWireframeWidth(wireframeWidth() * frameGraph.devicePixelRatio());
    primitive->setMesh(path.lastAs<TriangleMesh>());
    primitive->setCullFaces(backfaceCulling());

    // Add render primitive to graph.
    frameGraph.addPrimitive(frameGraph.addCommandGroup(FrameGraph::SceneLayer), std::move(primitive), sceneNode);

    return {};
}

}   // End of namespace
