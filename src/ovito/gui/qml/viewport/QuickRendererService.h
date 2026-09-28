// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/qml/QmlFrontend.h>
#include <ovito/core/rendering/RendererService.h>
#include <ovito/core/rendering/RendererResourceCache.h>
#include <ovito/core/rendering/ObjectIdAllocator.h>

class QQuickWindow;

namespace Ovito {

class QuickViewportRenderer;

/**
 * \brief Supplies the GPU resources that the viewport items of one Qt Quick window render with.
 *
 * A renderer implementation is driven by a \ref RendererService: it provides the QRhi instance, the shared
 * resource and pipeline caches, the object ID allocator and the shader loader. In the Qt Quick frontend the
 * QRhi instance belongs to the scene graph of a QQuickWindow, so the service belongs to the window as well
 * and is shared by all viewport items of that window. That is the same relationship the classic frontend has
 * between one RenderThread and its viewport windows.
 *
 * Sharing has two effects: every shader pipeline is compiled once per window instead of once per viewport,
 * and GPU buffers that two viewports need at the same time (the same particle positions shown in three
 * orthogonal views, for example) are uploaded once, because the resource cache keeps an entry alive while any
 * of its resource frames is in flight.
 *
 * The service is created by QuickViewportItem (on the GUI thread) and parented to the window. Its GPU
 * resources are dropped when the window invalidates its scene graph, because the QRhi instance they were
 * created with dies at that point.
 */
class OVITO_GUIQML_EXPORT QuickRendererService : public QObject, public RendererService
{
    Q_OBJECT

public:

    /// Constructor. The window becomes the owner of the service.
    explicit QuickRendererService(QQuickWindow* window);

    /// Registers the QRhi instance of the scene graph, which the window provides to its renderers.
    /// Called from the render thread, during scene graph synchronization.
    void setRhi(QRhi* rhi) { _rhi = rhi; }

    /// Returns the QRhi instance of the scene graph.
    QRhi* rhi() const override { return _rhi; }

    /// Returns the graphics API the scene graph renders with.
    QRhi::Implementation graphicsApi() const override;

    /// Returns the cache for QRhi resources such as vertex buffers and textures.
    RendererResourceCache& rhiResourceCache() override { return *_resourceCache; }

    /// Returns the object ID allocator used for rendering and object picking.
    ObjectIdAllocator& objectIdAllocator() override { return _objectIdAllocator; }

    /// Loads a compiled .qsb shader from the Qt resource system.
    [[nodiscard]] QShader loadShader(const QString& resourcePath) override;

    /// Records a non-fatal warning encountered during rendering.
    void reportWarning(const QString& message) override;

    /// All rendering with this service happens on the scene graph's render thread.
    bool isRendererThread() const override { return true; }

    /// Adds a renderer that draws with the GPU resources of this service. Called by the renderer itself when
    /// it is created, from the scene graph's render thread.
    void registerRenderer(QuickViewportRenderer* renderer);

    /// Removes a renderer again, e.g. because the scene graph replaced it.
    void unregisterRenderer(QuickViewportRenderer* renderer);

private:

    /// Releases the GPU resources of this service and tells the renderers to do the same. Called when the
    /// window invalidates its scene graph, before the QRhi instance the resources belong to is destroyed.
    void releaseGraphicsResources();

    /// The window whose scene graph provides the QRhi instance.
    QQuickWindow* _window;

    /// The QRhi instance of the scene graph, valid between setRhi() and the invalidation of the scene graph.
    QRhi* _rhi = nullptr;

    /// Cache for QRhi resources such as vertex buffers and textures, shared by the viewport items.
    std::shared_ptr<RendererResourceCache> _resourceCache = std::make_shared<RendererResourceCache>();

    /// Allocator for unique object IDs used in rendering and object picking.
    ObjectIdAllocator _objectIdAllocator;

    /// Warning messages collected during the last rendered frame.
    QStringList _warnings;

    /// The renderers drawing with this service's GPU resources. All of them belong to the scene graph's render
    /// thread, which is where they are added, removed and notified.
    std::vector<QuickViewportRenderer*> _renderers;
};

}   // End of namespace
