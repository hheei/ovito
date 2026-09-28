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
#include "ParticlePrimitiveRenderer.h"
#include "StandardRendererImplementation.h"

namespace Ovito {

/// Returns the bytes consumed per vertex in Metal's TBDR parameter buffer for each particle shader variant.
/// Values measured from the `out` varying declarations in the geometry shader snippets plus gl_Position (16B).
static size_t metalBytesPerVertex(ParticlePrimitiveRenderer::ShaderVariant v)
{
    using SV = ParticlePrimitiveRenderer::ShaderVariant;
    switch(v) {
        // RaycastSphere: vec4+vec3+float+vec3+vec3+uint + gl_Position = 76B
        case SV::RaycastSphere:
        case SV::RaycastSpherePicking:
        case SV::RaycastSphereSorted:
            return 76;
        // ImposterSphereWithDepth: vec4+vec2+vec2+uint + gl_Position = 52B
        case SV::ImposterSphereWithDepth:
        case SV::ImposterSphereWithDepthPicking:
        case SV::ImposterSphereWithDepthSorted:
            return 52;
        // ImposterSphereNoDepth/FlatShading: vec4+vec2+vec2 + gl_Position = 44B (no uint instanceIndex)
        case SV::ImposterSphereNoDepth:
        case SV::ImposterSphereNoDepthPicking:
        case SV::ImposterSphereNoDepthSorted:
        case SV::ImposterSphereFlatShading:
        case SV::ImposterSphereFlatShadingSorted:
            return 44;
        // SquareBillboard: vec4+vec2+vec2 + gl_Position = 36B (compact layout)
        case SV::SquareBillboard:
        case SV::SquareBillboardPicking:
        case SV::SquareBillboardSorted:
            return 36;
        // CubeMesh / BoxOrientedMesh: 14-vert box, 48B/vert
        case SV::CubeMesh:
        case SV::CubeMeshPicking:
        case SV::CubeMeshSorted:
        case SV::BoxOrientedMesh:
        case SV::BoxOrientedMeshPicking:
        case SV::BoxOrientedMeshSorted:
            return 48;
        // EllipsoidRaycast: vec4+2×mat3+3×vec3+uint + gl_Position = 144B
        case SV::EllipsoidRaycast:
        case SV::EllipsoidRaycastPicking:
        case SV::EllipsoidRaycastSorted:
            return 144;
        // SuperquadricRaycast: vec4+mat3+vec3+vec2+2×vec3+uint + gl_Position = 116B
        case SV::SuperquadricRaycast:
        case SV::SuperquadricRaycastPicking:
        case SV::SuperquadricRaycastSorted:
            return 116;
        default:
            return 64; // Conservative fallback.
    }
}

size_t ParticlePrimitiveRenderer::metalParamBufferUsage() const {
    size_t total = 0;
    for(const DrawCall& dc : _drawCalls)
        total += metalBytesPerVertex(dc.shader) * dc.particleCount * dc.verticesPerInstance;
    return total;
}

/// Must match the std140 'DrawParams' UBO layout in the particle shaders.
struct DrawParamsData {
    Matrix4F modelViewMatrix;
    ColorAF selectionColor;
    float uniformRadius;
    uint32_t pickingBaseObjectId;
    float uniformModelScale; // = length(modelViewMatrix[0].xyz), precomputed on CPU
    float _pad1; // Padding to ensure 16-byte alignment of the struct size.
};
static_assert(sizeof(DrawParamsData) == 96);

/// Must match the std140 'SortParams' UBO in particle_compute_distances.comp.
/// std140 layout: vec3 has 16-byte alignment, so _pad0 fills the gap after viewDir.
struct SortParamsData {
    float viewDirX, viewDirY, viewDirZ;
    float _pad0;          // std140 vec3 alignment padding (offset 12 -> next member at 16)
    uint32_t count;       // offset 16: actual particle count
    uint32_t paddedCount; // offset 20: power-of-two padded count (sort array size)
    uint32_t _pad1[2];    // pad to multiple of 16 bytes (std140 block alignment)
};
static_assert(sizeof(SortParamsData) == 32);

/// Must match the std140 'SortStep' UBO in particle_bitonic_sort.comp.
struct SortStepData {
    uint32_t step;
    uint32_t stage;
    uint32_t paddedCount;
    uint32_t _pad;
};
static_assert(sizeof(SortStepData) == 16);

/******************************************************************************
* Quantizes a view direction for cache key purposes.
* Reduces the frequency of re-sorts during interactive camera rotation.
******************************************************************************/
static Vector3F coarsenViewDir(const Vector3F& dir, bool isInteractive)
{
    if(!isInteractive)
        return dir;
    // Round each component to the nearest 0.5 to create a coarser grid.
    return Vector3F(
        std::round(dir.x() * 2.0f) * 0.5f,
        std::round(dir.y() * 2.0f) * 0.5f,
        std::round(dir.z() * 2.0f) * 0.5f
    );
}

/******************************************************************************
* Determines the shader variant for a given particle primitive.
******************************************************************************/
ParticlePrimitiveRenderer::ShaderVariant ParticlePrimitiveRenderer::selectShaderVariant(const ParticlePrimitive& primitive, bool isPicking) const
{
    switch(primitive.particleShape()) {
        case ParticlePrimitive::SphericalShape:
            // FlatShading → circle billboard (no-depth imposter reused).
            if(primitive.shadingMode() != ParticlePrimitive::NormalShading)
                return isPicking ? ShaderVariant::ImposterSphereNoDepthPicking : ShaderVariant::ImposterSphereFlatShading;
            switch(primitive.renderingQuality()) {
                case ParticlePrimitive::HighQuality:
                    return isPicking ? ShaderVariant::RaycastSpherePicking : ShaderVariant::RaycastSphere;
                case ParticlePrimitive::MediumQuality:
                    return isPicking ? ShaderVariant::ImposterSphereWithDepthPicking : ShaderVariant::ImposterSphereWithDepth;
                case ParticlePrimitive::LowQuality:
                    return isPicking ? ShaderVariant::ImposterSphereNoDepthPicking : ShaderVariant::ImposterSphereNoDepth;
                default:
                    return isPicking ? ShaderVariant::ImposterSphereWithDepthPicking : ShaderVariant::ImposterSphereWithDepth;
            }
        case ParticlePrimitive::SquareCubicShape:
            if(primitive.shadingMode() == ParticlePrimitive::FlatShading)
                return isPicking ? ShaderVariant::SquareBillboardPicking : ShaderVariant::SquareBillboard;
            return isPicking ? ShaderVariant::CubeMeshPicking : ShaderVariant::CubeMesh;
        case ParticlePrimitive::BoxShape:
            // FlatShading for BoxShape is not rendered (mirrors legacy behavior).
            if(primitive.shadingMode() != ParticlePrimitive::NormalShading)
                return isPicking ? ShaderVariant::ImposterSphereNoDepthPicking : ShaderVariant::ImposterSphereNoDepth;
            return isPicking ? ShaderVariant::BoxOrientedMeshPicking : ShaderVariant::BoxOrientedMesh;
        case ParticlePrimitive::EllipsoidShape:
            return isPicking ? ShaderVariant::EllipsoidRaycastPicking : ShaderVariant::EllipsoidRaycast;
        case ParticlePrimitive::SuperquadricShape:
            return isPicking ? ShaderVariant::SuperquadricRaycastPicking : ShaderVariant::SuperquadricRaycast;
        default:
            return isPicking ? ShaderVariant::ImposterSphereNoDepthPicking : ShaderVariant::ImposterSphereNoDepth;
    }
}

/******************************************************************************
* Returns the SSBO-based sorted variant of a non-picking shader variant.
******************************************************************************/
ParticlePrimitiveRenderer::ShaderVariant ParticlePrimitiveRenderer::toSortedVariant(ShaderVariant v)
{
    switch(v) {
        case ShaderVariant::RaycastSphere:           return ShaderVariant::RaycastSphereSorted;
        case ShaderVariant::ImposterSphereWithDepth: return ShaderVariant::ImposterSphereWithDepthSorted;
        case ShaderVariant::ImposterSphereNoDepth:   return ShaderVariant::ImposterSphereNoDepthSorted;
        case ShaderVariant::ImposterSphereFlatShading: return ShaderVariant::ImposterSphereFlatShadingSorted;
        case ShaderVariant::CubeMesh:                return ShaderVariant::CubeMeshSorted;
        case ShaderVariant::SquareBillboard:         return ShaderVariant::SquareBillboardSorted;
        case ShaderVariant::BoxOrientedMesh:         return ShaderVariant::BoxOrientedMeshSorted;
        case ShaderVariant::EllipsoidRaycast:        return ShaderVariant::EllipsoidRaycastSorted;
        case ShaderVariant::SuperquadricRaycast:     return ShaderVariant::SuperquadricRaycastSorted;
        default:                                     return v;
    }
}

/******************************************************************************
* Phase 1: Builds draw calls from particle primitives.
******************************************************************************/
void ParticlePrimitiveRenderer::buildDrawCalls(const ParticlePrimitive& primitive, const FrameGraph::RenderingCommand& command,
                                               bool isPickingPass, ObjectPickingMap* pickingMap,
                                               FrameGraph::RenderLayerType layer, bool filteredOut)
{
    // Skip empty primitives.
    if(!primitive.positions() || primitive.positions()->size() == 0)
        return;

    // BoxShape + FlatShading is not a supported combination.
    if(primitive.particleShape() == ParticlePrimitive::BoxShape && primitive.shadingMode() != ParticlePrimitive::NormalShading)
        return;

    DrawCall& dc = _drawCalls.emplace_back();
    dc.primitive = &primitive;
    dc.layer = layer;
    dc.modelWorldTM = command.modelWorldTM();
    dc.shader = selectShaderVariant(primitive, isPickingPass);
    dc.particleCount = primitive.positions()->size();

    // Mesh-based shapes use 14-vertex triangle strips; billboard/raycast use 4-vertex quads.
    const auto shape = primitive.particleShape();
    if(shape == ParticlePrimitive::SquareCubicShape && primitive.shadingMode() == ParticlePrimitive::NormalShading)
        dc.verticesPerInstance = 14;
    else if(shape == ParticlePrimitive::BoxShape || shape == ParticlePrimitive::EllipsoidShape || shape == ParticlePrimitive::SuperquadricShape)
        dc.verticesPerInstance = 14;
    else
        dc.verticesPerInstance = 4;

    // Determine if this draw call has transparency.
    dc.hasTransparency = !isPickingPass && primitive.transparencies();

    // Store whether this command is excluded from the depth-aware outline pre-pass.
    dc.excludeFromOutline = command.excludeFromOutline();
    dc.filteredOut = filteredOut;

    // The maximum number of particles that can be rendered by QRhi at a time.
    // This is limited by the per-particle data size and QRhiBuffers being restricted to 2^32 bytes.
    const size_t renderLimit = (size_t)std::numeric_limits<uint32_t>::max() / (3 * sizeof(Point3F));
    const size_t totalParticleCount = primitive.positions()->size();
    OVITO_ASSERT(totalParticleCount <= (size_t)std::numeric_limits<int>::max());
    if(totalParticleCount <= renderLimit) {
        // Generate a range of object picking IDs.
        if(isPickingPass && pickingMap) {
            dc.objectId = pickingMap->registerObjectId(rt()->objectIdAllocator().allocate(), command);
        }
    }
    else {
        // For very large numbers of particles, we render the data in chunks.
        // Register one draw call per chunks.
        for(size_t baseIndex = 0; baseIndex < totalParticleCount; baseIndex += renderLimit) {
            // Create draw call for next chunk. Adopt most settings from previous draw call.
            if(baseIndex != 0)
                _drawCalls.push_back(_drawCalls.back());
            DrawCall& dc2 = _drawCalls.back();

            // Calculate size of the current chunk.
            dc2.particleCount = std::min(renderLimit, totalParticleCount - baseIndex);
            dc2.baseParticleIndex = baseIndex;

            // Generate a sub-range of object picking IDs.
            if(isPickingPass && pickingMap) {
                BufferFactory<int32_t> indices(dc2.particleCount);
                std::iota(indices.begin(), indices.end(), (int32_t)baseIndex);
                dc2.objectId = pickingMap->registerObjectId(rt()->objectIdAllocator().allocate(), command, indices.take());
            }
        }
    }
}

/******************************************************************************
* Phase 2: Uploads vertex buffers and UBOs.
******************************************************************************/
void ParticlePrimitiveRenderer::prepareResourceUpdates(QRhiResourceUpdateBatch* batch,
                                                       const ViewProjectionParameters& projParams,
                                                       QSize renderSize, bool isYUpInNDC, bool isYUpInFramebuffer,
                                                       bool isPicking, bool isInteractive)
{
    if(_drawCalls.empty())
        return;

    // Compute aligned size for one draw-params slot.
    _drawParamsAlignedSize = rhi()->ubufAligned(sizeof(DrawParamsData));

    // Create or resize the per-draw params UBO (one slot per draw call, with alignment).
    quint32 requiredSize = _drawParamsAlignedSize * static_cast<quint32>(_drawCalls.size());
    if(!_drawParamsUBO) {
        _drawParamsUBO.reset(rhi()->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, requiredSize));
        if(!_drawParamsUBO->create()) {
            rt()->reportWarning("ParticlePrimitiveRenderer: Failed to create draw params UBO.");
            _drawParamsUBO.reset();
            return;
        }
    }
    else if(_drawParamsUBO->size() < requiredSize) {
        _drawParamsUBO->setSize(requiredSize);
        if(!_drawParamsUBO->create()) {
            rt()->reportWarning("ParticlePrimitiveRenderer: Failed to resize draw params UBO.");
            _drawParamsUBO.reset();
            return;
        }
    }

    // Upload per-draw params UBO for each draw call at its aligned offset.
    for(size_t i = 0; i < _drawCalls.size(); i++) {
        const DrawCall& dc = _drawCalls[i];
        DrawParamsData drawParams;
        const auto modelViewTM = projParams.viewMatrix * dc.modelWorldTM;
        drawParams.modelViewMatrix = Matrix4F(modelViewTM.toDataType<float>());
        drawParams.selectionColor = dc.primitive->selectionColor().toDataType<float>();
        drawParams.uniformRadius = static_cast<float>(dc.primitive->uniformRadius());
        drawParams.pickingBaseObjectId = dc.objectId;
        drawParams.uniformModelScale = static_cast<float>(modelViewTM.linear().column(0).length());
        drawParams._pad1 = 0;

        quint32 offset = _drawParamsAlignedSize * static_cast<quint32>(i);
        batch->updateDynamicBuffer(_drawParamsUBO.get(), offset, sizeof(DrawParamsData), &drawParams);
    }

    // Upload per-instance data for each draw call.
    for(auto& dc : _drawCalls) {
        if(isPicking || impl()->orderIndependentTransparency() || !dc.hasTransparency) {
            uploadInstanceData(batch, dc, isPicking);
        }
        else {
            uploadInstanceDataSorted(batch, dc, isInteractive, projParams);
        }
    }
}

/******************************************************************************
* Uploads per-instance data buffers for a draw call (VBO-based, standard path).
******************************************************************************/
void ParticlePrimitiveRenderer::uploadInstanceData(QRhiResourceUpdateBatch* batch, DrawCall& dc, bool isPicking)
{
    const ParticlePrimitive& prim = *dc.primitive;
    const size_t baseIndex = dc.baseParticleIndex;
    const size_t count = dc.particleCount;

    // Upload positions (vec3, 3 components per particle).
    dc.bufs.positions = impl()->uploadDataBuffer(prim.positions(), baseIndex, count, batch);
    OVITO_ASSERT(dc.bufs.positions->size() == sizeof(float) * 3 * count);

    // Upload radii (float, 1 component per particle).
    if(prim.radii())
        dc.bufs.radii = impl()->uploadDataBuffer(prim.radii(), baseIndex, count, batch);
    else
        dc.bufs.radii = impl()->uniformValueBuffer(static_cast<float>(prim.uniformRadius()), count, batch);
    OVITO_ASSERT(dc.bufs.radii->size() == sizeof(float) * count);

    // Visual-only attributes.
    if(!isPicking) {
        // Upload colors (vec3, 3 components per particle).
        if(prim.colors())
            dc.bufs.colors = impl()->uploadDataBuffer(prim.colors(), baseIndex, count, batch);
        else
            dc.bufs.colors = impl()->uniformValueBuffer(prim.uniformColor().toDataType<float>(), count, batch);
        OVITO_ASSERT(dc.bufs.colors->size() == sizeof(float) * 3 * count);

        // Upload transparencies (float, 1 component per particle).
        if(prim.transparencies())
            dc.bufs.transparencies = impl()->uploadDataBuffer(prim.transparencies(), baseIndex, count, batch);
        else if(rhi()->isFeatureSupported(QRhi::CustomInstanceStepRate))
            dc.bufs.transparencies = impl()->uniformValueBuffer(0.0f, 1, batch);
        else
            dc.bufs.transparencies = impl()->uniformValueBuffer(0.0f, count, batch);

        // Upload selection (UNormByte, 1 byte per particle).
        if(prim.selection())
            dc.bufs.selection = impl()->uploadDataBuffer(prim.selection(), baseIndex, count, batch);
        else if(rhi()->isFeatureSupported(QRhi::CustomInstanceStepRate))
            dc.bufs.selection = impl()->uniformValueBuffer(int8_t{0}, 1, batch);
        else
            dc.bufs.selection = impl()->uniformValueBuffer(int8_t{0}, count, batch);
    }

    // Upload oriented-shape buffers (box, ellipsoid, superquadric).
    const auto shape = prim.particleShape();
    const bool isOriented = (shape == ParticlePrimitive::BoxShape
                          || shape == ParticlePrimitive::EllipsoidShape
                          || shape == ParticlePrimitive::SuperquadricShape);
    if(isOriented) {
        // asphericalShapes: vec3 per particle (principal semi-axes).
        if(prim.asphericalShapes())
            dc.bufs.asphericalShapes = impl()->uploadDataBuffer(prim.asphericalShapes(), baseIndex, count, batch);
        else
            dc.bufs.asphericalShapes = impl()->uniformValueBuffer(Vector3F(0.f, 0.f, 0.f), count, batch);
        // orientations: vec4 per particle (quaternion).
        if(prim.orientations())
            dc.bufs.orientations = impl()->uploadDataBuffer(prim.orientations(), baseIndex, count, batch);
        else
            dc.bufs.orientations = impl()->uniformValueBuffer(QuaternionF(0.f, 0.f, 0.f, 1.f), count, batch);
        // roundness: vec2 per particle (superquadric exponents), only for SuperquadricShape.
        if(shape == ParticlePrimitive::SuperquadricShape) {
            if(prim.roundness())
                dc.bufs.roundness = impl()->uploadDataBuffer(prim.roundness(), baseIndex, count, batch);
            else
                dc.bufs.roundness = impl()->uniformValueBuffer(Vector2F(1.f, 1.f), count, batch);
        }
    }
}

/******************************************************************************
* Uploads per-instance data as SSBOs for sorted rendering.
* Creates the sorted render bindings (scene/draw UBOs + 6 SSBOs).
******************************************************************************/
void ParticlePrimitiveRenderer::uploadInstanceDataSorted(QRhiResourceUpdateBatch* batch, DrawCall& dc, bool isInteractive, const ViewProjectionParameters& projParams)
{
    const ParticlePrimitive& prim = *dc.primitive;
    const size_t baseIndex = dc.baseParticleIndex;
    const size_t count = dc.particleCount;

    // Upload positions (vec3, 3 components per particle).
    dc.bufs.positions = impl()->uploadDataBuffer(prim.positions(), baseIndex, count, batch, QRhiBuffer::StorageBuffer);
    OVITO_ASSERT(dc.bufs.positions->size() == sizeof(float) * 3 * count);

    // Upload radii as SSBO.
    if(prim.radii())
        dc.bufs.radii = impl()->uploadDataBuffer(prim.radii(), baseIndex, count, batch, QRhiBuffer::StorageBuffer);
    else
        dc.bufs.radii = impl()->uniformValueBuffer(static_cast<float>(prim.uniformRadius()), count, batch, QRhiBuffer::StorageBuffer);
    OVITO_ASSERT(dc.bufs.radii->size() == sizeof(float) * count);

    // Upload colors as SSBO.
    if(prim.colors())
        dc.bufs.colors = impl()->uploadDataBuffer(prim.colors(), baseIndex, count, batch, QRhiBuffer::StorageBuffer);
    else
        dc.bufs.colors = impl()->uniformValueBuffer(prim.uniformColor().toDataType<float>(), count, batch, QRhiBuffer::StorageBuffer);
    OVITO_ASSERT(dc.bufs.colors->size() == sizeof(float) * 3 * count);

    // Upload transparencies as SSBO.
    if(prim.transparencies())
        dc.bufs.transparencies = impl()->uploadDataBuffer(prim.transparencies(), baseIndex, count, batch, QRhiBuffer::StorageBuffer);
    else
        dc.bufs.transparencies = impl()->uniformValueBuffer(0.0f, count, batch, QRhiBuffer::StorageBuffer);
    OVITO_ASSERT(dc.bufs.transparencies->size() == sizeof(float) * count);

    // Upload selection as a bit-packed SSBO: one uint32 per 32 particles.
    // GLSL std430 SSBOs have no sub-32-bit scalar type without GL_EXT_shader_8bit_storage, so we
    // cannot store the boolean flag as a single byte per particle. Expanding to one float per particle
    // (the naive approach) wastes 4× memory. Instead we pack 32 flags into each uint32_t.
    // The shader unpacks via: (ssboSelectionData[idx >> 5u] >> (idx & 31u)) & 1u.
    const size_t wordCount = (count + 31) / 32;
    if(prim.selection()) {
        dc.bufs.selection = impl()->createCachedBuffer(
            RendererResourceKey<struct SelectionSSBOCache, ConstDataBufferPtr, size_t, size_t>{prim.selection(), baseIndex, count}, batch, QRhiBuffer::StorageBuffer,
            [&]() -> QByteArray {
                BufferReadAccess<int8_t> access(prim.selection());
                const int8_t* sel = access.cbegin() + baseIndex;
                QByteArray data(static_cast<qsizetype>(wordCount * sizeof(uint32_t)), 0);
                uint32_t* words = reinterpret_cast<uint32_t*>(data.data());
                for(size_t k = 0; k < count; ++k) {
                    if(sel[k])
                        words[k >> 5] |= (1u << (k & 31));
                }
                return data;
            });
    } else {
        dc.bufs.selection = impl()->uniformValueBuffer(uint32_t{0}, wordCount, batch, QRhiBuffer::StorageBuffer);
    }

    // For painter's algorithm (non-OIT, non-picking), sort transparent draw calls back-to-front.
    // Compute the view direction in model space.
    const Matrix3F modelViewTM = (projParams.viewMatrix * dc.modelWorldTM).linear().toDataType<float>();
    const Vector3F viewDirInModelSpace = modelViewTM.inverse().column(2);
    const Vector3F coarseDir = coarsenViewDir(viewDirInModelSpace, isInteractive);

    // Compute distances and sort indices on the CPU, upload sorted indices as SSBO.
    dc.sorted.sortedIndexBuffer = impl()->createCachedBuffer(
        RendererResourceKey<struct ParticleOrderingCache, ConstDataBufferPtr, size_t, size_t, Vector3F>{dc.primitive->positions(), baseIndex, count, coarseDir}, batch, QRhiBuffer::StorageBuffer,
        [&]() -> QByteArray {
            // Compute per-particle distances (dot product with view direction) and sort ascending.
            // Ascending = smaller distance first = further from camera first = back-to-front.
            std::vector<float> distances(count);
            if(dc.primitive->positions()->dataType() == DataBuffer::Float64) {
                const auto dir = viewDirInModelSpace.toDataType<double>();
                auto posAccess = BufferReadAccess<Vector3D>(dc.primitive->positions()).subrange(baseIndex, baseIndex + count);
                std::ranges::transform(posAccess, distances.begin(),
                    [&](const Vector3D& pos) { return static_cast<float>(dir.dot(pos)); });
            }
            else {
                auto posAccess = BufferReadAccess<Vector3F>(dc.primitive->positions()).subrange(baseIndex, baseIndex + count);
                std::ranges::transform(posAccess, distances.begin(),
                    [&](const Vector3F& pos) { return viewDirInModelSpace.dot(pos); });
            }

            QByteArray bufferData(sizeof(uint32_t) * count, Qt::Uninitialized);
            std::span<uint32_t> indices(reinterpret_cast<uint32_t*>(bufferData.data()), count);
            std::iota(indices.begin(), indices.end(), 0u);
            Ovito::sort_zipped(distances, indices);
            return bufferData;
        });

    // Upload extra SSBOs for oriented shapes (box, ellipsoid, superquadric).
    const auto sortedShape = prim.particleShape();
    const bool sortedIsOriented = (sortedShape == ParticlePrimitive::BoxShape
                                || sortedShape == ParticlePrimitive::EllipsoidShape
                                || sortedShape == ParticlePrimitive::SuperquadricShape);
    if(sortedIsOriented) {
        if(prim.asphericalShapes())
            dc.bufs.asphericalShapes = impl()->uploadDataBuffer(prim.asphericalShapes(), baseIndex, count, batch, QRhiBuffer::StorageBuffer);
        else
            dc.bufs.asphericalShapes = impl()->uniformValueBuffer(Vector3F(0.f,0.f,0.f), count, batch, QRhiBuffer::StorageBuffer);
        if(prim.orientations())
            dc.bufs.orientations = impl()->uploadDataBuffer(prim.orientations(), baseIndex, count, batch, QRhiBuffer::StorageBuffer);
        else
            dc.bufs.orientations = impl()->uniformValueBuffer(QuaternionF(0.f, 0.f, 0.f, 1.f), count, batch, QRhiBuffer::StorageBuffer);
        if(sortedShape == ParticlePrimitive::SuperquadricShape) {
            if(prim.roundness())
                dc.bufs.roundness = impl()->uploadDataBuffer(prim.roundness(), baseIndex, count, batch, QRhiBuffer::StorageBuffer);
            else
                dc.bufs.roundness = impl()->uniformValueBuffer(Vector2F(1.f,1.f), count, batch, QRhiBuffer::StorageBuffer);
        }
    }

    // Always recreate the sorted render bindings with fresh buffer pointers.
    // Recreating bindings every frame is cheap: pipelines are compatible with any
    // bindings object that has the same layout, so the cached pipeline is reused.
    dc.sorted.shaderResourceBindings.reset();
    const bool sortedBuffersReady = impl()->sceneParamsUBO() && _drawParamsUBO
        && dc.bufs.positions && dc.bufs.radii && dc.bufs.colors
        && dc.bufs.transparencies && dc.bufs.selection && dc.sorted.sortedIndexBuffer
        && (!sortedIsOriented || (dc.bufs.asphericalShapes && dc.bufs.orientations))
        && (sortedShape != ParticlePrimitive::SuperquadricShape || dc.bufs.roundness);
    if(sortedBuffersReady) {
        dc.sorted.shaderResourceBindings.reset(rhi()->newShaderResourceBindings());
        QVarLengthArray<QRhiShaderResourceBinding, 12> bindings = {
            QRhiShaderResourceBinding::uniformBuffer(0,
                QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage,
                impl()->sceneParamsUBO()),
            QRhiShaderResourceBinding::uniformBufferWithDynamicOffset(1,
                QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage,
                _drawParamsUBO.get(), sizeof(DrawParamsData)),
            QRhiShaderResourceBinding::bufferLoad(2, QRhiShaderResourceBinding::VertexStage, dc.bufs.positions),
            QRhiShaderResourceBinding::bufferLoad(3, QRhiShaderResourceBinding::VertexStage, dc.bufs.radii),
            QRhiShaderResourceBinding::bufferLoad(4, QRhiShaderResourceBinding::VertexStage, dc.bufs.colors),
            QRhiShaderResourceBinding::bufferLoad(5, QRhiShaderResourceBinding::VertexStage, dc.bufs.transparencies),
            QRhiShaderResourceBinding::bufferLoad(6, QRhiShaderResourceBinding::VertexStage, dc.bufs.selection),
            QRhiShaderResourceBinding::bufferLoad(7, QRhiShaderResourceBinding::VertexStage, dc.sorted.sortedIndexBuffer),
        };
        if(sortedIsOriented) {
            bindings.append(QRhiShaderResourceBinding::bufferLoad(8, QRhiShaderResourceBinding::VertexStage, dc.bufs.asphericalShapes));
            bindings.append(QRhiShaderResourceBinding::bufferLoad(9, QRhiShaderResourceBinding::VertexStage, dc.bufs.orientations));
            if(sortedShape == ParticlePrimitive::SuperquadricShape)
                bindings.append(QRhiShaderResourceBinding::bufferLoad(10, QRhiShaderResourceBinding::VertexStage, dc.bufs.roundness));
        }
        dc.sorted.shaderResourceBindings->setBindings(bindings.cbegin(), bindings.cend());
        if(!dc.sorted.shaderResourceBindings->create()) {
            rt()->reportWarning("ParticlePrimitiveRenderer: Failed to create sorted render bindings.");
            dc.sorted.shaderResourceBindings.reset();
        }
    }
}

/******************************************************************************
* Returns true if any draw call requires transparency.
******************************************************************************/
bool ParticlePrimitiveRenderer::hasTransparentDrawCalls() const
{
    return std::ranges::any_of(_drawCalls, [](const DrawCall& dc) {
        return dc.hasTransparency && dc.layer != FrameGraph::OverLayer;
    });
}

/******************************************************************************
* Draws transparent particles into the OIT accumulation pass (additive blend).
******************************************************************************/
void ParticlePrimitiveRenderer::drawOITAccumPass(QRhiCommandBuffer* cb, QRhiRenderPassDescriptor* rpd)
{
    // Depth prime: draw opaque particles first with color writes suppressed so that
    // transparent particles behind opaque ones fail the depth test during OIT accumulation.
    draw(cb, rpd, /*opaqueOnly=*/true, /*transparentOnly=*/false, /*isPicking=*/false,
         /*oitAccum=*/false, /*oitReveal=*/false, /*oitDepthPrime=*/true);
    // OIT accumulation: draw transparent particles with additive blending.
    draw(cb, rpd, /*opaqueOnly=*/false, /*transparentOnly=*/true, /*isPicking=*/false,
         /*oitAccum=*/true, /*oitReveal=*/false);
}

/******************************************************************************
* Draws transparent particles into the OIT reveal pass (multiplicative blend).
******************************************************************************/
void ParticlePrimitiveRenderer::drawOITRevealPass(QRhiCommandBuffer* cb, QRhiRenderPassDescriptor* rpd)
{
    draw(cb, rpd, /*opaqueOnly=*/false, /*transparentOnly=*/true, /*isPicking=*/false,
         /*oitAccum=*/false, /*oitReveal=*/true);
}

/******************************************************************************
* Phase 3: Issues draw calls inside the render pass.
******************************************************************************/
void ParticlePrimitiveRenderer::draw(QRhiCommandBuffer* cb, QRhiRenderPassDescriptor* rpd,
                                     bool opaqueOnly, bool transparentOnly, bool isPicking)
{
    draw(cb, rpd, opaqueOnly, transparentOnly, isPicking, /*oitAccum=*/false, /*oitReveal=*/false);
}

/******************************************************************************
* Draws all OverLayer particles with depth testing disabled.
******************************************************************************/
void ParticlePrimitiveRenderer::drawOverlay(QRhiCommandBuffer* cb, QRhiRenderPassDescriptor* rpd)
{
    draw(cb, rpd, /*opaqueOnly=*/false, /*transparentOnly=*/false, /*isPicking=*/false,
         /*oitAccum=*/false, /*oitReveal=*/false, /*oitDepthPrime=*/false, /*overlayOnly=*/true);
}

/******************************************************************************
* Draws all HighlightLayer particles depth-test-disabled into the silhouette mask target.
******************************************************************************/
void ParticlePrimitiveRenderer::drawHighlightSilhouette(QRhiCommandBuffer* cb, QRhiRenderPassDescriptor* rpd)
{
    draw(cb, rpd, /*opaqueOnly=*/false, /*transparentOnly=*/false, /*isPicking=*/false,
         /*oitAccum=*/false, /*oitReveal=*/false, /*oitDepthPrime=*/false,
         /*overlayOnly=*/false, /*highlightSilhouetteOnly=*/true);
}

/******************************************************************************
* Draws ExcludeFromOutline SceneLayer particles into the depth-only excluded-depth pre-pass target.
******************************************************************************/
void ParticlePrimitiveRenderer::drawExclusionDepth(QRhiCommandBuffer* cb, QRhiRenderPassDescriptor* rpd)
{
    draw(cb, rpd, /*opaqueOnly=*/false, /*transparentOnly=*/false, /*isPicking=*/false,
         /*oitAccum=*/false, /*oitReveal=*/false, /*oitDepthPrime=*/false,
         /*overlayOnly=*/false, /*highlightSilhouetteOnly=*/false, /*excludedDepthOnly=*/true);
}

/******************************************************************************
* Phase 3: Issues draw calls inside the render pass.
******************************************************************************/
void ParticlePrimitiveRenderer::draw(QRhiCommandBuffer* cb, QRhiRenderPassDescriptor* rpd,
                                     bool opaqueOnly, bool transparentOnly, bool isPicking,
                                     bool oitAccum, bool oitReveal, bool oitDepthPrime,
                                     bool overlayOnly, bool highlightSilhouetteOnly,
                                     bool excludedDepthOnly)
{
    if(!impl()->sceneParamsUBO() || !_drawParamsUBO)
        return;

    for(const auto& [i, dc] : Ovito::enumerate(_drawCalls)) {

        // Filter by opacity.
        if(opaqueOnly && dc.hasTransparency)
            continue;
        if(transparentOnly && !dc.hasTransparency)
            continue;

        // Route draw calls to the correct rendering path.
        if(excludedDepthOnly) {
            // Excluded-depth pre-pass: render ONLY ExcludeFromOutline SceneLayer geometry.
            // Raytraced excluded objects (filteredOut) are handled by the raytracer's own
            // excluded-depth contribution, so they are not re-rasterized here.
            if(dc.layer != FrameGraph::SceneLayer || !dc.excludeFromOutline || dc.filteredOut) continue;
        } else if(highlightSilhouetteOnly) {
            if(dc.layer != FrameGraph::HighlightLayer) continue;
        } else {
            if(overlayOnly != (dc.layer == FrameGraph::OverLayer)) continue;
            // Skip draw calls that are handled by a parent renderer (e.g. ANARI/OSPRay).
            // They are built to populate the outline depth pre-pass, not the main color pass.
            if(dc.filteredOut) continue;
        }

        // Determine rendering mode for this draw call.
        // sorted.shaderResourceBindings being non-null implies sorted rendering is set up.
        // Overlay and highlight-silhouette draw calls never use sorted SSBO (depth is off).
        // Depth-only pass DOES use sorted SSBO when available: the color/transp/selection
        // buffers are SSBOs and cannot be bound as vertex buffers.
        bool useSortedSSBO = !overlayOnly && !highlightSilhouetteOnly && dc.hasTransparency && !impl()->orderIndependentTransparency() && !oitAccum && !oitReveal && !isPicking && dc.sorted.shaderResourceBindings;

        // Get the per-instance buffers for this draw call.
        if(!dc.bufs.positions || !dc.bufs.radii)
            continue;

        // Determine shader variant and pipeline flags.
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
        } else {
            flags.setFlag(PipelineFlag::DepthTest, true);
            if(dc.hasTransparency) {
                if(oitAccum) {
                    flags.setFlag(PipelineFlag::OITAccum, true);
                    flags.setFlag(PipelineFlag::DepthWrite, false); // WBOIT: depth test on, writes off so all transparent fragments accumulate.
                } else if(oitReveal) {
                    flags.setFlag(PipelineFlag::OITReveal, true);
                    flags.setFlag(PipelineFlag::DepthWrite, false); // WBOIT: same as accum pass.
                } else if(useSortedSSBO) {
                    flags.setFlag(PipelineFlag::Blend, true);     // Standard alpha blend (sorted back-to-front).
                    flags.setFlag(PipelineFlag::DepthWrite, true);
                    flags.setFlag(PipelineFlag::SortedSSBO, true);
                } else {
                    flags.setFlag(PipelineFlag::Blend, true);
                    flags.setFlag(PipelineFlag::DepthWrite, false);
                }
            } else {
                flags.setFlag(PipelineFlag::DepthWrite, true);
            }
        }
        flags.setFlag(PipelineFlag::NoTransparency, !dc.primitive->transparencies() || isPicking || excludedDepthOnly);
        flags.setFlag(PipelineFlag::NoSelection, !dc.primitive->selection() || isPicking || excludedDepthOnly);
        flags.setFlag(PipelineFlag::OITDepthPrime, oitDepthPrime);

        QRhiGraphicsPipeline* pipeline = ensurePipeline(rpd, variant, flags, dc);
        if(!pipeline)
            continue;

        QRhiCommandBuffer::DynamicOffset dynOffset(1, _drawParamsAlignedSize * static_cast<quint32>(i));

        // Determine whether this shape needs oriented (asphericalShapes+orientation) or superquadric (+roundness) buffers.
        using PS = ParticlePrimitive::ParticleShape;
        const PS drawShape = dc.primitive->particleShape();
        const bool drawIsOriented = (drawShape == PS::BoxShape || drawShape == PS::EllipsoidShape);
        const bool drawIsSuperquadric = (drawShape == PS::SuperquadricShape);

        if(useSortedSSBO) {
            // Sorted rendering: use SSBO-based bindings, no vertex buffer inputs.
            QRhiShaderResourceBindings* sortedBindings = dc.sorted.shaderResourceBindings.get();
            cb->setGraphicsPipeline(pipeline);
            cb->setShaderResources(sortedBindings, 1, &dynOffset);
            cb->setVertexInput(0, 0, nullptr);
            cb->draw(dc.verticesPerInstance, static_cast<quint32>(dc.particleCount), 0, 0);
        } else {
            // Standard VBO-based rendering.
            QRhiShaderResourceBindings* bindings = ensureShaderResourceBindings();
            if(!bindings) continue;

            cb->setGraphicsPipeline(pipeline);
            cb->setShaderResources(bindings, 1, &dynOffset);

            if(isPicking) {
                if(drawIsSuperquadric) {
                    const QRhiCommandBuffer::VertexInput vbufBindings[] = {
                        { dc.bufs.positions, 0 }, { dc.bufs.radii, 0 },
                        { dc.bufs.asphericalShapes, 0 }, { dc.bufs.orientations, 0 },
                        { dc.bufs.roundness, 0 },
                    };
                    cb->setVertexInput(0, 5, vbufBindings);
                } else if(drawIsOriented) {
                    const QRhiCommandBuffer::VertexInput vbufBindings[] = {
                        { dc.bufs.positions, 0 }, { dc.bufs.radii, 0 },
                        { dc.bufs.asphericalShapes, 0 }, { dc.bufs.orientations, 0 },
                    };
                    cb->setVertexInput(0, 4, vbufBindings);
                } else {
                    const QRhiCommandBuffer::VertexInput vbufBindings[] = {
                        { dc.bufs.positions, 0 }, { dc.bufs.radii, 0 },
                    };
                    cb->setVertexInput(0, 2, vbufBindings);
                }
            } else {
                if(drawIsSuperquadric) {
                    const QRhiCommandBuffer::VertexInput vbufBindings[] = {
                        { dc.bufs.positions, 0 }, { dc.bufs.radii, 0 },
                        { dc.bufs.colors, 0 }, { dc.bufs.transparencies, 0 }, { dc.bufs.selection, 0 },
                        { dc.bufs.asphericalShapes, 0 }, { dc.bufs.orientations, 0 },
                        { dc.bufs.roundness, 0 },
                    };
                    cb->setVertexInput(0, 8, vbufBindings);
                } else if(drawIsOriented) {
                    const QRhiCommandBuffer::VertexInput vbufBindings[] = {
                        { dc.bufs.positions, 0 }, { dc.bufs.radii, 0 },
                        { dc.bufs.colors, 0 }, { dc.bufs.transparencies, 0 }, { dc.bufs.selection, 0 },
                        { dc.bufs.asphericalShapes, 0 }, { dc.bufs.orientations, 0 },
                    };
                    cb->setVertexInput(0, 7, vbufBindings);
                } else {
                    const QRhiCommandBuffer::VertexInput vbufBindings[] = {
                        { dc.bufs.positions, 0 }, { dc.bufs.radii, 0 },
                        { dc.bufs.colors, 0 }, { dc.bufs.transparencies, 0 }, { dc.bufs.selection, 0 },
                    };
                    cb->setVertexInput(0, 5, vbufBindings);
                }
            }
            cb->draw(dc.verticesPerInstance, static_cast<quint32>(dc.particleCount), 0, 0);
        }
    }
}

/******************************************************************************
* Clears the draw call list for the next frame.
******************************************************************************/
void ParticlePrimitiveRenderer::clear()
{
    _drawCalls.clear();
}

/******************************************************************************
* Ensures the standard shader resource bindings are created.
******************************************************************************/
QRhiShaderResourceBindings* ParticlePrimitiveRenderer::ensureShaderResourceBindings()
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
                _drawParamsUBO.get(), sizeof(DrawParamsData)),
        });
        if(!_bindings->create()) {
            rt()->reportWarning("ParticlePrimitiveRenderer: Failed to create shader resource bindings.");
            _bindings.reset();
        }
    }
    return _bindings.get();
}

/******************************************************************************
* Ensures a graphics pipeline is valid for the given configuration.
******************************************************************************/
QRhiGraphicsPipeline* ParticlePrimitiveRenderer::ensurePipeline(QRhiRenderPassDescriptor* rpd, ShaderVariant variant, PipelineFlags flags, const DrawCall& dc)
{
    // Lookup key for cached pipelines.
    struct PipelineCacheKey {
        ShaderVariant variant;
        PipelineFlags flags;
        bool operator==(const PipelineCacheKey& other) const = default;
    };

    return rt()->ensureGraphicsPipeline(rpd, PipelineCacheKey{variant, flags}, [&]() -> std::unique_ptr<QRhiGraphicsPipeline> {
        if(!impl()->sceneParamsUBO() || !_drawParamsUBO)
            return {};

        // Select shader files based on variant and pipeline flags.
        QString vsPath, fsPath;
        bool isPicking    = false;
        bool isSortedSSBO = flags.testFlag(PipelineFlag::SortedSSBO);
        bool isOITAccum   = flags.testFlag(PipelineFlag::OITAccum);
        bool isOITReveal  = flags.testFlag(PipelineFlag::OITReveal);
        bool isDepthOnly  = flags.testFlag(PipelineFlag::DepthOnly);

        static const QLatin1String BASE(":/ovito/core/rendering/standard/shaders/particles/variants/");

        // Helper: select the appropriate FS path based on pipeline flags.
        // Depth-only pass uses a no-color-output variant to avoid D3D12 warning #679.
        auto selectFrag = [&](QLatin1String stem) {
            if(isDepthOnly) return BASE + stem + QLatin1String("__depth_only.frag.qsb");
            if(isOITAccum)  return BASE + stem + QLatin1String("__oit_accum.frag.qsb");
            if(isOITReveal) return BASE + stem + QLatin1String("__oit_reveal.frag.qsb");
            return BASE + stem + QLatin1String("__color.frag.qsb");
        };
        auto selectFragFlat = [&](QLatin1String stem) {
            if(isDepthOnly) return BASE + stem + QLatin1String("__depth_only.frag.qsb");
            if(isOITAccum)  return BASE + stem + QLatin1String("__oit_accum.frag.qsb");
            if(isOITReveal) return BASE + stem + QLatin1String("__oit_reveal.frag.qsb");
            return BASE + stem + QLatin1String("__color.frag.qsb");
        };
        (void)selectFragFlat;

        // Track whether this is an oriented or superquadric shape variant (affects vertex layout).
        enum class VertexLayout { Spherical, Oriented, Superquadric } vertexLayout = VertexLayout::Spherical;

        switch(variant) {
            // ---- SphericalShape ----
            case ShaderVariant::RaycastSphere:
            case ShaderVariant::RaycastSphereSorted:
                vsPath = BASE + (isSortedSSBO
                    ? QLatin1String("sphere_raycast__ssbo_sorted.vert.qsb")
                    : QLatin1String("sphere_raycast__vbo_visual.vert.qsb"));
                fsPath = selectFrag(QLatin1String("sphere_raycast"));
                break;
            case ShaderVariant::RaycastSpherePicking:
                vsPath = BASE + QLatin1String("sphere_raycast__vbo_picking.vert.qsb");
                fsPath = BASE + QLatin1String("sphere_raycast__picking.frag.qsb");
                isPicking = true;
                break;
            case ShaderVariant::ImposterSphereWithDepth:
            case ShaderVariant::ImposterSphereWithDepthSorted:
                vsPath = BASE + (isSortedSSBO
                    ? QLatin1String("sphere_imposter_wd__ssbo_sorted.vert.qsb")
                    : QLatin1String("sphere_imposter_wd__vbo_visual.vert.qsb"));
                fsPath = selectFrag(QLatin1String("sphere_imposter_wd"));
                break;
            case ShaderVariant::ImposterSphereWithDepthPicking:
                vsPath = BASE + QLatin1String("sphere_imposter_wd__vbo_picking.vert.qsb");
                fsPath = BASE + QLatin1String("sphere_imposter_wd__picking.frag.qsb");
                isPicking = true;
                break;
            case ShaderVariant::ImposterSphereNoDepth:
            case ShaderVariant::ImposterSphereNoDepthSorted:
                vsPath = BASE + (isSortedSSBO
                    ? QLatin1String("sphere_imposter_nd__ssbo_sorted.vert.qsb")
                    : QLatin1String("sphere_imposter_nd__vbo_visual.vert.qsb"));
                if(isOITAccum)       fsPath = BASE + QLatin1String("sphere_imposter_nd__oit_accum.frag.qsb");
                else if(isOITReveal) fsPath = BASE + QLatin1String("sphere_imposter_nd__oit_reveal.frag.qsb");
                else                 fsPath = BASE + QLatin1String("sphere_imposter_nd__color.frag.qsb");
                break;
            case ShaderVariant::ImposterSphereFlatShading:
            case ShaderVariant::ImposterSphereFlatShadingSorted:
                vsPath = BASE + (isSortedSSBO
                    ? QLatin1String("sphere_imposter_nd__ssbo_sorted.vert.qsb")
                    : QLatin1String("sphere_imposter_nd__vbo_visual.vert.qsb"));
                if(isDepthOnly)      fsPath = BASE + QLatin1String("sphere_imposter_nd__depth_only.frag.qsb");
                else if(isOITAccum)  fsPath = BASE + QLatin1String("sphere_imposter_flat__oit_accum.frag.qsb");
                else if(isOITReveal) fsPath = BASE + QLatin1String("sphere_imposter_flat__oit_reveal.frag.qsb");
                else                 fsPath = BASE + QLatin1String("sphere_imposter_flat__color.frag.qsb");
                break;
            case ShaderVariant::ImposterSphereNoDepthPicking:
                vsPath = BASE + QLatin1String("sphere_imposter_nd__vbo_picking.vert.qsb");
                fsPath = BASE + QLatin1String("sphere_imposter_nd__picking.frag.qsb");
                isPicking = true;
                break;
            // ---- SquareCubicShape – NormalShading (cube mesh) ----
            case ShaderVariant::CubeMesh:
            case ShaderVariant::CubeMeshSorted:
                vsPath = BASE + (isSortedSSBO
                    ? QLatin1String("cube_mesh__ssbo_sorted.vert.qsb")
                    : QLatin1String("cube_mesh__vbo_visual.vert.qsb"));
                fsPath = selectFrag(QLatin1String("cube_mesh"));
                break;
            case ShaderVariant::CubeMeshPicking:
                vsPath = BASE + QLatin1String("cube_mesh__vbo_picking.vert.qsb");
                fsPath = BASE + QLatin1String("cube_mesh__picking.frag.qsb");
                isPicking = true;
                break;
            // ---- SquareCubicShape – FlatShading (square billboard) ----
            case ShaderVariant::SquareBillboard:
            case ShaderVariant::SquareBillboardSorted:
                vsPath = BASE + (isSortedSSBO
                    ? QLatin1String("square_billboard__ssbo_sorted.vert.qsb")
                    : QLatin1String("square_billboard__vbo_visual.vert.qsb"));
                if(isOITAccum)       fsPath = BASE + QLatin1String("square_billboard__oit_accum.frag.qsb");
                else if(isOITReveal) fsPath = BASE + QLatin1String("square_billboard__oit_reveal.frag.qsb");
                else                 fsPath = BASE + QLatin1String("square_billboard__color.frag.qsb");
                break;
            case ShaderVariant::SquareBillboardPicking:
                vsPath = BASE + QLatin1String("square_billboard__vbo_picking.vert.qsb");
                fsPath = BASE + QLatin1String("square_billboard__picking.frag.qsb");
                isPicking = true;
                break;
            // ---- BoxShape – NormalShading (oriented box mesh) ----
            case ShaderVariant::BoxOrientedMesh:
            case ShaderVariant::BoxOrientedMeshSorted:
                vsPath = BASE + (isSortedSSBO
                    ? QLatin1String("box_mesh__ssbo_sorted.vert.qsb")
                    : QLatin1String("box_mesh__vbo_visual.vert.qsb"));
                fsPath = selectFrag(QLatin1String("box_mesh"));
                vertexLayout = VertexLayout::Oriented;
                break;
            case ShaderVariant::BoxOrientedMeshPicking:
                vsPath = BASE + QLatin1String("box_mesh__vbo_picking.vert.qsb");
                fsPath = BASE + QLatin1String("box_mesh__picking.frag.qsb");
                isPicking = true;
                vertexLayout = VertexLayout::Oriented;
                break;
            // ---- EllipsoidShape ----
            case ShaderVariant::EllipsoidRaycast:
            case ShaderVariant::EllipsoidRaycastSorted:
                vsPath = BASE + (isSortedSSBO
                    ? QLatin1String("ellipsoid_raycast__ssbo_sorted.vert.qsb")
                    : QLatin1String("ellipsoid_raycast__vbo_visual.vert.qsb"));
                fsPath = selectFrag(QLatin1String("ellipsoid_raycast"));
                vertexLayout = VertexLayout::Oriented;
                break;
            case ShaderVariant::EllipsoidRaycastPicking:
                vsPath = BASE + QLatin1String("ellipsoid_raycast__vbo_picking.vert.qsb");
                fsPath = BASE + QLatin1String("ellipsoid_raycast__picking.frag.qsb");
                isPicking = true;
                vertexLayout = VertexLayout::Oriented;
                break;
            // ---- SuperquadricShape ----
            case ShaderVariant::SuperquadricRaycast:
            case ShaderVariant::SuperquadricRaycastSorted:
                vsPath = BASE + (isSortedSSBO
                    ? QLatin1String("superquadric_raycast__ssbo_sorted.vert.qsb")
                    : QLatin1String("superquadric_raycast__vbo_visual.vert.qsb"));
                fsPath = selectFrag(QLatin1String("superquadric_raycast"));
                vertexLayout = VertexLayout::Superquadric;
                break;
            case ShaderVariant::SuperquadricRaycastPicking:
                vsPath = BASE + QLatin1String("superquadric_raycast__vbo_picking.vert.qsb");
                fsPath = BASE + QLatin1String("superquadric_raycast__picking.frag.qsb");
                isPicking = true;
                vertexLayout = VertexLayout::Superquadric;
                break;
            default:
                return {};
        }

        QShader vs = rt()->loadShader(vsPath);
        QShader fs = rt()->loadShader(fsPath);
        if(!vs.isValid() || !fs.isValid())
            return {};

        // Build vertex input layout.
        QRhiVertexInputLayout inputLayout;
        if(!isSortedSSBO) {
            QVector<QRhiVertexInputBinding> inputBindings;
            QVector<QRhiVertexInputAttribute> attributes;

            // Common head: position (vec3) + radius (float).
            inputBindings.append(QRhiVertexInputBinding(3 * sizeof(float), QRhiVertexInputBinding::PerInstance));
            attributes.append(QRhiVertexInputAttribute(0, 0, QRhiVertexInputAttribute::Float3, 0));
            inputBindings.append(QRhiVertexInputBinding(sizeof(float), QRhiVertexInputBinding::PerInstance));
            attributes.append(QRhiVertexInputAttribute(1, 1, QRhiVertexInputAttribute::Float, 0));

            if(!isPicking) {
                // Visual path: color (vec3), transparency (float), selection (UNormByte).
                inputBindings.append(QRhiVertexInputBinding(3 * sizeof(float), QRhiVertexInputBinding::PerInstance));
                attributes.append(QRhiVertexInputAttribute(2, 2, QRhiVertexInputAttribute::Float3, 0));
                quint32 transpRate = (flags.testFlag(PipelineFlag::NoTransparency) && rhi()->isFeatureSupported(QRhi::CustomInstanceStepRate)) ? std::numeric_limits<quint32>::max() : 1;
                inputBindings.append(QRhiVertexInputBinding(sizeof(float), QRhiVertexInputBinding::PerInstance, transpRate));
                attributes.append(QRhiVertexInputAttribute(3, 3, QRhiVertexInputAttribute::Float, 0));
                quint32 selRate = (flags.testFlag(PipelineFlag::NoSelection) && rhi()->isFeatureSupported(QRhi::CustomInstanceStepRate)) ? std::numeric_limits<quint32>::max() : 1;
                inputBindings.append(QRhiVertexInputBinding(sizeof(int8_t), QRhiVertexInputBinding::PerInstance, selRate));
                attributes.append(QRhiVertexInputAttribute(4, 4, QRhiVertexInputAttribute::UNormByte, 0));
                // Extra oriented/superquadric buffers follow (next location is 5 for visual).
                if(vertexLayout == VertexLayout::Oriented || vertexLayout == VertexLayout::Superquadric) {
                    inputBindings.append(QRhiVertexInputBinding(3 * sizeof(float), QRhiVertexInputBinding::PerInstance));
                    attributes.append(QRhiVertexInputAttribute(5, 5, QRhiVertexInputAttribute::Float3, 0)); // asphericalShapes
                    inputBindings.append(QRhiVertexInputBinding(4 * sizeof(float), QRhiVertexInputBinding::PerInstance));
                    attributes.append(QRhiVertexInputAttribute(6, 6, QRhiVertexInputAttribute::Float4, 0)); // orientation
                }
                if(vertexLayout == VertexLayout::Superquadric) {
                    inputBindings.append(QRhiVertexInputBinding(2 * sizeof(float), QRhiVertexInputBinding::PerInstance));
                    attributes.append(QRhiVertexInputAttribute(7, 7, QRhiVertexInputAttribute::Float2, 0)); // roundness
                }
            } else {
                // Picking path: next location is 2 after position+radius.
                if(vertexLayout == VertexLayout::Oriented || vertexLayout == VertexLayout::Superquadric) {
                    inputBindings.append(QRhiVertexInputBinding(3 * sizeof(float), QRhiVertexInputBinding::PerInstance));
                    attributes.append(QRhiVertexInputAttribute(2, 2, QRhiVertexInputAttribute::Float3, 0)); // asphericalShapes
                    inputBindings.append(QRhiVertexInputBinding(4 * sizeof(float), QRhiVertexInputBinding::PerInstance));
                    attributes.append(QRhiVertexInputAttribute(3, 3, QRhiVertexInputAttribute::Float4, 0)); // orientation
                }
                if(vertexLayout == VertexLayout::Superquadric) {
                    inputBindings.append(QRhiVertexInputBinding(2 * sizeof(float), QRhiVertexInputBinding::PerInstance));
                    attributes.append(QRhiVertexInputAttribute(4, 4, QRhiVertexInputAttribute::Float2, 0)); // roundness
                }
            }
            inputLayout.setBindings(inputBindings.cbegin(), inputBindings.cend());
            inputLayout.setAttributes(attributes.cbegin(), attributes.cend());
        }
        // else: sorted SSBO variant has empty vertex input layout (all data from SSBOs).

        // Create graphics pipeline.
        std::unique_ptr<QRhiGraphicsPipeline> pipeline(rhi()->newGraphicsPipeline());
        pipeline->setShaderStages({
            { QRhiShaderStage::Vertex,   vs },
            { QRhiShaderStage::Fragment, fs }
        });
        pipeline->setVertexInputLayout(inputLayout);
        pipeline->setTopology(QRhiGraphicsPipeline::TriangleStrip);
        pipeline->setDepthTest(flags.testFlag(PipelineFlag::DepthTest));
        pipeline->setDepthWrite(flags.testFlag(PipelineFlag::DepthWrite));
        // Mesh-based cube/box variants need back-face culling to match the legacy OpenGL renderer,
        // which enables GL_CULL_FACE / GL_BACK during particle rendering.
        // Ellipsoid/superquadric raycast variants also use back-face culling: their geometry is a
        // 14-vertex triangle-strip bounding box, so culling back faces means only the bbox front is
        // rasterized — which lets the surface shader's layout(depth_greater) gl_FragDepth re-enable
        // HiZ / early-Z. Trade-off: if the camera enters the bbox, the particle becomes invisible.
        bool useBackFaceCulling = (variant == ShaderVariant::CubeMesh ||
                                   variant == ShaderVariant::CubeMeshSorted ||
                                   variant == ShaderVariant::CubeMeshPicking ||
                                   variant == ShaderVariant::BoxOrientedMesh ||
                                   variant == ShaderVariant::BoxOrientedMeshSorted ||
                                   variant == ShaderVariant::BoxOrientedMeshPicking ||
                                   variant == ShaderVariant::EllipsoidRaycast ||
                                   variant == ShaderVariant::EllipsoidRaycastSorted ||
                                   variant == ShaderVariant::EllipsoidRaycastPicking ||
                                   variant == ShaderVariant::SuperquadricRaycast ||
                                   variant == ShaderVariant::SuperquadricRaycastSorted ||
                                   variant == ShaderVariant::SuperquadricRaycastPicking);
        pipeline->setCullMode(useBackFaceCulling ? QRhiGraphicsPipeline::Back : QRhiGraphicsPipeline::None);

        // Set blend factors.
        if(flags.testFlag(PipelineFlag::Blend)) {
            OVITO_ASSERT(!isPicking);
            QRhiGraphicsPipeline::TargetBlend tb;
            tb.enable   = true;
            tb.srcColor = QRhiGraphicsPipeline::SrcAlpha;
            tb.dstColor = QRhiGraphicsPipeline::OneMinusSrcAlpha;
            tb.srcAlpha = QRhiGraphicsPipeline::One;
            tb.dstAlpha = QRhiGraphicsPipeline::OneMinusSrcAlpha;
            pipeline->setTargetBlends({ tb });
        }
        else if(flags.testFlag(PipelineFlag::OITAccum)) {
            // Additive blend for accum buffer: dst.rgba += src.rgba * weight.
            QRhiGraphicsPipeline::TargetBlend tb;
            tb.enable   = true;
            tb.srcColor = QRhiGraphicsPipeline::One;
            tb.dstColor = QRhiGraphicsPipeline::One;
            tb.srcAlpha = QRhiGraphicsPipeline::One;
            tb.dstAlpha = QRhiGraphicsPipeline::One;
            pipeline->setTargetBlends({ tb });
        }
        else if(flags.testFlag(PipelineFlag::OITReveal)) {
            // Multiplicative blend for reveal: dst.r = dst.r * src.r (= product of transparencies).
            QRhiGraphicsPipeline::TargetBlend tb;
            tb.enable   = true;
            tb.srcColor = QRhiGraphicsPipeline::Zero;
            tb.dstColor = QRhiGraphicsPipeline::SrcColor;
            tb.srcAlpha = QRhiGraphicsPipeline::Zero;
            tb.dstAlpha = QRhiGraphicsPipeline::SrcAlpha;
            pipeline->setTargetBlends({ tb });
        }
        else if(flags.testFlag(PipelineFlag::OITDepthPrime)) {
            // OIT depth-only pre-pass: write opaque particle depths, suppress all color output.
            QRhiGraphicsPipeline::TargetBlend tb;
            tb.enable = false;
            tb.colorWrite = QRhiGraphicsPipeline::ColorMask(0);
            pipeline->setTargetBlends({ tb });
        }
        else if(flags.testFlag(PipelineFlag::DepthOnly)) {
            // Outline depth pre-pass: depth-only render target, no color attachments.
            pipeline->setTargetBlends({});
        }
        else if(isPicking) {
            QRhiGraphicsPipeline::TargetBlend tb;
            tb.enable = false;
            pipeline->setTargetBlends({ tb, tb }); // Two color attachments in picking render pass.
        }

        // Get the shader resource bindings for pipeline creation.
        // For sorted SSBO, we need the more complex bindings structure.
        // Use the standard bindings as the reference layout (they share the same binding points 0 and 1).
        QRhiShaderResourceBindings* bindings;
        if(isSortedSSBO) {
            // We need a temporary bindings object for pipeline creation that has the full SSBO layout.
            // This can be any compatible bindings;
            bindings = dc.sorted.shaderResourceBindings.get();
        }
        else {
            bindings = ensureShaderResourceBindings();
        }

        if(!bindings) {
            rt()->reportWarning("ParticlePrimitiveRenderer: Failed to get shader resource bindings for pipeline creation.");
            return {};
        }

        pipeline->setShaderResourceBindings(bindings);
        pipeline->setRenderPassDescriptor(rpd);

        if(!pipeline->create()) {
            rt()->reportWarning("ParticlePrimitiveRenderer: Failed to create pipeline.");
            return {};
        }
        return pipeline;
    });
}

}   // End of namespace
