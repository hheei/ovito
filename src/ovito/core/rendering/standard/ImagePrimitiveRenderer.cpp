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
#include <ovito/core/rendering/RenderThread.h>
#include "ImagePrimitiveRenderer.h"

namespace Ovito {

/// Must match the std140 'ImageParams' UBO layout in image_quad.vert.
struct ImageParamsData {
    float ndcRect[4]; // x_min, y_min, x_max, y_max
    int isYUpInNDC;
    int _pad[3];
};
static_assert(sizeof(ImageParamsData) == 32);

/******************************************************************************
* Clears the draw call list for the next frame.
******************************************************************************/
void ImagePrimitiveRenderer::clear()
{
    _numDrawCalls = 0;
    // Don't clear _drawCalls — textures/bindings can be reused across frames.
    // They'll be rebuilt as needed in prepareResourceUpdates().
}

/******************************************************************************
* Phase 1: Builds draw calls from image primitives in the frame graph.
******************************************************************************/
void ImagePrimitiveRenderer::buildDrawCalls(const ImagePrimitive& primitive, const FrameGraph::RenderingCommand& command,
                                       FrameGraph::RenderLayerType layer, QSize renderSize, bool isYUpInNDC)
{
    if(primitive.image().isNull() || primitive.windowRect().isEmpty())
        return;

    // Transform the window rectangle from pixel coordinates to NDC.
    const Box2& b = primitive.windowRect();
    float w = static_cast<float>(renderSize.width());
    float h = static_cast<float>(renderSize.height());

    float x_min = b.minc.x() / w * 2.0f - 1.0f;
    float x_max = b.maxc.x() / w * 2.0f - 1.0f;

    float y_min, y_max;
    if(isYUpInNDC) {
        // OpenGL/Metal: NDC y=+1 is top.
        y_min = 1.0f - b.maxc.y() / h * 2.0f;
        y_max = 1.0f - b.minc.y() / h * 2.0f;
    }
    else {
        // Vulkan: NDC y=-1 is top.
        y_min = b.minc.y() / h * 2.0f - 1.0f;
        y_max = b.maxc.y() / h * 2.0f - 1.0f;
    }

    DrawCall& dc = (_numDrawCalls >= _drawCalls.size()) ? _drawCalls.emplace_back() : _drawCalls[_numDrawCalls];
    _numDrawCalls++;
    dc.image = primitive.image();
    dc.ndcRect = QRectF(x_min, y_min, x_max - x_min, y_max - y_min);
    dc.layer = layer;
}

/******************************************************************************
* Phase 2: Uploads image textures and updates UBOs.
******************************************************************************/
void ImagePrimitiveRenderer::prepareResourceUpdates(QRhiResourceUpdateBatch* batch, QSize renderSize, bool isYUpInNDC)
{
    _drawCalls.resize(_numDrawCalls);
    if(_drawCalls.empty())
        return;

    // Compute aligned size for one image-params slot.
    _imageParamsAlignedSize = rhi()->ubufAligned(sizeof(ImageParamsData));

    // Create or resize the image params UBO (one slot per draw call).
    quint32 requiredSize = _imageParamsAlignedSize * _numDrawCalls;
    if(!_imageParamsUBO) {
        _imageParamsUBO.reset(rhi()->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, requiredSize));
        if(!_imageParamsUBO->create()) {
            rt()->reportWarning("ImagePrimitiveRenderer: Failed to create image params UBO.");
            _imageParamsUBO.reset();
            return;
        }
    }
    else if(_imageParamsUBO->size() < requiredSize) {
        _imageParamsUBO->setSize(requiredSize);
        if(!_imageParamsUBO->create()) {
            rt()->reportWarning("ImagePrimitiveRenderer: Failed to resize image params UBO.");
            _imageParamsUBO.reset();
            return;
        }
    }

    // Create the sampler if needed.
    if(!_sampler) {
        _sampler.reset(rhi()->newSampler(
            QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
            QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge));
        if(!_sampler->create()) {
            rt()->reportWarning("ImagePrimitiveRenderer: Failed to create sampler.");
            _sampler.reset();
            return;
        }
    }

    // Upload image params and textures for each draw call.
    quint32 offset = 0;
    for(DrawCall& dc : _drawCalls) {

        // Upload image params at the aligned offset.
        ImageParamsData imageParamsData;
        imageParamsData.ndcRect[0] = dc.ndcRect.left();
        imageParamsData.ndcRect[1] = dc.ndcRect.top();
        imageParamsData.ndcRect[2] = dc.ndcRect.right();
        imageParamsData.ndcRect[3] = dc.ndcRect.bottom();
        imageParamsData.isYUpInNDC = isYUpInNDC ? 1 : 0;
        imageParamsData._pad[0] = imageParamsData._pad[1] = imageParamsData._pad[2] = 0;
        batch->updateDynamicBuffer(_imageParamsUBO.get(), offset, sizeof(ImageParamsData), &imageParamsData);
        offset += _imageParamsAlignedSize;

        // Create or resize the texture.
        const QSize imageSize = dc.image.size();
        const bool uploadNewImage = !dc.texture || dc.uploadedCacheKey != dc.image.cacheKey();
        if(!dc.texture) {
            dc.texture.reset(rhi()->newTexture(QRhiTexture::RGBA8, imageSize, 1));
            if(!dc.texture->create()) {
                rt()->reportWarning("ImagePrimitiveRenderer: Failed to create image texture.");
                dc.texture.reset();
                continue;
            }
        }
        else if(dc.texture->pixelSize() != imageSize) {
            dc.texture->setPixelSize(imageSize);
            if(!dc.texture->create()) {
                rt()->reportWarning("ImagePrimitiveRenderer: Failed to resize image texture.");
                dc.texture.reset();
                continue;
            }
        }
        if(uploadNewImage) {
            dc.uploadedCacheKey = dc.image.cacheKey();
            batch->uploadTexture(dc.texture.get(),
                QRhiTextureUploadDescription(QRhiTextureUploadEntry(0, 0,
                    QRhiTextureSubresourceUploadDescription(std::move(dc.image).convertToFormat(QImage::Format_RGBA8888)))));
        }

        // Create shader resource bindings for this draw call if needed.
        if(!dc.bindings) {
            dc.bindings.reset(rhi()->newShaderResourceBindings());
            dc.bindings->setBindings({
                QRhiShaderResourceBinding::uniformBufferWithDynamicOffset(0,
                    QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage,
                    _imageParamsUBO.get(), sizeof(ImageParamsData)),
                QRhiShaderResourceBinding::sampledTexture(1,
                    QRhiShaderResourceBinding::FragmentStage,
                    dc.texture.get(), _sampler.get()),
            });
            if(!dc.bindings->create()) {
                rt()->reportWarning("ImagePrimitiveRenderer: Failed to create shader resource bindings for draw call.");
                dc.bindings.reset();
            }
        }
    }
}

/******************************************************************************
* Phase 3: Issues draw calls inside the render pass.
******************************************************************************/
void ImagePrimitiveRenderer::draw(QRhiCommandBuffer* cb, QRhiRenderPassDescriptor* rpd, FrameGraph::RenderLayerType layer)
{
    if(_drawCalls.empty() || !_imageParamsUBO || !_sampler)
        return;

    QRhiGraphicsPipeline* pipeline = ensurePipeline(rpd);
    if(!pipeline)
        return;

    for(size_t i = 0; i < _drawCalls.size(); i++) {
        const DrawCall& dc = _drawCalls[i];
        if(dc.layer != layer)
            continue;
        if(!dc.texture || !dc.bindings)
            continue;

        cb->setGraphicsPipeline(pipeline);
        QRhiCommandBuffer::DynamicOffset dynOffset(0, _imageParamsAlignedSize * static_cast<quint32>(i));
        cb->setShaderResources(dc.bindings.get(), 1, &dynOffset);
        cb->draw(4);
    }
}

/******************************************************************************
* Ensures the pipeline is valid for the given RPD.
******************************************************************************/
QRhiGraphicsPipeline* ImagePrimitiveRenderer::ensurePipeline(QRhiRenderPassDescriptor* rpd)
{
    if(!_imageParamsUBO || !_sampler)
        return nullptr;

    using PipelineCacheKey = RendererResourceKey<struct Tag>; // No variant flags for now, but we might add some in the future.

    // Check if a compatible pipeline already exists in the cache.
    return rt()->ensureGraphicsPipeline(rpd, PipelineCacheKey{}, [&]() -> std::unique_ptr<QRhiGraphicsPipeline> {

        // Load compiled shader binaries.
        QShader vs = rt()->loadShader(QStringLiteral(":/ovito/core/rendering/standard/shaders/image_quad.vert.qsb"));
        QShader fs = rt()->loadShader(QStringLiteral(":/ovito/core/rendering/standard/shaders/image_quad.frag.qsb"));
        if(!vs.isValid() || !fs.isValid())
            return {};

        // We need a "compatible" SRB for pipeline creation.
        // QRhi requires the pipeline's SRB to be layout-compatible with the one used at draw time.
        // We'll use the first draw call's bindings if available.
        QRhiShaderResourceBindings* srbForPipeline = nullptr;
        for(auto& dc : _drawCalls) {
            if(dc.bindings) {
                srbForPipeline = dc.bindings.get();
                break;
            }
        }
        if(!srbForPipeline)
            return {};

        std::unique_ptr<QRhiGraphicsPipeline> pipeline(rhi()->newGraphicsPipeline());
        pipeline->setShaderStages({
            { QRhiShaderStage::Vertex,   vs },
            { QRhiShaderStage::Fragment, fs }
        });
        pipeline->setVertexInputLayout(QRhiVertexInputLayout{});
        pipeline->setTopology(QRhiGraphicsPipeline::TriangleStrip);
        pipeline->setDepthTest(false);
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
            rt()->reportWarning("ImagePrimitiveRenderer: Failed to create graphics pipeline.");
            return {};
        }
        return pipeline;
    });
}

}   // End of namespace
