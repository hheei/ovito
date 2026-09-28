// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/core/Core.h>
#include "WatermarkRenderer.h"
#include "RenderThread.h"

namespace Ovito {

/******************************************************************************
* Loads a compiled .qsb shader from the Qt resource system.
******************************************************************************/
static QShader loadWatermarkShader(const QString& resourcePath)
{
    QFile f(resourcePath);
    if(!f.open(QIODevice::ReadOnly)) {
        qWarning() << "WatermarkRenderer: Could not open shader resource:" << resourcePath;
        return {};
    }
    return QShader::fromSerialized(f.readAll());
}

/******************************************************************************
* Constructor.
******************************************************************************/
WatermarkRenderer::WatermarkRenderer(RenderThread* rt, QImage watermarkImage)
    : _rt(rt)
    , _rhi(rt->rhi())
    , _watermarkImage(std::move(watermarkImage).convertToFormat(QImage::Format_RGBA8888))
{
}

/******************************************************************************
* Destructor.
******************************************************************************/
WatermarkRenderer::~WatermarkRenderer() = default;

/******************************************************************************
* Uploads the watermark texture on first use and updates the tiling UBO.
* Called BEFORE beginPass().
******************************************************************************/
void WatermarkRenderer::prepareResourceUpdates(QRhiResourceUpdateBatch* batch, QRhiRenderTarget* renderTarget)
{
    // Lazy texture creation and upload.
    if(!_watermarkImage.isNull() && !_textureUploaded) {
        if(!_watermarkTexture) {
            _watermarkTexture.reset(_rhi->newTexture(QRhiTexture::RGBA8, _watermarkImage.size(), 1));
            if(!_watermarkTexture->create()) {
                qWarning("WatermarkRenderer: Failed to create watermark texture.");
                _watermarkTexture.reset();
                return;
            }
        }
        batch->uploadTexture(_watermarkTexture.get(),
            QRhiTextureUploadDescription(QRhiTextureUploadEntry(0, 0,
                QRhiTextureSubresourceUploadDescription(_watermarkImage))));
        _textureUploaded = true;
        // Release source image memory after upload.
        _watermarkImage = {};
    }

    if(!_watermarkTexture)
        return;

    // Update the tiling UBO with the current render target dimensions.
    UBOData uboData;
    uboData.renderWidth        = float(renderTarget->pixelSize().width());
    uboData.renderHeight       = float(renderTarget->pixelSize().height());
    uboData.watermarkWidth     = float(_watermarkTexture->pixelSize().width());
    uboData.watermarkHeight    = float(_watermarkTexture->pixelSize().height());
    uboData.isYUpInNDC         = _rhi->isYUpInNDC()         ? 1 : 0;
    uboData.isYUpInFramebuffer = _rhi->isYUpInFramebuffer() ? 1 : 0;
    uboData._pad0 = uboData._pad1 = 0;

    if(!_ubo) {
        _ubo.reset(_rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(UBOData)));
        if(!_ubo->create()) {
            qWarning("WatermarkRenderer: Failed to create UBO.");
            _ubo.reset();
            return;
        }
    }
    batch->updateDynamicBuffer(_ubo.get(), 0, sizeof(UBOData), &uboData);
}

/******************************************************************************
* Issues the draw call for the fullscreen tiled watermark. Called INSIDE the render pass.
******************************************************************************/
void WatermarkRenderer::compositeInPass(QRhiCommandBuffer* cb, QRhiRenderPassDescriptor* rpd)
{
    if(!_watermarkTexture || !_ubo)
        return;

    ensurePipeline(rpd);

    if(!_pipeline || !_bindings)
        return;

    cb->setGraphicsPipeline(_pipeline.get());
    cb->setShaderResources(_bindings.get());
    cb->draw(4); // 4 vertices, triangle strip = 2 triangles covering the full screen.
}

/******************************************************************************
* (Re-)creates the graphics pipeline lazily when the RPD changes.
******************************************************************************/
void WatermarkRenderer::ensurePipeline(QRhiRenderPassDescriptor* rpd)
{
    if(_currentRPD == rpd && _pipeline)
        return;

    // Tear down stale resources.
    _pipeline.reset();
    _bindings.reset();
    _watermarkSampler.reset();
    _currentRPD = nullptr;

    if(!_watermarkTexture || !_ubo)
        return;

    // Create sampler with Repeat wrap mode so the watermark tiles across the output.
    _watermarkSampler.reset(_rhi->newSampler(
        QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
        QRhiSampler::Repeat, QRhiSampler::Repeat));
    if(!_watermarkSampler->create()) {
        qWarning("WatermarkRenderer: Failed to create sampler.");
        _watermarkSampler.reset();
        return;
    }

    // Create shader resource bindings.
    _bindings.reset(_rhi->newShaderResourceBindings());
    _bindings->setBindings({
        QRhiShaderResourceBinding::uniformBuffer(0,
            QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage,
            _ubo.get()),
        QRhiShaderResourceBinding::sampledTexture(1,
            QRhiShaderResourceBinding::FragmentStage,
            _watermarkTexture.get(), _watermarkSampler.get()),
    });
    if(!_bindings->create()) {
        qWarning("WatermarkRenderer: Failed to create shader resource bindings.");
        _watermarkSampler.reset();
        _bindings.reset();
        return;
    }

    // Load compiled shader binaries.
    QShader vs = loadWatermarkShader(
        QStringLiteral(":/ovito/core/rendering/standard/shaders/watermark.vert.qsb"));
    QShader fs = loadWatermarkShader(
        QStringLiteral(":/ovito/core/rendering/standard/shaders/watermark.frag.qsb"));
    if(!vs.isValid() || !fs.isValid()) {
        _watermarkSampler.reset();
        _bindings.reset();
        return;
    }

    // Create graphics pipeline with alpha blending.
    _pipeline.reset(_rhi->newGraphicsPipeline());
    _pipeline->setShaderStages({
        { QRhiShaderStage::Vertex,   vs },
        { QRhiShaderStage::Fragment, fs }
    });
    _pipeline->setVertexInputLayout(QRhiVertexInputLayout{});
    _pipeline->setTopology(QRhiGraphicsPipeline::TriangleStrip);
    _pipeline->setDepthTest(false);
    _pipeline->setDepthWrite(false);
    _pipeline->setCullMode(QRhiGraphicsPipeline::None);

    QRhiGraphicsPipeline::TargetBlend blend;
    blend.enable   = true;
    blend.srcColor = QRhiGraphicsPipeline::SrcAlpha;
    blend.dstColor = QRhiGraphicsPipeline::OneMinusSrcAlpha;
    blend.srcAlpha = QRhiGraphicsPipeline::One;
    blend.dstAlpha = QRhiGraphicsPipeline::OneMinusSrcAlpha;
    _pipeline->setTargetBlends({ blend });

    _pipeline->setShaderResourceBindings(_bindings.get());
    _pipeline->setRenderPassDescriptor(rpd);

    if(!_pipeline->create()) {
        qWarning("WatermarkRenderer: Failed to create graphics pipeline.");
        _pipeline.reset();
        _bindings.reset();
        _watermarkSampler.reset();
        return;
    }

    _currentRPD = rpd;
}

}   // End of namespace
