// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/core/Core.h>
#include <ovito/core/viewport/ViewProjectionParameters.h>
#include <ovito/core/rendering/RenderThread.h>
#include "TextBillboardPrimitiveRenderer.h"
#include "StandardRendererImplementation.h"

namespace Ovito {

/// Must match the std140 'TextBillboardDrawParams' UBO layout in text_billboard_draw_params.glsl.
struct TextBillboardDrawParamsData {
    Matrix4F modelViewMatrix;     ///< view * model (the projection comes from SceneParams).
    float    alignmentFactor[2];  ///< Fraction of the quad size between anchor and quad top-left corner.
    float    shiftDirection[2];   ///< Unit vector (window coords, y down) of the radius shift; may be zero.
    float    pixelOffset[2];      ///< Constant label offset in device pixels (window coords).
    float    depthOffset;         ///< Extra camera-facing shift in world units, added to the label radius.
    float    _pad0;
};
static_assert(sizeof(TextBillboardDrawParamsData) == 96);

/******************************************************************************
* Clears the draw call list for the next frame.
******************************************************************************/
void TextBillboardPrimitiveRenderer::clear()
{
    _numDrawCalls = 0;
    // Don't clear _drawCalls - the shader resource bindings can be reused across
    // frames as long as the atlas texture stays the same.
}

/******************************************************************************
* Phase 1: Builds draw calls from text billboard primitives.
******************************************************************************/
void TextBillboardPrimitiveRenderer::buildDrawCalls(const TextBillboardPrimitive& primitive,
                                                    const FrameGraph::RenderingCommand& command,
                                                    FrameGraph::RenderLayerType layer)
{
    // Skip empty primitives.
    if(!primitive.positions() || primitive.positions()->size() == 0 || primitive.atlasImage().isNull())
        return;
    OVITO_ASSERT(primitive.uvRects() && primitive.uvRects()->size() == primitive.positions()->size());
    OVITO_ASSERT(primitive.sizes() && primitive.sizes()->size() == primitive.positions()->size());

    DrawCall& dc = (_numDrawCalls >= _drawCalls.size()) ? _drawCalls.emplace_back() : _drawCalls[_numDrawCalls];
    _numDrawCalls++;
    dc.primitive = &primitive;
    dc.modelWorldTM = command.modelWorldTM();
    dc.layer = layer;
    dc.labelCount = primitive.positions()->size();
    dc.alwaysInFront = primitive.alwaysInFront();
    dc.bufs = {};
    dc.texture = nullptr;
}

/******************************************************************************
* Phase 2: Uploads instance buffers, the atlas texture, and UBOs.
******************************************************************************/
void TextBillboardPrimitiveRenderer::prepareResourceUpdates(QRhiResourceUpdateBatch* batch,
                                                            const ViewProjectionParameters& projParams)
{
    _drawCalls.resize(_numDrawCalls);
    if(_drawCalls.empty())
        return;

    // Compute aligned size for one draw-params slot.
    _drawParamsAlignedSize = rhi()->ubufAligned(sizeof(TextBillboardDrawParamsData));

    // Create or resize the per-draw params UBO.
    quint32 requiredSize = _drawParamsAlignedSize * static_cast<quint32>(_drawCalls.size());
    if(!_drawParamsUBO) {
        _drawParamsUBO.reset(rhi()->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, requiredSize));
        if(!_drawParamsUBO->create()) {
            service()->reportWarning("TextBillboardPrimitiveRenderer: Failed to create draw params UBO.");
            _drawParamsUBO.reset();
            return;
        }
    }
    else if(_drawParamsUBO->size() < requiredSize) {
        _drawParamsUBO->setSize(requiredSize);
        if(!_drawParamsUBO->create()) {
            service()->reportWarning("TextBillboardPrimitiveRenderer: Failed to resize draw params UBO.");
            _drawParamsUBO.reset();
            return;
        }
    }

    // Create the shared atlas sampler if needed.
    if(!_sampler) {
        _sampler.reset(rhi()->newSampler(
            QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
            QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge));
        if(!_sampler->create()) {
            service()->reportWarning("TextBillboardPrimitiveRenderer: Failed to create atlas sampler.");
            _sampler.reset();
            return;
        }
    }

    for(auto [i, dc] : Ovito::enumerate(_drawCalls)) {

        // Upload the per-draw parameters at the aligned offset.
        TextBillboardDrawParamsData drawParams;
        const AffineTransformation viewModel = projParams.viewMatrix * dc.modelWorldTM;
        drawParams.modelViewMatrix = Matrix4(viewModel).toDataType<float>();
        drawParams.alignmentFactor[0] = static_cast<float>(dc.primitive->alignmentFactor().x());
        drawParams.alignmentFactor[1] = static_cast<float>(dc.primitive->alignmentFactor().y());
        drawParams.shiftDirection[0] = static_cast<float>(dc.primitive->shiftDirection().x());
        drawParams.shiftDirection[1] = static_cast<float>(dc.primitive->shiftDirection().y());
        drawParams.pixelOffset[0] = static_cast<float>(dc.primitive->pixelOffset().x());
        drawParams.pixelOffset[1] = static_cast<float>(dc.primitive->pixelOffset().y());
        drawParams.depthOffset = static_cast<float>(dc.primitive->depthOffset());
        drawParams._pad0 = 0;
        batch->updateDynamicBuffer(_drawParamsUBO.get(), _drawParamsAlignedSize * static_cast<quint32>(i),
                                   sizeof(TextBillboardDrawParamsData), &drawParams);

        // Upload the per-instance buffers. A missing radius buffer is substituted by a
        // buffer repeating the uniform radius, so the shader always sees a per-instance value.
        dc.bufs.positions = impl()->uploadDataBuffer(dc.primitive->positions(), batch);
        if(dc.primitive->radii())
            dc.bufs.radii = impl()->uploadDataBuffer(dc.primitive->radii(), batch);
        else
            dc.bufs.radii = impl()->uniformValueBuffer<float>(static_cast<float>(dc.primitive->uniformRadius()),
                                                              static_cast<quint32>(dc.labelCount), batch);
        dc.bufs.uvRects = impl()->uploadDataBuffer(dc.primitive->uvRects(), batch);
        dc.bufs.sizes = impl()->uploadDataBuffer(dc.primitive->sizes(), batch);

        // Look up (or create and upload) the atlas texture in the graphics resource cache.
        // Keying on the QImage's cache key deduplicates the texture across draw calls and
        // frames: the same atlas image is uploaded to the GPU only once.
        dc.texture = impl()->rhiCache().lookup<std::unique_ptr<QRhiTexture>>(
            RendererResourceKey<struct TextAtlasTextureCache, qint64>{dc.primitive->atlasImage().cacheKey()},
            [&](std::unique_ptr<QRhiTexture>& texture) {
                texture.reset(rhi()->newTexture(QRhiTexture::RGBA8, dc.primitive->atlasImage().size(), 1));
                if(!texture->create()) {
                    service()->reportWarning("TextBillboardPrimitiveRenderer: Failed to create atlas texture.");
                    texture.reset();
                    return;
                }
                batch->uploadTexture(texture.get(),
                    QRhiTextureUploadDescription(QRhiTextureUploadEntry(0, 0,
                        QRhiTextureSubresourceUploadDescription(
                            dc.primitive->atlasImage().convertToFormat(QImage::Format_RGBA8888)))));
            }).get();
        if(!dc.texture)
            continue;

        // (Re-)create the shader resource bindings for this draw call if the atlas texture changed.
        // Note: Rebuilding the UBOs with create() keeps the QRhiBuffer objects intact, which QRhi
        // handles transparently, so only a texture object change invalidates the bindings.
        if(!dc.bindings || dc.boundTexture != dc.texture) {
            dc.bindings.reset(rhi()->newShaderResourceBindings());
            dc.bindings->setBindings({
                QRhiShaderResourceBinding::uniformBuffer(0,
                    QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage,
                    impl()->sceneParamsUBO()),
                QRhiShaderResourceBinding::uniformBufferWithDynamicOffset(1,
                    QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage,
                    _drawParamsUBO.get(), sizeof(TextBillboardDrawParamsData)),
                QRhiShaderResourceBinding::sampledTexture(2,
                    QRhiShaderResourceBinding::FragmentStage,
                    dc.texture, _sampler.get()),
            });
            if(!dc.bindings->create()) {
                service()->reportWarning("TextBillboardPrimitiveRenderer: Failed to create shader resource bindings.");
                dc.bindings.reset();
                dc.boundTexture = nullptr;
                continue;
            }
            dc.boundTexture = dc.texture;
        }
    }
}

/******************************************************************************
* Phase 3: Issues draw calls for the given layer inside the render pass.
******************************************************************************/
void TextBillboardPrimitiveRenderer::draw(QRhiCommandBuffer* cb, QRhiRenderPassDescriptor* rpd,
                                          FrameGraph::RenderLayerType layer)
{
    if(_drawCalls.empty() || !impl()->sceneParamsUBO() || !_drawParamsUBO)
        return;

    for(auto [i, dc] : Ovito::enumerate(_drawCalls)) {
        // Filter by layer.
        if(dc.layer != layer)
            continue;
        if(!dc.texture || !dc.bindings || !dc.bufs.positions || !dc.bufs.radii || !dc.bufs.uvRects || !dc.bufs.sizes)
            continue;

        // Labels snapped in front of the scene are drawn without depth testing.
        QRhiGraphicsPipeline* pipeline = ensurePipeline(rpd, !dc.alwaysInFront);
        if(!pipeline)
            continue;

        // Bind pipeline and shader resources with the dynamic UBO offset for this draw call.
        QRhiCommandBuffer::DynamicOffset dynOffset(1, _drawParamsAlignedSize * static_cast<quint32>(i));
        cb->setGraphicsPipeline(pipeline);
        cb->setShaderResources(dc.bindings.get(), 1, &dynOffset);

        // Per-instance vertex buffers.
        const QRhiCommandBuffer::VertexInput vbufBindings[] = {
            { dc.bufs.positions, 0 },
            { dc.bufs.radii, 0 },
            { dc.bufs.uvRects, 0 },
            { dc.bufs.sizes, 0 },
        };
        cb->setVertexInput(0, 4, vbufBindings);

        // Draw one camera-facing quad (triangle strip) per label instance.
        cb->draw(4, static_cast<quint32>(dc.labelCount));
    }
}

/******************************************************************************
* Ensures the graphics pipeline is valid for the given RPD.
******************************************************************************/
QRhiGraphicsPipeline* TextBillboardPrimitiveRenderer::ensurePipeline(QRhiRenderPassDescriptor* rpd, bool depthTest)
{
    if(!impl()->sceneParamsUBO() || !_drawParamsUBO || !_sampler)
        return nullptr;

    using PipelineCacheKey = RendererResourceKey<struct TextBillboardPipelineCache, bool>;

    return service()->ensureGraphicsPipeline(rpd, PipelineCacheKey{depthTest}, [&]() -> std::unique_ptr<QRhiGraphicsPipeline> {

        // Load compiled shader binaries.
        QShader vs = service()->loadShader(QStringLiteral(":/ovito/core/rendering/standard/shaders/text_billboard.vert.qsb"));
        QShader fs = service()->loadShader(QStringLiteral(":/ovito/core/rendering/standard/shaders/text_billboard.frag.qsb"));
        if(!vs.isValid() || !fs.isValid())
            return {};

        // QRhi requires the pipeline's SRB to be layout-compatible with the one used at draw
        // time. All draw calls share the same layout, so use the first available bindings.
        QRhiShaderResourceBindings* srbForPipeline = nullptr;
        for(const DrawCall& dc : _drawCalls) {
            if(dc.bindings) {
                srbForPipeline = dc.bindings.get();
                break;
            }
        }
        if(!srbForPipeline)
            return {};

        // Vertex input: four per-instance buffers.
        QRhiVertexInputLayout inputLayout;
        inputLayout.setBindings({
            QRhiVertexInputBinding(3 * sizeof(float), QRhiVertexInputBinding::PerInstance), // anchor position
            QRhiVertexInputBinding(    sizeof(float), QRhiVertexInputBinding::PerInstance), // radius
            QRhiVertexInputBinding(4 * sizeof(float), QRhiVertexInputBinding::PerInstance), // atlas uv rect
            QRhiVertexInputBinding(2 * sizeof(float), QRhiVertexInputBinding::PerInstance), // pixel size
        });
        inputLayout.setAttributes({
            QRhiVertexInputAttribute(0, 0, QRhiVertexInputAttribute::Float3, 0),
            QRhiVertexInputAttribute(1, 1, QRhiVertexInputAttribute::Float,  0),
            QRhiVertexInputAttribute(2, 2, QRhiVertexInputAttribute::Float4, 0),
            QRhiVertexInputAttribute(3, 3, QRhiVertexInputAttribute::Float2, 0),
        });

        // Create the graphics pipeline: depth-tested against the scene (unless the labels are
        // snapped in front of it), but never writing depth, because the labels are alpha-blended
        // back to front and must not occlude each other via the depth buffer.
        std::unique_ptr<QRhiGraphicsPipeline> pipeline(rhi()->newGraphicsPipeline());
        pipeline->setShaderStages({
            { QRhiShaderStage::Vertex,   vs },
            { QRhiShaderStage::Fragment, fs }
        });
        pipeline->setVertexInputLayout(inputLayout);
        pipeline->setTopology(QRhiGraphicsPipeline::TriangleStrip);
        pipeline->setDepthTest(depthTest);
        pipeline->setDepthWrite(false);
        pipeline->setCullMode(QRhiGraphicsPipeline::None);

        QRhiGraphicsPipeline::TargetBlend blend;
        blend.enable   = true;
        blend.srcColor = QRhiGraphicsPipeline::SrcAlpha;
        blend.dstColor = QRhiGraphicsPipeline::OneMinusSrcAlpha;
        blend.srcAlpha = QRhiGraphicsPipeline::One;
        blend.dstAlpha = QRhiGraphicsPipeline::OneMinusSrcAlpha;
        pipeline->setTargetBlends({ blend });

        pipeline->setShaderResourceBindings(srbForPipeline);
        pipeline->setRenderPassDescriptor(rpd);

        if(!pipeline->create()) {
            service()->reportWarning("TextBillboardPrimitiveRenderer: Failed to create graphics pipeline.");
            return {};
        }
        return pipeline;
    });
}

}   // End of namespace
