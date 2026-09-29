// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/gui/base/GUIBase.h>
#include <ovito/core/rendering/standard/StandardRenderer.h>
#include <ovito/core/app/PluginManager.h>
#include <ovito/core/utilities/concurrent/NoninteractiveContext.h>
#include <ovito/core/utilities/io/ObjectSaveStream.h>
#include <ovito/core/utilities/io/ObjectLoadStream.h>
#include <QSettings>
#include "ViewportRendererRegistry.h"

namespace Ovito {

/******************************************************************************
* Returns the application-wide instance of the registry.
******************************************************************************/
ViewportRendererRegistry& ViewportRendererRegistry::instance()
{
    static ViewportRendererRegistry registry;
    return registry;
}

/******************************************************************************
* Returns the renderer implementations available in this build.
******************************************************************************/
std::vector<ViewportRendererRegistry::RendererInfo> ViewportRendererRegistry::availableRenderers() const
{
    std::vector<RendererInfo> renderers;

    // The standard renderer, which every build provides. The legacy name "opengl" is kept for backward compatibility,
    // because it is the value stored in the settings of existing installations.
    renderers.push_back(RendererInfo{
        QStringLiteral("opengl"),
        tr("GPU rasterizer (default)"),
        &StandardRenderer::OOClass()
    });

    // The ANARI renderer (NVIDIA VisRTX), which the AnariRenderer plugin provides.
    renderers.push_back(RendererInfo{
        QStringLiteral("anari"),
        tr("NVIDIA VisRTX (requires CUDA-capable device)"),
        PluginManager::instance().findClass("AnariRenderer", "AnariRenderer")
    });
#if defined(Q_OS_MACOS) && !defined(OVITO_DEBUG)
    // Do not offer the ANARI option in macOS release builds: VisRTX is not available on this platform, even if the
    // renderer class happens to be present.
    renderers.back().rendererClass = nullptr;
#endif

    return renderers;
}

/******************************************************************************
* Returns the identifier of the renderer the user selected.
******************************************************************************/
QString ViewportRendererRegistry::selectedRendererId() const
{
    // The environment variable takes precedence, so that a user (or a test) can override the stored selection without
    // modifying the settings of the installation.
    return qEnvironmentVariable("OVITO_VIEWPORT_RENDERER",
        QSettings().value(QStringLiteral("rendering/selected_graphics_api"),
            QStringLiteral("opengl")).toString().toLower());
}

/******************************************************************************
* Selects the renderer to use and stores the selection in the application settings.
******************************************************************************/
bool ViewportRendererRegistry::setSelectedRendererId(const QString& id)
{
    const QString previousId = selectedRendererId();
    if(id.compare(previousId, Qt::CaseInsensitive) == 0)
        return false;

    QSettings settings;
    if(!id.isEmpty())
        settings.setValue(QStringLiteral("rendering/selected_graphics_api"), id);
    else
        settings.remove(QStringLiteral("rendering/selected_graphics_api"));

    Q_EMIT rendererSelectionChanged();
    return true;
}

/******************************************************************************
* Switches back to the default renderer for interactive viewports.
******************************************************************************/
bool ViewportRendererRegistry::revertToDefaultRenderer()
{
    // Note: a selection made through the environment variable cannot be reverted, so it is not touched here.
    QSettings settings;
    if(qgetenv("OVITO_VIEWPORT_RENDERER").isEmpty() && settings.value(QStringLiteral("rendering/selected_graphics_api")).isValid()) {
        settings.remove(QStringLiteral("rendering/selected_graphics_api"));
        Q_EMIT rendererSelectionChanged();
        return true;
    }
    return false;
}

/******************************************************************************
* Returns the instance of the given renderer, or of the selected one.
******************************************************************************/
OORef<SceneRenderer> ViewportRendererRegistry::renderer(const QString& id)
{
    OVITO_ASSERT(this_task::isMainThread());
    OVITO_ASSERT(this_task::get());

    // An explicitly requested renderer implementation is returned as it is, so that a caller can check whether it is
    // available.
    if(!id.isEmpty())
        return cachedRenderer(id);

    OORef<SceneRenderer> rendererInstance = cachedRenderer(selectedRendererId());
    if(!rendererInstance) {
        // The renderer the user selected is not available in this build (a plugin was removed, or the settings were
        // copied to another installation). Falling back to the default renderer keeps the viewports working instead of
        // leaving them blank.
        qWarning() << "The selected interactive viewport renderer" << selectedRendererId()
                   << "is not available in this build; using the default renderer instead.";
        // Note: a selection made through the environment variable is not stored in the settings, so there is nothing to
        // remove in that case.
        if(qgetenv("OVITO_VIEWPORT_RENDERER").isEmpty())
            QSettings().remove(QStringLiteral("rendering/selected_graphics_api"));
        const auto renderers = availableRenderers();
        const auto defaultRenderer = std::ranges::find_if(renderers, [](const RendererInfo& info) { return info.rendererClass != nullptr; });
        OVITO_ASSERT(defaultRenderer != renderers.end());
        rendererInstance = defaultRenderer != renderers.end() ? cachedRenderer(defaultRenderer->id) : OORef<SceneRenderer>();
    }
    return rendererInstance;
}

/******************************************************************************
* Returns the instance of the given renderer implementation, creating it if it does not exist yet.
******************************************************************************/
OORef<SceneRenderer> ViewportRendererRegistry::cachedRenderer(const QString& id)
{
    // Every renderer implementation has one instance, which the viewport windows of all datasets share - so that a
    // setting the user changes in the renderer's own property editor applies to every viewport.
    if(auto it = _renderers.find(id); it != _renderers.end())
        return it->second;

    OORef<SceneRenderer> rendererInstance = loadRenderer(id);
    _renderers.emplace(id, rendererInstance);
    return rendererInstance;
}

/******************************************************************************
* Creates the instance of the given renderer implementation.
******************************************************************************/
OORef<SceneRenderer> ViewportRendererRegistry::loadRenderer(const QString& id)
{
    const auto renderers = availableRenderers();
    const auto entry = std::ranges::find_if(renderers, [&id](const RendererInfo& info) { return info.id == id; });
    if(entry == renderers.end() || !entry->rendererClass)
        return {};

    // Create the renderer outside of any interactive context, so that its parameters always start from the factory
    // defaults and the constructor does not depend on a dataset that happens to be open.
    NoninteractiveContext noninteractiveContext;
    OORef<SceneRenderer> rendererInstance;

    // Take the settings the user made in the renderer's property editor from the settings store.
    QSettings settings;
    settings.beginGroup(QStringLiteral("rendering/interactive_window_renderers"));
    try {
        const QByteArray buffer = settings.value(entry->id).toByteArray();
        if(!buffer.isEmpty()) {
            QDataStream dataStream(buffer);
            ObjectLoadStream stream(dataStream);
            rendererInstance = stream.loadObject<SceneRenderer>();
            if(rendererInstance && !entry->rendererClass->isMember(rendererInstance))
                rendererInstance.reset();
            stream.close();
        }
    }
    catch(const Exception& ex) {
        qWarning() << "Failed to load the settings of the interactive viewport renderer" << entry->id << ":";
        ex.logError();
        settings.remove(entry->id);
    }

    if(!rendererInstance)
        rendererInstance = dynamic_object_cast<SceneRenderer>(entry->rendererClass->createInstance());

    return rendererInstance;
}

/******************************************************************************
* Stores the settings of all renderer instances that have been created.
******************************************************************************/
void ViewportRendererRegistry::saveRendererSettings()
{
    OVITO_ASSERT(this_task::isMainThread());

    QSettings settings;
    settings.beginGroup(QStringLiteral("rendering/interactive_window_renderers"));
    for(const auto& [id, rendererInstance] : _renderers) {
        if(!rendererInstance)
            continue;
        QByteArray buffer;
        QDataStream dataStream(&buffer, QIODevice::WriteOnly);
        ObjectSaveStream stream(dataStream);
        stream.saveObject(rendererInstance);
        stream.close();
        settings.setValue(id, std::move(buffer));
    }
}

}   // End of namespace
