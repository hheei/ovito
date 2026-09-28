// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/core/Core.h>
#include "WarningIndicatorRenderer.h"
#include "RenderThread.h"

namespace Ovito {

/******************************************************************************
* Loads a compiled .qsb shader from the Qt resource system.
******************************************************************************/
static QShader loadWarningShader(const QString& resourcePath)
{
    QFile f(resourcePath);
    if(!f.open(QIODevice::ReadOnly)) {
        qWarning() << "WarningIndicatorRenderer: Could not open shader resource:" << resourcePath;
        return {};
    }
    return QShader::fromSerialized(f.readAll());
}

/******************************************************************************
* Constructor.
******************************************************************************/
WarningIndicatorRenderer::WarningIndicatorRenderer(RenderThread* rt, QImage iconImage)
    : _rt(rt)
    , _rhi(rt->rhi())
    , _iconImage(std::move(iconImage).convertToFormat(QImage::Format_RGBA8888))
{
}

/******************************************************************************
* Destructor.
******************************************************************************/
WarningIndicatorRenderer::~WarningIndicatorRenderer() = default;

/******************************************************************************
* Uploads the icon texture on first use and updates the position/size UBO.
* Called BEFORE beginPass().
******************************************************************************/
void WarningIndicatorRenderer::prepareResourceUpdates(QRhiResourceUpdateBatch* batch, QRhiRenderTarget* renderTarget, qreal devicePixelRatio)
{
    // Lazy texture creation and upload.
    if(!_iconImage.isNull() && !_textureUploaded) {
        if(!_iconTexture) {
            _iconTexture.reset(_rhi->newTexture(QRhiTexture::RGBA8, _iconImage.size(), 1));
            if(!_iconTexture->create()) {
                qWarning("WarningIndicatorRenderer: Failed to create icon texture.");
                _iconTexture.reset();
                return;
            }
        }
        batch->uploadTexture(_iconTexture.get(),
            QRhiTextureUploadDescription(QRhiTextureUploadEntry(0, 0,
                QRhiTextureSubresourceUploadDescription(_iconImage))));
        _textureUploaded = true;
        // Release source image memory after upload.
        _iconImage = {};
    }

    if(!_iconTexture)
        return;

    // Compute icon position in NDC coordinates.
    const float dpr = float(devicePixelRatio);
    const float iconSizePx = 22.0f * dpr;   // device pixels
    const float marginPx   =  8.0f * dpr;   // device pixels
    const float targetW    = float(renderTarget->pixelSize().width());
    const float targetH    = float(renderTarget->pixelSize().height());

    UBOData uboData;
    uboData.x = 2.0f * (targetW - marginPx - iconSizePx) / targetW - 1.0f;
    uboData.w = 2.0f * iconSizePx / targetW;
    uboData.isYUpInFramebuffer = _rhi->isYUpInFramebuffer() ? 1 : 0;
    // Use isYUpInNDC() (not isYUpInFramebuffer()) for NDC position.
    // isYUpInNDC() is false only for Vulkan (NDC y=-1 at top).
    // isYUpInFramebuffer() is false for Vulkan, Metal, and D3D (only OpenGL has y-up framebuffer).
    if(_rhi->isYUpInNDC()) {
        // OpenGL and Metal: NDC y=+1 is at the top of the screen.
        uboData.y_top = 1.0f - 2.0f * marginPx / targetH;
        uboData.y_bot = uboData.y_top - 2.0f * iconSizePx / targetH;
    }
    else {
        // Vulkan: NDC y=-1 is at the top of the screen.
        uboData.y_top = -1.0f + 2.0f * marginPx / targetH;
        uboData.y_bot = uboData.y_top + 2.0f * iconSizePx / targetH;
    }
    uboData._pad0 = uboData._pad1 = uboData._pad2 = 0;

    // Create or update the UBO.
    if(!_ubo) {
        _ubo.reset(_rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(UBOData)));
        if(!_ubo->create()) {
            qWarning("WarningIndicatorRenderer: Failed to create UBO.");
            _ubo.reset();
            return;
        }
    }
    batch->updateDynamicBuffer(_ubo.get(), 0, sizeof(UBOData), &uboData);

    // Store the icon area in device-independent pixels for hit-testing.
    _lastIconArea = QRectF(
        (targetW - marginPx - iconSizePx) / dpr,
        marginPx / dpr,
        iconSizePx / dpr,
        iconSizePx / dpr
    );
}

/******************************************************************************
* Issues the draw call for the icon quad. Called INSIDE the render pass.
******************************************************************************/
QRectF WarningIndicatorRenderer::compositeInPass(QRhiCommandBuffer* cb, QRhiRenderPassDescriptor* rpd)
{
    if(!_iconTexture || !_ubo)
        return {};

    ensurePipeline(rpd);

    if(!_pipeline || !_bindings)
        return {};

    cb->setGraphicsPipeline(_pipeline.get());
    cb->setShaderResources(_bindings.get());
    cb->draw(4); // 4 vertices, triangle strip = 2 triangles

    return _lastIconArea;
}

/******************************************************************************
* (Re-)creates the graphics pipeline lazily when the RPD changes.
******************************************************************************/
void WarningIndicatorRenderer::ensurePipeline(QRhiRenderPassDescriptor* rpd)
{
    if(_currentRPD == rpd && _pipeline)
        return;

    // Tear down stale resources.
    _pipeline.reset();
    _bindings.reset();
    _iconSampler.reset();
    _currentRPD = nullptr;

    if(!_iconTexture || !_ubo)
        return;

    // Create sampler.
    _iconSampler.reset(_rhi->newSampler(
        QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
        QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge));
    if(!_iconSampler->create()) {
        qWarning("WarningIndicatorRenderer: Failed to create sampler.");
        _iconSampler.reset();
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
            _iconTexture.get(), _iconSampler.get()),
    });
    if(!_bindings->create()) {
        qWarning("WarningIndicatorRenderer: Failed to create shader resource bindings.");
        _iconSampler.reset();
        _bindings.reset();
        return;
    }

    // Load compiled shader binaries.
    QShader vs = loadWarningShader(QStringLiteral(":/ovito/core/rendering/standard/shaders/warning_indicator.vert.qsb"));
    QShader fs = loadWarningShader(QStringLiteral(":/ovito/core/rendering/standard/shaders/warning_indicator.frag.qsb"));
    if(!vs.isValid() || !fs.isValid()) {
        _iconSampler.reset();
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
        qWarning("WarningIndicatorRenderer: Failed to create graphics pipeline.");
        _pipeline.reset();
        _bindings.reset();
        _iconSampler.reset();
        return;
    }

    _currentRPD = rpd;
}

}   // End of namespace
