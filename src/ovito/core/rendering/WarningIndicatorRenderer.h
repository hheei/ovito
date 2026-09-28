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
 * Draws a small warning icon quad in the upper-right corner of a render pass
 * using QRhi directly, without modifying the FrameGraph.
 *
 * Lives on the render thread. Owned by RenderThread::TargetState (onscreen targets only).
 * Lazily initializes GPU resources on first use.
 */
class OVITO_CORE_EXPORT WarningIndicatorRenderer
{
public:

    /// Constructor. The icon image is converted to RGBA8888 for GPU upload.
    WarningIndicatorRenderer(RenderThread* rt, QImage iconImage);

    /// Destructor.
    ~WarningIndicatorRenderer();

    /// Uploads the icon texture (lazy, first call only) and updates the position UBO.
    /// Must be called BEFORE beginPass() with the same resource update batch.
    /// \param batch    Resource update batch to append upload commands to.
    /// \param renderTarget  The render target that will be used for rendering.
    /// \param devicePixelRatio  HiDPI scale factor for the window.
    void prepareResourceUpdates(QRhiResourceUpdateBatch* batch, QRhiRenderTarget* renderTarget, qreal devicePixelRatio);

    /// Draws the icon quad. Must be called INSIDE an active render pass (after beginPass(), before endPass()).
    /// Ensures the graphics pipeline is valid for the given RPD.
    /// \returns The icon area in device-independent pixels (for tooltip hit-testing), or empty QRectF on failure.
    QRectF compositeInPass(QRhiCommandBuffer* cb, QRhiRenderPassDescriptor* rpd);

private:

    /// (Re-)creates the graphics pipeline for the given RPD.
    void ensurePipeline(QRhiRenderPassDescriptor* rpd);

    RenderThread* _rt = nullptr;
    QRhi* _rhi = nullptr;

    /// Source icon image (RGBA8888, kept until first GPU upload).
    QImage _iconImage;

    bool _textureUploaded = false;

    /// Icon area in device-independent pixels; set in prepareResourceUpdates, returned by compositeInPass.
    QRectF _lastIconArea;

    std::unique_ptr<QRhiTexture> _iconTexture;
    std::unique_ptr<QRhiSampler> _iconSampler;
    std::unique_ptr<QRhiBuffer>  _ubo;
    std::unique_ptr<QRhiShaderResourceBindings> _bindings;
    std::unique_ptr<QRhiGraphicsPipeline> _pipeline;

    /// The RPD the current pipeline was built for. Rebuild when it changes.
    QRhiRenderPassDescriptor* _currentRPD = nullptr;

    /// Plain-old-data struct matching the std140 UBO in warning_indicator.vert.
    struct UBOData {
        float x;                 ///< NDC x of left edge.
        float y_top;             ///< NDC y of top edge (pre-computed for the current NDC convention).
        float w;                 ///< NDC width (positive).
        float y_bot;             ///< NDC y of bottom edge (pre-computed for the current NDC convention).
        int   isYUpInFramebuffer;///< 1 = OpenGL (v=0 at bottom), 0 = Vulkan/D3D/Metal (v=0 at top).
        int   _pad0, _pad1, _pad2; ///< Pad struct to 32 bytes (std140 requires block size multiple of 16).
    };
};

}   // End of namespace
