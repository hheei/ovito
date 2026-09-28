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

#pragma once

#include <ovito/core/Core.h>
#include <rhi/qrhi.h>
#include <memory>
#include <array>

namespace Ovito {

/*
 * GPU outline pass implemented via QRhi compute pipelines.
 *
 * Port of the old Outlines_Cuda pipeline: init → (cleanup×N) → setup → expand×M → resolve.
 * In the OVITO integration cleanupPasses is always 0 because the per-pixel exclusion flag
 * (derived from excludedDepthTex) already handles ExcludeFromOutline geometry; instance IDs
 * in the SSBO are all 0 (valid).
 *
 * Two interchangeable implementations of the outline dilation are provided:
 *
 *   Jump-flood path (default): seed → flood×log2(maxLineWidth) → resolve.
 *     Every edge pixel becomes a seed carrying its pen size; a jump-flood pass
 *     propagates, per pixel, the seed maximizing (pen − distance); the resolve
 *     stage converts that into coverage with a ~1 px smooth rim. At most
 *     2 + log2(64) = 9 dispatches regardless of line width (e.g. width 4 → 5
 *     dispatches), instead of maxLineWidth+1 expand passes (up to 64). The
 *     pen footprint is circular with gradient anti-aliasing, visually near-
 *     identical to (slightly smoother than) the triangle scheme, but not
 *     bit-identical.
 *
 *   Triangle path (setUseJumpFlood(false)): the faithful port of the CUDA
 *     triangle expansion — roughly octagonal pen, 1/8-step popcount coverage.
 *     Bit-identical to the previous behavior of this class.
 *
 * The seed/flood ping-pong reuses the _triBuf buffers (uvec2 per pixel:
 * packed seed coords + pen size), so the JFA path allocates no extra
 * per-pixel memory. If the JFA shaders are missing from the resource system,
 * initialize() falls back to the triangle path instead of failing.
 *
 * Input:
 *   sceneDepthTex    — D32F depth of the full composited scene (all rendering backends),
 *                      used for both edge detection and pen-width scaling.
 *   excludedDepthTex — D32F depth of ExcludeFromOutline geometry ONLY (simulation cell,
 *                      gizmos, slicing plane), cleared to 1.0 elsewhere. The init stage
 *                      derives a per-pixel exclusion flag (excluded surface frontmost)
 *                      from it. Excluded pixels are suppressed in the resolve stage and
 *                      ignored in edge detection, so excluded objects neither receive an
 *                      outline nor create spurious edges against the eligible geometry
 *                      behind them.
 *
 * Output:
 *   outlineTexture() — RGBA8 premultiplied outline layer.
 *                      Blend onto the viewport target with (One, OneMinusSrcAlpha).
 *
 * Usage:
 *   1. Call initialize() once (or after resize).
 *   2. Before each frame: set parameters (setMinDepthDiff etc.) and call record(cb).
 *   3. In the post-process fragment pass: sample outlineTexture() and blend.
 *
 * Requires rhi->isFeatureSupported(QRhi::Compute).
 */
class OVITO_CORE_EXPORT OutlinesRhi
{
public:
    bool initialize(QRhi* rhi, int width, int height,
                    QRhiTexture* sceneDepthTex, QRhiTexture* excludedDepthTex);
    void destroy();

    // Parameters (mirrors Outlines_Cuda interface).
    void setMinDepthDiff(float v) { _minDepthDiff = v; }
    void setMaxDepthDiff(float v) { _maxDepthDiff = v; }
    void setMinLineWidth(float v) { _minLineWidth = v; }
    // 6-bit step counts in the packed triangle buffer -> max 63 px.
    void setMaxLineWidth(float v) { _maxLineWidth = std::min(v, 63.0F); }
    void setColor(float r, float g, float b) { _color = { r, g, b }; }

    void setNearPlane(float v)    { _nearPlane = v; }
    void setFarPlane(float v)     { _farPlane = v; }
    void setIsPerspective(bool v) { _isPerspective = v; }

    // Enables despiking of isolated depth outliers in the init stage. Needed only when the
    // scene depth comes from a raytracer casting a single crisp primary ray (VisRTX/OSPRay
    // compositing), which can miss an analytic primitive at an internal seam and leave a lone
    // far-plane pixel. Pure QRhi rasterizer depth is spatially coherent, so it stays off there
    // and the neighbour fetches are skipped.
    void setDespike(bool v) { _despike = v; }

    // Fixed number of cleanup iterations. In the OVITO integration this is always 0.
    void setCleanupPasses(int n) { _cleanupPasses = n; }

    // Selects the dilation implementation: jump-flood (default, O(log width)
    // dispatches, circular anti-aliased pen) or the faithful CUDA triangle
    // expansion (O(width) dispatches, bit-identical to the previous output).
    // Ignored (treated as false) when the JFA pipelines are unavailable.
    void setUseJumpFlood(bool v) { _useJumpFlood = v; }
    [[nodiscard]] bool isJumpFloodAvailable() const { return _jfaAvailable; }

    // Records init -> cleanup* -> dilation -> resolve into one compute pass on cb.
    void record(QRhiCommandBuffer* cb);

    [[nodiscard]] QRhiTexture* outlineTexture() const { return _outlineTex.get(); }

    int width()  const { return _width; }
    int height() const { return _height; }

private:
    QShader loadShader(const QString& name);
    void dispatch(QRhiCommandBuffer* cb, QRhiComputePipeline* ps,
                  QRhiShaderResourceBindings* srb, QRhiResourceUpdateBatch* rub = nullptr);

    // Number of jump-flood step levels: steps 64, 32, 16, 8, 4, 2, 1.
    // Level l uses step (kMaxJfaStep >> l). 64 covers the maximum pen radius
    // (63 px) representable by the 6-bit triangle packing.
    static constexpr int kJfaLevels  = 7;
    static constexpr int kMaxJfaStep = 64;

    QRhi* _rhi = nullptr;
    int _width = 0;
    int _height = 0;

    float _minDepthDiff = 0.0F;
    float _maxDepthDiff = 0.0F;
    float _minLineWidth = 0.0F;
    float _maxLineWidth = 0.0F;
    std::array<float, 3> _color = {0.0F, 0.0F, 0.0F};
    float _nearPlane = 0.1F;
    float _farPlane = 1000.0F;
    bool  _isPerspective = true;
    bool  _despike = false;
    int   _cleanupPasses = 0; // always 0 in OVITO (outlineDepthTexture pre-filters)
    bool  _useJumpFlood  = true;
    bool  _jfaAvailable  = false;

    // Not owned:
    QRhiTexture* _sceneDepthTex    = nullptr; ///< D32F full composited scene depth.
    QRhiTexture* _excludedDepthTex = nullptr; ///< D32F depth of ExcludeFromOutline geometry only.

    // Owned resources:
    std::unique_ptr<QRhiSampler> _nearestSampler;
    std::unique_ptr<QRhiBuffer>  _ubuf;            ///< Params UBO (std140, 64 bytes).
    std::unique_ptr<QRhiBuffer>  _depthBuf[2];     ///< float per pixel, ping-pong.
    std::unique_ptr<QRhiBuffer> _instBuf[2];       ///< uint per pixel, ping-pong.
    std::unique_ptr<QRhiBuffer>  _triBuf[2];       ///< uvec2 per pixel, ping-pong.
                                                   ///< Triangle path: packed triangle state.
                                                   ///< JFA path: packed seed coords + pen size.
    std::unique_ptr<QRhiBuffer>  _counterBuf;      ///< Single uint, cleanup convergence.
    std::unique_ptr<QRhiBuffer>  _exclDepthBuf;    ///< float per pixel: linearised depth of the frontmost excluded surface (large = none).
    std::unique_ptr<QRhiBuffer>  _jfaStepUbuf[kJfaLevels]; ///< StepParams UBOs, one per flood level.
    std::unique_ptr<QRhiTexture> _outlineTex;      ///< RGBA8 premultiplied output layer.

    std::unique_ptr<QRhiComputePipeline> _initPipeline;
    std::unique_ptr<QRhiComputePipeline> _cleanupPipeline;
    std::unique_ptr<QRhiComputePipeline> _setupPipeline;
    std::unique_ptr<QRhiComputePipeline> _expandPipeline;
    std::unique_ptr<QRhiComputePipeline> _resolvePipeline;
    std::unique_ptr<QRhiComputePipeline> _jfaSeedPipeline;    ///< Reuses _setupSrb layout.
    std::unique_ptr<QRhiComputePipeline> _jfaFloodPipeline;
    std::unique_ptr<QRhiComputePipeline> _jfaResolvePipeline; ///< Reuses _resolveSrb layout.

    std::unique_ptr<QRhiShaderResourceBindings> _initSrb;
    std::unique_ptr<QRhiShaderResourceBindings> _cleanupSrb[2];
    std::unique_ptr<QRhiShaderResourceBindings> _setupSrb[2];   ///< Also used by the JFA seed stage.
    std::unique_ptr<QRhiShaderResourceBindings> _expandSrb[2];
    std::unique_ptr<QRhiShaderResourceBindings> _resolveSrb[2]; ///< Also used by the JFA resolve stage.
    std::unique_ptr<QRhiShaderResourceBindings> _jfaFloodSrb[kJfaLevels][2]; ///< [level][source parity].
};

}   // namespace Ovito
