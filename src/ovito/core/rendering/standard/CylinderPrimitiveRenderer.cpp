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
#include <ovito/core/viewport/ViewProjectionParameters.h>
#include <ovito/core/dataset/data/BufferAccess.h>
#include <ovito/core/utilities/SortZipped.h>
#include <ovito/core/rendering/RenderThread.h>
#include "CylinderPrimitiveRenderer.h"
#include "StandardRendererImplementation.h"

namespace Ovito {

/// Returns the bytes consumed per vertex in Metal's TBDR parameter buffer for each cylinder shader variant.
/// Values measured from the `out` varying declarations in the geometry shader snippets plus gl_Position (16B).
static size_t metalBytesPerVertex(CylinderPrimitiveRenderer::ShaderVariant v)
{
    using SV = CylinderPrimitiveRenderer::ShaderVariant;
    switch(v) {
        // CylinderRaycast: 2×vec4+2×vec3+2×float+uint + gl_Position = 84B
        case SV::CylinderRaycast:
        case SV::CylinderRaycastPicking:
        case SV::CylinderRaycastSorted:
            return 84;
        // ArrowHeadRaycast: vec4+3×vec3+float+uint + gl_Position = 88B
        case SV::ArrowHeadRaycast:
        case SV::ArrowHeadRaycastPicking:
        case SV::ArrowHeadRaycastSorted:
            return 88;
        // ArrowTailRaycast: vec4+2×vec3+2×float+2×vec3+uint + gl_Position = 92B
        case SV::ArrowTailRaycast:
        case SV::ArrowTailRaycastPicking:
        case SV::ArrowTailRaycastSorted:
            return 92;
        // CylinderFlat / ArrowFlat (flat billboard): compact varying layout, 36B
        case SV::CylinderFlat:
        case SV::CylinderFlatPicking:
        case SV::CylinderFlatSorted:
        case SV::ArrowFlat:
        case SV::ArrowFlatPicking:
        case SV::ArrowFlatSorted:
            return 36;
        default:
            return 64; // Conservative fallback.
    }
}

size_t CylinderPrimitiveRenderer::metalParamBufferUsage() const {
    size_t total = 0;
    for(const DrawCall& dc : _drawCalls)
        total += metalBytesPerVertex(dc.shader) * dc.cylinderCount * dc.verticesPerInstance;
    return total;
}

/// Must match the std140 'CylinderDrawParams' UBO layout in cylinders_draw_params.glsl.
struct CylinderDrawParamsData {
    Matrix4F  modelViewMatrix;       // offset   0, 64 bytes
    ColorAF   selectionColor;        // offset  64, 16 bytes
    float     uniformWidth;          // offset  80
    uint32_t  pickingBaseObjectId;   // offset  84
    float     colorRangeMin;         // offset  88
    float     colorRangeMax;         // offset  92
    float     viewDirEyePosX;        // offset  96
    float     viewDirEyePosY;        // offset 100
    float     viewDirEyePosZ;        // offset 104
    float     viewDirEyePosW;        // offset 108  (0=ortho, 1=perspective)
    int32_t   singleCylinderCap;     // offset 112
    float     _pad0;                 // offset 116
    float     _pad1;                 // offset 120
    float     _pad2;                 // offset 124
};  // 128 bytes total
static_assert(sizeof(CylinderDrawParamsData) == 128);

/******************************************************************************
* Quantizes a view direction for cache key purposes (reduces re-sort frequency).
******************************************************************************/
static Vector3F coarsenViewDir(const Vector3F& dir, bool isInteractive)
{
    if(!isInteractive)
        return dir;
    return Vector3F(
        std::round(dir.x() * 2.0f) * 0.5f,
        std::round(dir.y() * 2.0f) * 0.5f,
        std::round(dir.z() * 2.0f) * 0.5f);
}

/******************************************************************************
* Returns the SSBO sorted variant of a non-picking variant.
******************************************************************************/
CylinderPrimitiveRenderer::ShaderVariant CylinderPrimitiveRenderer::toSortedVariant(ShaderVariant v)
{
    switch(v) {
        case ShaderVariant::CylinderRaycast:   return ShaderVariant::CylinderRaycastSorted;
        case ShaderVariant::CylinderFlat:      return ShaderVariant::CylinderFlatSorted;
        case ShaderVariant::ArrowHeadRaycast:  return ShaderVariant::ArrowHeadRaycastSorted;
        case ShaderVariant::ArrowTailRaycast:  return ShaderVariant::ArrowTailRaycastSorted;
        case ShaderVariant::ArrowFlat:         return ShaderVariant::ArrowFlatSorted;
        default:                               return v;
    }
}

/******************************************************************************
* Phase 1: Builds draw calls from cylinder primitives.
******************************************************************************/
void CylinderPrimitiveRenderer::buildDrawCalls(const CylinderPrimitive& primitive,
                                               const FrameGraph::RenderingCommand& command,
                                               bool isPickingPass, ObjectPickingMap* pickingMap,
                                               FrameGraph::RenderLayerType layer, bool filteredOut)
{
    if(!primitive.vertexPositions() || primitive.vertexPositions()->size() == 0)
        return;
    if(primitive.vertexPositions()->size() % 2 != 0) {
        rt()->reportWarning("CylinderPrimitiveRenderer: vertexPositions buffer size must be a multiple of 2 (two endpoints per cylinder).");
        return;
    }

    const bool isArrow  = (primitive.shape() == CylinderPrimitive::ArrowShape);
    const bool isNormal = (primitive.shadingMode() == CylinderPrimitive::NormalShading);

    // Determine base shader variant.
    ShaderVariant variant;
    uint32_t verticesPerInstance;
    QRhiGraphicsPipeline::Topology topology = QRhiGraphicsPipeline::TriangleStrip;

    if(isArrow) {
        if(isNormal) {
            variant = isPickingPass ? ShaderVariant::ArrowHeadRaycastPicking : ShaderVariant::ArrowHeadRaycast;
            verticesPerInstance = 14;
        } else {
            variant = isPickingPass ? ShaderVariant::ArrowFlatPicking : ShaderVariant::ArrowFlat;
            verticesPerInstance = 15;
            topology = QRhiGraphicsPipeline::Triangles;
        }
    }
    else {
        if(isNormal) {
            variant = isPickingPass ? ShaderVariant::CylinderRaycastPicking : ShaderVariant::CylinderRaycast;
            verticesPerInstance = 14;
        } else {
            variant = isPickingPass ? ShaderVariant::CylinderFlatPicking : ShaderVariant::CylinderFlat;
            verticesPerInstance = 4;
        }
    }

    const bool isPseudoColor = !isPickingPass
        && primitive.colors()
        && primitive.colors()->componentCount() == 1
        && primitive.pseudoColorMapping().isValid();

    // Allocate picking object ID once for this primitive.
    uint32_t objectId = 0;
    if(isPickingPass && pickingMap) {
        objectId = pickingMap->registerObjectId(rt()->objectIdAllocator().allocate(), command);
    }

    // Capture these before emplace_back calls that could invalidate any reference to dc.
    const size_t cylinderCount    = primitive.vertexPositions()->size() / 2;
    const bool   hasTransparency  = !isPickingPass && (bool)primitive.transparencies();

    DrawCall& dc = _drawCalls.emplace_back();
    dc.primitive           = &primitive;
    dc.layer               = layer;
    dc.modelWorldTM        = command.modelWorldTM();
    dc.shader              = variant;
    dc.cylinderCount       = cylinderCount;
    dc.verticesPerInstance = verticesPerInstance;
    dc.topology            = topology;
    dc.hasTransparency     = hasTransparency;
    dc.isPseudoColor       = isPseudoColor;
    dc.objectId            = objectId;
    dc.excludeFromOutline  = command.excludeFromOutline();
    dc.filteredOut         = filteredOut;

    // NormalShading arrows need a second draw call for the cylinder tail.
    if(isArrow && isNormal) {
        DrawCall& tailDc = _drawCalls.emplace_back();
        tailDc.primitive           = &primitive;
        tailDc.layer               = layer;
        tailDc.modelWorldTM        = command.modelWorldTM();
        tailDc.shader              = isPickingPass ? ShaderVariant::ArrowTailRaycastPicking : ShaderVariant::ArrowTailRaycast;
        tailDc.cylinderCount       = cylinderCount;
        tailDc.verticesPerInstance = 14;
        tailDc.topology            = QRhiGraphicsPipeline::TriangleStrip;
        tailDc.hasTransparency     = hasTransparency;
        tailDc.isPseudoColor       = isPseudoColor;
        tailDc.objectId            = objectId;  // Same object ID → picking selects same bond.
        tailDc.excludeFromOutline  = command.excludeFromOutline();
        tailDc.filteredOut         = filteredOut;
    }
}

/******************************************************************************
* Phase 2: Uploads vertex buffers and UBOs.
******************************************************************************/
void CylinderPrimitiveRenderer::prepareResourceUpdates(QRhiResourceUpdateBatch* batch,
                                                       const ViewProjectionParameters& projParams,
                                                       QSize renderSize, bool isYUpInNDC, bool isYUpInFramebuffer,
                                                       bool isPicking, bool isInteractive)
{
    if(_drawCalls.empty())
        return;

    _drawParamsAlignedSize = rhi()->ubufAligned(sizeof(CylinderDrawParamsData));
    const quint32 requiredSize = _drawParamsAlignedSize * static_cast<quint32>(_drawCalls.size());
    if(!_drawParamsUBO) {
        _drawParamsUBO.reset(rhi()->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, requiredSize));
        if(!_drawParamsUBO->create()) {
            rt()->reportWarning("CylinderPrimitiveRenderer: Failed to create draw params UBO.");
            _drawParamsUBO.reset();
            return;
        }
    }
    else if(_drawParamsUBO->size() < requiredSize) {
        _drawParamsUBO->setSize(requiredSize);
        if(!_drawParamsUBO->create()) {
            rt()->reportWarning("CylinderPrimitiveRenderer: Failed to resize draw params UBO.");
            _drawParamsUBO.reset();
            return;
        }
    }

    // Upload per-draw params.
    for(size_t i = 0; i < _drawCalls.size(); i++) {
        const DrawCall& dc = _drawCalls[i];
        const CylinderPrimitive& prim = *dc.primitive;

        // Compute modelView matrix.
        AffineTransformation modelViewAff = projParams.viewMatrix * dc.modelWorldTM;

        // Compute viewDirEyePos in object space.
        AffineTransformation invModelView = modelViewAff.inverse();
        float vdX, vdY, vdZ, vdW;
        if(projParams.isPerspective) {
            // Eye position in object space = translation of inverse(modelViewMatrix).
            const auto t = invModelView.translation().toDataType<float>();
            vdX = t.x(); vdY = t.y(); vdZ = t.z(); vdW = 1.0f;
        }
        else {
            // View "+Z" direction in object space (toward viewer).
            const auto col2 = invModelView.linear().column(2).toDataType<float>();
            vdX = col2.x(); vdY = col2.y(); vdZ = col2.z(); vdW = 0.0f;
        }

        CylinderDrawParamsData drawParams{};
        drawParams.modelViewMatrix      = Matrix4F(modelViewAff.toDataType<float>());
        drawParams.selectionColor       = prim.selectionColor().toDataType<float>();
        drawParams.uniformWidth         = static_cast<float>(prim.uniformWidth());
        drawParams.pickingBaseObjectId  = dc.objectId;
        if(dc.isPseudoColor) {
            drawParams.colorRangeMin = static_cast<float>(prim.pseudoColorMapping().minValue());
            drawParams.colorRangeMax = static_cast<float>(prim.pseudoColorMapping().maxValue());
        }
        else {
            drawParams.colorRangeMin = drawParams.colorRangeMax = 0.0f;
        }
        drawParams.viewDirEyePosX    = vdX;
        drawParams.viewDirEyePosY    = vdY;
        drawParams.viewDirEyePosZ    = vdZ;
        drawParams.viewDirEyePosW    = vdW;
        drawParams.singleCylinderCap = prim.renderSingleCylinderCap() ? 1 : 0;

        const quint32 offset = _drawParamsAlignedSize * static_cast<quint32>(i);
        batch->updateDynamicBuffer(_drawParamsUBO.get(), offset, sizeof(CylinderDrawParamsData), &drawParams);
    }

    // Upload per-instance data.
    for(auto& dc : _drawCalls) {
        if(isPicking || impl()->orderIndependentTransparency() || !dc.hasTransparency)
            uploadInstanceData(batch, dc, isPicking);
        else
            uploadInstanceDataSorted(batch, dc, isInteractive, projParams);
    }
}

/******************************************************************************
* Uploads per-instance VBO data for a draw call.
******************************************************************************/
void CylinderPrimitiveRenderer::uploadInstanceData(QRhiResourceUpdateBatch* batch, DrawCall& dc, bool isPicking)
{
    const CylinderPrimitive& prim = *dc.primitive;
    const size_t N = dc.cylinderCount;

    // Base and head positions (vec3, always needed).
    dc.bufs.positions = impl()->uploadDataBuffer(prim.vertexPositions(), batch);

    // Width (float per cylinder or uniform).
    if(prim.widths())
        dc.bufs.widths = impl()->uploadDataBuffer(prim.widths(), batch);
    else
        dc.bufs.widths = impl()->uniformValueBuffer(static_cast<float>(prim.uniformWidth()), N, batch);

    if(isPicking) {
        // Picking path: no color/transparency/selection needed.
        dc.bufs.color1 = dc.bufs.color2 = dc.bufs.transp1 = dc.bufs.transp2 = dc.bufs.selection = nullptr;
        return;
    }

    // Colors.
    const size_t colorCount = prim.colors() ? prim.colors()->size() : 0;
    const bool dualColor = (colorCount == 2 * N);

    if(dc.isPseudoColor) {
        // Pseudo-color: upload float scalars (1 component per element).
        if(dualColor) {
            dc.bufs.color1 = extractStridedBuffer(prim.colors(), 0, 2, N, 1, batch);
            dc.bufs.color2 = extractStridedBuffer(prim.colors(), 1, 2, N, 1, batch);
        }
        else if(prim.colors()) {
            dc.bufs.color1 = dc.bufs.color2 = impl()->uploadDataBuffer(prim.colors(), batch);
        }
        else {
            // No color data - fall back to uniform white (renders as mid-range color).
            dc.bufs.color1 = dc.bufs.color2 = impl()->uniformValueBuffer(0.5f, N, batch);
        }
        QRhiSampler* sampler;
        std::tie(dc.colorMapTexture, sampler) = impl()->ensureColorMapTexture(prim.pseudoColorMapping(), batch);
        // Create per-draw bindings including the colorMap texture.
        if(_drawParamsUBO && dc.colorMapTexture && sampler) {
            dc.vboBindings.reset(rhi()->newShaderResourceBindings());
            dc.vboBindings->setBindings({
                QRhiShaderResourceBinding::uniformBuffer(0,
                    QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage,
                    impl()->sceneParamsUBO()),
                QRhiShaderResourceBinding::uniformBufferWithDynamicOffset(1,
                    QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage,
                    _drawParamsUBO.get(), sizeof(CylinderDrawParamsData)),
                QRhiShaderResourceBinding::sampledTexture(2,
                    QRhiShaderResourceBinding::FragmentStage,
                    dc.colorMapTexture, sampler),
            });
            if(!dc.vboBindings->create()) {
                rt()->reportWarning("CylinderPrimitiveRenderer: Failed to create pseudo-color bindings.");
                dc.vboBindings.reset();
            }
        }
    } else {
        // RGB colors.
        if(dualColor) {
            dc.bufs.color1 = extractStridedBuffer(prim.colors(), 0, 2, N, 3, batch);
            dc.bufs.color2 = extractStridedBuffer(prim.colors(), 1, 2, N, 3, batch);
        }
        else if(prim.colors()) {
            dc.bufs.color1 = dc.bufs.color2 = impl()->uploadDataBuffer(prim.colors(), batch);
        }
        else {
            const auto uc = prim.uniformColor().toDataType<float>();
            dc.bufs.color1 = dc.bufs.color2 = impl()->uniformValueBuffer(uc, N, batch);
        }
    }

    // Transparencies.
    const size_t transpCount = prim.transparencies() ? prim.transparencies()->size() : 0;
    const bool dualTransp = (transpCount == 2 * N);
    if(prim.transparencies()) {
        if(dualTransp) {
            dc.bufs.transp1 = extractStridedBuffer(prim.transparencies(), 0, 2, N, 1, batch);
            dc.bufs.transp2 = extractStridedBuffer(prim.transparencies(), 1, 2, N, 1, batch);
        }
        else {
            dc.bufs.transp1 = dc.bufs.transp2 = impl()->uploadDataBuffer(prim.transparencies(), batch);
        }
    }
    else if(rhi()->isFeatureSupported(QRhi::CustomInstanceStepRate)) {
        dc.bufs.transp1 = dc.bufs.transp2 = impl()->uniformValueBuffer(0.0f, 1, batch);
    }
    else {
        dc.bufs.transp1 = dc.bufs.transp2 = impl()->uniformValueBuffer(0.0f, N, batch);
    }

    // Selection flags (int8, 1 per cylinder).
    if(prim.selection())
        dc.bufs.selection = impl()->uploadDataBuffer(prim.selection(), batch);
    else if(rhi()->isFeatureSupported(QRhi::CustomInstanceStepRate))
        dc.bufs.selection = impl()->uniformValueBuffer(int8_t{0}, 1, batch);
    else
        dc.bufs.selection = impl()->uniformValueBuffer(int8_t{0}, N, batch);
}

/******************************************************************************
* Uploads per-instance SSBO data for sorted transparent rendering.
******************************************************************************/
void CylinderPrimitiveRenderer::uploadInstanceDataSorted(QRhiResourceUpdateBatch* batch, DrawCall& dc,
                                                         bool isInteractive, const ViewProjectionParameters& projParams)
{
    const CylinderPrimitive& prim = *dc.primitive;
    const size_t N = dc.cylinderCount;
    const QRhiBuffer::UsageFlags SSBO = QRhiBuffer::StorageBuffer;

    dc.bufs.positions = impl()->uploadDataBuffer(prim.vertexPositions(), batch, SSBO);

    if(prim.widths())
        dc.bufs.widths = impl()->uploadDataBuffer(prim.widths(), batch, SSBO);
    else
        dc.bufs.widths = impl()->uniformValueBuffer(static_cast<float>(prim.uniformWidth()), N, batch, SSBO);

    // Colors as RGB SSBOs (pseudo-color converted on CPU).
    const size_t colorCount = prim.colors() ? prim.colors()->size() : 0;
    const bool dualColor    = (colorCount == 2 * N);

    auto uploadRGBSSBO = [&](size_t elementOffset, size_t elementStride) -> QRhiBuffer* {
        if(dc.isPseudoColor && prim.colors()) {
            const PseudoColorMapping& mapping = prim.pseudoColorMapping();
            return impl()->createCachedBuffer(
                RendererResourceKey<struct CylPseudoRGBSSBOCache,
                    ConstDataBufferPtr, size_t, size_t, PseudoColorMapping>{
                    prim.colors(), elementOffset, elementStride, mapping},
                batch, SSBO, [&]() -> QByteArray {
                    QByteArray result(N * sizeof(ColorF), Qt::Uninitialized);
                    ColorF* dst = reinterpret_cast<ColorF*>(result.data());
                    if(prim.colors()->dataType() == DataBuffer::Float64) {
                        BufferReadAccess<double> access(prim.colors());
                        for(size_t i = 0; i < N; ++i) {
                            double v = access.get(elementOffset + i * elementStride);
                            ColorF c = mapping.valueToColor(v).toDataType<float>();
                            dst[i] = c;
                        }
                    }
                    else {
                        BufferReadAccess<float> access(prim.colors());
                        for(size_t i = 0; i < N; ++i) {
                            float v = access.get(elementOffset + i * elementStride);
                            ColorF c = mapping.valueToColor(v).toDataType<float>();
                            dst[i] = c;
                        }
                    }
                    return result;
                });
        }
        else if(prim.colors()) {
            if(elementStride == 1)
                return impl()->uploadDataBuffer(prim.colors(), batch, SSBO);
            return extractStridedBuffer(prim.colors(), elementOffset, elementStride, N, 3, batch, SSBO);
        }
        else {
            const auto uc = prim.uniformColor().toDataType<float>();
            return impl()->uniformValueBuffer(uc, N, batch, SSBO);
        }
    };

    if(dualColor) {
        dc.bufs.color1 = uploadRGBSSBO(0, 2);
        dc.bufs.color2 = uploadRGBSSBO(1, 2);
    }
    else {
        dc.bufs.color1 = dc.bufs.color2 = uploadRGBSSBO(0, 1);
    }

    // Transparencies as SSBOs.
    const size_t transpCount = prim.transparencies() ? prim.transparencies()->size() : 0;
    const bool dualTransp = (transpCount == 2 * N);
    if(prim.transparencies()) {
        if(dualTransp) {
            dc.bufs.transp1 = extractStridedBuffer(prim.transparencies(), 0, 2, N, 1, batch, SSBO);
            dc.bufs.transp2 = extractStridedBuffer(prim.transparencies(), 1, 2, N, 1, batch, SSBO);
        }
        else {
            dc.bufs.transp1 = dc.bufs.transp2 = impl()->uploadDataBuffer(prim.transparencies(), batch, SSBO);
        }
    }
    else {
        dc.bufs.transp1 = dc.bufs.transp2 = impl()->uniformValueBuffer(0.0f, N, batch, SSBO);
    }

    // Selection as bit-packed SSBO.
    const size_t wordCount = (N + 31) / 32;
    if(prim.selection()) {
        dc.bufs.selection = impl()->createCachedBuffer(
            RendererResourceKey<struct CylSelSSBOCache, ConstDataBufferPtr>{prim.selection()},
            batch, SSBO, [&]() -> QByteArray {
                BufferReadAccess<int8_t> access(prim.selection());
                QByteArray data(static_cast<qsizetype>(wordCount * sizeof(uint32_t)), 0);
                uint32_t* words = reinterpret_cast<uint32_t*>(data.data());
                const int8_t* sel = access.cbegin();
                for(size_t k = 0; k < N; ++k)
                    if(sel[k]) words[k >> 5] |= (1u << (k & 31));
                return data;
            });
    }
    else {
        dc.bufs.selection = impl()->uniformValueBuffer(uint32_t{0}, wordCount, batch, SSBO);
    }

    // Sort cylinders by midpoint distance from viewer (back-to-front).
    const Matrix3F modelViewLinear = (projParams.viewMatrix * dc.modelWorldTM).linear().toDataType<float>();
    const Vector3F viewDirInModel  = modelViewLinear.inverse().column(2);
    const Vector3F coarseDir       = coarsenViewDir(viewDirInModel, isInteractive);

    dc.sorted.sortedIndexBuffer = impl()->createCachedBuffer(
        RendererResourceKey<struct CylSortCache,
            ConstDataBufferPtr, Vector3F>{
            prim.vertexPositions(), coarseDir},
        batch, SSBO, [&]() -> QByteArray {
            std::vector<float> distances(N);
            if(prim.vertexPositions()->dataType() == DataBuffer::Float64) {
                const auto dir = viewDirInModel.toDataType<double>();
                BufferReadAccess<Point3D> vertexAcc(prim.vertexPositions());
                for(size_t k = 0; k < N; ++k) {
                    auto mid = vertexAcc[2 * k].midpoint(vertexAcc[2 * k + 1]);
                    distances[k] = static_cast<float>(dir.dot(mid - Point3D::Origin()));
                }
            }
            else {
                BufferReadAccess<Point3F> vertexAcc(prim.vertexPositions());
                for(size_t k = 0; k < N; ++k) {
                    auto mid = vertexAcc[2 * k].midpoint(vertexAcc[2 * k + 1]);
                    distances[k] = viewDirInModel.dot(mid - Point3F::Origin());
                }
            }

            QByteArray bufferData(sizeof(uint32_t) * N, Qt::Uninitialized);
            std::span<uint32_t> indices(reinterpret_cast<uint32_t*>(bufferData.data()), N);
            std::iota(indices.begin(), indices.end(), 0u);
            Ovito::sort_zipped(distances, indices);
            return bufferData;
        });

    // Build sorted SSBO shader resource bindings.
    dc.sorted.shaderResourceBindings.reset();
    const bool ready = impl()->sceneParamsUBO() && _drawParamsUBO
        && dc.bufs.positions && dc.bufs.widths
        && dc.bufs.color1 && dc.bufs.color2
        && dc.bufs.transp1 && dc.bufs.transp2
        && dc.bufs.selection && dc.sorted.sortedIndexBuffer;
    if(ready) {
        dc.sorted.shaderResourceBindings.reset(rhi()->newShaderResourceBindings());
        dc.sorted.shaderResourceBindings->setBindings({
            QRhiShaderResourceBinding::uniformBuffer(0,
                QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage,
                impl()->sceneParamsUBO()),
            QRhiShaderResourceBinding::uniformBufferWithDynamicOffset(1,
                QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage,
                _drawParamsUBO.get(), sizeof(CylinderDrawParamsData)),
            QRhiShaderResourceBinding::bufferLoad(3, QRhiShaderResourceBinding::VertexStage, dc.bufs.positions),
            QRhiShaderResourceBinding::bufferLoad(4, QRhiShaderResourceBinding::VertexStage, dc.bufs.widths),
            QRhiShaderResourceBinding::bufferLoad(5, QRhiShaderResourceBinding::VertexStage, dc.bufs.color1),
            QRhiShaderResourceBinding::bufferLoad(6, QRhiShaderResourceBinding::VertexStage, dc.bufs.color2),
            QRhiShaderResourceBinding::bufferLoad(7, QRhiShaderResourceBinding::VertexStage, dc.bufs.transp1),
            QRhiShaderResourceBinding::bufferLoad(8, QRhiShaderResourceBinding::VertexStage, dc.bufs.transp2),
            QRhiShaderResourceBinding::bufferLoad(9, QRhiShaderResourceBinding::VertexStage, dc.bufs.selection),
            QRhiShaderResourceBinding::bufferLoad(10, QRhiShaderResourceBinding::VertexStage, dc.sorted.sortedIndexBuffer),
        });
        if(!dc.sorted.shaderResourceBindings->create()) {
            rt()->reportWarning("CylinderPrimitiveRenderer: Failed to create sorted bindings.");
            dc.sorted.shaderResourceBindings.reset();
        }
    }
}

/******************************************************************************
* Returns true if any draw call has transparency.
******************************************************************************/
bool CylinderPrimitiveRenderer::hasTransparentDrawCalls() const
{
    return std::ranges::any_of(_drawCalls, [](const DrawCall& dc) {
        return dc.hasTransparency && dc.layer != FrameGraph::OverLayer;
    });
}

/******************************************************************************
* Draws transparent cylinders into the OIT accumulation pass.
******************************************************************************/
void CylinderPrimitiveRenderer::drawOITAccumPass(QRhiCommandBuffer* cb, QRhiRenderPassDescriptor* rpd)
{
    draw(cb, rpd, true, false, false, false, false, true);
    draw(cb, rpd, false, true, false, true, false);
}

/******************************************************************************
* Draws transparent cylinders into the OIT reveal pass.
******************************************************************************/
void CylinderPrimitiveRenderer::drawOITRevealPass(QRhiCommandBuffer* cb, QRhiRenderPassDescriptor* rpd)
{
    draw(cb, rpd, false, true, false, false, true);
}

/******************************************************************************
* Phase 3: Issues draw calls inside the render pass.
******************************************************************************/
void CylinderPrimitiveRenderer::draw(QRhiCommandBuffer* cb, QRhiRenderPassDescriptor* rpd,
                                      bool opaqueOnly, bool transparentOnly, bool isPicking)
{
    draw(cb, rpd, opaqueOnly, transparentOnly, isPicking, false, false);
}

/******************************************************************************
* Draws all OverLayer cylinders with depth testing disabled.
******************************************************************************/
void CylinderPrimitiveRenderer::drawOverlay(QRhiCommandBuffer* cb, QRhiRenderPassDescriptor* rpd)
{
    draw(cb, rpd, /*opaqueOnly=*/false, /*transparentOnly=*/false, /*isPicking=*/false,
         /*oitAccum=*/false, /*oitReveal=*/false, /*oitDepthPrime=*/false, /*overlayOnly=*/true);
}

/******************************************************************************
* Draws all HighlightLayer cylinders depth-test-disabled into the silhouette mask target.
******************************************************************************/
void CylinderPrimitiveRenderer::drawHighlightSilhouette(QRhiCommandBuffer* cb, QRhiRenderPassDescriptor* rpd)
{
    draw(cb, rpd, /*opaqueOnly=*/false, /*transparentOnly=*/false, /*isPicking=*/false,
         /*oitAccum=*/false, /*oitReveal=*/false, /*oitDepthPrime=*/false,
         /*overlayOnly=*/false, /*highlightSilhouetteOnly=*/true);
}

/******************************************************************************
* Draws ExcludeFromOutline SceneLayer cylinders into the depth-only excluded-depth pre-pass target.
******************************************************************************/
void CylinderPrimitiveRenderer::drawExclusionDepth(QRhiCommandBuffer* cb, QRhiRenderPassDescriptor* rpd)
{
    draw(cb, rpd, /*opaqueOnly=*/false, /*transparentOnly=*/false, /*isPicking=*/false,
         /*oitAccum=*/false, /*oitReveal=*/false, /*oitDepthPrime=*/false,
         /*overlayOnly=*/false, /*highlightSilhouetteOnly=*/false, /*excludedDepthOnly=*/true);
}

/******************************************************************************
* Phase 3: Internal draw overload (handles all rendering modes).
******************************************************************************/
void CylinderPrimitiveRenderer::draw(QRhiCommandBuffer* cb, QRhiRenderPassDescriptor* rpd,
                                     bool opaqueOnly, bool transparentOnly, bool isPicking,
                                     bool oitAccum, bool oitReveal, bool oitDepthPrime,
                                     bool overlayOnly, bool highlightSilhouetteOnly,
                                     bool excludedDepthOnly)
{
    if(!impl()->sceneParamsUBO() || !_drawParamsUBO)
        return;

    for(const auto& [i, dc] : Ovito::enumerate(_drawCalls)) {
        if(opaqueOnly && dc.hasTransparency) continue;
        if(transparentOnly && !dc.hasTransparency) continue;

        // Route draw calls to the correct rendering path.
        if(excludedDepthOnly) {
            // Render ONLY ExcludeFromOutline geometry; raytraced excluded objects (filteredOut)
            // are handled by the raytracer's own excluded-depth contribution.
            if(dc.layer != FrameGraph::SceneLayer || !dc.excludeFromOutline || dc.filteredOut) continue;
        } else if(highlightSilhouetteOnly) {
            if(dc.layer != FrameGraph::HighlightLayer) continue;
        } else {
            if(overlayOnly != (dc.layer == FrameGraph::OverLayer)) continue;
            if(dc.filteredOut) continue;
        }

        // Check required buffers.
        if(!dc.bufs.positions || !dc.bufs.widths)
            continue;

        // Decide whether to use sorted SSBO.
        // Overlay and highlight-silhouette draw calls never use sorted SSBO (depth is off).
        // Depth-only pass DOES use sorted SSBO when available: the color/transp/selection
        // buffers are SSBOs and cannot be bound as vertex buffers.
        const bool useSortedSSBO = !overlayOnly && !highlightSilhouetteOnly && dc.hasTransparency && !impl()->orderIndependentTransparency()
            && !oitAccum && !oitReveal && !isPicking && dc.sorted.shaderResourceBindings;

        ShaderVariant variant = dc.shader;
        if(useSortedSSBO)
            variant = toSortedVariant(dc.shader);

        PipelineFlags flags;
        if(excludedDepthOnly) {
            // Outline depth pre-pass: depth test + write, depth-only target (no color attachments).
            flags.setFlag(PipelineFlag::DepthTest,  true);
            flags.setFlag(PipelineFlag::DepthWrite, true);
            flags.setFlag(PipelineFlag::DepthOnly,  true);
            // Keep SortedSSBO set so ensurePipeline() picks the ssbo_sorted vertex shader
            // and setVertexInput() is called with no buffers (avoids VertexBuffer assertion).
            if(useSortedSSBO)
                flags.setFlag(PipelineFlag::SortedSSBO, true);
        } else if(overlayOnly || highlightSilhouetteOnly) {
            // Overlay / highlight silhouette: depth testing disabled so geometry is always visible.
            flags.setFlag(PipelineFlag::DepthTest,  false);
            flags.setFlag(PipelineFlag::DepthWrite, false);
            if(dc.hasTransparency)
                flags.setFlag(PipelineFlag::Blend, true);
        }
        else {
            flags.setFlag(PipelineFlag::DepthTest, true);
            if(dc.hasTransparency) {
                if(oitAccum) {
                    flags.setFlag(PipelineFlag::OITAccum, true);
                    flags.setFlag(PipelineFlag::DepthWrite, false);
                }
                else if(oitReveal) {
                    flags.setFlag(PipelineFlag::OITReveal, true);
                    flags.setFlag(PipelineFlag::DepthWrite, false);
                }
                else if(useSortedSSBO) {
                    flags.setFlag(PipelineFlag::Blend, true);
                    flags.setFlag(PipelineFlag::DepthWrite, true);
                    flags.setFlag(PipelineFlag::SortedSSBO, true);
                }
                else {
                    flags.setFlag(PipelineFlag::Blend, true);
                    flags.setFlag(PipelineFlag::DepthWrite, false);
                }
            }
            else {
                flags.setFlag(PipelineFlag::DepthWrite, true);
            }
        }
        flags.setFlag(PipelineFlag::NoTransparency, !dc.primitive->transparencies() || isPicking || excludedDepthOnly);
        flags.setFlag(PipelineFlag::NoSelection,    !dc.primitive->selection()      || isPicking || excludedDepthOnly);
        flags.setFlag(PipelineFlag::OITDepthPrime,  oitDepthPrime);
        flags.setFlag(PipelineFlag::PseudoColor,    !overlayOnly && !excludedDepthOnly && dc.isPseudoColor && !useSortedSSBO && !isPicking);

        QRhiGraphicsPipeline* pipeline = ensurePipeline(rpd, variant, flags, dc);
        if(!pipeline) continue;

        QRhiCommandBuffer::DynamicOffset dynOffset(1, _drawParamsAlignedSize * static_cast<quint32>(i));

        if(useSortedSSBO) {
            cb->setGraphicsPipeline(pipeline);
            cb->setShaderResources(dc.sorted.shaderResourceBindings.get(), 1, &dynOffset);
            cb->setVertexInput(0, 0, nullptr);
            cb->draw(dc.verticesPerInstance, static_cast<quint32>(dc.cylinderCount), 0, 0);
        }
        else {
            // VBO path.
            QRhiShaderResourceBindings* bindings;
            if(dc.isPseudoColor && dc.vboBindings)
                bindings = dc.vboBindings.get();
            else
                bindings = ensureShaderResourceBindings();
            if(!bindings) continue;

            cb->setGraphicsPipeline(pipeline);
            cb->setShaderResources(bindings, 1, &dynOffset);

            if(isPicking) {
                const QRhiCommandBuffer::VertexInput vbufBindings[] = {
                    { dc.bufs.positions, 0 },
                    { dc.bufs.widths,    0 },
                };
                cb->setVertexInput(0, std::size(vbufBindings), vbufBindings);
            }
            else {
                const QRhiCommandBuffer::VertexInput vbufBindings[] = {
                    { dc.bufs.positions,     0 },
                    { dc.bufs.widths,        0 },
                    { dc.bufs.color1,        0 },
                    { dc.bufs.color2,        0 },
                    { dc.bufs.transp1,       0 },
                    { dc.bufs.transp2,       0 },
                    { dc.bufs.selection,     0 },
                };
                cb->setVertexInput(0, std::size(vbufBindings), vbufBindings);
            }
            cb->draw(dc.verticesPerInstance, static_cast<quint32>(dc.cylinderCount), 0, 0);
        }
    }
}

/******************************************************************************
* Clears the draw call list.
******************************************************************************/
void CylinderPrimitiveRenderer::clear()
{
    _drawCalls.clear();
}

/******************************************************************************
* Returns the shared VBO shader resource bindings (UBOs only).
******************************************************************************/
QRhiShaderResourceBindings* CylinderPrimitiveRenderer::ensureShaderResourceBindings()
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
                _drawParamsUBO.get(), sizeof(CylinderDrawParamsData)),
        });
        if(!_bindings->create()) {
            rt()->reportWarning("CylinderPrimitiveRenderer: Failed to create shader resource bindings.");
            _bindings.reset();
        }
    }
    return _bindings.get();
}

/******************************************************************************
* Ensures a graphics pipeline for the given variant and flags.
******************************************************************************/
QRhiGraphicsPipeline* CylinderPrimitiveRenderer::ensurePipeline(
    QRhiRenderPassDescriptor* rpd, ShaderVariant variant, PipelineFlags flags, const DrawCall& dc)
{
    struct PipelineCacheKey {
        ShaderVariant variant;
        PipelineFlags flags;
        bool operator==(const PipelineCacheKey&) const = default;
    };

    return rt()->ensureGraphicsPipeline(rpd, PipelineCacheKey{variant, flags}, [&]() -> std::unique_ptr<QRhiGraphicsPipeline> {
        if(!impl()->sceneParamsUBO() || !_drawParamsUBO)
            return {};

        const bool isPicking    = flags.testFlag(PipelineFlag::NoTransparency)
                                  && variant == ShaderVariant::CylinderRaycastPicking
                                  || variant == ShaderVariant::CylinderFlatPicking
                                  || variant == ShaderVariant::ArrowHeadRaycastPicking
                                  || variant == ShaderVariant::ArrowTailRaycastPicking
                                  || variant == ShaderVariant::ArrowFlatPicking;
        const bool isSortedSSBO = flags.testFlag(PipelineFlag::SortedSSBO);
        const bool isPseudo     = flags.testFlag(PipelineFlag::PseudoColor);
        const bool isOITAccum   = flags.testFlag(PipelineFlag::OITAccum);
        const bool isOITReveal  = flags.testFlag(PipelineFlag::OITReveal);

        static const QLatin1String BASE(":/ovito/core/rendering/standard/shaders/cylinders/variants/");

        // For pseudo-color VBO variants, use dedicated fragment shaders that include the
        // colorMap sampler (binding 2) and apply the transfer function per-fragment.
        // Depth-only pass uses a no-color-output variant to avoid D3D12 warning #679.
        const bool isDepthOnly = flags.testFlag(PipelineFlag::DepthOnly);
        auto selectFrag = [&](QLatin1String stem) -> QString {
            QString s = QString(BASE) + stem;
            if(!isDepthOnly && isPseudo && !isSortedSSBO) s += QLatin1String("__pseudo");
            if(isDepthOnly) return s + QLatin1String("__depth_only.frag.qsb");
            if(isOITAccum)  return s + QLatin1String("__oit_accum.frag.qsb");
            if(isOITReveal) return s + QLatin1String("__oit_reveal.frag.qsb");
            return s + QLatin1String("__color.frag.qsb");
        };

        QString vsPath, fsPath;

        // Detect picking based on variant directly (more reliable).
        bool variantIsPicking = (variant == ShaderVariant::CylinderRaycastPicking
                              || variant == ShaderVariant::CylinderFlatPicking
                              || variant == ShaderVariant::ArrowHeadRaycastPicking
                              || variant == ShaderVariant::ArrowTailRaycastPicking
                              || variant == ShaderVariant::ArrowFlatPicking);

        QRhiGraphicsPipeline::Topology topology = QRhiGraphicsPipeline::TriangleStrip;

        switch(variant) {
            case ShaderVariant::CylinderRaycast:
            case ShaderVariant::CylinderRaycastSorted:
                vsPath = BASE + (isSortedSSBO ? QLatin1String("cylinder_raycast__ssbo_sorted.vert.qsb")
                               : isPseudo     ? QLatin1String("cylinder_raycast__vbo_visual_pseudo.vert.qsb")
                                              : QLatin1String("cylinder_raycast__vbo_visual_rgb.vert.qsb"));
                fsPath = selectFrag(QLatin1String("cylinder_raycast"));
                break;
            case ShaderVariant::CylinderRaycastPicking:
                vsPath = BASE + QLatin1String("cylinder_raycast__vbo_picking.vert.qsb");
                fsPath = BASE + QLatin1String("cylinder_raycast__picking.frag.qsb");
                break;
            case ShaderVariant::CylinderFlat:
            case ShaderVariant::CylinderFlatSorted:
                vsPath = BASE + (isSortedSSBO ? QLatin1String("cylinder_flat__ssbo_sorted.vert.qsb")
                               : isPseudo     ? QLatin1String("cylinder_flat__vbo_visual_pseudo.vert.qsb")
                                              : QLatin1String("cylinder_flat__vbo_visual_rgb.vert.qsb"));
                fsPath = selectFrag(QLatin1String("cylinder_flat"));
                break;
            case ShaderVariant::CylinderFlatPicking:
                vsPath = BASE + QLatin1String("cylinder_flat__vbo_picking.vert.qsb");
                fsPath = BASE + QLatin1String("cylinder_flat__picking.frag.qsb");
                break;
            case ShaderVariant::ArrowHeadRaycast:
            case ShaderVariant::ArrowHeadRaycastSorted:
                vsPath = BASE + (isSortedSSBO ? QLatin1String("arrow_head_raycast__ssbo_sorted.vert.qsb")
                                              : QLatin1String("arrow_head_raycast__vbo_visual.vert.qsb"));
                fsPath = selectFrag(QLatin1String("arrow_head_raycast"));
                break;
            case ShaderVariant::ArrowHeadRaycastPicking:
                vsPath = BASE + QLatin1String("arrow_head_raycast__vbo_picking.vert.qsb");
                fsPath = BASE + QLatin1String("arrow_head_raycast__picking.frag.qsb");
                break;
            case ShaderVariant::ArrowTailRaycast:
            case ShaderVariant::ArrowTailRaycastSorted:
                vsPath = BASE + (isSortedSSBO ? QLatin1String("arrow_tail_raycast__ssbo_sorted.vert.qsb")
                                              : QLatin1String("arrow_tail_raycast__vbo_visual.vert.qsb"));
                fsPath = selectFrag(QLatin1String("arrow_tail_raycast"));
                break;
            case ShaderVariant::ArrowTailRaycastPicking:
                vsPath = BASE + QLatin1String("arrow_tail_raycast__vbo_picking.vert.qsb");
                fsPath = BASE + QLatin1String("arrow_tail_raycast__picking.frag.qsb");
                break;
            case ShaderVariant::ArrowFlat:
            case ShaderVariant::ArrowFlatSorted:
                vsPath = BASE + (isSortedSSBO ? QLatin1String("arrow_flat__ssbo_sorted.vert.qsb")
                                              : QLatin1String("arrow_flat__vbo_visual.vert.qsb"));
                fsPath = selectFrag(QLatin1String("arrow_flat"));
                topology = QRhiGraphicsPipeline::Triangles;
                break;
            case ShaderVariant::ArrowFlatPicking:
                vsPath = BASE + QLatin1String("arrow_flat__vbo_picking.vert.qsb");
                fsPath = BASE + QLatin1String("arrow_flat__picking.frag.qsb");
                topology = QRhiGraphicsPipeline::Triangles;
                break;
            default:
                return {};
        }

        QShader vs = rt()->loadShader(vsPath);
        QShader fs = rt()->loadShader(fsPath);
        if(!vs.isValid() || !fs.isValid())
            return {};

        // Build vertex input layout (empty for sorted SSBO path).
        QRhiVertexInputLayout inputLayout;
        if(!isSortedSSBO) {
            QVector<QRhiVertexInputBinding> inputBindings;
            QVector<QRhiVertexInputAttribute> attributes;

            // Common: base (vec3), head (vec3), width (float).
            inputBindings.append(QRhiVertexInputBinding(6 * sizeof(float), QRhiVertexInputBinding::PerInstance));
            attributes.append(QRhiVertexInputAttribute(0, 0, QRhiVertexInputAttribute::Float3, 0));
            attributes.append(QRhiVertexInputAttribute(0, 1, QRhiVertexInputAttribute::Float3, 3 * sizeof(float)));
            inputBindings.append(QRhiVertexInputBinding(sizeof(float), QRhiVertexInputBinding::PerInstance));
            attributes.append(QRhiVertexInputAttribute(1, 2, QRhiVertexInputAttribute::Float, 0));

            if(!variantIsPicking) {
                // color1/color2: vec3 (RGB) or float (pseudo-color scalar).
                const auto colorFormat  = isPseudo ? QRhiVertexInputAttribute::Float  : QRhiVertexInputAttribute::Float3;
                const quint32 colorSize = isPseudo ? sizeof(float) : 3 * sizeof(float);
                inputBindings.append(QRhiVertexInputBinding(colorSize, QRhiVertexInputBinding::PerInstance));
                attributes.append(QRhiVertexInputAttribute(2, 3, colorFormat, 0));
                inputBindings.append(QRhiVertexInputBinding(colorSize, QRhiVertexInputBinding::PerInstance));
                attributes.append(QRhiVertexInputAttribute(3, 4, colorFormat, 0));

                // transp1, transp2: float.
                const quint32 transpRate = (flags.testFlag(PipelineFlag::NoTransparency) && rhi()->isFeatureSupported(QRhi::CustomInstanceStepRate))
                                           ? std::numeric_limits<quint32>::max() : 1;
                inputBindings.append(QRhiVertexInputBinding(sizeof(float), QRhiVertexInputBinding::PerInstance, transpRate));
                attributes.append(QRhiVertexInputAttribute(4, 5, QRhiVertexInputAttribute::Float, 0));
                inputBindings.append(QRhiVertexInputBinding(sizeof(float), QRhiVertexInputBinding::PerInstance, transpRate));
                attributes.append(QRhiVertexInputAttribute(5, 6, QRhiVertexInputAttribute::Float, 0));

                // selection: int8 (UNormByte).
                const quint32 selRate = (flags.testFlag(PipelineFlag::NoSelection) && rhi()->isFeatureSupported(QRhi::CustomInstanceStepRate))
                                        ? std::numeric_limits<quint32>::max() : 1;
                inputBindings.append(QRhiVertexInputBinding(sizeof(int8_t), QRhiVertexInputBinding::PerInstance, selRate));
                attributes.append(QRhiVertexInputAttribute(6, 7, QRhiVertexInputAttribute::UNormByte, 0));
            }

            inputLayout.setBindings(inputBindings.cbegin(), inputBindings.cend());
            inputLayout.setAttributes(attributes.cbegin(), attributes.cend());
        }

        std::unique_ptr<QRhiGraphicsPipeline> pipeline(rhi()->newGraphicsPipeline());
        pipeline->setShaderStages({
            { QRhiShaderStage::Vertex,   vs },
            { QRhiShaderStage::Fragment, fs },
        });
        pipeline->setVertexInputLayout(inputLayout);
        pipeline->setTopology(topology);
        pipeline->setDepthTest(flags.testFlag(PipelineFlag::DepthTest));
        pipeline->setDepthWrite(flags.testFlag(PipelineFlag::DepthWrite));
        pipeline->setCullMode(QRhiGraphicsPipeline::Back);

        if(flags.testFlag(PipelineFlag::Blend)) {
            QRhiGraphicsPipeline::TargetBlend tb;
            tb.enable   = true;
            tb.srcColor = QRhiGraphicsPipeline::SrcAlpha;
            tb.dstColor = QRhiGraphicsPipeline::OneMinusSrcAlpha;
            tb.srcAlpha = QRhiGraphicsPipeline::One;
            tb.dstAlpha = QRhiGraphicsPipeline::OneMinusSrcAlpha;
            pipeline->setTargetBlends({ tb });
        }
        else if(isOITAccum) {
            QRhiGraphicsPipeline::TargetBlend tb;
            tb.enable   = true;
            tb.srcColor = QRhiGraphicsPipeline::One;
            tb.dstColor = QRhiGraphicsPipeline::One;
            tb.srcAlpha = QRhiGraphicsPipeline::One;
            tb.dstAlpha = QRhiGraphicsPipeline::One;
            pipeline->setTargetBlends({ tb });
        }
        else if(isOITReveal) {
            QRhiGraphicsPipeline::TargetBlend tb;
            tb.enable   = true;
            tb.srcColor = QRhiGraphicsPipeline::Zero;
            tb.dstColor = QRhiGraphicsPipeline::SrcColor;
            tb.srcAlpha = QRhiGraphicsPipeline::Zero;
            tb.dstAlpha = QRhiGraphicsPipeline::SrcAlpha;
            pipeline->setTargetBlends({ tb });
        }
        else if(flags.testFlag(PipelineFlag::OITDepthPrime)) {
            QRhiGraphicsPipeline::TargetBlend tb;
            tb.enable = false;
            tb.colorWrite = QRhiGraphicsPipeline::ColorMask(0);
            pipeline->setTargetBlends({ tb });
        }
        else if(flags.testFlag(PipelineFlag::DepthOnly)) {
            // Outline depth pre-pass: depth-only render target, no color attachments.
            pipeline->setTargetBlends({});
        }
        else if(variantIsPicking) {
            QRhiGraphicsPipeline::TargetBlend tb;
            tb.enable = false;
            pipeline->setTargetBlends({ tb, tb });
        }

        // Shader resource bindings for pipeline creation (defines descriptor layout).
        QRhiShaderResourceBindings* bindingsForPipeline;
        if(isSortedSSBO)
            bindingsForPipeline = dc.sorted.shaderResourceBindings.get();
        else if(isPseudo && dc.vboBindings)
            bindingsForPipeline = dc.vboBindings.get();
        else
            bindingsForPipeline = ensureShaderResourceBindings();

        if(!bindingsForPipeline)
            return {};

        pipeline->setShaderResourceBindings(bindingsForPipeline);
        pipeline->setRenderPassDescriptor(rpd);

        if(!pipeline->create()) {
            rt()->reportWarning("CylinderPrimitiveRenderer: Failed to create pipeline.");
            return {};
        }
        return pipeline;
    });
}

/******************************************************************************
* Extracts every elementStride-th element starting at elementOffset.
******************************************************************************/
QRhiBuffer* CylinderPrimitiveRenderer::extractStridedBuffer(
    const ConstDataBufferPtr& buffer,
    size_t elementOffset, size_t elementStride,
    size_t count, size_t componentCount,
    QRhiResourceUpdateBatch* batch, QRhiBuffer::UsageFlags usage)
{
    return impl()->createCachedBuffer(
        RendererResourceKey<struct CylStridedExtractCache,
            ConstDataBufferPtr, size_t, size_t, size_t>{buffer, elementOffset, elementStride, componentCount},
        batch, usage, [&]() -> QByteArray {
            QByteArray result(static_cast<qsizetype>(count * componentCount * sizeof(float)), Qt::Uninitialized);
            float* dst = reinterpret_cast<float*>(result.data());
            if(buffer->dataType() == DataBuffer::Float64) {
                RawBufferReadAccess access(buffer);
                const double* src = reinterpret_cast<const double*>(access.cdata());
                for(size_t i = 0; i < count; ++i) {
                    for(size_t c = 0; c < componentCount; ++c)
                        dst[i * componentCount + c] = static_cast<float>(
                            src[(elementOffset + i * elementStride) * componentCount + c]);
                }
            }
            else {
                RawBufferReadAccess access(buffer);
                const float* src = reinterpret_cast<const float*>(access.cdata());
                for(size_t i = 0; i < count; ++i) {
                    for(size_t c = 0; c < componentCount; ++c)
                        dst[i * componentCount + c] = src[(elementOffset + i * elementStride) * componentCount + c];
                }
            }
            return result;
        });
}

}   // End of namespace
