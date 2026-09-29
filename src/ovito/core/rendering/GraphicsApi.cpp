// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/core/Core.h>
#include <ovito/core/app/Application.h>
#include "GraphicsApi.h"

#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QSettings>

#if QT_CONFIG(vulkan) && defined(Q_OS_LINUX)
#include <QtGui/QVulkanInstance>
#include <dlfcn.h>
#include <mutex>
#endif

namespace Ovito {

/******************************************************************************
* Picks the graphics API to render with on the current platform.
******************************************************************************/
QRhi::Implementation GraphicsApi::preferred()
{
#if defined(Q_OS_WIN)
    return QRhi::D3D12; // Prefer D3D12; RenderThread::createRhi() falls back to D3D11 if unavailable.
#elif QT_CONFIG(metal)
    return QRhi::Metal;
#elif QT_CONFIG(vulkan) && defined(Q_OS_LINUX)
    return QRhi::Vulkan;
#else
    return QRhi::OpenGLES2;
#endif
}

#if QT_CONFIG(vulkan) && defined(Q_OS_LINUX)

/******************************************************************************
* Returns true if the Vulkan loader itself is installed on the system.
******************************************************************************/
bool GraphicsApi::isVulkanLoaderInstalled()
{
    if(void* handle = ::dlopen("libvulkan.so.1", RTLD_LAZY | RTLD_LOCAL)) {
        ::dlclose(handle);
        return true;
    }
    return false;
}

/******************************************************************************
* Returns true if the system has at least one installed Vulkan driver, i.e. an
* ICD manifest in one of the directories searched by the Vulkan loader.
******************************************************************************/
bool GraphicsApi::isVulkanDriverInstalled()
{
    QStringList prefixes;
    if(qEnvironmentVariableIsSet("XDG_DATA_HOME"))
        prefixes.push_back(qEnvironmentVariable("XDG_DATA_HOME"));
    else if(!QDir::homePath().isEmpty())
        prefixes.push_back(QDir::homePath() + QStringLiteral("/.local/share"));
    prefixes += qEnvironmentVariable("XDG_DATA_DIRS").split(QChar(':'), Qt::SkipEmptyParts);
    prefixes.push_back(QStringLiteral("/usr/local/share"));
    prefixes.push_back(QStringLiteral("/usr/share"));

    QStringList searchDirs;
    for(const QString& prefix : prefixes)
        searchDirs.push_back(prefix + QStringLiteral("/vulkan/icd.d"));
    searchDirs.push_back(QStringLiteral("/usr/local/etc/vulkan/icd.d"));
    searchDirs.push_back(QStringLiteral("/etc/vulkan/icd.d"));

    for(const QString& path : searchDirs) {
        if(!QDir(path).entryList({ QStringLiteral("*.json") }, QDir::Files).isEmpty())
            return true;
    }
    return false;
}

/******************************************************************************
* Requests the Vulkan API version OVITO's instances are created with.
*
* A QVulkanInstance requests no particular API version by default, so the loader hands out the oldest one and the
* validation layers report VUID-VkApplicationInfo-apiVersion for the resulting all-zero VkApplicationInfo (observed
* with Radv and with the software rasterizer, neither of which fails for it). Ask for the newest version the loader
* offers, capped at the version the shaders and QRhi are built for.
******************************************************************************/
void GraphicsApi::configureVulkanApiVersion(QVulkanInstance& instance)
{
    const QVersionNumber maximumVersion(1, 3);

    QVersionNumber version = instance.supportedApiVersion();
    if(version.isNull() || version < QVersionNumber(1, 1))
        version = QVersionNumber(1, 1);
    else if(version > maximumVersion)
        version = maximumVersion;
    instance.setApiVersion(version);
}

/******************************************************************************
* Makes OVITO's bundled software Vulkan driver available, but only on systems
* that provide no Vulkan driver of their own.
*
* Qt does not link the Vulkan loader, it loads it with dlopen() at runtime, and
* the loader's driver selection variables replace its driver list rather than
* extend it. Activating the bundled driver unconditionally would therefore hide
* the native GPU driver on every properly equipped machine. This function only
* engages when the system offers nothing to hide.
******************************************************************************/
void GraphicsApi::prepareVulkanEnvironment()
{
#ifdef OVITO_VULKAN_FALLBACK_RELATIVE_PATH
    static std::once_flag onceFlag;
    std::call_once(onceFlag, []() {

        // Never interfere with an explicit driver selection made by the user.
        if(qEnvironmentVariableIsSet("VK_ICD_FILENAMES") || qEnvironmentVariableIsSet("VK_DRIVER_FILES") || qEnvironmentVariableIsSet("VK_ADD_DRIVER_FILES"))
            return;

        // OVITO_VULKAN_FALLBACK=0 disables the bundled driver, =1 forces its use.
        QByteArray fallbackSetting = qgetenv("OVITO_VULKAN_FALLBACK");
        if(fallbackSetting == "0")
            return;
        bool forceFallback = (fallbackSetting == "1");

        // Locate the bundled driver, which is only part of the redistributable Linux package.
        QDir fallbackDir(QDir(Application::instance()->applicationDirPath()).absoluteFilePath(QStringLiteral(OVITO_VULKAN_FALLBACK_RELATIVE_PATH)));
        QString manifestPath = fallbackDir.absoluteFilePath(QStringLiteral("lvp_icd.json"));
        if(!QFileInfo::exists(manifestPath))
            return;

        // Does the system provide a Vulkan loader of its own?
        bool haveSystemLoader = isVulkanLoaderInstalled();

        // A system that has both a loader and a driver is left alone.
        if(!forceFallback && haveSystemLoader && isVulkanDriverInstalled())
            return;

        // Point the loader at the bundled driver. VK_ADD_DRIVER_FILES is the additive variable
        // understood by loaders since version 1.3.207; VK_ICD_FILENAMES is its deprecated
        // predecessor, set here for older loaders. Overwriting the driver list is safe at this
        // point, because the system has no driver of its own that could be displaced.
        qputenv("VK_ADD_DRIVER_FILES", QFile::encodeName(manifestPath));
        qputenv("VK_ICD_FILENAMES", QFile::encodeName(manifestPath));

        // Mesa's lavapipe as patched by Red Hat fails vkCreateInstance() unless this is set.
        if(!qEnvironmentVariableIsSet("RH_SW_VULKAN"))
            qputenv("RH_SW_VULKAN", "1");

        if(!haveSystemLoader) {
            // Qt calls dlopen("libvulkan.so.1"), which searches only the system library paths.
            // Loading our copy first makes that call resolve to the already loaded SONAME.
            QByteArray loaderPath = QFile::encodeName(fallbackDir.absoluteFilePath(QStringLiteral("libvulkan.so.1")));
            if(!::dlopen(loaderPath.constData(), RTLD_NOW | RTLD_GLOBAL))
                qWarning("GraphicsApi: Could not load the bundled Vulkan loader %s: %s", loaderPath.constData(), ::dlerror());
        }
    });
#endif
}

#endif

/******************************************************************************
* Enumerates available GPU adapters for the given graphics API.
* Can be called on the GUI thread without creating a QRhi instance.
******************************************************************************/
QList<QRhiDriverInfo> GraphicsApi::enumerateAdapters(QRhi::Implementation graphicsApi)
{
    QList<QRhiDriverInfo> result;

    // Metal and OpenGL do not support adapter enumeration in QRhi.
    if(graphicsApi == QRhi::Metal || graphicsApi == QRhi::OpenGLES2)
        return result;

#if QT_CONFIG(vulkan) && defined(Q_OS_LINUX)
    if(graphicsApi == QRhi::Vulkan) {
	    // Ensure a QGuiApplication exists before the RenderThread constructor touches Vulkan/QPA.
        // In --nogui / PyPI headless mode the Qt application is created lazily; the RenderThread
        // constructor calls QVulkanInstance::create(), which dereferences
        // QGuiApplicationPrivate::platformIntegration() and crashes if it is null.
        Application::instance()->createQtApplication(false);
        prepareVulkanEnvironment();

        // A temporary Vulkan instance is needed for enumeration.
        QVulkanInstance tempInst;
        tempInst.setExtensions(QRhiVulkanInitParams::preferredInstanceExtensions());
        configureVulkanApiVersion(tempInst);
        if(tempInst.create()) {
            QRhiVulkanInitParams params;
            params.inst = &tempInst;
            QRhi::AdapterList adapters = QRhi::enumerateAdapters(QRhi::Vulkan, &params);
            for(QRhiAdapter* adapter : adapters)
                result.push_back(adapter->info());
            qDeleteAll(adapters);
        }
    }
#endif

#ifdef Q_OS_WIN
    if(graphicsApi == QRhi::D3D12) {
        QRhiD3D12InitParams params;
        QRhi::AdapterList adapters = QRhi::enumerateAdapters(QRhi::D3D12, &params);
        for(QRhiAdapter* adapter : adapters)
            result.push_back(adapter->info());
        qDeleteAll(adapters);
    }
    else if(graphicsApi == QRhi::D3D11) {
        QRhiD3D11InitParams params;
        QRhi::AdapterList adapters = QRhi::enumerateAdapters(QRhi::D3D11, &params);
        for(QRhiAdapter* adapter : adapters)
            result.push_back(adapter->info());
        qDeleteAll(adapters);
    }
#endif

    return result;
}

/******************************************************************************
* Loads the user-selected adapter name. In headless mode the GUI's QSettings
* store is not consulted to keep Python-scripting behavior isolated from
* desktop preferences; the OVITO_GPU_ADAPTER environment variable is used
* instead. Returns an empty byte array to select the default adapter.
******************************************************************************/
QByteArray GraphicsApi::selectedAdapterName()
{
    QByteArray adapterName = qgetenv("OVITO_GPU_ADAPTER");
    if(adapterName.isEmpty()) {
        QSettings settings;
        adapterName = settings.value(QLatin1String(adapterSettingsKey())).toByteArray();
    }
    return adapterName;
}

}	// End of namespace
