// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <QFile>
#include <QQuickWindow>
#include "QuickRendererService.h"
#include "QuickViewportRenderer.h"

namespace Ovito {

/******************************************************************************
* Constructor.
******************************************************************************/
QuickRendererService::QuickRendererService(QQuickWindow* window) : QObject(window), _window(window)
{
    OVITO_ASSERT(window);
    setObjectName(QStringLiteral("ovitoRendererService"));

    // The QRhi instance of the scene graph is about to be destroyed, and with it every GPU resource this
    // service handed out. Drop them while the instance is still alive. In a threaded render loop the signal
    // arrives on the render thread, which is where these resources are used; with the basic loop it arrives
    // on the GUI thread, while nothing is being rendered.
    connect(window, &QQuickWindow::sceneGraphInvalidated, this, &QuickRendererService::releaseGraphicsResources, Qt::DirectConnection);
}

/******************************************************************************
* Returns the graphics API the scene graph renders with.
******************************************************************************/
QRhi::Implementation QuickRendererService::graphicsApi() const
{
    return _rhi ? _rhi->backend() : QRhi::Null;
}

/******************************************************************************
* Loads a compiled .qsb shader from the Qt resource system.
******************************************************************************/
QShader QuickRendererService::loadShader(const QString& resourcePath)
{
    QFile file(resourcePath);
    if(!file.open(QIODevice::ReadOnly)) {
        reportWarning(tr("Could not open shader resource: %1").arg(resourcePath));
        return {};
    }
    return QShader::fromSerialized(file.readAll());
}

/******************************************************************************
* Records a non-fatal warning encountered during rendering.
******************************************************************************/
void QuickRendererService::reportWarning(const QString& message)
{
    _warnings.push_back(message);
    qWarning() << message;
}

/******************************************************************************
* Adds a renderer that draws with the GPU resources of this service.
******************************************************************************/
void QuickRendererService::registerRenderer(QuickViewportRenderer* renderer)
{
    OVITO_ASSERT(renderer);
    OVITO_ASSERT(std::ranges::find(_renderers, renderer) == _renderers.end());
    _renderers.push_back(renderer);
}

/******************************************************************************
* Removes a renderer again.
******************************************************************************/
void QuickRendererService::unregisterRenderer(QuickViewportRenderer* renderer)
{
    std::erase(_renderers, renderer);
}

/******************************************************************************
* Releases the GPU resources of this service.
******************************************************************************/
void QuickRendererService::releaseGraphicsResources()
{
    // The renderers must let go of their GPU resources first, so that every cache entry of the shared resource
    // cache is released and no renderer ever hands a QRhi resource of the dead instance out again.
    for(QuickViewportRenderer* renderer : _renderers)
        renderer->releaseGraphicsResources();

    _rhi = nullptr;
    discardCachedResources();
    _warnings.clear();
}

}   // End of namespace
