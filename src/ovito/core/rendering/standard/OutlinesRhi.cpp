// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/core/Core.h>
#include "OutlinesRhi.h"

namespace Ovito {

static constexpr int kLocalSize = 8;

/******************************************************************************
* Loads a pre-compiled QSB shader from the Qt resource system.
******************************************************************************/
QShader OutlinesRhi::loadShader(const QString& name)
{
    QFile f(QStringLiteral(":/ovito/core/rendering/standard/shaders/%1.qsb").arg(name));
    [[maybe_unused]] bool ok = f.open(QIODevice::ReadOnly);
    return QShader::fromSerialized(f.readAll());
}

/******************************************************************************
* Allocates all GPU resources for the outline compute pipeline.
* Must be called once after construction, and again after any resize.
******************************************************************************/
bool OutlinesRhi::initialize(QRhi* rhi, int width, int height,
                             QRhiTexture* sceneDepthTex, QRhiTexture* excludedDepthTex)
{
    if(!rhi->isFeatureSupported(QRhi::Compute))
        return false;

    _rhi = rhi;
    _width = width;
    _height = height;
    _sceneDepthTex    = sceneDepthTex;
    _excludedDepthTex = excludedDepthTex;

    const quint32 n = quint32(width) * quint32(height);

    _nearestSampler.reset(rhi->newSampler(QRhiSampler::Nearest, QRhiSampler::Nearest,
                                          QRhiSampler::None,
                                          QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge));
    if(!_nearestSampler->create()) return false;

    // Params UBO: 64 bytes (std140).
    _ubuf.reset(rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 64));
    if(!_ubuf->create()) return false;

    for(int i = 0; i < 2; ++i) {
        _depthBuf[i].reset(rhi->newBuffer(QRhiBuffer::Static, QRhiBuffer::StorageBuffer, n * 4));
        if(!_depthBuf[i]->create()) return false;
        _instBuf[i].reset(rhi->newBuffer(QRhiBuffer::Static, QRhiBuffer::StorageBuffer, n * 4));
        if(!_instBuf[i]->create()) return false;
        _triBuf[i].reset(rhi->newBuffer(QRhiBuffer::Static, QRhiBuffer::StorageBuffer, n * 8));
        if(!_triBuf[i]->create()) return false;
    }

    _counterBuf.reset(rhi->newBuffer(QRhiBuffer::Static, QRhiBuffer::StorageBuffer, 4));
    if(!_counterBuf->create()) return false;

    // Persistent (not ping-ponged) per-pixel linearised depth of the frontmost excluded
    // surface, written by the init stage and consumed by the seed/setup and resolve stages.
    _exclDepthBuf.reset(rhi->newBuffer(QRhiBuffer::Static, QRhiBuffer::StorageBuffer, n * 4));
    if(!_exclDepthBuf->create()) return false;

    // One small StepParams UBO per jump-flood level (step = kMaxJfaStep >> level).
    // Their contents never change; they are (re)uploaded in record()'s update batch.
    for(int l = 0; l < kJfaLevels; ++l) {
        _jfaStepUbuf[l].reset(rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 16));
        if(!_jfaStepUbuf[l]->create()) return false;
    }

    _outlineTex.reset(rhi->newTexture(QRhiTexture::RGBA8, QSize(width, height), 1,
                                      QRhiTexture::UsedWithLoadStore));
    if(!_outlineTex->create()) return false;

    using B = QRhiShaderResourceBinding;
    constexpr auto CS = B::ComputeStage;

    // --- init: sceneDepthTex + excludedDepthTex (D32F) -> depthBuf[0], instBuf[0], exclDepthBuf
    // Binding 0: Params UBO
    // Binding 1: composited scene depth texture (D32F sampler2D, full scene)
    // Binding 2: DepthOut SSBO
    // Binding 3: InstOut SSBO
    // Binding 4: excluded-only depth texture (D32F sampler2D)
    // Binding 5: ExclDepthOut SSBO (linearised excluded surface depth)
    _initSrb.reset(rhi->newShaderResourceBindings());
    _initSrb->setBindings({
        B::uniformBuffer(0, CS, _ubuf.get()),
        B::sampledTexture(1, CS, _sceneDepthTex, _nearestSampler.get()),
        B::bufferStore(2, CS, _depthBuf[0].get()),
        B::bufferStore(3, CS, _instBuf[0].get()),
        B::sampledTexture(4, CS, _excludedDepthTex, _nearestSampler.get()),
        B::bufferStore(5, CS, _exclDepthBuf.get()),
    });
    if(!_initSrb->create()) return false;

    // --- cleanup ping-pong: [i] reads buf[i], writes buf[1-i]
    for(int i = 0; i < 2; ++i) {
        _cleanupSrb[i].reset(rhi->newShaderResourceBindings());
        _cleanupSrb[i]->setBindings({
            B::uniformBuffer(0, CS, _ubuf.get()),
            B::bufferLoad(1, CS, _depthBuf[i].get()),
            B::bufferLoad(2, CS, _instBuf[i].get()),
            B::bufferStore(3, CS, _depthBuf[1 - i].get()),
            B::bufferStore(4, CS, _instBuf[1 - i].get()),
            B::bufferLoadStore(5, CS, _counterBuf.get()),
        });
        if(!_cleanupSrb[i]->create()) return false;
    }

    // --- setup / JFA seed: cleaned depthBuf[src] + exclDepthBuf -> triBuf[0]
    // (the JFA seed shader uses the same binding layout, so these SRBs serve
    // both the triangle setup and the JFA seed dispatch)
    // Binding 3: ExclDepthBuf SSBO, so the seed/setup stage can suppress seeds/edges only
    // where the excluded surface is actually in front of the eligible geometry.
    for(int i = 0; i < 2; ++i) {
        _setupSrb[i].reset(rhi->newShaderResourceBindings());
        _setupSrb[i]->setBindings({
            B::uniformBuffer(0, CS, _ubuf.get()),
            B::bufferLoad(1, CS, _depthBuf[i].get()),
            B::bufferStore(2, CS, _triBuf[0].get()),
            B::bufferLoad(3, CS, _exclDepthBuf.get()),
        });
        if(!_setupSrb[i]->create()) return false;
    }

    // --- expand ping-pong: [i] reads triBuf[i], writes triBuf[1-i]
    for(int i = 0; i < 2; ++i) {
        _expandSrb[i].reset(rhi->newShaderResourceBindings());
        _expandSrb[i]->setBindings({
            B::uniformBuffer(0, CS, _ubuf.get()),
            B::bufferLoad(1, CS, _triBuf[i].get()),
            B::bufferStore(2, CS, _triBuf[1 - i].get()),
        });
        if(!_expandSrb[i]->create()) return false;
    }

    // --- JFA flood ping-pong: [level][i] reads triBuf[i], writes triBuf[1-i]
    //     with the StepParams UBO of the given level at binding 1.
    for(int l = 0; l < kJfaLevels; ++l) {
        for(int i = 0; i < 2; ++i) {
            _jfaFloodSrb[l][i].reset(rhi->newShaderResourceBindings());
            _jfaFloodSrb[l][i]->setBindings({
                B::uniformBuffer(0, CS, _ubuf.get()),
                B::uniformBuffer(1, CS, _jfaStepUbuf[l].get()),
                B::bufferLoad(2, CS, _triBuf[i].get()),
                B::bufferStore(3, CS, _triBuf[1 - i].get()),
            });
            if(!_jfaFloodSrb[l][i]->create()) return false;
        }
    }

    // --- resolve / JFA resolve: triBuf[i] + exclDepthBuf + depthBuf[0] -> outlineTex
    // (identical binding layout for both resolve shaders). Binding 4 provides the linearised
    // eligible depth so the JFA resolve can look up the depth of a ring pixel's seed edge and
    // suppress only where an excluded surface is in front of it. cleanupPasses is 0, so the
    // cleaned depth lives in _depthBuf[0].
    for(int i = 0; i < 2; ++i) {
        _resolveSrb[i].reset(rhi->newShaderResourceBindings());
        _resolveSrb[i]->setBindings({
            B::uniformBuffer(0, CS, _ubuf.get()),
            B::bufferLoad(1, CS, _triBuf[i].get()),
            B::imageStore(2, CS, _outlineTex.get(), 0),
            B::bufferLoad(3, CS, _exclDepthBuf.get()),
            B::bufferLoad(4, CS, _depthBuf[0].get()),
        });
        if(!_resolveSrb[i]->create()) return false;
    }

    auto makePipeline = [&](const QString& shaderName,
                            QRhiShaderResourceBindings* layoutSrb)
        -> std::unique_ptr<QRhiComputePipeline>
    {
        QShader cs = loadShader(shaderName);
        if(!cs.isValid())
            return {};
        std::unique_ptr<QRhiComputePipeline> ps(rhi->newComputePipeline());
        ps->setShaderStage({ QRhiShaderStage::Compute, cs });
        ps->setShaderResourceBindings(layoutSrb);
        if(!ps->create())
            return {};
        return ps;
    };

    _initPipeline    = makePipeline(QStringLiteral("outlines_init.comp"),    _initSrb.get());
    _cleanupPipeline = makePipeline(QStringLiteral("outlines_cleanup.comp"), _cleanupSrb[0].get());
    _setupPipeline   = makePipeline(QStringLiteral("outlines_setup.comp"),   _setupSrb[0].get());
    _expandPipeline  = makePipeline(QStringLiteral("outlines_expand.comp"),  _expandSrb[0].get());
    _resolvePipeline = makePipeline(QStringLiteral("outlines_resolve.comp"), _resolveSrb[0].get());

    if(!_initPipeline || !_cleanupPipeline || !_setupPipeline
       || !_expandPipeline || !_resolvePipeline)
        return false;

    // Jump-flood pipelines. Optional: if the shaders are missing from the
    // resource system, fall back to the triangle path instead of failing.
    _jfaSeedPipeline    = makePipeline(QStringLiteral("outlines_jfa_seed.comp"),    _setupSrb[0].get());
    _jfaFloodPipeline   = makePipeline(QStringLiteral("outlines_jfa_flood.comp"),   _jfaFloodSrb[0][0].get());
    _jfaResolvePipeline = makePipeline(QStringLiteral("outlines_jfa_resolve.comp"), _resolveSrb[0].get());
    _jfaAvailable = _jfaSeedPipeline && _jfaFloodPipeline && _jfaResolvePipeline;

    return true;
}

/******************************************************************************
* Issues one compute dispatch covering the full frame, wrapped in its own
* compute pass so that the Vulkan backend inserts the required pipeline
* barriers between passes.  rub carries CPU→GPU uploads for the first pass;
* every subsequent call passes nullptr.
******************************************************************************/
void OutlinesRhi::dispatch(QRhiCommandBuffer* cb, QRhiComputePipeline* ps,
                           QRhiShaderResourceBindings* srb, QRhiResourceUpdateBatch* rub)
{
    cb->beginComputePass(rub);
    cb->setComputePipeline(ps);
    cb->setShaderResources(srb);
    cb->dispatch((_width  + kLocalSize - 1) / kLocalSize,
                 (_height + kLocalSize - 1) / kLocalSize, 1);
    cb->endComputePass();
}

/******************************************************************************
* Records the full outline compute pass into cb.
* Call this once per frame, after the outline depth pre-pass has finished.
******************************************************************************/
void OutlinesRhi::record(QRhiCommandBuffer* cb)
{
    const bool useJfa = _useJumpFlood && _jfaAvailable;

    // std140 layout, 64 bytes.
    struct Params {
        qint32 size[2];         // offset  0
        float  depthRange[2];   // offset  8
        float  widthRange[2];   // offset 16
        qint32 despike;         // offset 24
        qint32 pad;             // offset 28
        float  color[4];        // offset 32
        float  nearPlane;       // offset 48
        float  farPlane;        // offset 52
        qint32 isPerspective;   // offset 56
        qint32 padDepthRange;   // offset 60, unused (was depthZeroToOne; the sampled depth
                                //            attachment is always window-space [0,1] on every
                                //            backend, see outlines_init.comp)
    };
    static_assert(sizeof(Params) == 64);

    Params params{};
    params.size[0]       = _width;
    params.size[1]       = _height;
    params.depthRange[0] = _minDepthDiff;
    params.depthRange[1] = _maxDepthDiff;
    params.widthRange[0] = _minLineWidth;
    params.widthRange[1] = _maxLineWidth;
    params.despike       = _despike ? 1 : 0;
    params.pad           = 0;
    params.color[0]      = _color[0];
    params.color[1]      = _color[1];
    params.color[2]      = _color[2];
    params.color[3]      = 1.0f;
    params.nearPlane     = _nearPlane;
    params.farPlane      = _farPlane;
    params.isPerspective = _isPerspective  ? 1 : 0;
    params.padDepthRange = 0;

    QRhiResourceUpdateBatch* rub = _rhi->nextResourceUpdateBatch();
    rub->updateDynamicBuffer(_ubuf.get(), 0, sizeof(params), &params);
    const quint32 zero = 0;
    rub->uploadStaticBuffer(_counterBuf.get(), 0, 4, &zero);

    if(useJfa) {
        // StepParams UBOs (std140, single int padded to 16 bytes). Constant,
        // but uploading 7 x 16 bytes per frame is cheaper than tracking state.
        struct StepParams { qint32 step; qint32 pad[3]; };
        for(int l = 0; l < kJfaLevels; ++l) {
            StepParams sp{ kMaxJfaStep >> l, { 0, 0, 0 } };
            rub->updateDynamicBuffer(_jfaStepUbuf[l].get(), 0, sizeof(sp), &sp);
        }
    }

    // Each dispatch runs in its own compute pass.  The Vulkan backend inserts
    // a pipeline barrier at every beginComputePass, which is required because
    // the ping-pong buffers (_depthBuf, _triBuf) alternate between bufferStore
    // and bufferLoad access across successive dispatches.  Putting all
    // dispatches in one pass would prevent the backend from inserting those
    // barriers, causing the Vulkan validation warning
    // "Buffer used with different accesses within the same pass".

    // 1. Import linearised depth into SSBO set 0.
    dispatch(cb, _initPipeline.get(), _initSrb.get(), rub);

    // 2. Cleanup: 0 passes in the OVITO integration.
    int src = 0;
    for(int i = 0; i < _cleanupPasses; ++i) {
        dispatch(cb, _cleanupPipeline.get(), _cleanupSrb[src].get());
        src = 1 - src;
    }
    // Cleaned depth now lives in _depthBuf[src].

    int triSrc = 0;
    if(useJfa) {
        // 3a. Seed: edge pixels with their pen sizes into _triBuf[0].
        dispatch(cb, _jfaSeedPipeline.get(), _setupSrb[src].get());

        // 4a. Jump flood. Pick the first level so the initial step is the
        // smallest power of two covering the maximum pen radius; smaller
        // line widths skip the coarse levels entirely.
        const int maxPen   = std::clamp(int(_maxLineWidth), 0, 63);
        const int maxReach = maxPen + 1;  // +0.5 px AA rim, rounded up
        int level = 0;
        while(level < kJfaLevels - 1 && (kMaxJfaStep >> (level + 1)) >= maxReach)
            ++level;
        for(int l = level; l < kJfaLevels; ++l) {
            dispatch(cb, _jfaFloodPipeline.get(), _jfaFloodSrb[l][triSrc].get());
            triSrc = 1 - triSrc;
        }

        // 5a. Resolve seed field into the premultiplied RGBA8 outline layer.
        dispatch(cb, _jfaResolvePipeline.get(), _resolveSrb[triSrc].get());
    }
    else {
        // 3b. Setup initial triangle boundaries into _triBuf[0].
        dispatch(cb, _setupPipeline.get(), _setupSrb[src].get());

        // 4b. Expand outline rings (maxLineWidth+1 passes), ping-ponging the triangle buffers.
        const int expandPasses = int(_maxLineWidth) + 1;
        for(int i = 0; i < expandPasses; ++i) {
            dispatch(cb, _expandPipeline.get(), _expandSrb[triSrc].get());
            triSrc = 1 - triSrc;
        }

        // 5b. Resolve coverage into the premultiplied RGBA8 outline layer.
        dispatch(cb, _resolvePipeline.get(), _resolveSrb[triSrc].get());
    }
}

/******************************************************************************
* Releases all owned GPU resources.
******************************************************************************/
void OutlinesRhi::destroy()
{
    _jfaResolvePipeline.reset(); _jfaFloodPipeline.reset(); _jfaSeedPipeline.reset();
    _resolvePipeline.reset(); _expandPipeline.reset(); _setupPipeline.reset();
    _cleanupPipeline.reset(); _initPipeline.reset();
    for(int l = 0; l < kJfaLevels; ++l) {
        _jfaFloodSrb[l][0].reset(); _jfaFloodSrb[l][1].reset();
        _jfaStepUbuf[l].reset();
    }
    for(int i = 0; i < 2; ++i) {
        _resolveSrb[i].reset(); _expandSrb[i].reset();
        _setupSrb[i].reset();   _cleanupSrb[i].reset();
        _triBuf[i].reset(); _instBuf[i].reset(); _depthBuf[i].reset();
    }
    _initSrb.reset();
    _exclDepthBuf.reset();
    _counterBuf.reset(); _ubuf.reset();
    _outlineTex.reset(); _nearestSampler.reset();
    _jfaAvailable = false;
    _rhi = nullptr;
}

}   // namespace Ovito
