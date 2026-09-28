// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/core/Core.h>
#include <ovito/core/rendering/SceneRenderer.h>
#include <ovito/core/rendering/ObjectIdAllocator.h>
#include <ovito/core/rendering/RendererResourceCache.h>

namespace Ovito {

/**
 * \brief Provides the GPU resources and services a SceneRenderer::Implementation needs to render a frame graph.
 *
 * A renderer implementation is driven by a renderer service: it supplies the QRhi instance that all GPU
 * work must go through, the shared resource and pipeline caches, the object ID allocator used for picking,
 * and the shader loader.
 *
 * There are two kinds of services:
 *
 *  - RenderThread owns a QRhi instance created on its own thread and drives rendering for the classic
 *    viewport windows and for offscreen rendering.
 *  - A Qt Quick frontend supplies a service backed by the QRhi instance owned by the Qt Quick scene graph
 *    (see gui/qml), so that OVITO's frame graphs can be rendered directly into a QQuickRhiItem's
 *    texture render target.
 *
 * A service is not thread-safe. All methods must be called from the thread that owns the underlying QRhi
 * resources; see isRendererThread().
 */
class OVITO_CORE_EXPORT RendererService
{
public:

    /// Destructor.
    virtual ~RendererService() = default;

    /// Returns the QRhi instance that all GPU work of the renderer implementations must be performed with.
    /// Only callable from the renderer thread.
    virtual QRhi* rhi() const = 0;

    /// Returns the graphics API used by this service.
    virtual QRhi::Implementation graphicsApi() const = 0;

    /// Returns the shared cache for QRhi resources such as vertex buffers and textures.
    /// Only callable from the renderer thread.
    virtual RendererResourceCache& rhiResourceCache() = 0;

    /// Returns the object ID allocator used for rendering and object picking.
    /// Only callable from the renderer thread.
    virtual ObjectIdAllocator& objectIdAllocator() = 0;

    /// Loads a compiled .qsb shader from the Qt resource system.
    /// Only callable from the renderer thread.
    [[nodiscard]] virtual QShader loadShader(const QString& resourcePath) = 0;

    /// Records a non-fatal warning encountered during rendering.
    /// May be called from the renderFrame() method of a renderer implementation on the renderer thread.
    virtual void reportWarning(const QString& message) = 0;

    /// Indicates whether the calling thread is the thread that owns this service's GPU resources.
    virtual bool isRendererThread() const { return false; }

    /// Looks up a graphics pipeline in the cache matching the given render pass descriptor and caller-provided
    /// cache key. If no compatible pipeline is found, a new one is created by invoking the initializer.
    /// The cache key is an arbitrary value that is used to distinguish different pipeline configurations at the
    /// call site (e.g. shader variant, primitive type). The render pass descriptor is used to check compatibility
    /// of cached pipelines with the current render pass (e.g. attachment formats, sample count).
    /// The initializer is a callable that is invoked when no compatible pipeline is found in the cache. It is
    /// supposed to create and return a unique_ptr to a new pipeline.
    template<typename CacheKey, typename Initializer>
    QRhiGraphicsPipeline* ensureGraphicsPipeline(QRhiRenderPassDescriptor* rpd, CacheKey&& key, Initializer&& initializer) {
        for(const auto& [cachedRpdData, cachedKey, cachedPipeline] : _graphicsPipelineCache) {
            if(cachedKey.type() == typeid(CacheKey) && any_cast<const CacheKey&>(cachedKey) == key && cachedRpdData == rpd->serializedFormat())
                return cachedPipeline.get();
        }
        std::unique_ptr<QRhiGraphicsPipeline> pipeline = initializer();
        if(!pipeline)
            return nullptr;
        const auto& entry = _graphicsPipelineCache.emplace_back(rpd->serializedFormat(), std::forward<CacheKey>(key), std::move(pipeline));
        return std::get<2>(entry).get();
    }

    /// Finds a shared rendering device associated with this service matching the given predicate. Returns nullptr
    /// if no matching device is found. The predicate is a callable that takes a pointer to a SceneRenderer::Device
    /// and returns a bool indicating whether the device matches the search criteria.
    template<typename DeviceType, typename Predicate>
    std::shared_ptr<DeviceType> findSharedRenderingDevice(Predicate&& predicate) {
        OVITO_ASSERT(isRendererThread());
        for(const auto& weakDevice : _sharedRenderingDevices) {
            if(auto device = std::dynamic_pointer_cast<DeviceType>(weakDevice.lock())) {
                if(predicate(device.get())) {
                    // Cache the most recently accessed device to keep it alive across successive offscreen rendering requests.
                    _sharedRenderingDeviceCache = device;
                    return device;
                }
            }
        }
        return {};
    }

    /// Adds a shared rendering device to the list of devices associated with this service. The device is typically
    /// created by a renderer implementation and shared with other renderers running on the same thread.
    void addSharedRenderingDevice(const std::shared_ptr<SceneRenderer::Device>& device) {
        OVITO_ASSERT(isRendererThread());
        // Remove expired devices from the list before adding the new one.
        std::erase_if(_sharedRenderingDevices, [](const std::weak_ptr<SceneRenderer::Device>& d) { return d.expired(); });
        // Cache the most recently added device to keep it alive across successive offscreen rendering requests.
        _sharedRenderingDeviceCache = device;
        _sharedRenderingDevices.emplace_back(device);
    }

    /// Discards all cached GPU resources and shared rendering devices. Called when the QRhi instance is about
    /// to be destroyed, because the resources depend on it.
    void discardCachedResources() {
        _sharedRenderingDeviceCache.reset();
        _sharedRenderingDevices.clear();
        _graphicsPipelineCache.clear();
    }

private:

    /// Cache for QRhi graphics pipelines used by the \c ensureGraphicsPipeline() method. Managed on the renderer
    /// thread and shared by all render targets that use this service.
    /// Each entry consists of:
    /// - A serialized format of the render pass descriptor (e.g. attachment formats, sample count) for pipeline compatibility checks.
    /// - A call site-specific cache key (e.g. shader variant, primitive type) to distinguish different pipeline configurations at the call site.
    /// - The QRhiGraphicsPipeline.
    std::vector<std::tuple<
            QVector<quint32>,
            boost::anys::unique_any,
            std::unique_ptr<QRhiGraphicsPipeline>>>
        _graphicsPipelineCache;

    /// Weak pointers to all rendering devices created and shared by renderers using this service.
    std::vector<std::weak_ptr<SceneRenderer::Device>> _sharedRenderingDevices;

    /// A strong pointer to the most recently accessed shared rendering device, used to keep it alive
    /// across successive offscreen rendering requests.
    std::shared_ptr<SceneRenderer::Device> _sharedRenderingDeviceCache;
};

}   // End of namespace
