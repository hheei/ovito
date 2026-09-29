// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/core/Core.h>
#include <ovito/core/dataset/data/BufferAccess.h>
#include <ovito/core/utilities/SortZipped.h>
#include <ovito/core/rendering/RendererService.h>
#include "MeshPrimitiveRenderer.h"
#include "StandardRendererImplementation.h"

namespace Ovito {

/// Must match the std140 'MeshDrawParams' UBO layout in mesh_draw_params.glsl.
struct MeshDrawParamsData {
    Matrix4F modelViewMatrix;       // offset   0, 64 bytes
    ColorAF  selectionColor;        // offset  64, 16 bytes
    ColorAF  wireframeColor;        // offset  80, 16 bytes
    float    opacity;               // offset  96
    uint32_t pickingBaseObjectId;   // offset 100
    float    colorRangeMin;         // offset 104
    float    colorRangeMax;         // offset 108
    float    lineThickness;         // offset 112  (= wireframeWidth / viewportHeight)
    float    _pad0;                 // offset 116
    float    _pad1;                 // offset 120
    float    _pad2;                 // offset 124
};  // 128 bytes
static_assert(sizeof(MeshDrawParamsData) == 128);

/******************************************************************************
* Returns the OIT accumulation variant for a given base variant.
******************************************************************************/
MeshPrimitiveRenderer::ShaderVariant MeshPrimitiveRenderer::toOITAccumVariant(ShaderVariant v)
{
    switch(v) {
        case ShaderVariant::Mesh:                 return ShaderVariant::MeshOITAccum;
        case ShaderVariant::MeshInstanced:        return ShaderVariant::MeshInstancedOITAccum;
        case ShaderVariant::MeshInstancedColors:  return ShaderVariant::MeshInstancedColorsOITAccum;
        default:                                  return v;
    }
}

/******************************************************************************
* Returns the OIT reveal variant for a given base variant.
******************************************************************************/
MeshPrimitiveRenderer::ShaderVariant MeshPrimitiveRenderer::toOITRevealVariant(ShaderVariant v)
{
    switch(v) {
        case ShaderVariant::Mesh:                 return ShaderVariant::MeshOITReveal;
        case ShaderVariant::MeshInstanced:        return ShaderVariant::MeshInstancedOITReveal;
        case ShaderVariant::MeshInstancedColors:  return ShaderVariant::MeshInstancedColorsOITReveal;
        default:                                  return v;
    }
}

/******************************************************************************
* Clears the draw call list.
******************************************************************************/
void MeshPrimitiveRenderer::clear()
{
    _drawCalls.clear();
}

/******************************************************************************
* Phase 1: Build draw calls from mesh primitives.
******************************************************************************/
void MeshPrimitiveRenderer::buildDrawCalls(const MeshPrimitive& primitive,
                                           const FrameGraph::RenderingCommand& command,
                                           bool isPickingPass, ObjectPickingMap* pickingMap,
                                           bool renderOnlyWireframe, bool filteredOut)
{
    if(!primitive.mesh() || primitive.mesh()->faceCount() == 0)
        return;
    if(primitive.useInstancedRendering() && primitive.perInstanceTMs()->size() == 0)
        return;

    const bool isInstanced = primitive.useInstancedRendering();
    const bool hasInstColors = isInstanced && (bool)primitive.perInstanceColors();

    // Pseudo-color only for non-instanced, non-picking.
    const bool isPseudoColor = !isPickingPass && !isInstanced
        && primitive.pseudoColorMapping().isValid()
        && !primitive.mesh()->hasVertexColors()
        && !primitive.mesh()->hasFaceColors()
        && (primitive.mesh()->hasVertexPseudoColors() || primitive.mesh()->hasFacePseudoColors());

    // Select face shader variant.
    ShaderVariant faceShader;
    if(renderOnlyWireframe) {
        OVITO_ASSERT(primitive.emphasizeEdges() && !isPickingPass);
        faceShader = ShaderVariant::None; // Don't render faces at all, only wireframe lines.
    }
    else if(isPickingPass) {
        faceShader = isInstanced ? ShaderVariant::MeshInstancedPicking : ShaderVariant::MeshPicking;
    }
    else if(isPseudoColor) {
        faceShader = ShaderVariant::MeshColorMapping;
    }
    else if(isInstanced) {
        faceShader = hasInstColors ? ShaderVariant::MeshInstancedColors : ShaderVariant::MeshInstanced;
    }
    else {
        faceShader = ShaderVariant::Mesh;
    }

    // Select wireframe shader variant.
    const bool needsWireframe = primitive.emphasizeEdges() && !isPickingPass;
    const bool useThickWireframe = (primitive.wireframeWidth() != FloatType(0)) || renderOnlyWireframe;
    ShaderVariant wireframeShader = ShaderVariant::Wireframe;
    if(needsWireframe) {
        if(isInstanced)
            wireframeShader = useThickWireframe ? ShaderVariant::WireframeTriInstanced : ShaderVariant::WireframeInstanced;
        else
            wireframeShader = useThickWireframe ? ShaderVariant::WireframeTri : ShaderVariant::Wireframe;
    }

    // Allocate picking object ID.
    uint32_t objectId = 0;
    if(isPickingPass && pickingMap) {
        size_t idCount = isInstanced ? primitive.perInstanceTMs()->size() : (size_t)primitive.mesh()->faceCount();
        objectId = pickingMap->registerObjectId(service()->objectIdAllocator().allocate(idCount), command);
    }

    DrawCall& dc = _drawCalls.emplace_back();
    dc.primitive          = &primitive;
    dc.modelWorldTM       = command.modelWorldTM();
    dc.faceShader         = faceShader;
    dc.wireframeShader    = wireframeShader;
    dc.hasTransparency    = !isPickingPass && !primitive.isFullyOpaque();
    dc.isPseudoColor      = isPseudoColor;
    dc.needsWireframe     = needsWireframe;
    dc.isInstanced        = isInstanced;
    dc.hasInstanceColors  = hasInstColors;
    dc.excludeFromOutline = command.excludeFromOutline();
    dc.filteredOut        = filteredOut;
    dc.objectId           = objectId;
    dc.faceCount          = (size_t)primitive.mesh()->faceCount();
    dc.instanceCount      = isInstanced ? primitive.perInstanceTMs()->size() : 1;
    dc.wireframeEdgeCount = 0;  // computed in Phase 2
}

/******************************************************************************
* Phase 2: Upload vertex buffers and UBOs.
******************************************************************************/
void MeshPrimitiveRenderer::prepareResourceUpdates(QRhiResourceUpdateBatch* batch,
                                                   const ViewProjectionParameters& projParams,
                                                   QSize renderSize,
                                                   bool isYUpInNDC, bool isYUpInFramebuffer,
                                                   bool isPicking, bool isInteractive)
{
    if(_drawCalls.empty())
        return;

    _drawParamsAlignedSize = rhi()->ubufAligned(sizeof(MeshDrawParamsData));
    const quint32 requiredSize = _drawParamsAlignedSize * static_cast<quint32>(_drawCalls.size());

    if(!_drawParamsUBO) {
        _drawParamsUBO.reset(rhi()->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, requiredSize));
        if(!_drawParamsUBO->create()) {
            service()->reportWarning("MeshPrimitiveRenderer: Failed to create draw params UBO.");
            _drawParamsUBO.reset();
            return;
        }
    }
    else if(_drawParamsUBO->size() < requiredSize) {
        _drawParamsUBO->setSize(requiredSize);
        if(!_drawParamsUBO->create()) {
            service()->reportWarning("MeshPrimitiveRenderer: Failed to resize draw params UBO.");
            _drawParamsUBO.reset();
            return;
        }
    }

    for(auto [i, dc] : Ovito::enumerate(_drawCalls)) {
        const MeshPrimitive& prim = *dc.primitive;

        AffineTransformation modelView = projParams.viewMatrix * dc.modelWorldTM;

        MeshDrawParamsData params{};
        params.modelViewMatrix     = Matrix4F(modelView.toDataType<float>());
        params.selectionColor      = prim.faceSelectionColor().toDataType<float>();
        params.wireframeColor      = prim.wireframeColor().toDataType<float>();
        params.opacity             = static_cast<float>(prim.uniformColor().a());
        params.pickingBaseObjectId = dc.objectId;
        if(dc.isPseudoColor) {
            float minVal = static_cast<float>(prim.pseudoColorMapping().minValue());
            float maxVal = static_cast<float>(prim.pseudoColorMapping().maxValue());
            if(minVal == maxVal) {
                minVal = std::min(minVal - 1e-6f, std::nextafter(minVal, std::numeric_limits<float>::lowest()));
                maxVal = std::max(maxVal + 1e-6f, std::nextafter(maxVal, std::numeric_limits<float>::max()));
            }
            params.colorRangeMin = minVal;
            params.colorRangeMax = maxVal;
        }
        params.lineThickness = (prim.wireframeWidth() > 0)
            ? static_cast<float>(prim.wireframeWidth()) / static_cast<float>(renderSize.height())
            : 1.0f / static_cast<float>(renderSize.height());

        const quint32 offset = _drawParamsAlignedSize * static_cast<quint32>(i);
        batch->updateDynamicBuffer(_drawParamsUBO.get(), offset, sizeof(MeshDrawParamsData), &params);

        // Upload face vertex and sorting buffers.
        if(dc.faceShader != ShaderVariant::None) // Skip face data upload when only wireframe is needed.
            uploadFaceData(batch, dc, isPicking, isInteractive, projParams);

        // Upload wireframe buffers.
        if(dc.needsWireframe)
            uploadWireframeData(batch, dc);
    }
}

/******************************************************************************
* Uploads face vertex data and sorting/index buffers for one draw call.
******************************************************************************/
void MeshPrimitiveRenderer::uploadFaceData(QRhiResourceUpdateBatch* batch, DrawCall& dc,
                                            bool isPicking, bool isInteractive,
                                            const ViewProjectionParameters& projParams)
{
    const MeshPrimitive& prim = *dc.primitive;
    const size_t faceCount = dc.faceCount;
    const bool highlightSelectedFaces = !isPicking;

    // Upload (or retrieve from cache) the flat vertex buffer.
    struct VertexCacheKey {};
    dc.bufs.vertices = impl()->createCachedBuffer(
        RendererResourceKey<VertexCacheKey,
            DataOORef<const TriangleMesh>,
            std::vector<ColorA>, ColorA, Color, bool, bool>{
            prim.mesh(),
            prim.materialColors(),
            prim.uniformColor(),
            prim.faceSelectionColor(),
            highlightSelectedFaces,
            dc.isPseudoColor},
        batch, QRhiBuffer::VertexBuffer,
        [&]() -> QByteArray {
            QByteArray data(static_cast<qsizetype>(faceCount * 3 * sizeof(MeshPrimitive::RenderVertex)), Qt::Uninitialized);
            auto span = std::span(reinterpret_cast<MeshPrimitive::RenderVertex*>(data.data()), faceCount * 3);
            prim.generateRenderableVertices(span, highlightSelectedFaces, dc.isPseudoColor);
            return data;
        });

    if(dc.isInstanced) {
        // Upload per-instance transformation matrices (as 3 × vec4 per instance, row-major).
        struct InstTMCacheKey {};
        dc.bufs.instanceTMs = impl()->createCachedBuffer(
            RendererResourceKey<InstTMCacheKey, ConstDataBufferPtr>{prim.perInstanceTMs()},
            batch, QRhiBuffer::VertexBuffer,
            [&]() -> QByteArray {
                const size_t N = prim.perInstanceTMs()->size();
                QByteArray data(static_cast<qsizetype>(N * 3 * sizeof(Vector4F)), Qt::Uninitialized);
                Vector4F* dst = reinterpret_cast<Vector4F*>(data.data());
                if(prim.perInstanceTMs()->dataType() == DataBuffer::Float32) {
                    for(const AffineTransformationT<float>& tm : BufferReadAccess<AffineTransformationT<float>>(prim.perInstanceTMs())) {
                        *dst++ = tm.row(0);
                        *dst++ = tm.row(1);
                        *dst++ = tm.row(2);
                    }
                }
                else {
                    for(const AffineTransformationT<double>& tm : BufferReadAccess<AffineTransformationT<double>>(prim.perInstanceTMs())) {
                        *dst++ = tm.row(0).toDataType<float>();
                        *dst++ = tm.row(1).toDataType<float>();
                        *dst++ = tm.row(2).toDataType<float>();
                    }
                }
                return data;
            });

        if(dc.hasInstanceColors && prim.perInstanceColors()) {
            dc.bufs.instanceColors = impl()->uploadDataBuffer(prim.perInstanceColors(), batch);
        }

        // For transparent instanced meshes with painter's algorithm: sort instances back-to-front via SSBO.
        if(dc.hasTransparency && !impl()->orderIndependentTransparency() && !isPicking) {
            const Matrix3F modelViewLinear = (projParams.viewMatrix * dc.modelWorldTM).linear().toDataType<float>();
            const Vector3F viewDir = modelViewLinear.inverse().column(2);
            Vector3F coarseDir = viewDir;
            if(isInteractive) {
                for(size_t k = 0; k < 3; k++)
                    coarseDir[k] = std::round(viewDir[k] * 2.0f) * 0.5f;
            }

            // Sorted instance index SSBO.
            struct SortCacheKey {};
            dc.bufs.sortedInstIdxBuf = impl()->createCachedBuffer(
                RendererResourceKey<SortCacheKey, ConstDataBufferPtr, Vector3F>{prim.perInstanceTMs(), coarseDir},
                batch, QRhiBuffer::StorageBuffer,
                [&]() -> QByteArray {
                    const size_t N = dc.instanceCount;
                    std::vector<float> distances(N);
                    if(prim.perInstanceTMs()->dataType() == DataBuffer::Float32) {
                        const Vector3F dir = viewDir;
                        BufferReadAccess<AffineTransformationT<float>> acc(prim.perInstanceTMs());
                        for(size_t k = 0; k < N; k++)
                            distances[k] = dir.dot(acc[k].translation());
                    }
                    else {
                        const auto dir = viewDir.toDataType<double>();
                        BufferReadAccess<AffineTransformationT<double>> acc(prim.perInstanceTMs());
                        for(size_t k = 0; k < N; k++)
                            distances[k] = static_cast<float>(dir.dot(acc[k].translation()));
                    }
                    QByteArray data(static_cast<qsizetype>(N * sizeof(uint32_t)), Qt::Uninitialized);
                    std::span<uint32_t> indices(reinterpret_cast<uint32_t*>(data.data()), N);
                    std::iota(indices.begin(), indices.end(), 0u);
                    Ovito::sort_zipped(distances, indices);
                    return data;
                });

            // Instance TM SSBO (layout: 3 × vec4 per instance).
            struct TMSSBOKey {};
            dc.bufs.instanceTMsSSBO = impl()->createCachedBuffer(
                RendererResourceKey<TMSSBOKey, ConstDataBufferPtr>{prim.perInstanceTMs()},
                batch, QRhiBuffer::StorageBuffer,
                [&]() -> QByteArray {
                    const size_t N = prim.perInstanceTMs()->size();
                    QByteArray data(static_cast<qsizetype>(N * 3 * sizeof(Vector4F)), Qt::Uninitialized);
                    Vector4F* dst = reinterpret_cast<Vector4F*>(data.data());
                    if(prim.perInstanceTMs()->dataType() == DataBuffer::Float32) {
                        for(const AffineTransformationT<float>& tm : BufferReadAccess<AffineTransformationT<float>>(prim.perInstanceTMs())) {
                            *dst++ = tm.row(0); *dst++ = tm.row(1); *dst++ = tm.row(2);
                        }
                    } else {
                        for(const AffineTransformationT<double>& tm : BufferReadAccess<AffineTransformationT<double>>(prim.perInstanceTMs())) {
                            *dst++ = tm.row(0).toDataType<float>();
                            *dst++ = tm.row(1).toDataType<float>();
                            *dst++ = tm.row(2).toDataType<float>();
                        }
                    }
                    return data;
                });

            // Instance color SSBO: per-instance colors (or uniform fill).
            if(prim.perInstanceColors()) {
                dc.bufs.instanceColorsSSBO = impl()->uploadDataBuffer(prim.perInstanceColors(), batch, QRhiBuffer::StorageBuffer);
            }
            else {
                // Fill with uniform opaque white so the vertex shader can multiply without branching.
                const ColorAF uniformColor{1.0f, 1.0f, 1.0f, static_cast<float>(prim.uniformColor().a())};
                dc.bufs.instanceColorsSSBO = impl()->uniformValueBuffer(uniformColor, dc.instanceCount, batch, QRhiBuffer::StorageBuffer);
            }

            // Build sorted shader resource bindings.
            if(_drawParamsUBO && dc.bufs.sortedInstIdxBuf && dc.bufs.instanceTMsSSBO
                && dc.bufs.instanceColorsSSBO && dc.bufs.vertices && impl()->sceneParamsUBO()) {
                dc.sortedInstanceBindings.reset(rhi()->newShaderResourceBindings());
                dc.sortedInstanceBindings->setBindings({
                    QRhiShaderResourceBinding::uniformBuffer(0,
                        QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage,
                        impl()->sceneParamsUBO()),
                    QRhiShaderResourceBinding::uniformBufferWithDynamicOffset(1,
                        QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage,
                        _drawParamsUBO.get(), sizeof(MeshDrawParamsData)),
                    QRhiShaderResourceBinding::bufferLoad(3, QRhiShaderResourceBinding::VertexStage, dc.bufs.sortedInstIdxBuf),
                    QRhiShaderResourceBinding::bufferLoad(4, QRhiShaderResourceBinding::VertexStage, dc.bufs.instanceTMsSSBO),
                    QRhiShaderResourceBinding::bufferLoad(5, QRhiShaderResourceBinding::VertexStage, dc.bufs.instanceColorsSSBO),
                });
                if(!dc.sortedInstanceBindings->create()) {
                    service()->reportWarning("MeshPrimitiveRenderer: Failed to create sorted instance bindings.");
                    dc.sortedInstanceBindings.reset();
                }
            }
        }
    }
    else {
        // Non-instanced: for painter's algorithm transparent, sort faces back-to-front.
        if(dc.hasTransparency && !impl()->orderIndependentTransparency() && !isPicking
            && prim.depthSortingMode() != MeshPrimitive::ConvexShapeMode) {

            const Vector3F viewDirOS = (projParams.viewMatrix * dc.modelWorldTM).linear().inverse().toDataType<float>().column(2);
            Vector3F coarseDir = viewDirOS;
            if(isInteractive) {
                for(size_t k = 0; k < 3; k++)
                    coarseDir[k] = std::round(viewDirOS[k] * 2.0f) * 0.5f;
            }

            struct FaceSortCacheKey {};
            dc.bufs.sortedFaceIdxBuf = impl()->createCachedBuffer(
                RendererResourceKey<FaceSortCacheKey, DataOORef<const TriangleMesh>, Vector3F>{prim.mesh(), coarseDir},
                batch, QRhiBuffer::IndexBuffer,
                [&]() -> QByteArray {
                    const TriangleMesh& mesh = *prim.mesh();
                    const auto& verts = mesh.vertices();

                    std::vector<float> distances(faceCount);
                    auto faceIt = mesh.faces().cbegin();
                    for(size_t f = 0; f < faceCount; f++, ++faceIt) {
                        const auto& v0 = verts[faceIt->vertex(0)];
                        const auto& v1 = verts[faceIt->vertex(1)];
                        const auto& v2 = verts[faceIt->vertex(2)];
                        Vector3F centroid{
                            static_cast<float>(v0.x() + v1.x() + v2.x()) / 3.0f,
                            static_cast<float>(v0.y() + v1.y() + v2.y()) / 3.0f,
                            static_cast<float>(v0.z() + v1.z() + v2.z()) / 3.0f};
                        distances[f] = viewDirOS.dot(centroid);
                    }

                    std::vector<uint32_t> sortedFaces(faceCount);
                    std::iota(sortedFaces.begin(), sortedFaces.end(), 0u);
                    Ovito::sort_zipped(distances, sortedFaces);

                    // Each face → 3 vertex indices (stride-3 in the flat vertex buffer).
                    QByteArray data(static_cast<qsizetype>(faceCount * 3 * sizeof(uint32_t)), Qt::Uninitialized);
                    uint32_t* dst = reinterpret_cast<uint32_t*>(data.data());
                    for(uint32_t fi : sortedFaces) {
                        *dst++ = fi * 3;
                        *dst++ = fi * 3 + 1;
                        *dst++ = fi * 3 + 2;
                    }
                    return data;
                });
        }

        // Pseudo-color: build per-draw bindings with colorMap texture.
        if(dc.isPseudoColor && _drawParamsUBO && impl()->sceneParamsUBO()) {
            QRhiSampler* sampler;
            std::tie(dc.colorMapTexture, sampler) = impl()->ensureColorMapTexture(prim.pseudoColorMapping(), batch);
            if(dc.colorMapTexture && sampler) {
                dc.pseudoColorBindings.reset(rhi()->newShaderResourceBindings());
                dc.pseudoColorBindings->setBindings({
                    QRhiShaderResourceBinding::uniformBuffer(0,
                        QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage,
                        impl()->sceneParamsUBO()),
                    QRhiShaderResourceBinding::uniformBufferWithDynamicOffset(1,
                        QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage,
                        _drawParamsUBO.get(), sizeof(MeshDrawParamsData)),
                    QRhiShaderResourceBinding::sampledTexture(2,
                        QRhiShaderResourceBinding::FragmentStage,
                        dc.colorMapTexture, sampler),
                });
                if(!dc.pseudoColorBindings->create()) {
                    service()->reportWarning("MeshPrimitiveRenderer: Failed to create pseudo-color bindings.");
                    dc.pseudoColorBindings.reset();
                }
            }
        }
    }
}

/******************************************************************************
* Uploads wireframe line buffers for one draw call.
******************************************************************************/
void MeshPrimitiveRenderer::uploadWireframeData(QRhiResourceUpdateBatch* batch, DrawCall& dc)
{
    const MeshPrimitive& prim = *dc.primitive;

    // Generate / cache wireframe geometry.
    ConstDataBufferPtr wireframeLines = impl()->rhiCache().lookup<ConstDataBufferPtr>(
        RendererResourceKey<struct WireframeCacheKey, DataOORef<const TriangleMesh>>{prim.mesh()},
        [&](ConstDataBufferPtr& wireframeLines) {
            wireframeLines = prim.generateWireframeLines();
        });
    if(!wireframeLines || wireframeLines->size() == 0)
        return;

    dc.wireframeEdgeCount = wireframeLines->size() / 2;

    if(dc.wireframeShader == ShaderVariant::Wireframe || dc.wireframeShader == ShaderVariant::WireframeInstanced) {
        // Thin: simple per-vertex line positions.
        dc.bufs.wireframeLines = impl()->uploadDataBuffer(wireframeLines, batch);
    }
    else if(dc.wireframeShader == ShaderVariant::WireframeTri) {
        // Thick non-instanced: use existing wireframe buffer as per-instance endpoint pairs
        // (position_from @ offset 0, position_to @ offset sizeof(Point3F), stride = 2*sizeof(Point3F)).
        dc.bufs.wireframeLines = impl()->uploadDataBuffer(wireframeLines, batch);
    }
    else if(dc.wireframeShader == ShaderVariant::WireframeTriInstanced) {
        // Thick instanced: replicate the wireframe edge pairs N_instances times,
        // interleaving each edge pair with the repeated instance TM rows.
        // Combined instance = (meshInstIndex * N_edges + edgeIndex).
        // Buffer layout per combined instance: [position_from, position_to, tmRow0, tmRow1, tmRow2]
        const size_t N = dc.instanceCount;
        const size_t E = dc.wireframeEdgeCount;

        dc.bufs.wireframeTMsBuf = impl()->createCachedBuffer(
            RendererResourceKey<struct ThickInstCacheKey, ConstDataBufferPtr, ConstDataBufferPtr>{
                wireframeLines, prim.perInstanceTMs()},
            batch, QRhiBuffer::VertexBuffer,
            [&]() -> QByteArray {
                // Each combined instance: vec3 from + vec3 to + 3 × vec4 TM rows = 6+12 = 18 floats.
                struct CombinedEntry {
                    Point3F  posFrom;  // 12 bytes
                    Point3F  posTo;    // 12 bytes
                    Vector4F tmRow0;   // 16 bytes
                    Vector4F tmRow1;   // 16 bytes
                    Vector4F tmRow2;   // 16 bytes
                };
                QByteArray data(static_cast<qsizetype>(N * E * sizeof(CombinedEntry)), Qt::Uninitialized);
                CombinedEntry* dst = reinterpret_cast<CombinedEntry*>(data.data());

                BufferReadAccess<Point3G> wireAccess(wireframeLines);
                const Point3G* edges = wireAccess.begin();

                auto writeTM = [&](size_t instIdx, CombinedEntry* entries, size_t edgeCount) {
                    auto emitRows = [&](const Vector4F& r0, const Vector4F& r1, const Vector4F& r2) {
                        for(size_t e = 0; e < edgeCount; e++) {
                            entries[e].tmRow0 = r0;
                            entries[e].tmRow1 = r1;
                            entries[e].tmRow2 = r2;
                        }
                    };
                    if(prim.perInstanceTMs()->dataType() == DataBuffer::Float32) {
                        const AffineTransformationT<float>& tm = BufferReadAccess<AffineTransformationT<float>>(prim.perInstanceTMs())[instIdx];
                        emitRows(tm.row(0), tm.row(1), tm.row(2));
                    }
                    else {
                        const AffineTransformationT<double>& tm = BufferReadAccess<AffineTransformationT<double>>(prim.perInstanceTMs())[instIdx];
                        emitRows(tm.row(0).toDataType<float>(), tm.row(1).toDataType<float>(), tm.row(2).toDataType<float>());
                    }
                };

                for(size_t inst = 0; inst < N; inst++) {
                    CombinedEntry* base = dst + inst * E;
                    for(size_t e = 0; e < E; e++) {
                        base[e].posFrom = edges[2 * e + 0].toDataType<float>();
                        base[e].posTo   = edges[2 * e + 1].toDataType<float>();
                    }
                    writeTM(inst, base, E);
                }
                return data;
            });
    }
}

/******************************************************************************
* Returns true if any draw call has transparency.
******************************************************************************/
bool MeshPrimitiveRenderer::hasTransparentDrawCalls() const
{
    return std::ranges::any_of(_drawCalls, [](const DrawCall& dc) { return dc.hasTransparency && dc.faceShader != ShaderVariant::None; });
}

/******************************************************************************
* OIT accumulation pass.
******************************************************************************/
void MeshPrimitiveRenderer::drawOITAccumPass(QRhiCommandBuffer* cb, QRhiRenderPassDescriptor* rpd)
{
    // Depth prime pass: write opaque depth first, suppressing all color writes.
    // This ensures the particle OIT contribution already in accumTexture is not
    // overwritten by the opaque mesh color.
    draw(cb, rpd, true, false, false, false, false, false, /*oitDepthPrime=*/true);
    // Accumulation pass.
    draw(cb, rpd, false, true, false, true, false);
}

/******************************************************************************
* OIT reveal pass.
******************************************************************************/
void MeshPrimitiveRenderer::drawOITRevealPass(QRhiCommandBuffer* cb, QRhiRenderPassDescriptor* rpd)
{
    draw(cb, rpd, false, true, false, false, true);
}

/******************************************************************************
* Draws ExcludeFromOutline SceneLayer meshes into the depth-only excluded-depth pre-pass.
******************************************************************************/
void MeshPrimitiveRenderer::drawExclusionDepth(QRhiCommandBuffer* cb, QRhiRenderPassDescriptor* rpd)
{
    draw(cb, rpd, false, false, false, false, false, /*excludedDepthOnly=*/true);
}

/******************************************************************************
* Phase 3 (public): Issues draw calls.
******************************************************************************/
void MeshPrimitiveRenderer::draw(QRhiCommandBuffer* cb, QRhiRenderPassDescriptor* rpd,
                                  bool opaqueOnly, bool transparentOnly, bool isPicking)
{
    draw(cb, rpd, opaqueOnly, transparentOnly, isPicking, false, false);
}

/******************************************************************************
* Phase 3 (internal): handles all rendering modes.
******************************************************************************/
void MeshPrimitiveRenderer::draw(QRhiCommandBuffer* cb, QRhiRenderPassDescriptor* rpd,
                                  bool opaqueOnly, bool transparentOnly, bool isPicking,
                                  bool oitAccum, bool oitReveal, bool excludedDepthOnly,
                                  bool oitDepthPrime)
{
    if(!impl()->sceneParamsUBO() || !_drawParamsUBO)
        return;

    for(const auto& [i, dc] : Ovito::enumerate(_drawCalls)) {
        // Excluded-depth pre-pass: render ONLY ExcludeFromOutline geometry. Raytraced excluded
        // objects (filteredOut) are handled by the raytracer's own excluded-depth contribution.
        if(excludedDepthOnly && !dc.excludeFromOutline) continue;
        if(excludedDepthOnly && dc.filteredOut) continue;
        if(excludedDepthOnly && dc.faceShader == ShaderVariant::None) continue;
        if(!excludedDepthOnly && dc.filteredOut) continue; // handled by parent renderer
        if(opaqueOnly      &&  dc.hasTransparency) continue;
        if(transparentOnly && !dc.hasTransparency) continue;

        const quint32 dynOff = _drawParamsAlignedSize * static_cast<quint32>(i);
        QRhiCommandBuffer::DynamicOffset dynOffset(1, dynOff);

        // ── Wireframe pre-pass ──────────────────────────────────────────────
        if(dc.needsWireframe && !isPicking && !oitAccum && !oitReveal && !excludedDepthOnly && !oitDepthPrime) {
            PipelineFlags wfFlags;
            wfFlags.setFlag(PipelineFlag::DepthTest,  true);
            wfFlags.setFlag(PipelineFlag::DepthWrite, true);
            wfFlags.setFlag(PipelineFlag::NegativeDepthBias, dc.faceShader == ShaderVariant::None); // Render wireframe lines over faces rendered by a non-QRhi renderer.
            if(dc.primitive->wireframeColor().a() < FloatType(1))
                wfFlags.setFlag(PipelineFlag::Blend, true);

            QRhiGraphicsPipeline* wfPipeline = ensurePipeline(rpd, dc.wireframeShader, wfFlags, dc);
            // The combined VBO for thick instanced wireframe lives in wireframeTMsBuf;
            // all other variants source from wireframeLines.
            QRhiBuffer* const wfVbo = (dc.wireframeShader == ShaderVariant::WireframeTriInstanced)
                ? dc.bufs.wireframeTMsBuf : dc.bufs.wireframeLines;
            if(wfPipeline && wfVbo) {
                QRhiShaderResourceBindings* bindings = ensureShaderResourceBindings();
                if(bindings) {
                    cb->setGraphicsPipeline(wfPipeline);
                    cb->setShaderResources(bindings, 1, &dynOffset);

                    if(dc.wireframeShader == ShaderVariant::Wireframe) {
                        const QRhiCommandBuffer::VertexInput vb[] = { { dc.bufs.wireframeLines, 0 } };
                        cb->setVertexInput(0, 1, vb);
                        cb->draw(static_cast<quint32>(dc.wireframeEdgeCount * 2));
                    }
                    else if(dc.wireframeShader == ShaderVariant::WireframeInstanced && dc.bufs.instanceTMs) {
                        // Thin instanced: per-vertex wire positions + per-instance TM rows.
                        const QRhiCommandBuffer::VertexInput vb[] = {
                            { dc.bufs.wireframeLines, 0 },
                            { dc.bufs.instanceTMs,    0 },
                            { dc.bufs.instanceTMs,    sizeof(Vector4F) },
                            { dc.bufs.instanceTMs,    2 * sizeof(Vector4F) },
                        };
                        cb->setVertexInput(0, 4, vb);
                        cb->draw(static_cast<quint32>(dc.wireframeEdgeCount * 2),
                                    static_cast<quint32>(dc.instanceCount));
                    }
                    else if(dc.wireframeShader == ShaderVariant::WireframeTri) {
                        // Thick non-instanced: pairs as per-instance endpoints.
                        const QRhiCommandBuffer::VertexInput vb[] = {
                            { dc.bufs.wireframeLines, 0 },
                            { dc.bufs.wireframeLines, sizeof(Point3F) },
                        };
                        cb->setVertexInput(0, 2, vb);
                        cb->draw(4, static_cast<quint32>(dc.wireframeEdgeCount));
                    }
                    else if(dc.wireframeShader == ShaderVariant::WireframeTriInstanced && dc.bufs.wireframeTMsBuf) {
                        // Thick instanced: combined buffer with edge pairs + TM rows.
                        // position_from @ 0, position_to @ 12, tmRow0 @ 24, tmRow1 @ 40, tmRow2 @ 56.
                        const QRhiCommandBuffer::VertexInput vb[] = {
                            { dc.bufs.wireframeTMsBuf, 0 },                      // position_from
                            { dc.bufs.wireframeTMsBuf, sizeof(Point3F) },        // position_to
                            { dc.bufs.wireframeTMsBuf, 2 * sizeof(Point3F) },    // tmRow0
                            { dc.bufs.wireframeTMsBuf, 2 * sizeof(Point3F) + sizeof(Vector4F) }, // tmRow1
                            { dc.bufs.wireframeTMsBuf, 2 * sizeof(Point3F) + 2 * sizeof(Vector4F) }, // tmRow2
                        };
                        cb->setVertexInput(0, 5, vb);
                        cb->draw(4, static_cast<quint32>(dc.instanceCount * dc.wireframeEdgeCount));
                    }
                }
            }
        }

        if(!dc.bufs.vertices)
            continue;

        // ── Face draw pass ─────────────────────────────────────────────────
        ShaderVariant  faceShader = dc.faceShader;
        if(faceShader == ShaderVariant::None)
            continue; // Skip face rendering when only wireframe is needed.

        // For the outline depth pre-pass, use the simple mesh/instanced vertex shaders
        // (only depth is written; pseudo-color and per-instance-color variants not needed).
        if(excludedDepthOnly) {
            // Map pseudo-color/instance-color variants to their plain equivalents.
            ShaderVariant depthShader = faceShader;
            if(depthShader == ShaderVariant::MeshColorMapping)
                depthShader = ShaderVariant::Mesh;
            else if(depthShader == ShaderVariant::MeshInstancedColors)
                depthShader = ShaderVariant::MeshInstanced;

            PipelineFlags flags;
            flags.setFlag(PipelineFlag::DepthTest,  true);
            flags.setFlag(PipelineFlag::DepthWrite, true);
            flags.setFlag(PipelineFlag::CullBack,   dc.primitive->cullFaces());
            flags.setFlag(PipelineFlag::DepthOnly,  true);
            QRhiGraphicsPipeline* pipeline = ensurePipeline(rpd, depthShader, flags, dc);
            if(!pipeline)
                continue;
            QRhiShaderResourceBindings* bindings = ensureShaderResourceBindings();
            if(!bindings)
                continue;
            cb->setGraphicsPipeline(pipeline);
            cb->setShaderResources(bindings, 1, &dynOffset);
            if(!dc.isInstanced) {
                const QRhiCommandBuffer::VertexInput vb[] = {
                    { dc.bufs.vertices, 0 },
                    { dc.bufs.vertices, 0 },
                    { dc.bufs.vertices, 0 },
                };
                cb->setVertexInput(0, 3, vb);
                cb->draw(static_cast<quint32>(dc.faceCount * 3));
            }
            else if(dc.bufs.instanceTMs) {
                const QRhiCommandBuffer::VertexInput vb[] = {
                    { dc.bufs.vertices,    0 },
                    { dc.bufs.vertices,    0 },
                    { dc.bufs.vertices,    0 },
                    { dc.bufs.instanceTMs, 0 },
                    { dc.bufs.instanceTMs, sizeof(Vector4F) },
                    { dc.bufs.instanceTMs, 2 * sizeof(Vector4F) },
                };
                cb->setVertexInput(0, 6, vb);
                cb->draw(static_cast<quint32>(dc.faceCount * 3),
                         static_cast<quint32>(dc.instanceCount));
            }
            continue;
        }

        if(oitAccum)   faceShader = toOITAccumVariant(faceShader);
        if(oitReveal)  faceShader = toOITRevealVariant(faceShader);

        const bool useSortedInstances = dc.isInstanced && dc.hasTransparency
                                     && !impl()->orderIndependentTransparency()
                                     && !oitAccum && !oitReveal && !isPicking
                                     && dc.sortedInstanceBindings;

        if(useSortedInstances)
            faceShader = ShaderVariant::MeshInstancedSorted;

        PipelineFlags faceFlags;
        faceFlags.setFlag(PipelineFlag::DepthTest, true);
        faceFlags.setFlag(PipelineFlag::CullBack,  dc.primitive->cullFaces());
        faceFlags.setFlag(PipelineFlag::DepthBias, dc.needsWireframe && !isPicking);

        if(dc.hasTransparency) {
            if(oitAccum) {
                faceFlags.setFlag(PipelineFlag::OITAccum,  true);
                faceFlags.setFlag(PipelineFlag::DepthWrite, false);
            }
            else if(oitReveal) {
                faceFlags.setFlag(PipelineFlag::OITReveal,  true);
                faceFlags.setFlag(PipelineFlag::DepthWrite, false);
            }
            else if(useSortedInstances || dc.primitive->depthSortingMode() == MeshPrimitive::ConvexShapeMode
                      || dc.bufs.sortedFaceIdxBuf) {
                // Painter's algorithm: blend with depth write.
                faceFlags.setFlag(PipelineFlag::Blend,      true);
                faceFlags.setFlag(PipelineFlag::DepthWrite, true);
            }
            else {
                faceFlags.setFlag(PipelineFlag::Blend,      true);
                faceFlags.setFlag(PipelineFlag::DepthWrite, false);
            }
        }
        else {
            faceFlags.setFlag(PipelineFlag::DepthWrite, true);
            if(oitDepthPrime)
                faceFlags.setFlag(PipelineFlag::OITDepthPrime, true);
        }

        QRhiGraphicsPipeline* pipeline = ensurePipeline(rpd, faceShader, faceFlags, dc);
        if(!pipeline)
            continue;

        if(useSortedInstances) {
            cb->setGraphicsPipeline(pipeline);
            cb->setShaderResources(dc.sortedInstanceBindings.get(), 1, &dynOffset);
            const QRhiCommandBuffer::VertexInput vb[] = {
                { dc.bufs.vertices, 0 },
                { dc.bufs.vertices, 0 },
                { dc.bufs.vertices, 0 },
            };
            cb->setVertexInput(0, 3, vb);
            cb->draw(static_cast<quint32>(dc.faceCount * 3),
                     static_cast<quint32>(dc.instanceCount));
            continue;
        }

        QRhiShaderResourceBindings* bindings;
        if(dc.isPseudoColor && dc.pseudoColorBindings)
            bindings = dc.pseudoColorBindings.get();
        else
            bindings = ensureShaderResourceBindings();
        if(!bindings)
            continue;

        cb->setGraphicsPipeline(pipeline);
        cb->setShaderResources(bindings, 1, &dynOffset);

        // ConvexShapeMode semi-transparent: two-pass rendering (back faces, then front faces).
        const bool convexTransp = dc.hasTransparency
            && dc.primitive->depthSortingMode() == MeshPrimitive::ConvexShapeMode
            && !impl()->orderIndependentTransparency()
            && !oitAccum && !oitReveal && !isPicking;

        auto issueVertexInputAndDraw = [&](int passIndex) {
            if(!dc.isInstanced) {
                if(dc.bufs.sortedFaceIdxBuf) {
                    // Indexed draw with sorted face indices.
                    const QRhiCommandBuffer::VertexInput vb[] = {
                        { dc.bufs.vertices, 0 },
                        { dc.bufs.vertices, 0 },
                        { dc.bufs.vertices, 0 },
                    };
                    cb->setVertexInput(0, 3, vb, dc.bufs.sortedFaceIdxBuf, 0, QRhiCommandBuffer::IndexUInt32);
                    cb->drawIndexed(static_cast<quint32>(dc.faceCount * 3));
                } else {
                    const QRhiCommandBuffer::VertexInput vb[] = {
                        { dc.bufs.vertices, 0 },
                        { dc.bufs.vertices, 0 },
                        { dc.bufs.vertices, 0 },
                    };
                    cb->setVertexInput(0, isPicking ? 1 : 3, vb);
                    cb->draw(static_cast<quint32>(dc.faceCount * 3));
                }
            }
            else {
                // Instanced: per-vertex position/normal/color + per-instance TM rows + optional per-instance color.
                if(isPicking) {
                    const QRhiCommandBuffer::VertexInput vb[] = {
                        { dc.bufs.vertices,    0 },
                        { dc.bufs.instanceTMs, 0 },
                        { dc.bufs.instanceTMs, sizeof(Vector4F) },
                        { dc.bufs.instanceTMs, 2 * sizeof(Vector4F) },
                    };
                    cb->setVertexInput(0, 4, vb);
                }
                else if(dc.hasInstanceColors && dc.bufs.instanceColors) {
                    const QRhiCommandBuffer::VertexInput vb[] = {
                        { dc.bufs.vertices,      0 },
                        { dc.bufs.vertices,      0 },
                        { dc.bufs.vertices,      0 },
                        { dc.bufs.instanceTMs,   0 },
                        { dc.bufs.instanceTMs,   sizeof(Vector4F) },
                        { dc.bufs.instanceTMs,   2 * sizeof(Vector4F) },
                        { dc.bufs.instanceColors, 0 },
                    };
                    cb->setVertexInput(0, 7, vb);
                }
                else {
                    const QRhiCommandBuffer::VertexInput vb[] = {
                        { dc.bufs.vertices,    0 },
                        { dc.bufs.vertices,    0 },
                        { dc.bufs.vertices,    0 },
                        { dc.bufs.instanceTMs, 0 },
                        { dc.bufs.instanceTMs, sizeof(Vector4F) },
                        { dc.bufs.instanceTMs, 2 * sizeof(Vector4F) },
                    };
                    cb->setVertexInput(0, 6, vb);
                }
                cb->draw(static_cast<quint32>(dc.faceCount * 3),
                         static_cast<quint32>(dc.instanceCount));
            }
        };

        if(convexTransp && !dc.primitive->cullFaces()) {
            // First pass: back faces only.
            PipelineFlags backFlags = faceFlags;
            backFlags.setFlag(PipelineFlag::CullBack, false);
            // TODO: need a "cull front" flag. For now just render both sides and rely on depth sorting.
            issueVertexInputAndDraw(0);
        }
        else {
            issueVertexInputAndDraw(0);
        }
    }
}

/******************************************************************************
* Ensures the shared VBO shader resource bindings.
******************************************************************************/
QRhiShaderResourceBindings* MeshPrimitiveRenderer::ensureShaderResourceBindings()
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
                _drawParamsUBO.get(), sizeof(MeshDrawParamsData)),
        });
        if(!_bindings->create()) {
            service()->reportWarning("MeshPrimitiveRenderer: Failed to create shader resource bindings.");
            _bindings.reset();
        }
    }
    return _bindings.get();
}

/******************************************************************************
* Ensures a graphics pipeline for the given variant and flags.
******************************************************************************/
QRhiGraphicsPipeline* MeshPrimitiveRenderer::ensurePipeline(QRhiRenderPassDescriptor* rpd,
                                                             ShaderVariant variant,
                                                             PipelineFlags flags,
                                                             const DrawCall& dc)
{
    struct PipelineCacheKey {
        ShaderVariant variant;
        PipelineFlags flags;
        bool operator==(const PipelineCacheKey&) const = default;
    };

    return service()->ensureGraphicsPipeline(rpd, PipelineCacheKey{variant, flags}, [&]() -> std::unique_ptr<QRhiGraphicsPipeline> {
        if(!impl()->sceneParamsUBO() || !_drawParamsUBO)
            return {};

        static const QLatin1String BASE(":/ovito/core/rendering/standard/shaders/");

        QString vsPath, fsPath;
        QRhiGraphicsPipeline::Topology topology = QRhiGraphicsPipeline::Triangles;
        bool isPicking = false;
        bool isWireframe = false;
        bool isSortedSSBO = false;

        switch(variant) {
            case ShaderVariant::Mesh:
                vsPath = BASE + QLatin1String("mesh.vert.qsb");
                fsPath = BASE + QLatin1String("mesh_visual.frag.qsb");
                break;
            case ShaderVariant::MeshOITAccum:
                vsPath = BASE + QLatin1String("mesh.vert.qsb");
                fsPath = BASE + QLatin1String("mesh_oit_accum.frag.qsb");
                break;
            case ShaderVariant::MeshOITReveal:
                vsPath = BASE + QLatin1String("mesh.vert.qsb");
                fsPath = BASE + QLatin1String("mesh_oit_reveal.frag.qsb");
                break;
            case ShaderVariant::MeshPicking:
                vsPath = BASE + QLatin1String("mesh_picking.vert.qsb");
                fsPath = BASE + QLatin1String("mesh_picking.frag.qsb");
                isPicking = true;
                break;
            case ShaderVariant::MeshColorMapping:
                vsPath = BASE + QLatin1String("mesh_color_mapping.vert.qsb");
                fsPath = BASE + QLatin1String("mesh_color_mapping.frag.qsb");
                break;
            case ShaderVariant::MeshInstanced:
                vsPath = BASE + QLatin1String("mesh_instanced.vert.qsb");
                fsPath = BASE + QLatin1String("mesh_visual.frag.qsb");
                break;
            case ShaderVariant::MeshInstancedOITAccum:
                vsPath = BASE + QLatin1String("mesh_instanced.vert.qsb");
                fsPath = BASE + QLatin1String("mesh_oit_accum.frag.qsb");
                break;
            case ShaderVariant::MeshInstancedOITReveal:
                vsPath = BASE + QLatin1String("mesh_instanced.vert.qsb");
                fsPath = BASE + QLatin1String("mesh_oit_reveal.frag.qsb");
                break;
            case ShaderVariant::MeshInstancedColors:
                vsPath = BASE + QLatin1String("mesh_instanced_colors.vert.qsb");
                fsPath = BASE + QLatin1String("mesh_visual.frag.qsb");
                break;
            case ShaderVariant::MeshInstancedColorsOITAccum:
                vsPath = BASE + QLatin1String("mesh_instanced_colors.vert.qsb");
                fsPath = BASE + QLatin1String("mesh_oit_accum.frag.qsb");
                break;
            case ShaderVariant::MeshInstancedColorsOITReveal:
                vsPath = BASE + QLatin1String("mesh_instanced_colors.vert.qsb");
                fsPath = BASE + QLatin1String("mesh_oit_reveal.frag.qsb");
                break;
            case ShaderVariant::MeshInstancedSorted:
                vsPath = BASE + QLatin1String("mesh_instanced_sorted.vert.qsb");
                fsPath = BASE + QLatin1String("mesh_visual.frag.qsb");
                isSortedSSBO = true;
                break;
            case ShaderVariant::MeshInstancedPicking:
                vsPath = BASE + QLatin1String("mesh_instanced_picking.vert.qsb");
                fsPath = BASE + QLatin1String("mesh_picking.frag.qsb");
                isPicking = true;
                break;
            case ShaderVariant::Wireframe:
                vsPath = BASE + QLatin1String("mesh_wireframe.vert.qsb");
                fsPath = BASE + QLatin1String("mesh_wireframe.frag.qsb");
                topology = QRhiGraphicsPipeline::Lines;
                isWireframe = true;
                break;
            case ShaderVariant::WireframeInstanced:
                vsPath = BASE + QLatin1String("mesh_wireframe_instanced.vert.qsb");
                fsPath = BASE + QLatin1String("mesh_wireframe.frag.qsb");
                topology = QRhiGraphicsPipeline::Lines;
                isWireframe = true;
                break;
            case ShaderVariant::WireframeTri:
                vsPath = BASE + QLatin1String("mesh_wireframe_tri.vert.qsb");
                fsPath = BASE + QLatin1String("mesh_wireframe.frag.qsb");
                topology = QRhiGraphicsPipeline::TriangleStrip;
                isWireframe = true;
                break;
            case ShaderVariant::WireframeTriInstanced:
                vsPath = BASE + QLatin1String("mesh_wireframe_tri_instanced.vert.qsb");
                fsPath = BASE + QLatin1String("mesh_wireframe.frag.qsb");
                topology = QRhiGraphicsPipeline::TriangleStrip;
                isWireframe = true;
                break;
            default:
                return {};
        }

        QShader vs = service()->loadShader(vsPath);
        QShader fs = service()->loadShader(fsPath);
        if(!vs.isValid() || !fs.isValid())
            return {};

        // Build vertex input layout.
        QRhiVertexInputLayout inputLayout;
        QVector<QRhiVertexInputBinding> bindings;
        QVector<QRhiVertexInputAttribute> attrs;

        // RenderVertex layout: position (vec3) + normal (vec3) + color (vec4) = 40 bytes.
        constexpr quint32 RV_STRIDE = sizeof(MeshPrimitive::RenderVertex);
        constexpr quint32 POS_OFF   = offsetof(MeshPrimitive::RenderVertex, position);
        constexpr quint32 NRM_OFF   = offsetof(MeshPrimitive::RenderVertex, normal);
        constexpr quint32 CLR_OFF   = offsetof(MeshPrimitive::RenderVertex, color);

        if(isWireframe) {
            switch(variant) {
                case ShaderVariant::Wireframe:
                    bindings.append(QRhiVertexInputBinding(sizeof(Point3F), QRhiVertexInputBinding::PerVertex));
                    attrs.append(QRhiVertexInputAttribute(0, 0, QRhiVertexInputAttribute::Float3, 0));
                    break;
                case ShaderVariant::WireframeInstanced:
                    bindings.append(QRhiVertexInputBinding(sizeof(Point3F), QRhiVertexInputBinding::PerVertex));
                    attrs.append(QRhiVertexInputAttribute(0, 0, QRhiVertexInputAttribute::Float3, 0));
                    // Per-instance TM rows: three vec4 in the same buffer at different offsets.
                    bindings.append(QRhiVertexInputBinding(3 * sizeof(Vector4F), QRhiVertexInputBinding::PerInstance));
                    attrs.append(QRhiVertexInputAttribute(1, 1, QRhiVertexInputAttribute::Float4, 0));
                    bindings.append(QRhiVertexInputBinding(3 * sizeof(Vector4F), QRhiVertexInputBinding::PerInstance));
                    attrs.append(QRhiVertexInputAttribute(2, 2, QRhiVertexInputAttribute::Float4, 0));
                    bindings.append(QRhiVertexInputBinding(3 * sizeof(Vector4F), QRhiVertexInputBinding::PerInstance));
                    attrs.append(QRhiVertexInputAttribute(3, 3, QRhiVertexInputAttribute::Float4, 0));
                    break;
                case ShaderVariant::WireframeTri:
                    // position_from and position_to as per-instance attributes from the same buffer.
                    bindings.append(QRhiVertexInputBinding(2 * sizeof(Point3F), QRhiVertexInputBinding::PerInstance));
                    attrs.append(QRhiVertexInputAttribute(0, 0, QRhiVertexInputAttribute::Float3, 0));
                    bindings.append(QRhiVertexInputBinding(2 * sizeof(Point3F), QRhiVertexInputBinding::PerInstance));
                    attrs.append(QRhiVertexInputAttribute(1, 1, QRhiVertexInputAttribute::Float3, 0));
                    break;
                case ShaderVariant::WireframeTriInstanced: {
                    // Combined per-instance buffer: position_from, position_to, tmRow0, tmRow1, tmRow2.
                    // Each attribute uses its own binding aliased to the same buffer; the byte offset
                    // of the field within the entry is supplied via VertexInput.offset in setVertexInput().
                    constexpr quint32 STRIDE = 2 * sizeof(Point3F) + 3 * sizeof(Vector4F);
                    bindings.append(QRhiVertexInputBinding(STRIDE, QRhiVertexInputBinding::PerInstance));
                    attrs.append(QRhiVertexInputAttribute(0, 0, QRhiVertexInputAttribute::Float3, 0));
                    bindings.append(QRhiVertexInputBinding(STRIDE, QRhiVertexInputBinding::PerInstance));
                    attrs.append(QRhiVertexInputAttribute(1, 1, QRhiVertexInputAttribute::Float3, 0));
                    bindings.append(QRhiVertexInputBinding(STRIDE, QRhiVertexInputBinding::PerInstance));
                    attrs.append(QRhiVertexInputAttribute(2, 2, QRhiVertexInputAttribute::Float4, 0));
                    bindings.append(QRhiVertexInputBinding(STRIDE, QRhiVertexInputBinding::PerInstance));
                    attrs.append(QRhiVertexInputAttribute(3, 3, QRhiVertexInputAttribute::Float4, 0));
                    bindings.append(QRhiVertexInputBinding(STRIDE, QRhiVertexInputBinding::PerInstance));
                    attrs.append(QRhiVertexInputAttribute(4, 4, QRhiVertexInputAttribute::Float4, 0));
                    break;
                }
                default: break;
            }
        }
        else if(isSortedSSBO) {
            // Sorted SSBO path: per-vertex position, normal, color from VBO (no per-instance).
            bindings.append(QRhiVertexInputBinding(RV_STRIDE, QRhiVertexInputBinding::PerVertex));
            attrs.append(QRhiVertexInputAttribute(0, 0, QRhiVertexInputAttribute::Float3, POS_OFF));
            bindings.append(QRhiVertexInputBinding(RV_STRIDE, QRhiVertexInputBinding::PerVertex));
            attrs.append(QRhiVertexInputAttribute(1, 1, QRhiVertexInputAttribute::Float3, NRM_OFF));
            bindings.append(QRhiVertexInputBinding(RV_STRIDE, QRhiVertexInputBinding::PerVertex));
            attrs.append(QRhiVertexInputAttribute(2, 2, QRhiVertexInputAttribute::Float4, CLR_OFF));
        }
        else if(isPicking) {
            if(variant == ShaderVariant::MeshPicking) {
                bindings.append(QRhiVertexInputBinding(RV_STRIDE, QRhiVertexInputBinding::PerVertex));
                attrs.append(QRhiVertexInputAttribute(0, 0, QRhiVertexInputAttribute::Float3, POS_OFF));
            }
            else {
                // MeshInstancedPicking: per-vertex pos + per-instance TM rows at locations 3/4/5.
                bindings.append(QRhiVertexInputBinding(RV_STRIDE, QRhiVertexInputBinding::PerVertex));
                attrs.append(QRhiVertexInputAttribute(0, 0, QRhiVertexInputAttribute::Float3, POS_OFF));
                bindings.append(QRhiVertexInputBinding(3 * sizeof(Vector4F), QRhiVertexInputBinding::PerInstance));
                attrs.append(QRhiVertexInputAttribute(1, 3, QRhiVertexInputAttribute::Float4, 0));
                bindings.append(QRhiVertexInputBinding(3 * sizeof(Vector4F), QRhiVertexInputBinding::PerInstance));
                attrs.append(QRhiVertexInputAttribute(2, 4, QRhiVertexInputAttribute::Float4, 0));
                bindings.append(QRhiVertexInputBinding(3 * sizeof(Vector4F), QRhiVertexInputBinding::PerInstance));
                attrs.append(QRhiVertexInputAttribute(3, 5, QRhiVertexInputAttribute::Float4, 0));
            }
        }
        else {
            // Visual non-SSBO paths.
            const bool hasInstColors = (variant == ShaderVariant::MeshInstancedColors
                                     || variant == ShaderVariant::MeshInstancedColorsOITAccum
                                     || variant == ShaderVariant::MeshInstancedColorsOITReveal);
            const bool isInstancedVariant = (variant == ShaderVariant::MeshInstanced
                                          || variant == ShaderVariant::MeshInstancedOITAccum
                                          || variant == ShaderVariant::MeshInstancedOITReveal
                                          || hasInstColors);

            // Per-vertex: position (loc 0), normal (loc 1), color (loc 2).
            bindings.append(QRhiVertexInputBinding(RV_STRIDE, QRhiVertexInputBinding::PerVertex));
            attrs.append(QRhiVertexInputAttribute(0, 0, QRhiVertexInputAttribute::Float3, POS_OFF));
            bindings.append(QRhiVertexInputBinding(RV_STRIDE, QRhiVertexInputBinding::PerVertex));
            attrs.append(QRhiVertexInputAttribute(1, 1, QRhiVertexInputAttribute::Float3, NRM_OFF));
            bindings.append(QRhiVertexInputBinding(RV_STRIDE, QRhiVertexInputBinding::PerVertex));
            attrs.append(QRhiVertexInputAttribute(2, 2, QRhiVertexInputAttribute::Float4, CLR_OFF));

            if(isInstancedVariant) {
                // Per-instance TM rows at locations 3/4/5 (three separate bindings sharing the TM buffer).
                bindings.append(QRhiVertexInputBinding(3 * sizeof(Vector4F), QRhiVertexInputBinding::PerInstance));
                attrs.append(QRhiVertexInputAttribute(3, 3, QRhiVertexInputAttribute::Float4, 0));
                bindings.append(QRhiVertexInputBinding(3 * sizeof(Vector4F), QRhiVertexInputBinding::PerInstance));
                attrs.append(QRhiVertexInputAttribute(4, 4, QRhiVertexInputAttribute::Float4, 0));
                bindings.append(QRhiVertexInputBinding(3 * sizeof(Vector4F), QRhiVertexInputBinding::PerInstance));
                attrs.append(QRhiVertexInputAttribute(5, 5, QRhiVertexInputAttribute::Float4, 0));

                if(hasInstColors) {
                    bindings.append(QRhiVertexInputBinding(sizeof(ColorAF), QRhiVertexInputBinding::PerInstance));
                    attrs.append(QRhiVertexInputAttribute(6, 6, QRhiVertexInputAttribute::Float4, 0));
                }
            }
            // MeshColorMapping uses same position/normal/color layout as Mesh but
            // the frag shader reads from the colorMap texture instead.
        }

        inputLayout.setBindings(bindings.cbegin(), bindings.cend());
        inputLayout.setAttributes(attrs.cbegin(), attrs.cend());

        // Build pipeline.
        std::unique_ptr<QRhiGraphicsPipeline> pipeline(rhi()->newGraphicsPipeline());
        pipeline->setShaderStages({
            { QRhiShaderStage::Vertex,   vs },
            { QRhiShaderStage::Fragment, fs },
        });
        pipeline->setVertexInputLayout(inputLayout);
        pipeline->setTopology(topology);
        pipeline->setDepthTest(flags.testFlag(PipelineFlag::DepthTest));
        pipeline->setDepthWrite(flags.testFlag(PipelineFlag::DepthWrite));
        pipeline->setCullMode(flags.testFlag(PipelineFlag::CullBack)
                              ? QRhiGraphicsPipeline::Back
                              : QRhiGraphicsPipeline::None);

        if(flags.testFlag(PipelineFlag::DepthBias)) {
            pipeline->setDepthBias(1);
            pipeline->setSlopeScaledDepthBias(1.0f);
        }
        else if(flags.testFlag(PipelineFlag::NegativeDepthBias)) {
            pipeline->setDepthBias(-5);
            pipeline->setSlopeScaledDepthBias(-5.0f);
        }

        const bool isOITAccum     = flags.testFlag(PipelineFlag::OITAccum);
        const bool isOITReveal    = flags.testFlag(PipelineFlag::OITReveal);
        const bool isDepthOnly    = flags.testFlag(PipelineFlag::DepthOnly);
        const bool isOITDepthPrime = flags.testFlag(PipelineFlag::OITDepthPrime);

        if(isDepthOnly) {
            pipeline->setTargetBlends({});  // No color attachments in depth-only pass.
        }
        else if(isOITDepthPrime) {
            // OIT depth prime: write opaque mesh depths into the OIT depth buffer
            // while suppressing all color writes so previously accumulated transparent
            // geometry contributions in accumTexture are not overwritten.
            QRhiGraphicsPipeline::TargetBlend tb;
            tb.enable = false;
            tb.colorWrite = QRhiGraphicsPipeline::ColorMask(0);
            pipeline->setTargetBlends({ tb });
        }
        else if(flags.testFlag(PipelineFlag::Blend) && !isOITAccum && !isOITReveal) {
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
        else if(isPicking) {
            QRhiGraphicsPipeline::TargetBlend tb;
            tb.enable = false;
            pipeline->setTargetBlends({ tb, tb });
        }
        // else: opaque (no blend override, default is no blending)

        // Shader resource bindings for pipeline layout compatibility.
        QRhiShaderResourceBindings* bindingsForPipeline;
        if(isSortedSSBO && dc.sortedInstanceBindings)
            bindingsForPipeline = dc.sortedInstanceBindings.get();
        else if(dc.isPseudoColor && dc.pseudoColorBindings && !isWireframe)
            bindingsForPipeline = dc.pseudoColorBindings.get();
        else
            bindingsForPipeline = ensureShaderResourceBindings();

        if(!bindingsForPipeline)
            return {};

        pipeline->setShaderResourceBindings(bindingsForPipeline);
        pipeline->setRenderPassDescriptor(rpd);

        if(!pipeline->create()) {
            service()->reportWarning("MeshPrimitiveRenderer: Failed to create graphics pipeline.");
            return {};
        }
        return pipeline;
    });
}

}   // End of namespace
