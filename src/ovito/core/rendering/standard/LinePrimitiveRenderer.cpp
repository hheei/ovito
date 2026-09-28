// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/core/Core.h>
#include <ovito/core/viewport/ViewProjectionParameters.h>
#include <ovito/core/rendering/RenderThread.h>
#include "LinePrimitiveRenderer.h"
#include "StandardRendererImplementation.h"

namespace Ovito {

/// Must match the std140 'LineDrawParams' UBO layout in lines_draw_params.glsl.
struct LineDrawParamsData {
    Matrix4F modelViewProjectionMatrix;  ///< proj * view * model, or identity for pre-projected NDC coords.
    float lineThickness;                 ///< Half line width in normalized viewport height units.
    uint32_t pickingBaseObjectId;
    float _pad0, _pad1;
};
static_assert(sizeof(LineDrawParamsData) == 80);

/******************************************************************************
* Clears the draw call list for the next frame.
******************************************************************************/
void LinePrimitiveRenderer::clear()
{
    _drawCalls.clear();
}

/******************************************************************************
* Phase 1: Builds draw calls from line primitives.
******************************************************************************/
void LinePrimitiveRenderer::buildDrawCalls(const LinePrimitive& primitive,
                                           const FrameGraph::RenderingCommand& command,
                                           FrameGraph::RenderLayerType layer,
                                           bool isPickingPass, ObjectPickingMap* pickingMap)
{
    // Skip empty primitives.
    if(!primitive.positions() || primitive.positions()->size() == 0)
        return;

    // Determine effective line width for the current pass.
    float lineWidth = static_cast<float>(isPickingPass ? primitive.pickingLineWidth() : primitive.lineWidth());
    if(lineWidth <= 0)
        return;

    bool isThin = (lineWidth == 1.0f);
    bool isPreprojected = (command.modelWorldTM() == AffineTransformation::Zero());

    // Assign picking IDs if needed.
    uint32_t objectId = 0;
    if(isPickingPass && pickingMap) {
        objectId = pickingMap->registerObjectId(service()->objectIdAllocator().allocate(), command);
    }

    ShaderVariant variant;
    if(isThin)
        variant = isPickingPass ? ShaderVariant::ThinLinePicking : ShaderVariant::ThinLine;
    else
        variant = isPickingPass ? ShaderVariant::ThickLinePicking : ShaderVariant::ThickLine;

    DrawCall& dc = _drawCalls.emplace_back();
    dc.primitive = &primitive;
    dc.modelWorldTM = command.modelWorldTM();
    dc.layer = layer;
    dc.shader = variant;
    dc.isPreprojected = isPreprojected;
    dc.lineWidth = lineWidth;
    dc.objectId = objectId;
    dc.lineSegmentCount = primitive.positions()->size() / 2;
    dc.excludeFromOutline = command.excludeFromOutline();
}

/******************************************************************************
* Phase 2: Uploads vertex buffers and UBOs.
******************************************************************************/
void LinePrimitiveRenderer::prepareResourceUpdates(QRhiResourceUpdateBatch* batch,
                                                   const ViewProjectionParameters& projParams,
                                                   QSize renderSize, bool isYUpInNDC,
                                                   bool isYUpInFramebuffer, bool isPicking)
{
    if(_drawCalls.empty())
        return;

    // Compute aligned size for one draw-params slot.
    _drawParamsAlignedSize = rhi()->ubufAligned(sizeof(LineDrawParamsData));

    // Create or resize the per-draw params UBO.
    quint32 requiredSize = _drawParamsAlignedSize * static_cast<quint32>(_drawCalls.size());
    if(!_drawParamsUBO) {
        _drawParamsUBO.reset(rhi()->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, requiredSize));
        if(!_drawParamsUBO->create()) {
            service()->reportWarning("LinePrimitiveRenderer: Failed to create draw params UBO.");
            _drawParamsUBO.reset();
            return;
        }
    }
    else if(_drawParamsUBO->size() < requiredSize) {
        _drawParamsUBO->setSize(requiredSize);
        if(!_drawParamsUBO->create()) {
            service()->reportWarning("LinePrimitiveRenderer: Failed to resize draw params UBO.");
            _drawParamsUBO.reset();
            return;
        }
    }

    // Upload per-draw params for each draw call.
    for(auto [i, dc] : Ovito::enumerate(_drawCalls)) {

        LineDrawParamsData drawParams;
        if(dc.isPreprojected) {
            // Positions are already in NDC space: use identity matrix so they pass through unchanged.
            drawParams.modelViewProjectionMatrix = Matrix4F::Identity();
        }
        else {
            // Compute combined model-view-projection matrix (without clip-space correction).
            // The clip-space correction is applied by the shader after quad expansion.
            AffineTransformation viewModel = projParams.viewMatrix * dc.modelWorldTM;
            drawParams.modelViewProjectionMatrix = (projParams.projectionMatrix * Matrix4(viewModel)).toDataType<float>();
        }
        drawParams.lineThickness = dc.lineWidth / static_cast<float>(renderSize.height());
        drawParams.pickingBaseObjectId = dc.objectId;
        drawParams._pad0 = drawParams._pad1 = 0;

        quint32 offset = _drawParamsAlignedSize * static_cast<quint32>(i);
        batch->updateDynamicBuffer(_drawParamsUBO.get(), offset, sizeof(LineDrawParamsData), &drawParams);
    }

    // Upload vertex data for each draw call.
    for(DrawCall& dc : _drawCalls) {
        uploadVertexData(batch, dc, isPicking);
    }
}

/******************************************************************************
* Uploads vertex buffers for one draw call.
******************************************************************************/
void LinePrimitiveRenderer::uploadVertexData(QRhiResourceUpdateBatch* batch,
                                             DrawCall& dc, bool isPicking)
{
    // Upload position data (used by both thin and thick line shaders).
    dc.bufs.positions = impl()->uploadDataBuffer(dc.primitive->positions(), batch);

    if(!isPicking) {
        // Upload color data (per-vertex for thin, per-instance pairs for thick).
        if(dc.primitive->colors()) {
            OVITO_ASSERT(dc.primitive->colors()->componentCount() == 4); // Expect RGBA colors
            dc.bufs.colors = impl()->uploadDataBuffer(dc.primitive->colors(), batch);
        }
        else {
            // No per-vertex colors: use uniform color.
            const bool isThin = (dc.shader == ShaderVariant::ThinLine);
            if(!isThin && rhi()->isFeatureSupported(QRhi::CustomInstanceStepRate)) {
                // Thick lines: create a 2-element buffer (one from+to color pair with the uniform color)
                // and use max step rate on the pipeline binding to avoid allocating a large buffer.
                dc.bufs.colors = impl()->uniformValueBuffer(
                    dc.primitive->uniformColor().toDataType<float>(), 2, batch);
            }
            else {
                // Thin lines or no step-rate support: fill buffer with the uniform color for every vertex/segment.
                dc.bufs.colors = impl()->uniformValueBuffer(
                    dc.primitive->uniformColor().toDataType<float>(),
                    static_cast<quint32>(dc.primitive->positions()->size()),
                    batch);
            }
        }
    }
    else {
        dc.bufs.colors = nullptr;
    }
}

/******************************************************************************
* Phase 3: Issues draw calls for the given layer inside the render pass.
******************************************************************************/
void LinePrimitiveRenderer::draw(QRhiCommandBuffer* cb, QRhiRenderPassDescriptor* rpd,
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

        const auto& bufs = dc.bufs;
        if(!bufs.positions)
            continue;

        // Determine pipeline flags based on layer.
        // SceneLayer lines use depth testing; overlay lines do not.
        PipelineFlags flags;
        flags.setFlag(PipelineFlag::DepthTest,  layer == FrameGraph::SceneLayer);
        flags.setFlag(PipelineFlag::DepthWrite, layer == FrameGraph::SceneLayer);
        const bool isThin = (dc.shader == ShaderVariant::ThinLine || dc.shader == ShaderVariant::ThinLinePicking);
        flags.setFlag(PipelineFlag::UniformColor, !isPicking && !isThin && !dc.primitive->colors());

        QRhiGraphicsPipeline* pipeline = ensurePipeline(rpd, dc.shader, flags);
        if(!pipeline)
            continue;

        // Bind pipeline and shader resources with the dynamic UBO offset for this draw call.
        QRhiCommandBuffer::DynamicOffset dynOffset(1, _drawParamsAlignedSize * static_cast<quint32>(i));
        cb->setGraphicsPipeline(pipeline);
        cb->setShaderResources(bindings, 1, &dynOffset);

        if(isThin) {
            // Thin lines: one vertex per position, two bindings (positions + colors).
            if(!isPicking) {
                const QRhiCommandBuffer::VertexInput vbufBindings[] = {
                    { bufs.positions, 0 },
                    { bufs.colors,    0 },
                };
                cb->setVertexInput(0, 2, vbufBindings);
            }
            else {
                const QRhiCommandBuffer::VertexInput vbufBindings[] = {
                    { bufs.positions, 0 },
                };
                cb->setVertexInput(0, 1, vbufBindings);
            }
            // Draw all vertices: 2 per line segment.
            cb->draw(static_cast<quint32>(dc.lineSegmentCount * 2));
        }
        else {
            // Thick lines: 4 vertices per instance (triangle strip quad), one instance per line segment.
            // position_from and position_to share the same positions buffer at different byte offsets.
            // color_from and color_to share the same colors buffer at different byte offsets.
            if(!isPicking) {
                const QRhiCommandBuffer::VertexInput vbufBindings[] = {
                    { bufs.positions, 0 },                          // position_from (stride 24, offset 0)
                    { bufs.positions, sizeof(Point3F) },            // position_to   (stride 24, offset 12)
                    { bufs.colors,    0 },                          // color_from    (stride 32, offset 0)
                    { bufs.colors,    sizeof(ColorAF) },            // color_to      (stride 32, offset 16)
                };
                cb->setVertexInput(0, 4, vbufBindings);
            }
            else {
                const QRhiCommandBuffer::VertexInput vbufBindings[] = {
                    { bufs.positions, 0 },               // position_from
                    { bufs.positions, sizeof(Point3F) }, // position_to
                };
                cb->setVertexInput(0, 2, vbufBindings);
            }
            // Draw 4 vertices per instance (triangle strip), one instance per line segment.
            cb->draw(4, static_cast<quint32>(dc.lineSegmentCount));
        }
    }
}

/******************************************************************************
* Draws ExcludeFromOutline SceneLayer lines into the depth-only excluded-depth pre-pass.
* Renders geometry (positions/colors) with the empty line_depth.frag so only depth is written.
******************************************************************************/
void LinePrimitiveRenderer::drawExclusionDepth(QRhiCommandBuffer* cb, QRhiRenderPassDescriptor* rpd)
{
    if(_drawCalls.empty() || !impl()->sceneParamsUBO() || !_drawParamsUBO)
        return;

    QRhiShaderResourceBindings* bindings = ensureShaderResourceBindings();
    if(!bindings)
        return;

    for(auto [i, dc] : Ovito::enumerate(_drawCalls)) {
        // Render ONLY ExcludeFromOutline SceneLayer lines (e.g. the simulation cell wireframe).
        if(dc.layer != FrameGraph::SceneLayer || !dc.excludeFromOutline)
            continue;

        const auto& bufs = dc.bufs;
        if(!bufs.positions || !bufs.colors)
            continue;

        PipelineFlags flags;
        flags.setFlag(PipelineFlag::DepthTest,  true);
        flags.setFlag(PipelineFlag::DepthWrite, true);
        flags.setFlag(PipelineFlag::DepthOnly,  true);
        const bool isThin = (dc.shader == ShaderVariant::ThinLine || dc.shader == ShaderVariant::ThinLinePicking);
        flags.setFlag(PipelineFlag::UniformColor, !isThin && !dc.primitive->colors());

        QRhiGraphicsPipeline* pipeline = ensurePipeline(rpd, dc.shader, flags);
        if(!pipeline)
            continue;

        QRhiCommandBuffer::DynamicOffset dynOffset(1, _drawParamsAlignedSize * static_cast<quint32>(i));
        cb->setGraphicsPipeline(pipeline);
        cb->setShaderResources(bindings, 1, &dynOffset);

        if(isThin) {
            const QRhiCommandBuffer::VertexInput vbufBindings[] = {
                { bufs.positions, 0 },
                { bufs.colors,    0 },
            };
            cb->setVertexInput(0, 2, vbufBindings);
            cb->draw(static_cast<quint32>(dc.lineSegmentCount * 2));
        }
        else {
            const QRhiCommandBuffer::VertexInput vbufBindings[] = {
                { bufs.positions, 0 },
                { bufs.positions, sizeof(Point3F) },
                { bufs.colors,    0 },
                { bufs.colors,    sizeof(ColorAF) },
            };
            cb->setVertexInput(0, 4, vbufBindings);
            cb->draw(4, static_cast<quint32>(dc.lineSegmentCount));
        }
    }
}

/******************************************************************************
* Ensures the shared shader resource bindings are created.
******************************************************************************/
QRhiShaderResourceBindings* LinePrimitiveRenderer::ensureShaderResourceBindings()
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
                _drawParamsUBO.get(), sizeof(LineDrawParamsData)),
        });
        if(!_bindings->create()) {
            service()->reportWarning("LinePrimitiveRenderer: Failed to create shader resource bindings.");
            _bindings.reset();
        }
    }
    return _bindings.get();
}

/******************************************************************************
* Ensures a graphics pipeline is valid for the given variant and flags.
******************************************************************************/
QRhiGraphicsPipeline* LinePrimitiveRenderer::ensurePipeline(QRhiRenderPassDescriptor* rpd,
                                                             ShaderVariant variant,
                                                             PipelineFlags flags)
{
    struct PipelineCacheKey {
        ShaderVariant variant;
        PipelineFlags flags;
        bool operator==(const PipelineCacheKey& other) const = default;
    };

    return service()->ensureGraphicsPipeline(rpd, PipelineCacheKey{variant, flags}, [&]() -> std::unique_ptr<QRhiGraphicsPipeline> {

        if(!impl()->sceneParamsUBO() || !_drawParamsUBO)
            return {};

        // Select shader files based on variant.
        QString vsPath, fsPath;
        bool isPicking = false;
        bool isThin = false;
        switch(variant) {
            case ShaderVariant::ThinLine:
                vsPath = QStringLiteral(":/ovito/core/rendering/standard/shaders/line_thin.vert.qsb");
                fsPath = QStringLiteral(":/ovito/core/rendering/standard/shaders/line_thin.frag.qsb");
                isThin = true;
                break;
            case ShaderVariant::ThinLinePicking:
                vsPath = QStringLiteral(":/ovito/core/rendering/standard/shaders/line_thin_picking.vert.qsb");
                fsPath = QStringLiteral(":/ovito/core/rendering/standard/shaders/line_thin_picking.frag.qsb");
                isThin = true;
                isPicking = true;
                break;
            case ShaderVariant::ThickLine:
                vsPath = QStringLiteral(":/ovito/core/rendering/standard/shaders/line_thick.vert.qsb");
                fsPath = QStringLiteral(":/ovito/core/rendering/standard/shaders/line_thin.frag.qsb"); // Reuse the simple pass-through frag shader.
                break;
            case ShaderVariant::ThickLinePicking:
                vsPath = QStringLiteral(":/ovito/core/rendering/standard/shaders/line_thick_picking.vert.qsb");
                fsPath = QStringLiteral(":/ovito/core/rendering/standard/shaders/line_thick_picking.frag.qsb");
                isPicking = true;
                break;
            default:
                return {};
        }

        // Depth-only pass (excluded-depth pre-pass): override the fragment shader with an empty
        // depth-only shader so the pipeline is compatible with a target that has no color attachments.
        const bool isDepthOnly = flags.testFlag(PipelineFlag::DepthOnly);
        if(isDepthOnly)
            fsPath = QStringLiteral(":/ovito/core/rendering/standard/shaders/line_depth.frag.qsb");

        QShader vs = service()->loadShader(vsPath);
        QShader fs = service()->loadShader(fsPath);
        if(!vs.isValid() || !fs.isValid())
            return {};

        // Build vertex input layout.
        QRhiVertexInputLayout inputLayout;
        QVector<QRhiVertexInputBinding> inputBindings;
        QVector<QRhiVertexInputAttribute> attributes;

        if(isThin) {
            // Binding 0: per-vertex position (vec3).
            inputBindings.append(QRhiVertexInputBinding(3 * sizeof(float), QRhiVertexInputBinding::PerVertex));
            attributes.append(QRhiVertexInputAttribute(0, 0, QRhiVertexInputAttribute::Float3, 0));
            if(!isPicking) {
                // Binding 1: per-vertex color (vec4).
                inputBindings.append(QRhiVertexInputBinding(4 * sizeof(float), QRhiVertexInputBinding::PerVertex));
                attributes.append(QRhiVertexInputAttribute(1, 1, QRhiVertexInputAttribute::Float4, 0));
            }
        }
        else {
            // Binding 0: position_from (vec3) — stride covers a pair of positions (2 * vec3 = 24 bytes).
            inputBindings.append(QRhiVertexInputBinding(2 * 3 * sizeof(float), QRhiVertexInputBinding::PerInstance));
            attributes.append(QRhiVertexInputAttribute(0, 0, QRhiVertexInputAttribute::Float3, 0));
            // Binding 1: position_to (vec3) — same stride, buffer is offset by sizeof(Point3F) in setVertexInput.
            inputBindings.append(QRhiVertexInputBinding(2 * 3 * sizeof(float), QRhiVertexInputBinding::PerInstance));
            attributes.append(QRhiVertexInputAttribute(1, 1, QRhiVertexInputAttribute::Float3, 0));
            if(!isPicking) {
                // When using a uniform color (no per-segment colors), use max step rate so the single
                // color pair in the VBO is reused for all instances, minimizing memory usage.
                quint32 colorStepRate = (flags.testFlag(PipelineFlag::UniformColor) && rhi()->isFeatureSupported(QRhi::CustomInstanceStepRate))
                    ? std::numeric_limits<quint32>::max() : 1;
                // Binding 2: color_from (vec4) — stride covers a pair of colors (2 * vec4 = 32 bytes).
                inputBindings.append(QRhiVertexInputBinding(2 * 4 * sizeof(float), QRhiVertexInputBinding::PerInstance, colorStepRate));
                attributes.append(QRhiVertexInputAttribute(2, 2, QRhiVertexInputAttribute::Float4, 0));
                // Binding 3: color_to (vec4) — same stride, buffer is offset by sizeof(ColorAF) in setVertexInput.
                inputBindings.append(QRhiVertexInputBinding(2 * 4 * sizeof(float), QRhiVertexInputBinding::PerInstance, colorStepRate));
                attributes.append(QRhiVertexInputAttribute(3, 3, QRhiVertexInputAttribute::Float4, 0));
            }
        }
        inputLayout.setBindings(inputBindings.cbegin(), inputBindings.cend());
        inputLayout.setAttributes(attributes.cbegin(), attributes.cend());

        // Create graphics pipeline.
        std::unique_ptr<QRhiGraphicsPipeline> pipeline(rhi()->newGraphicsPipeline());
        pipeline->setShaderStages({
            { QRhiShaderStage::Vertex,   vs },
            { QRhiShaderStage::Fragment, fs }
        });
        pipeline->setVertexInputLayout(inputLayout);
        pipeline->setTopology(isThin ? QRhiGraphicsPipeline::Lines : QRhiGraphicsPipeline::TriangleStrip);
        pipeline->setDepthTest(flags.testFlag(PipelineFlag::DepthTest));
        pipeline->setDepthWrite(flags.testFlag(PipelineFlag::DepthWrite));
        pipeline->setCullMode(QRhiGraphicsPipeline::None);

        if(isDepthOnly) {
            // Depth-only target has no color attachments.
            pipeline->setTargetBlends({});
        }
        else if(isPicking) {
            // Picking render target has two R32UI color attachments; disable blending for both.
            QRhiGraphicsPipeline::TargetBlend tb;
            tb.enable = false;
            pipeline->setTargetBlends({ tb, tb });
        }
        else {
            // Visual: enable alpha blending to support semi-transparent line colors.
            QRhiGraphicsPipeline::TargetBlend tb;
            tb.enable   = true;
            tb.srcColor = QRhiGraphicsPipeline::SrcAlpha;
            tb.dstColor = QRhiGraphicsPipeline::OneMinusSrcAlpha;
            tb.srcAlpha = QRhiGraphicsPipeline::One;
            tb.dstAlpha = QRhiGraphicsPipeline::OneMinusSrcAlpha;
            pipeline->setTargetBlends({ tb });
        }

        // Get shader resource bindings for pipeline creation (must be layout-compatible with draw-time bindings).
        QRhiShaderResourceBindings* bindings = ensureShaderResourceBindings();
        if(!bindings) {
            service()->reportWarning("LinePrimitiveRenderer: Failed to get shader resource bindings for pipeline creation.");
            return {};
        }

        pipeline->setShaderResourceBindings(bindings);
        pipeline->setRenderPassDescriptor(rpd);

        if(!pipeline->create()) {
            service()->reportWarning("LinePrimitiveRenderer: Failed to create graphics pipeline.");
            return {};
        }
        return pipeline;
    });
}

}   // End of namespace
