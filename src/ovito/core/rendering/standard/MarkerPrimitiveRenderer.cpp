////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2026 OVITO GmbH, Germany
//
//  This file is part of OVITO (Open Visualization Tool).
//
//  OVITO is free software; you can redistribute it and/or modify it either under the
//  terms of the GNU General Public License version 3 as published by the Free Software
//  Foundation (the "GPL") or, at your option, under the terms of the MIT License.
//  If you do not alter this notice, a recipient may use your version of this
//  file under either the GPL or the MIT License.
//
//  You should have received a copy of the GPL along with this program in a
//  file LICENSE.GPL.txt.  You should have received a copy of the MIT License along
//  with this program in a file LICENSE.MIT.txt
//
//  This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND,
//  either express or implied. See the GPL or the MIT License for the specific language
//  governing rights and limitations.
//
////////////////////////////////////////////////////////////////////////////////////////

#include <ovito/core/Core.h>
#include <ovito/core/viewport/ViewProjectionParameters.h>
#include <ovito/core/rendering/RenderThread.h>
#include "MarkerPrimitiveRenderer.h"
#include "StandardRendererImplementation.h"

namespace Ovito {

/// Must match the std140 'MarkerDrawParams' UBO layout in markers_draw_params.glsl.
struct MarkerDrawParamsData {
    Matrix4F modelViewProjectionMatrix;  ///< proj * view * model.
    ColorAF  markerColor;                ///< Uniform RGBA color for all markers in this draw call.
    float    markerSize;                 ///< Screen-space scale factor (= 4.0 / viewportHeight).
    uint32_t pickingBaseObjectId;
    float    _pad0, _pad1;
};
static_assert(sizeof(MarkerDrawParamsData) == 96);

/******************************************************************************
* Clears the draw call list for the next frame.
******************************************************************************/
void MarkerPrimitiveRenderer::clear()
{
    _drawCalls.clear();
}

/******************************************************************************
* Phase 1: Builds draw calls from marker primitives.
******************************************************************************/
void MarkerPrimitiveRenderer::buildDrawCalls(const MarkerPrimitive& primitive,
                                             const FrameGraph::RenderingCommand& command,
                                             FrameGraph::RenderLayerType layer,
                                             bool isPickingPass, ObjectPickingMap* pickingMap)
{
    // Only BoxShape is supported; DotShape is not rendered (matches legacy behaviour).
    if(primitive.shape() != MarkerPrimitive::BoxShape)
        return;

    // Skip empty primitives.
    if(!primitive.positions() || primitive.positions()->size() == 0)
        return;

    // Assign a picking ID block if needed.
    uint32_t objectId = 0;
    if(isPickingPass && pickingMap) {
        objectId = pickingMap->registerObjectId(rt()->objectIdAllocator().allocate(), command);
    }

    DrawCall& dc = _drawCalls.emplace_back();
    dc.primitive   = &primitive;
    dc.modelWorldTM = command.modelWorldTM();
    dc.layer       = layer;
    dc.shader      = isPickingPass ? ShaderVariant::BoxPicking : ShaderVariant::BoxVisual;
    dc.objectId    = objectId;
    dc.markerCount = primitive.positions()->size();
}

/******************************************************************************
* Phase 2: Uploads position buffers and UBOs.
******************************************************************************/
void MarkerPrimitiveRenderer::prepareResourceUpdates(QRhiResourceUpdateBatch* batch,
                                                     const ViewProjectionParameters& projParams,
                                                     QSize renderSize, bool isYUpInNDC,
                                                     bool isYUpInFramebuffer, bool isPicking)
{
    if(_drawCalls.empty())
        return;

    // Compute aligned size for one draw-params slot.
    _drawParamsAlignedSize = rhi()->ubufAligned(sizeof(MarkerDrawParamsData));

    // Create or resize the per-draw params UBO.
    quint32 requiredSize = _drawParamsAlignedSize * static_cast<quint32>(_drawCalls.size());
    if(!_drawParamsUBO) {
        _drawParamsUBO.reset(rhi()->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, requiredSize));
        if(!_drawParamsUBO->create()) {
            rt()->reportWarning("MarkerPrimitiveRenderer: Failed to create draw params UBO.");
            _drawParamsUBO.reset();
            return;
        }
    }
    else if(_drawParamsUBO->size() < requiredSize) {
        _drawParamsUBO->setSize(requiredSize);
        if(!_drawParamsUBO->create()) {
            rt()->reportWarning("MarkerPrimitiveRenderer: Failed to resize draw params UBO.");
            _drawParamsUBO.reset();
            return;
        }
    }

    // Upload per-draw params for each draw call.
    for(size_t i = 0; i < _drawCalls.size(); i++) {
        const DrawCall& dc = _drawCalls[i];

        MarkerDrawParamsData drawParams;
        AffineTransformation viewModel = projParams.viewMatrix * dc.modelWorldTM;
        drawParams.modelViewProjectionMatrix = (projParams.projectionMatrix * Matrix4(viewModel)).toDataType<float>();
        drawParams.markerColor = dc.primitive->color().toDataType<float>();
        drawParams.markerSize  = 4.0f / static_cast<float>(renderSize.height());
        drawParams.pickingBaseObjectId = dc.objectId;
        drawParams._pad0 = drawParams._pad1 = 0;

        quint32 offset = _drawParamsAlignedSize * static_cast<quint32>(i);
        batch->updateDynamicBuffer(_drawParamsUBO.get(), offset, sizeof(MarkerDrawParamsData), &drawParams);
    }

    // Upload position buffers for each draw call.
    for(DrawCall& dc : _drawCalls) {
        dc.bufs.positions = impl()->uploadDataBuffer(dc.primitive->positions(), batch);
    }
}

/******************************************************************************
* Phase 3: Issues draw calls for the given layer inside the render pass.
******************************************************************************/
void MarkerPrimitiveRenderer::draw(QRhiCommandBuffer* cb, QRhiRenderPassDescriptor* rpd,
                                   FrameGraph::RenderLayerType layer, bool isPicking)
{
    if(_drawCalls.empty() || !impl()->sceneParamsUBO() || !_drawParamsUBO)
        return;

    QRhiShaderResourceBindings* bindings = ensureShaderResourceBindings();
    if(!bindings)
        return;

    for(auto [i, dc] : Ovito::enumerate(_drawCalls)) {
        // Filter by layer.
        if(dc.layer != layer)
            continue;

        QRhiBuffer* posBuf = dc.bufs.positions;
        if(!posBuf)
            continue;

        // Use depth test/write only in the scene layer.
        const bool depthTest  = (layer == FrameGraph::SceneLayer);
        const bool depthWrite = (layer == FrameGraph::SceneLayer);

        QRhiGraphicsPipeline* pipeline = ensurePipeline(rpd, dc.shader, depthTest, depthWrite);
        if(!pipeline)
            continue;

        // Bind pipeline and shader resources with the dynamic UBO offset for this draw call.
        QRhiCommandBuffer::DynamicOffset dynOffset(1, _drawParamsAlignedSize * static_cast<quint32>(i));
        cb->setGraphicsPipeline(pipeline);
        cb->setShaderResources(bindings, 1, &dynOffset);

        // Binding 0: per-instance center positions.
        const QRhiCommandBuffer::VertexInput vbufBindings[] = {
            { posBuf, 0 },
        };
        cb->setVertexInput(0, 1, vbufBindings);

        // Draw 24 line vertices per instance (12 box edges × 2 endpoints).
        cb->draw(24, static_cast<quint32>(dc.markerCount));
    }
}

/******************************************************************************
* Ensures the shared shader resource bindings are created.
******************************************************************************/
QRhiShaderResourceBindings* MarkerPrimitiveRenderer::ensureShaderResourceBindings()
{
    if(!impl()->sceneParamsUBO() || !_drawParamsUBO)
        return nullptr;

    if(!_bindings) {
        _bindings.reset(rhi()->newShaderResourceBindings());
        _bindings->setBindings({
            QRhiShaderResourceBinding::uniformBuffer(0,
                QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage,
                impl()->sceneParamsUBO()),
            QRhiShaderResourceBinding::uniformBufferWithDynamicOffset(1,
                QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage,
                _drawParamsUBO.get(), sizeof(MarkerDrawParamsData)),
        });
        if(!_bindings->create()) {
            rt()->reportWarning("MarkerPrimitiveRenderer: Failed to create shader resource bindings.");
            _bindings.reset();
        }
    }
    return _bindings.get();
}

/******************************************************************************
* Ensures a graphics pipeline is valid for the given variant and depth settings.
******************************************************************************/
QRhiGraphicsPipeline* MarkerPrimitiveRenderer::ensurePipeline(QRhiRenderPassDescriptor* rpd,
                                                               ShaderVariant variant,
                                                               bool depthTest, bool depthWrite)
{
    struct PipelineCacheKey {
        ShaderVariant variant;
        bool depthTest;
        bool depthWrite;
        bool operator==(const PipelineCacheKey& other) const = default;
    };

    return rt()->ensureGraphicsPipeline(rpd, PipelineCacheKey{variant, depthTest, depthWrite},
                                        [&]() -> std::unique_ptr<QRhiGraphicsPipeline> {

        if(!impl()->sceneParamsUBO() || !_drawParamsUBO)
            return {};

        const bool isPicking = (variant == ShaderVariant::BoxPicking);

        // Select shader files.
        QString vsPath, fsPath;
        if(!isPicking) {
            vsPath = QStringLiteral(":/ovito/core/rendering/standard/shaders/marker_box.vert.qsb");
            fsPath = QStringLiteral(":/ovito/core/rendering/standard/shaders/marker_box.frag.qsb");
        }
        else {
            vsPath = QStringLiteral(":/ovito/core/rendering/standard/shaders/marker_box_picking.vert.qsb");
            fsPath = QStringLiteral(":/ovito/core/rendering/standard/shaders/marker_box_picking.frag.qsb");
        }

        QShader vs = rt()->loadShader(vsPath);
        QShader fs = rt()->loadShader(fsPath);
        if(!vs.isValid() || !fs.isValid())
            return {};

        // Vertex input: binding 0 — per-instance center positions (vec3).
        QRhiVertexInputLayout inputLayout;
        const QRhiVertexInputBinding inputBinding(3 * sizeof(float), QRhiVertexInputBinding::PerInstance);
        inputLayout.setBindings({ inputBinding });
        inputLayout.setAttributes({ QRhiVertexInputAttribute(0, 0, QRhiVertexInputAttribute::Float3, 0) });

        // Create graphics pipeline.
        std::unique_ptr<QRhiGraphicsPipeline> pipeline(rhi()->newGraphicsPipeline());
        pipeline->setShaderStages({
            { QRhiShaderStage::Vertex,   vs },
            { QRhiShaderStage::Fragment, fs }
        });
        pipeline->setVertexInputLayout(inputLayout);
        pipeline->setTopology(QRhiGraphicsPipeline::Lines);
        pipeline->setDepthTest(depthTest);
        pipeline->setDepthWrite(depthWrite);
        pipeline->setCullMode(QRhiGraphicsPipeline::None);

        if(isPicking) {
            // Picking render target has two R32UI color attachments; disable blending for both.
            QRhiGraphicsPipeline::TargetBlend tb;
            tb.enable = false;
            pipeline->setTargetBlends({ tb, tb });
        }
        else {
            // Visual: enable alpha blending to support semi-transparent marker colors.
            QRhiGraphicsPipeline::TargetBlend tb;
            tb.enable   = true;
            tb.srcColor = QRhiGraphicsPipeline::SrcAlpha;
            tb.dstColor = QRhiGraphicsPipeline::OneMinusSrcAlpha;
            tb.srcAlpha = QRhiGraphicsPipeline::One;
            tb.dstAlpha = QRhiGraphicsPipeline::OneMinusSrcAlpha;
            pipeline->setTargetBlends({ tb });
        }

        QRhiShaderResourceBindings* bindings = ensureShaderResourceBindings();
        if(!bindings) {
            rt()->reportWarning("MarkerPrimitiveRenderer: Failed to get shader resource bindings for pipeline creation.");
            return {};
        }

        pipeline->setShaderResourceBindings(bindings);
        pipeline->setRenderPassDescriptor(rpd);

        if(!pipeline->create()) {
            rt()->reportWarning("MarkerPrimitiveRenderer: Failed to create graphics pipeline.");
            return {};
        }
        return pipeline;
    });
}

}   // End of namespace
