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
#include <ovito/core/rendering/ImagePrimitive.h>
#include <ovito/core/rendering/FrameGraph.h>
#include "PrimitiveRenderer.h"

namespace Ovito {

/**
 * Renders ImagePrimitive commands using QRhi.
 * Used by StandardRendererImplementation.
 */
class OVITO_CORE_EXPORT ImagePrimitiveRenderer : public PrimitiveRenderer
{
public:

    /// Constructor.
    using PrimitiveRenderer::PrimitiveRenderer;

    /// Phase 1: Builds draw calls from image primitives in the frame graph.
    void buildDrawCalls(const ImagePrimitive& primitive, const FrameGraph::RenderingCommand& command,
                        FrameGraph::RenderLayerType layer, QSize renderSize, bool isYUpInNDC);

    /// Phase 2: Uploads image textures and updates UBOs.
    void prepareResourceUpdates(QRhiResourceUpdateBatch* batch, QSize renderSize, bool isYUpInNDC);

    /// Phase 3: Issues draw calls for the given layer inside the render pass.
    void draw(QRhiCommandBuffer* cb, QRhiRenderPassDescriptor* rpd, FrameGraph::RenderLayerType layer);

    /// Clears the draw call list for the next frame.
    void clear();

private:

    /// Ensures the pipeline is valid for the given RPD.
    QRhiGraphicsPipeline* ensurePipeline(QRhiRenderPassDescriptor* rpd);

private:

    /// Per-draw-call GPU resources (texture + bindings).
    struct DrawCall
    {
        QImage image;                               ///< The image data to upload.
        qint64 uploadedCacheKey;                    ///< The cache key of the last image uploaded as a texture.
        QRectF ndcRect;                             ///< The NDC rectangle for rendering.
        FrameGraph::RenderLayerType layer;          ///< The rendering layer.
        std::unique_ptr<QRhiTexture> texture;       ///< The GPU texture for the image.
        std::unique_ptr<QRhiShaderResourceBindings> bindings; ///< The shader resource bindings for the draw call.
    };
    std::vector<DrawCall> _drawCalls;
    quint32 _numDrawCalls = 0; ///< Number of recorded draw calls in the current frame.

    /// GPU resources shared across all image draw calls.
    std::unique_ptr<QRhiBuffer> _imageParamsUBO;
    std::unique_ptr<QRhiSampler> _sampler;

    /// Aligned size of one image-params slot in the dynamic UBO.
    quint32 _imageParamsAlignedSize = 0;
};

}   // End of namespace
