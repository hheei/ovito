// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/core/Core.h>
#include <ovito/core/utilities/units/UnitsManager.h>
#include <ovito/core/rendering/RenderThread.h>
#include <ovito/core/rendering/standard/StandardRendererImplementation.h>
#include "StandardRenderer.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(StandardRenderer);
OVITO_CLASSINFO(StandardRenderer, "ClassNameAlias", "StandardSceneRenderer");  // For backward compatibility with OVITO 3.10
OVITO_CLASSINFO(StandardRenderer, "ClassNameAlias", "OpenGLSceneRenderer");  // For backward compatibility with OVITO 3.10
OVITO_CLASSINFO(StandardRenderer, "ClassNameAlias", "OpenGLRenderer");  // For backward compatibility with OVITO 3.15
OVITO_CLASSINFO(StandardRenderer, "DisplayName", "Standard renderer");
OVITO_CLASSINFO(StandardRenderer, "Description", "GPU-accelerated rendering engine, also used by OVITO's interactive viewports.");
DEFINE_PROPERTY_FIELD(StandardRenderer, antialiasingLevel);
DEFINE_PROPERTY_FIELD(StandardRenderer, orderIndependentTransparency);
SET_PROPERTY_FIELD_LABEL(StandardRenderer, antialiasingLevel, "Antialiasing level");
SET_PROPERTY_FIELD_LABEL(StandardRenderer, orderIndependentTransparency, "Order-independent transparency");
SET_PROPERTY_FIELD_UNITS_AND_RANGE(StandardRenderer, antialiasingLevel, IntegerParameterUnit, 1, 6);

/******************************************************************************
* Constructor.
******************************************************************************/
void StandardRenderer::initializeObject(ObjectInitializationFlags flags)
{
    SceneRenderer::initializeObject(flags);

    if(this_task::isInteractive()) {
        // Check which transparency rendering method has been selected by the user in the application settings dialog.
#ifndef OVITO_DISABLE_QSETTINGS
        QSettings applicationSettings;
        if(applicationSettings.value("rendering/transparency_method").toInt() == 2) {
            // Activate the Weighted Blended Order-Independent Transparency method.
            setOrderIndependentTransparency(true);
        }
#endif
    }
}

/******************************************************************************
* Creates a new configuration object encapsulating the current renderer configuration.
******************************************************************************/
std::unique_ptr<SceneRenderer::Configuration> StandardRenderer::createConfiguration(const FrameGraph& frameGraph) const
{
    auto config = std::make_unique<StandardRenderer::Configuration>(orderIndependentTransparency());
    config->outlineSettings = frameGraph.outlineSettings();
    return config;
}

/******************************************************************************
* Creates an implementation object for visual rendering.
******************************************************************************/
std::unique_ptr<SceneRenderer::Implementation> StandardRenderer::Configuration::createImplementationForVisual(RendererService* service, std::unique_ptr<Implementation> existingImpl) const
{
    // Reuse the existing implementation if it's already a StandardRendererImplementation.
    if(auto* existing = dynamic_cast<StandardRendererImplementation*>(existingImpl.get())) {
        existing->setOrderIndependentTransparency(_orderIndependentTransparency);
        return existingImpl;
    }
    return std::make_unique<StandardRendererImplementation>(service, _orderIndependentTransparency);
}

/******************************************************************************
* Creates an implementation object for picking rendering.
******************************************************************************/
std::unique_ptr<SceneRenderer::Implementation> StandardRenderer::Configuration::createImplementationForPicking(RendererService* service, std::unique_ptr<Implementation> existingImpl) const
{
    // Reuse the existing implementation if it's already a StandardRendererImplementation.
    if(auto* existing = dynamic_cast<StandardRendererImplementation*>(existingImpl.get()))
        return existingImpl;
    return std::make_unique<StandardRendererImplementation>(service);
}

/******************************************************************************
* Is called by OVITO to query the class for any information that should be
* included in the application's system report.
******************************************************************************/
void StandardRenderer::OOMetaClass::querySystemInformation(QTextStream& stream, UserInterface& userInterface) const
{
    if(this == &StandardRenderer::OOClass()) {
        stream << "======= Graphics hardware =======" << "\n";
        QRhi::Implementation graphicsApi = RenderThread::pickGraphicsApi();
        stream << "Graphics API: " << QString::fromUtf8(QRhi::backendName(graphicsApi)) << "\n";

        // In GUI mode, report the active real-time viewport rendering backend.
        // The selection is stored in QSettings (key: "rendering/selected_graphics_api") and may be overridden by the
        // OVITO_VIEWPORT_RENDERER environment variable - the same two sources the frontends' ViewportRendererRegistry
        // reads. This renderer lives in core and cannot use that GUI service, so it resolves the selection once more.
        if(Application::guiEnabled()) {
            QString rendererKey = qEnvironmentVariable("OVITO_VIEWPORT_RENDERER",
                QSettings().value(QStringLiteral("rendering/selected_graphics_api")).toString().toLower());
            QString rendererLabel = (rendererKey == QStringLiteral("anari"))
                ? tr("VisRTX")
                : tr("default");
            stream << "Viewport renderer: " << rendererLabel << "\n";
        }

        QList<QRhiDriverInfo> adapters = RenderThread::enumerateAdapters(graphicsApi);
        QByteArray selectedName = RenderThread::selectedAdapterName();
        // When no adapter is explicitly selected the first one in the list is the effective default.
        bool foundSelected = false;
        for(int i = 0; i < adapters.size(); ++i) {
            const QRhiDriverInfo& info = adapters[i];
            QString label = QString::fromUtf8(info.deviceName);
            switch(info.deviceType) {
            case QRhiDriverInfo::IntegratedDevice: label += tr(" (integrated)"); break;
            case QRhiDriverInfo::DiscreteDevice:   label += tr(" (discrete)"); break;
            case QRhiDriverInfo::CpuDevice:        label += tr(" (cpu)"); break;
            case QRhiDriverInfo::ExternalDevice:   label += tr(" (external)"); break;
            case QRhiDriverInfo::VirtualDevice:    label += tr(" (virtual)"); break;
            default: break;
            }
            bool isActive = !foundSelected && (selectedName.isEmpty() ? i == 0 : info.deviceName == selectedName);
            if(isActive) {
                foundSelected = true;
                label += tr(" [active]");
            }
            stream << "  " << label << "\n";
        }
    }
}

}   // End of namespace
