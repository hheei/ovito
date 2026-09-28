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

namespace Ovito {

class RenderThread; // Forward declaration

/**
 * Tiles the "OVITO Pro Demo" watermark image across the full render target
 * using QRhi directly, without modifying the FrameGraph.
 *
 * Only compiled in OVITO Basic builds (OVITO_BUILD_BASIC).
 * Lives on the render thread. Owned by RenderThread::TargetState.
 * Lazily initializes GPU resources on first use.
 */
class OVITO_CORE_EXPORT WatermarkRenderer
{
public:

    /// Constructor. The watermark image is converted to RGBA8888 for GPU upload.
    WatermarkRenderer(RenderThread* rt, QImage watermarkImage);

    /// Destructor.
    ~WatermarkRenderer();

    /// Uploads the watermark texture (lazy, first call only) and updates the tiling UBO.
    /// Must be called BEFORE beginPass() with the same resource update batch.
    /// \param batch         Resource update batch to append upload commands to.
    /// \param renderTarget  The final render target (used for render size in the UBO).
    void prepareResourceUpdates(QRhiResourceUpdateBatch* batch, QRhiRenderTarget* renderTarget);

    /// Draws the fullscreen tiled watermark quad.
    /// Must be called INSIDE an active render pass (after beginPass(), before endPass()).
    void compositeInPass(QRhiCommandBuffer* cb, QRhiRenderPassDescriptor* rpd);

private:

    /// (Re-)creates the graphics pipeline for the given RPD.
    void ensurePipeline(QRhiRenderPassDescriptor* rpd);

    RenderThread* _rt = nullptr;
    QRhi* _rhi = nullptr;

    /// Source watermark image (RGBA8888, kept until first GPU upload).
    QImage _watermarkImage;

    bool _textureUploaded = false;

    std::unique_ptr<QRhiTexture> _watermarkTexture;
    std::unique_ptr<QRhiSampler> _watermarkSampler;   ///< Repeat wrap mode for tiling.
    std::unique_ptr<QRhiBuffer>  _ubo;
    std::unique_ptr<QRhiShaderResourceBindings> _bindings;
    std::unique_ptr<QRhiGraphicsPipeline> _pipeline;

    /// The RPD the current pipeline was built for. Rebuild when it changes.
    QRhiRenderPassDescriptor* _currentRPD = nullptr;

    /// Plain-old-data struct matching the std140 UBO in watermark.vert.
    struct UBOData {
        float renderWidth;          ///< Render target width in pixels.
        float renderHeight;         ///< Render target height in pixels.
        float watermarkWidth;       ///< Watermark texture width in pixels.
        float watermarkHeight;      ///< Watermark texture height in pixels.
        int   isYUpInNDC;           ///< 1 = Metal/D3D/OpenGL (NDC y-up), 0 = Vulkan (NDC y-down).
        int   isYUpInFramebuffer;   ///< 1 = OpenGL only (framebuffer v=0 at bottom), 0 = others.
        int   _pad0, _pad1;         ///< Pad struct to 32 bytes (std140 requires block size multiple of 16).
    };
};

}   // End of namespace
