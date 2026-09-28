// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/core/Core.h>
#include <ovito/core/rendering/FrameBuffer.h>
#include <ovito/core/rendering/standard/StandardRenderer.h>
#include <ovito/core/app/Application.h>
#include "RenderThread.h"

#include <QPlatformSurfaceEvent>

#ifdef Q_OS_DARWIN
#include <QtCore/private/qcore_mac_p.h>
#endif

#if QT_CONFIG(vulkan) && defined(Q_OS_LINUX)
#include <dlfcn.h>
#include <mutex>
#endif

namespace Ovito {

/******************************************************************************
* Picks the platform-specific graphics API for the current platform.
******************************************************************************/
QRhi::Implementation RenderThread::pickGraphicsApi()
{
#if defined(Q_OS_WIN)
    return QRhi::D3D12; // Prefer D3D12; createRhi() falls back to D3D11 if unavailable.
#elif QT_CONFIG(metal)
    return QRhi::Metal;
#elif QT_CONFIG(vulkan) && defined(Q_OS_LINUX)
    return QRhi::Vulkan;
#else
    return QRhi::OpenGLES2;
#endif
}

#if QT_CONFIG(vulkan) && defined(Q_OS_LINUX)
namespace {

/******************************************************************************
* Returns true if the Vulkan loader itself is installed on the system.
******************************************************************************/
bool hasSystemVulkanLoader()
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
bool hasSystemVulkanDriver()
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

} // End of anonymous namespace
#endif

#if QT_CONFIG(vulkan) && defined(Q_OS_LINUX)

/******************************************************************************
* Requests the Vulkan API version OVITO's instances are created with.
*
* A QVulkanInstance requests no particular API version by default, so the loader hands out the oldest one and the
* validation layers report VUID-VkApplicationInfo-apiVersion for the resulting all-zero VkApplicationInfo (observed
* with Radv and with the software rasterizer, neither of which fails for it). Ask for the newest version the loader
* offers, capped at the version the shaders and QRhi are built for.
******************************************************************************/
static void configureVulkanApiVersion(QVulkanInstance& instance)
{
    const QVersionNumber maximumVersion(1, 3);

    QVersionNumber version = instance.supportedApiVersion();
    if(version.isNull() || version < QVersionNumber(1, 1))
        version = QVersionNumber(1, 1);
    else if(version > maximumVersion)
        version = maximumVersion;
    instance.setApiVersion(version);
}

#endif

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
void RenderThread::prepareVulkanEnvironment()
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
        bool haveSystemLoader = hasSystemVulkanLoader();

        // A system that has both a loader and a driver is left alone.
        if(!forceFallback && haveSystemLoader && hasSystemVulkanDriver())
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
                qWarning("RenderThread: Could not load the bundled Vulkan loader %s: %s", loaderPath.constData(), ::dlerror());
        }
    });
#endif
}

/******************************************************************************
* Enumerates available GPU adapters for the given graphics API.
* Can be called on the GUI thread without creating a QRhi instance.
******************************************************************************/
QList<QRhiDriverInfo> RenderThread::enumerateAdapters(QRhi::Implementation graphicsApi)
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
QByteArray RenderThread::selectedAdapterName()
{
    QByteArray adapterName = qgetenv("OVITO_GPU_ADAPTER");
    if(adapterName.isEmpty()) {
        QSettings settings;
        adapterName = settings.value(QLatin1String(adapterSettingsKey())).toByteArray();
    }
    return adapterName;
}

/******************************************************************************
* Constructor. Determines the graphics API to use.
******************************************************************************/
RenderThread::RenderThread(UserInterface& ui, QRhi::Implementation graphicsApi)
    : UserInterfaceComponent(ui)
    , _graphicsApi(graphicsApi)
    , _selectedAdapterName(selectedAdapterName())
{
    OVITO_ASSERT(this_task::isMainThread());

#ifdef OVITO_DEBUG
    // Enable QRhi debug output for all backends. This is useful to detect RHI usage errors.
    // To also enable the Metal validation layer on macOS, run OVITO with these environment variables set:
    //    METAL_DEVICE_WRAPPER_TYPE=1
    //    MTL_SHADER_VALIDATION=1
    //    MTL_SHADER_VALIDATION_DEFAULT_STATE=all
    //    MTL_SHADER_VALIDATION_ENABLE_ERROR_REPORTING=1
    //    MTL_SHADER_VALIDATION_REPORT_TO_STDERR=1
    QLoggingCategory::setFilterRules(QStringLiteral("qt.rhi.general=true"));
#endif

#if QT_CONFIG(opengl)
    // The fallback surface must be created on the GUI thread.
    if(_graphicsApi == QRhi::OpenGLES2)
        _fallbackSurface.reset(QRhiGles2InitParams::newFallbackSurface());
#endif

#if QT_CONFIG(vulkan) && defined(Q_OS_LINUX)
    // The Vulkan instance must be created on the GUI thread, before any
    // QWindow with VulkanSurface type is shown. Each QWindow must call
    // setVulkanInstance() with this instance before being shown.
    if(_graphicsApi == QRhi::Vulkan) {
        // QVulkanInstance::create() dereferences QGuiApplicationPrivate::platformIntegration(),
        // which requires a fully constructed Qt application object. In headless mode the
        // application is created lazily, so it must be brought up now.
        Application::instance()->createQtApplication(false);
        prepareVulkanEnvironment();
        _vulkanInstance = std::make_unique<QVulkanInstance>();
        QList<QByteArray> instExts = QRhiVulkanInitParams::preferredInstanceExtensions();
#ifdef OVITO_USE_CUDA
        instExts.append(QByteArrayLiteral("VK_KHR_external_memory_capabilities"));
#endif
        _vulkanInstance->setExtensions(instExts);
        configureVulkanApiVersion(*_vulkanInstance);
#ifdef OVITO_DEBUG
        _vulkanInstance->setLayers({ "VK_LAYER_KHRONOS_validation" });
#endif
        if(!_vulkanInstance->create())
            qWarning("RenderThread: Failed to create Vulkan instance.");
    }
#endif

    start();
}

/******************************************************************************
* Destructor. Shuts down the render thread and releases all resources.
******************************************************************************/
RenderThread::~RenderThread()
{
    OVITO_ASSERT(this_task::isMainThread());
    _eventQueue.addEvent(new ShutdownEvent);
    wait();
}

/******************************************************************************
* Destroys the held render target (if any) and resets this guard to null.
******************************************************************************/
void RenderTarget::reset() noexcept
{
    if(_handle != 0) {
        OVITO_ASSERT(_thread);
        // Note: The reset() method may called from any thread, e.g., by the destructor of the coroutine state of RenderSettings::render().
        if(this_task::isMainThread()) {
            // When already in the main thread, we can directly call destroyTarget() and wait for it to complete.
            _thread->destroyTarget(_handle);
            _handle = 0;
            _thread.reset();
        }
        else {
            // When called from another thread, we need to post the reset operation to the main thread.
            Application::instance()->taskManager().submitWork([self = std::move(*this)]() mutable noexcept {
                OVITO_ASSERT(this_task::isMainThread());
                self.reset();
            });
        }
    }
}

/******************************************************************************
* Creates an onscreen render target backed by a new QWindow.
* Called from the GUI thread. Blocks until the render thread has created the
* RHI resources. Throws Exception on failure.
******************************************************************************/
std::pair<RenderTarget, QWindow*> RenderThread::createOnscreenTarget(ViewportWindow* viewportWindow)
{
    OVITO_ASSERT(this_task::isMainThread());

    RenderTargetHandle handle = _nextHandle++;

    // Create a QWindow on the GUI thread.
    QWindow* window = new QWindow();

    // Without Qt::FramelessWindowHint the HWND is created with WS_THICKFRAME, causing
    // QWindowsWindow to cache non-zero frame margins that are later used by setGeometry_sys()
    // to inflate the child window's size even after the window is reparented into a
    // QWindowContainer. Setting this flag ensures zero frame margins for the window's lifetime.
    window->setFlag(Qt::FramelessWindowHint);

    // Set the surface type matching the graphics API.
    switch(_graphicsApi) {
#if defined(Q_OS_WIN)
    case QRhi::D3D11:
    case QRhi::D3D12:
        window->setSurfaceType(QSurface::Direct3DSurface);
        break;
#endif
#if QT_CONFIG(metal)
    case QRhi::Metal:
        window->setSurfaceType(QSurface::MetalSurface);
        break;
#endif
#if QT_CONFIG(vulkan) && defined(Q_OS_LINUX)
    case QRhi::Vulkan:
        window->setVulkanInstance(_vulkanInstance.get());
        window->setSurfaceType(QSurface::VulkanSurface);
        break;
#endif
    default:
        window->setSurfaceType(QSurface::OpenGLSurface);
        break;
    }

    // Create the native platform window so that the Vulkan surface can be
    // obtained by the render thread when setting up the swap chain.
    window->create();

    // Install ourselves as event filter to handle PlatformSurface events.
    window->installEventFilter(this);

    // Fetch the warning icon image from the viewport window (must be done on the GUI thread).
    QImage warningIconImage = viewportWindow ? viewportWindow->warningIcon() : QImage{};

    // Post event to render thread and wait for the swap chain to be set up.
    QMutexLocker locker(&_syncMutex);
    _eventQueue.addEvent(new CreateOnscreenTargetEvent(handle, window,
                                                       std::move(warningIconImage), viewportWindow));
    _syncCondition.wait(&_syncMutex);

    // Check if the render thread reported an error.
    if(_syncError) {
        delete window;
        std::rethrow_exception(std::exchange(_syncError, {}));
    }

    return {RenderTarget(handle, shared_from_this()), window};
}

/******************************************************************************
* Creates a reusable offscreen render target at the given pixel resolution.
* Blocks the calling thread until the GPU resources have been allocated.
* Throws Exception on failure.
******************************************************************************/
RenderTarget RenderThread::createOffscreenTarget(const QSize& size, bool forPickingOnly)
{
    OVITO_ASSERT(this_task::isMainThread());
    OVITO_ASSERT(!size.isEmpty());

    RenderTargetHandle handle = _nextHandle++;
    QMutexLocker locker(&_syncMutex);
    _eventQueue.addEvent(new CreateOffscreenTargetEvent(handle, size, forPickingOnly));
    _syncCondition.wait(&_syncMutex);

    // Check if the render thread reported an error.
    if(_syncError)
        std::rethrow_exception(std::exchange(_syncError, {}));

    return RenderTarget(handle, shared_from_this());
}

/******************************************************************************
* Destroys a render target (onscreen or offscreen) and releases its GPU
* resources. Blocks the calling thread until all GPU resources have been
* released. For onscreen targets, does NOT destroy the QWindow.
******************************************************************************/
void RenderThread::destroyTarget(RenderTargetHandle handle)
{
    OVITO_ASSERT(this_task::isMainThread());

    QMutexLocker locker(&_syncMutex);
    _eventQueue.addEvent(new DestroyTargetEvent(handle));
    _syncCondition.wait(&_syncMutex);
}

/******************************************************************************
* Submits a frame graph for rendering to the specified onscreen target.
* Called from the GUI thread.
******************************************************************************/
void RenderThread::renderOnscreenFrame(RenderTargetHandle handle, OORef<FrameGraph> frameGraph,
                                       std::unique_ptr<SceneRenderer::Configuration> rendererConfig)
{
    OVITO_ASSERT(this_task::isMainThread());
    OVITO_ASSERT(frameGraph);

    // Onscreen: fire-and-forget.
    _eventQueue.addEvent(new RenderOnscreenFrameEvent(handle, std::move(frameGraph), std::move(rendererConfig)));
}

/******************************************************************************
* Submits a frame graph for rendering to the specified offscreen target.
* Called from the GUI thread.
******************************************************************************/
ScopedFuture<void> RenderThread::renderOffscreenFrame(RenderTargetHandle handle, OORef<FrameGraph> frameGraph,
                                                      std::unique_ptr<SceneRenderer::Configuration> rendererConfig,
                                                      std::shared_ptr<FrameBuffer> frameBuffer, TaskProgress& progress)
{
    OVITO_ASSERT(this_task::isMainThread());
    OVITO_ASSERT(frameGraph);
    OVITO_ASSERT(frameBuffer);
    OVITO_ASSERT(this_task::get());
    OVITO_ASSERT(this_task::ui());

    // Offscreen: create a promise/future pair for async completion notification.
    Promise<void> promise = Promise<void>::create();
    ScopedFuture<void> future = promise.future();
    promise.task()->inheritContextFromCurrentTask();

    _eventQueue.addEvent(new RenderOffscreenFrameEvent(handle, std::move(frameGraph), std::move(rendererConfig), std::move(frameBuffer), progress, std::move(promise)));
    return future;
}

/******************************************************************************
* Submits a frame graph for ambient occlusion sampling to the specified target.
* Called from the GUI thread.
* This method returns the contents of the object ID and primitive ID buffers as
* byte arrays, which are used to determine the AO contribution for each particle.
* The method may be called from a worker thread.
******************************************************************************/
ScopedFuture<std::pair<QByteArray, QByteArray>> RenderThread::renderAOFrame(RenderTargetHandle handle, OORef<FrameGraph> frameGraph)
{
    OVITO_ASSERT(this_task::get());
    OVITO_ASSERT(this_task::ui());
    OVITO_ASSERT(frameGraph);

    // Offscreen: create a promise/future pair for async completion notification.
    Promise<std::pair<QByteArray, QByteArray>> promise = Promise<std::pair<QByteArray, QByteArray>>::create();
    ScopedFuture<std::pair<QByteArray, QByteArray>> future = promise.future();
    promise.task()->inheritContextFromCurrentTask();

    _eventQueue.addEvent(new RenderAOFrameEvent(handle, std::move(frameGraph), std::move(promise)));
    return future;
}

/******************************************************************************
* Renders a picking pass for the given frame graph into the target's offscreen
* picking buffers.
* Called from the GUI thread or a worker thread.
******************************************************************************/
ScopedFuture<ObjectPickingBuffer> RenderThread::renderPickingFrame(RenderTargetHandle handle, OORef<FrameGraph> frameGraph,
    std::unique_ptr<SceneRenderer::Configuration> rendererConfig)
{
    OVITO_ASSERT(this_task::get());
    OVITO_ASSERT(this_task::ui());
    OVITO_ASSERT(frameGraph);
    OVITO_ASSERT(rendererConfig);

    Promise<ObjectPickingBuffer> promise = Promise<ObjectPickingBuffer>::create();
    ScopedFuture<ObjectPickingBuffer> future = promise.future();
    promise.task()->inheritContextFromCurrentTask();

    _eventQueue.addEvent(new RenderPickingFrameEvent(handle, std::move(frameGraph), std::move(rendererConfig), std::move(promise)));
    return future;
}

/******************************************************************************
* Requests a picking operation at the given position.
* Blocks the calling (GUI) thread until the result is available.
* Called from the GUI thread.
******************************************************************************/
std::optional<ViewportWindow::PickResult> RenderThread::requestPick(RenderTargetHandle handle, const QPointF& pos, int pickRadius)
{
    OVITO_ASSERT(this_task::isMainThread());
    OVITO_ASSERT(this_task::get());

    QMutexLocker locker(&_syncMutex);

    if(!_canServePickRequests) {
        // The render thread has not yet completed its first onscreen render, so it cannot serve pick requests yet.
        // This can happen when the user clicks on the viewport before the first frame has been rendered.
        // In this case, we simply return an empty result to indicate that nothing was picked.
        return std::nullopt;
    }

    _eventQueue.addEvent(new RequestPickEvent(handle, pos, pickRadius, this_task::get()));
    _syncCondition.wait(&_syncMutex);

    // Check if the render thread reported an error.
    if(_syncError)
        std::rethrow_exception(std::exchange(_syncError, {}));

    return _pickResult;
}

/******************************************************************************
* Handles events on the GUI thread. Installed as event filter on QWindows
* created by createOnscreenTarget() to handle surface lifecycle events.
******************************************************************************/
bool RenderThread::eventFilter(QObject* obj, QEvent* event)
{
    if(event->type() == QEvent::PlatformSurface) {
        auto* pse = static_cast<QPlatformSurfaceEvent*>(event);
        if(pse->surfaceEventType() == QPlatformSurfaceEvent::SurfaceAboutToBeDestroyed) {
            QWindow* window = qobject_cast<QWindow*>(obj);
            if(window) {
                RenderTargetHandle handle = findHandleForWindow(window);
                if(handle) {
                    QMutexLocker locker(&_syncMutex);
                    _eventQueue.addEvent(new SurfaceGoingAwayEvent(handle));
                    _syncCondition.wait(&_syncMutex);
                }
            }
        }
    }
    return false;
}

/******************************************************************************
* Finds the handle for a given QWindow by scanning all targets.
* Returns 0 if not found.
******************************************************************************/
RenderThread::RenderTargetHandle RenderThread::findHandleForWindow(QWindow* window) const
{
    // Note: _targets is accessed from the render thread, but the window pointers
    // are set once during creation and never changed, so this read is safe from
    // the GUI thread as long as the target has not been destroyed.
    for(const auto& [handle, state] : _targets) {
        if(state.window == window)
            return handle;
    }
    return 0;
}

/******************************************************************************
* Finds the adapter matching the user's selection in the given adapter list.
* Returns nullptr if no match is found (will use the default adapter).
******************************************************************************/
static QRhiAdapter* findSelectedAdapter(const QRhi::AdapterList& adapters, const QByteArray& selectedName)
{
    if(selectedName.isEmpty())
        return nullptr;
    for(QRhiAdapter* adapter : adapters) {
        if(adapter->info().deviceName == selectedName)
            return adapter;
    }
    return nullptr;
}

/******************************************************************************
* Creates the QRhi instance on the render thread.
******************************************************************************/
void RenderThread::createRhi()
{
    if(_rhi)
        return;

#if QT_CONFIG(opengl)
    if(_graphicsApi == QRhi::OpenGLES2) {
        QRhiGles2InitParams params;
        params.fallbackSurface = _fallbackSurface.get();
        // OpenGL does not support adapter enumeration.
        _rhi.reset(QRhi::create(QRhi::OpenGLES2, &params));
    }
#endif

#if QT_CONFIG(vulkan) && defined(Q_OS_LINUX)
    // Note: QRhi::enumerateAdapters() dereferences the QVulkanInstance's function table without
    // checking validity first, unlike QRhi::create(). If the Vulkan instance failed to initialize
    // (e.g. no Vulkan loader library present), calling it anyway crashes with a segfault instead
    // of failing gracefully. So we must skip straight to the "no _rhi" fallback below in that case.
    if(_graphicsApi == QRhi::Vulkan && _vulkanInstance && _vulkanInstance->isValid()) {
        QRhiVulkanInitParams params;
        params.inst = _vulkanInstance.get();
#ifdef OVITO_USE_CUDA
        params.deviceExtensions = {
            QByteArrayLiteral("VK_KHR_external_memory"),
            QByteArrayLiteral("VK_KHR_external_memory_fd"),
            QByteArrayLiteral("VK_KHR_dedicated_allocation"),
            QByteArrayLiteral("VK_KHR_get_memory_requirements2"),
        };
#endif
        QRhi::AdapterList adapters = QRhi::enumerateAdapters(QRhi::Vulkan, &params);
        QRhiAdapter* selectedAdapter = findSelectedAdapter(adapters, _selectedAdapterName);
        if(!selectedAdapter && !_selectedAdapterName.isEmpty()) {
            qWarning("Warning: Could not find user-requested GPU adapter '%s'; using default adapter instead.", _selectedAdapterName.constData());
        }
        _rhi.reset(QRhi::create(QRhi::Vulkan, &params, {}, nullptr, selectedAdapter));
        qDeleteAll(adapters);
    }
#endif

#ifdef Q_OS_WIN
    if(_graphicsApi == QRhi::D3D12) {
        QRhiD3D12InitParams params;
#ifdef OVITO_DEBUG
        params.enableDebugLayer = true;
#endif
        QRhi::AdapterList adapters = QRhi::enumerateAdapters(QRhi::D3D12, &params);
        QRhiAdapter* selectedAdapter = findSelectedAdapter(adapters, _selectedAdapterName);
        _rhi.reset(QRhi::create(QRhi::D3D12, &params, {}, nullptr, selectedAdapter));
        qDeleteAll(adapters);
        if(!_rhi)
            _graphicsApi = QRhi::D3D11; // D3D12 unavailable; fall back to D3D11.
    }
    if(_graphicsApi == QRhi::D3D11) {
        QRhiD3D11InitParams params;
#ifdef OVITO_DEBUG
        params.enableDebugLayer = true;
#endif
        QRhi::AdapterList adapters = QRhi::enumerateAdapters(QRhi::D3D11, &params);
        QRhiAdapter* selectedAdapter = findSelectedAdapter(adapters, _selectedAdapterName);
        _rhi.reset(QRhi::create(QRhi::D3D11, &params, {}, nullptr, selectedAdapter));
        qDeleteAll(adapters);
    }
#endif

#if QT_CONFIG(metal)
    if(_graphicsApi == QRhi::Metal) {
        QRhiMetalInitParams params;
        // Metal does not support adapter enumeration in QRhi.
        _rhi.reset(QRhi::create(QRhi::Metal, &params));
    }
#endif

    if(!_rhi) {
        // Build a human-readable name for the API that failed so the message is actionable.
        QString apiName;
        switch(_graphicsApi) {
            case QRhi::Vulkan:   apiName = QStringLiteral("Vulkan");   break;
            case QRhi::OpenGLES2: apiName = QStringLiteral("OpenGL");  break;
#ifdef Q_OS_WIN
            case QRhi::D3D11:    apiName = QStringLiteral("Direct3D 11"); break;
            case QRhi::D3D12:    apiName = QStringLiteral("Direct3D 12"); break;
#endif
#if QT_CONFIG(metal)
            case QRhi::Metal:    apiName = QStringLiteral("Metal");    break;
#endif
            default:             apiName = QStringLiteral("graphics"); break;
        }
        // Say what is actually missing. "No graphics driver was found" leaves the user to guess
        // which of the two parts of a Vulkan installation is absent -- the loader and the driver
        // come from different packages, and on a headless node both are usually missing.
        QString remedy;
#if QT_CONFIG(vulkan) && defined(Q_OS_LINUX)
        if(_graphicsApi == QRhi::Vulkan) {
#ifdef OVITO_VULKAN_FALLBACK_RELATIVE_PATH
            remedy = tr("On a headless server, an HPC compute node or in a container without GPU "
                "access, OVITO falls back to the software Vulkan driver that ships with the Linux "
                "package. Set the environment variable OVITO_VULKAN_FALLBACK=1 to force its use, "
                "or install a Vulkan driver on the system.");
#else
            if(!hasSystemVulkanLoader()) {
                remedy = tr("The Vulkan loader library (libvulkan.so.1) is not installed on this "
                    "system. Install it together with a driver:\n\n"
                    "  Debian/Ubuntu:  sudo apt install libvulkan1 mesa-vulkan-drivers\n"
                    "  RHEL/Fedora:    sudo dnf install vulkan-loader mesa-vulkan-drivers\n"
                    "  openSUSE:       sudo zypper install libvulkan1 libvulkan_lvp\n\n"
                    "mesa-vulkan-drivers provides a software renderer that needs no GPU. If the "
                    "machine has a GPU, install the driver of its vendor instead.");
            }
            else if(!hasSystemVulkanDriver()) {
                remedy = tr("A Vulkan loader is installed, but no Vulkan driver. Install one:\n\n"
                    "  Debian/Ubuntu:  sudo apt install mesa-vulkan-drivers\n"
                    "  RHEL/Fedora:    sudo dnf install mesa-vulkan-drivers\n"
                    "  openSUSE:       sudo zypper install libvulkan_lvp\n\n"
                    "This package provides a software renderer that needs no GPU. If the machine "
                    "has a GPU, install the driver of its vendor instead.");
            }
            else {
                remedy = tr("A Vulkan loader and at least one driver are installed, so the driver "
                    "itself appears to be failing. Run with VK_LOADER_DEBUG=all to see what the "
                    "Vulkan loader reports.");
            }
#ifdef OVITO_BUILD_PYPI
            remedy += tr("\n\nThe OVITO Python module does not ship a Vulkan driver of its own, "
                "because a driver matching the machine always works better than a bundled copy. "
                "If installing system packages is not possible here, note that the OVITO Pro "
                "program package does include a software Vulkan driver.");
#endif
#endif
        }
#endif
        if(remedy.isEmpty())
            remedy = tr("This typically means no compatible GPU or graphics driver was found.");

        throw Exception(tr("Could not initialize the %1 graphics backend. %2\n\n"
            "See the troubleshooting section of the OVITO user manual for details.")
            .arg(apiName).arg(remedy));
    }
}

/******************************************************************************
* The render thread's main loop.
******************************************************************************/
void RenderThread::run()
{
    while(_active) {
#ifdef Q_OS_DARWIN
        // The Metal backend requires that an autorelease pool is available on the rendering thread.
        QMacAutoReleasePool autoReleasePool;
#endif

        // Render all onscreen targets that need rendering.
        for(auto& [handle, state] : _targets) {
            // Process all render frame events for this particular onscreen target so that the viewport gets the
            // freshest frame graph available, reducing the chance of rendering with
            // stale projection parameters after a viewport resize.
            while(QEvent* e = _eventQueue.takeParticularEvent([handle](QEvent* e) {
                return static_cast<RenderThread::EventType>(e->type()) == RenderThread::EventType::RenderOnscreenFrame &&
                       static_cast<RenderThread::RenderOnscreenFrameEvent*>(e)->handle == handle;
            })) {
                processEvent(e);
                delete e;
            }
            if(state.window && state.needsRender)
                renderOnscreen(state);
        }

        // Process any remaining queued events.
        while(_eventQueue.hasMoreEvents()) {
            QEvent* e = _eventQueue.takeEvent(false);
            processEvent(e);
            delete e;
        }

        // If there are new pending frame graphs (submitted during event processing),
        // loop back to render them before going to sleep.
        bool hasPendingWork = std::ranges::any_of(_targets, [](const auto& pair) {
            return pair.second.window && pair.second.needsRender;
        });
        if(hasPendingWork)
            continue;

        // Block until the next event arrives.
        if(_active) {
            QEvent* e = _eventQueue.takeEvent(true);
            if(e) {
                processEvent(e);
                delete e;
            }
        }
    }
}

/******************************************************************************
* Processes a single event on the render thread.
******************************************************************************/
void RenderThread::processEvent(QEvent* e)
{
    switch(static_cast<EventType>(e->type())) {
    case EventType::CreateOnscreenTarget:  handleCreateOnscreenTarget(static_cast<CreateOnscreenTargetEvent*>(e)); break;
    case EventType::CreateOffscreenTarget: handleCreateOffscreenTarget(static_cast<CreateOffscreenTargetEvent*>(e)); break;
    case EventType::DestroyTarget:         handleDestroyTarget(static_cast<DestroyTargetEvent*>(e)); break;
    case EventType::RenderOnscreenFrame:   handleRenderOnscreenFrame(static_cast<RenderOnscreenFrameEvent*>(e)); break;
    case EventType::RenderOffscreenFrame:  handleRenderOffscreenFrame(static_cast<RenderOffscreenFrameEvent*>(e)); break;
    case EventType::RenderAOFrame:         handleRenderAOFrame(static_cast<RenderAOFrameEvent*>(e)); break;
    case EventType::RenderPickingFrame:    handleRenderPickingFrame(static_cast<RenderPickingFrameEvent*>(e)); break;
    case EventType::RequestPick:           handleRequestPick(static_cast<RequestPickEvent*>(e)); break;
    case EventType::SurfaceGoingAway:      handleSurfaceGoingAway(static_cast<SurfaceGoingAwayEvent*>(e)); break;
    case EventType::SuspendTarget:         handleSuspendTarget(static_cast<SuspendTargetEvent*>(e)); break;
    case EventType::Shutdown:              handleShutdown(); break;
    }
}

/******************************************************************************
* Handles a CreateOnscreenTarget event: initializes QRhi (if needed) and
* creates the swap chain resources for a new onscreen render target.
******************************************************************************/
void RenderThread::handleCreateOnscreenTarget(CreateOnscreenTargetEvent* event)
{
    QMutexLocker locker(&_syncMutex);
    try {
        // Create QRhi on first target registration.
        createRhi();

        // Set up per-target rendering state.
        TargetState& state = _targets[event->handle];
        state.window = event->window;

        // Create RHI swap chain.
        state.swapChain.reset(rhi()->newSwapChain());
        if(!state.swapChain)
            throw Exception(tr("Failed to create swap chain for onscreen render target."));
        state.swapChainDepthStencil.reset(rhi()->newRenderBuffer(
            QRhiRenderBuffer::DepthStencil, QSize(), 1,
            QRhiRenderBuffer::UsedWithSwapChainOnly));
        if(!state.swapChainDepthStencil)
            throw Exception(tr("Failed to create depth/stencil buffer for onscreen render target."));
        state.swapChain->setWindow(event->window);
        if(qEnvironmentVariableIsSet("OVITO_DISABLE_VSYNC")) // This can be set when running benchmarks to measure rendering performance without vsync throttling.
            state.swapChain->setFlags(QRhiSwapChain::NoVSync);
        state.swapChain->setDepthStencil(state.swapChainDepthStencil.get());
        state.swapChainRenderPassDesc.reset(state.swapChain->newCompatibleRenderPassDescriptor());
        if(!state.swapChainRenderPassDesc)
            throw Exception(tr("Failed to create render pass descriptor for onscreen render target."));
        state.swapChain->setRenderPassDescriptor(state.swapChainRenderPassDesc.get());

        // Store a weak reference to the ViewportWindow for warning reporting and fatal error notification.
        state.viewportWindow = OOWeakRef<ViewportWindow>(event->viewportWindow);

        // Create the warning indicator renderer if an icon image was provided.
        if(!event->warningIconImage.isNull())
            state.warningIndicator = std::make_unique<WarningIndicatorRenderer>(this, std::move(event->warningIconImage));
    }
    catch(...) {
        _syncError = std::current_exception();
        _targets.erase(event->handle);
    }
    _syncCondition.wakeOne();
}

/******************************************************************************
* Handles a CreateOffscreenTarget event: initializes QRhi (if needed) and
* allocates GPU resources for a new offscreen render target.
******************************************************************************/
void RenderThread::handleCreateOffscreenTarget(CreateOffscreenTargetEvent* event)
{
    QMutexLocker locker(&_syncMutex);
    try {
        // Create QRhi if not yet initialized.
        createRhi();

        TargetState& state = _targets[event->handle];
        state.offscreenSize = event->size;
        state.forPickingOnly = event->forPickingOnly;

        // Allocate GPU resources for the offscreen target (only for visual rendering).
        // For picking-only targets, renderPickingPass() takes care of allocating the target buffer.
        if(!state.forPickingOnly) {
            state.colorTexture.reset(rhi()->newTexture(QRhiTexture::RGBA8, event->size, 1, QRhiTexture::RenderTarget));
            if(!state.colorTexture || !state.colorTexture->create())
                throw Exception(tr("Failed to create color texture for offscreen render target."));
            state.offscreenDepthStencil.reset(rhi()->newRenderBuffer(QRhiRenderBuffer::DepthStencil, event->size));
            if(!state.offscreenDepthStencil || !state.offscreenDepthStencil->create())
                throw Exception(tr("Failed to create depth/stencil buffer for offscreen render target."));
            QRhiTextureRenderTargetDescription desc(state.colorTexture.get());
            desc.setDepthStencilBuffer(state.offscreenDepthStencil.get());
            state.offscreenRenderTarget.reset(rhi()->newTextureRenderTarget(desc));
            if(!state.offscreenRenderTarget)
                throw Exception(tr("Failed to create offscreen render target."));
            state.offscreenRenderPassDesc.reset(state.offscreenRenderTarget->newCompatibleRenderPassDescriptor());
            if(!state.offscreenRenderPassDesc)
                throw Exception(tr("Failed to create render pass descriptor for offscreen render target."));
            state.offscreenRenderTarget->setRenderPassDescriptor(state.offscreenRenderPassDesc.get());
            if(!state.offscreenRenderTarget->create())
                throw Exception(tr("Failed to initialize offscreen render target."));
        }
    }
    catch(...) {
        _syncError = std::current_exception();
        _targets.erase(event->handle);
    }
    _syncCondition.wakeOne();
}

/******************************************************************************
* Handles a DestroyTarget event: releases all GPU resources for the target
* and removes it from the targets map.
******************************************************************************/
void RenderThread::handleDestroyTarget(DestroyTargetEvent* event)
{
    auto it = _targets.find(event->handle);
    if(it != _targets.end()) {
        // Release resources associated with the target.
        _targets.erase(it);
    }
    QMutexLocker locker(&_syncMutex);
    _syncCondition.wakeOne();
}

/******************************************************************************
* Handles a RenderOnscreenFrame event: stores the pending frame
* graph for onscreen targets.
******************************************************************************/
void RenderThread::handleRenderOnscreenFrame(RenderOnscreenFrameEvent* event)
{
    auto it = _targets.find(event->handle);
    if(it != _targets.end()) {
        TargetState& state = it->second;
        OVITO_ASSERT(state.window); // RenderOnscreenFrame events should only be posted for onscreen targets.
        // Onscreen target: store pending frame graph, set render flag.
        state.pendingFrameGraph = std::move(event->frameGraph);
        state.rendererConfig = std::move(event->rendererConfig);
        state.needsRender = true;
        // Interrupt any ongoing refinement – the new FrameGraph will restart from scratch.
        state.refinementIteration = 0;
    }
}

/******************************************************************************
* Handles a RenderOffscreenFrame event: renders immediately and signals completion.
******************************************************************************/
void RenderThread::handleRenderOffscreenFrame(RenderOffscreenFrameEvent* event)
{
    auto it = _targets.find(event->handle);
    if(it != _targets.end()) {
        TargetState& state = it->second;
        OVITO_ASSERT(!state.window); // RenderOffscreenFrame events should only be posted for offscreen targets.
        OVITO_ASSERT(!state.forPickingOnly);

        // Offscreen target: render immediately.
        renderOffscreen(state, event);
    }
}

/******************************************************************************
* Handles a RenderAOFrame event.
******************************************************************************/
void RenderThread::handleRenderAOFrame(RenderAOFrameEvent* event)
{
    auto it = _targets.find(event->handle);
    if(it != _targets.end()) {
        TargetState& state = it->second;
        OVITO_ASSERT(!state.window); // AO sampling is only supported for offscreen targets.
        OVITO_ASSERT(state.forPickingOnly); // Target must have been created with createOffscreenTarget(..., forPickingOnly=true).

        // Create a Task::Scope to associate the rendering work with the caller's task for cancellation support.
        Task::Scope taskScope(event->promise.task());
        OVITO_ASSERT(this_task::ui());

        // Offscreen target for ambient occlusion sampling: render the picking pass to the offscreen buffer.
        state.lastRenderedFrameGraph = event->frameGraph;
        state.pickBufferValid = false;
        if(!state.rendererConfig)
            state.rendererConfig = std::make_unique<StandardRenderer::Configuration>(false);
        renderPickingPass(state, state.offscreenSize);
        if(state.pickBufferValid) {
            event->promise.setResult(std::make_pair(std::move(state.pickObjectIdData), std::move(state.pickPrimitiveIdData)));
            event->promise.setFinished();
        }
    }
}

/******************************************************************************
* Handles a RenderPickingFrame event: renders the picking pass of the given
* frame graph and reads the picking buffers back to CPU memory.
******************************************************************************/
void RenderThread::handleRenderPickingFrame(RenderPickingFrameEvent* event)
{
    auto it = _targets.find(event->handle);
    if(it == _targets.end())
        return;

    TargetState& state = it->second;
    OVITO_ASSERT(!state.window); // Picking passes are rendered into offscreen targets only.
    OVITO_ASSERT(state.forPickingOnly); // Target must have been created with createOffscreenTarget(..., forPickingOnly=true).

    // Create a Task::Scope to associate the rendering work with the caller's task for cancellation support.
    Task::Scope taskScope(event->promise.task());
    OVITO_ASSERT(this_task::ui());

    // The picking pass is executed by the same code path as the onscreen picking of the classic frontend.
    state.lastRenderedFrameGraph = event->frameGraph;
    state.rendererConfig = std::move(event->rendererConfig);
    state.pickBufferValid = false;

    if(!state.offscreenSize.isEmpty())
        renderPickingPass(state, state.offscreenSize);

    // Hand the result over to the caller. The picking buffer takes ownership of the readback data;
    // if the picking pass failed, an invalid buffer is delivered instead.
    if(state.pickBufferValid) {
        ObjectPickingBuffer buffer(std::move(state.pickingMap),
            std::move(state.pickObjectIdData), std::move(state.pickPrimitiveIdData), std::move(state.pickDepthData),
            state.pickBufferSize, event->frameGraph->projectionParams());

        // The readback data has been handed over; the target holds no picking buffer anymore.
        state.pickBufferSize = QSize();
        state.pickBufferValid = false;

        event->promise.setResult(std::move(buffer));
        event->promise.setFinished();
    }
    else {
        event->promise.setResult(ObjectPickingBuffer());
        event->promise.setFinished();
    }
}

/******************************************************************************
* Handles a RequestPick event: renders the picking pass if the buffer is stale,
* then looks up the object ID at the requested position.
******************************************************************************/
void RenderThread::handleRequestPick(RequestPickEvent* event)
{
    QMutexLocker locker(&_syncMutex);
    try {
        Task::Scope taskScope(event->task);

        _pickResult.reset();
        auto it = _targets.find(event->handle);
        if(it != _targets.end()) {
            TargetState& state = it->second;
            OVITO_ASSERT(state.window);

            // Re-render the picking pass if the buffer is stale.
            if(!state.pickBufferValid && state.lastRenderedFrameGraph && state.rendererConfig) {
                // Use the same size as the swap chain's current pixel size.
                const QSize size = state.swapChain->currentPixelSize();
                // Render the frame graph from the last visual pass to the picking render buffer.
                if(!size.isEmpty())
                    renderPickingPass(state, size);
            }
            // Look up the object ID at the requested position.
            if(state.pickBufferValid && !this_task::isCanceled())
                _pickResult = lookupPickBuffer(state, event->position, event->pickRadius);
        }
    }
    catch(...) {
        _syncError = std::current_exception();
    }
    _syncCondition.wakeOne();
}

/******************************************************************************
* Handles a SurfaceGoingAway event: releases the swap chain and other
* resources before the native window surface is destroyed.
******************************************************************************/
void RenderThread::handleSurfaceGoingAway(SurfaceGoingAwayEvent* event)
{
    QMutexLocker locker(&_syncMutex);
    auto it = _targets.find(event->handle);
    if(it != _targets.end()) {
        it->second.rendererConfig.reset();
        it->second.pendingFrameGraph.reset();
        it->second.lastRenderedFrameGraph.reset();
        it->second.refinementIteration = 0;
        it->second.renderers.reset();
        it->second.pickObjectIdData.clear();
        it->second.pickPrimitiveIdData.clear();
        it->second.pickDepthData.clear();
        it->second.pickBufferSize = QSize();
        it->second.pickBufferValid = false;
        it->second.needsRender = false;  // Prevent re-entering renderOnscreen() for the surface that is going away.
        if(it->second.hasSwapChain) {
            it->second.hasSwapChain = false;
            it->second.swapChain->destroy();
        }
    }
    _syncCondition.wakeOne();
}

/******************************************************************************
* Posts a SuspendTargetEvent to release GPU resources for a hidden viewport.
* Fire-and-forget: does not block the calling thread.
******************************************************************************/
void RenderThread::suspendTarget(RenderTargetHandle handle)
{
    _eventQueue.addEvent(new SuspendTargetEvent(handle));
}

/******************************************************************************
* Handles a SuspendTarget event: releases GPU resources (swap chain, renderer
* state, picking buffers) for a viewport that has been hidden but whose platform
* surface remains valid. The QRhiSwapChain C++ object is kept alive so that
* renderOnscreen() can call createOrResize() to resume without any special logic.
******************************************************************************/
void RenderThread::handleSuspendTarget(SuspendTargetEvent* event)
{
    auto it = _targets.find(event->handle);
    if(it != _targets.end()) {
        it->second.rendererConfig.reset();
        it->second.pendingFrameGraph.reset();
        it->second.lastRenderedFrameGraph.reset();
        it->second.refinementIteration = 0;
        it->second.renderers.reset();
        it->second.pickObjectIdData.clear();
        it->second.pickPrimitiveIdData.clear();
        it->second.pickDepthData.clear();
        it->second.pickBufferSize = QSize();
        it->second.pickBufferValid = false;
        if(it->second.hasSwapChain) {
            it->second.hasSwapChain = false;
            it->second.swapChain->destroy();
        }
    }
}

/******************************************************************************
* Handles a Shutdown event: releases all GPU resources and stops the render
* thread's main loop.
******************************************************************************/
void RenderThread::handleShutdown()
{
    // Release shared rendering devices and cached graphics pipelines.
    discardCachedResources();

    // Destroy per-target states and release associated resources.
    _targets.clear();

    // Release picking render pass resources.
    _pickTarget.reset();
    _pickRenderPassDesc.reset();
    _pickDepthTexture.reset();
    _pickObjectIdTexture.reset();
    _pickPrimitiveIdTexture.reset();

    // Release cached QRhi resource cache.
    _rhiResourceCache.reset();

    // Then release QRhi.
    _rhi.reset();

    _active = false;
}

/******************************************************************************
* Reports a fatal rendering error to the owning ViewportWindow (for onscreen targets).
******************************************************************************/
void RenderThread::reportFatalError(TargetState& state, const Exception& exception)
{
    if(auto vpw = state.viewportWindow.lock()) {
        // Post the error notification to the GUI thread.
        // The fatalError signal will be handled by the ViewportsPanel class,
        // which will show a message box and disable the viewport.
        ObjectExecutor(vpw).execute([vpw, exception]() noexcept {
            Q_EMIT vpw->fatalError(exception);
        });

        // Clear the reference to the viewport window to avoid repeated error notifications.
        state.viewportWindow.reset();
    }
}

/******************************************************************************
* Renders a single onscreen target's pending frame graph.
* Called on the render thread.
******************************************************************************/
void RenderThread::renderOnscreen(TargetState& state)
{
    OVITO_ASSERT(QThread::currentThread() == this);
    OVITO_ASSERT(state.window);

    state.needsRender = false;

    // Never realize the native window from the render thread: if the GUI thread has already
    // destroyed the platform surface, skip rendering instead of recreating it. QWindow::handle()
    // returns the existing QPlatformWindow without creating one (unlike winId()), so this avoids
    // a cross-thread CreateWindowEx call that would deadlock against the GUI thread.
    if(!state.window->handle())
        return;

    // A newly-arrived FrameGraph always takes priority over an ongoing refinement.
    int refinementIteration;
    OORef<FrameGraph> frameGraph;
    if(state.pendingFrameGraph) {
        frameGraph = std::move(state.pendingFrameGraph);
        refinementIteration = state.refinementIteration = 0;
        state.lastRenderedFrameGraph.reset();   // discard previous refinement FrameGraph
    }
    else if(state.refinementIteration > 0) {
        frameGraph = state.lastRenderedFrameGraph;  // OORef copy (shared ownership)
        refinementIteration = state.refinementIteration;
    }
    else {
        frameGraph = nullptr;
        refinementIteration = 0;
    }

    // Build or resize the swap chain if needed.
    const QSize surfaceSize = state.swapChain->surfacePixelSize();
    if(surfaceSize.isEmpty())
        return;
    if(!state.hasSwapChain || state.swapChain->currentPixelSize() != surfaceSize) {
        state.hasSwapChain = state.swapChain->createOrResize();
        if(!state.hasSwapChain)
            return;
    }

    // Begin recording commands for this target.
    QRhi::FrameOpResult result = _rhi->beginFrame(state.swapChain.get());
    if(result == QRhi::FrameOpSwapChainOutOfDate) {
        state.hasSwapChain = state.swapChain->createOrResize();
        if(!state.hasSwapChain)
            return;
        result = rhi()->beginFrame(state.swapChain.get());
    }
    if(result != QRhi::FrameOpSuccess) {
        qWarning("RenderThread: beginFrame() failed with %d", result);
        return;
    }

    // Notify the GUI thread as early as possible so it can start preparing the next animation
    // frame concurrently with this render pass. This is particularly important when using the
    // ANARI renderer, which blocks the render thread for a long time in renderFrameGraph() below.
    // We emit here (after beginFrame() succeeded) rather than after endFrame() because at this
    // point the frame is committed: endFrame() is guaranteed to be called regardless of whether
    // rendering succeeds or throws an exception.
    if(refinementIteration == 0 && frameGraph && !frameGraph->isPreliminaryState()) {
        if(OORef<ViewportWindow> vpWin = state.viewportWindow.lock()) {
            ObjectExecutor(vpWin).execute([vpWin=vpWin.get()]() noexcept {
                Q_EMIT vpWin->frameCompleted();
            });
        }
    }

    QRhi::EndFrameFlags endFrameFlags = {};
    try {
        // Establish a local Task::Scope for renderFrameGraph().
        auto promise = Promise<void>::create();
        promise.task()->setIsInteractive();
        promise.task()->setUserInterface(ui().shared_from_this());
        Task::Scope taskScope(promise.task());

        QRhiCommandBuffer* cb = state.swapChain->currentFrameCommandBuffer();
        if(frameGraph && state.rendererConfig) {
            // Record the render pass using the shared code path.
            renderFrameGraph(cb, state.swapChain->currentFrameRenderTarget(), state, frameGraph.get(), *state.rendererConfig,
                             TaskProgress::Ignore, /*isPickingPass=*/false, refinementIteration);
        }
        else {
            // If no frame graph is available, at least clear the render target to avoid showing uninitialized memory.
            cb->beginPass(state.swapChain->currentFrameRenderTarget(), Qt::black, { 1.0f, 0 });
            cb->endPass();
        }
    }
    catch(const OperationCanceled&) {
        // If the operation was canceled, we can simply end the frame and skip presenting.
        // The next iteration will pick up the latest pending frame graph (if any) and render it.
        endFrameFlags |= QRhi::EndFrameFlag::SkipPresent;
    }
    catch(const Exception& ex) {
        qWarning("RenderThread: Exception during viewport window rendering");
        endFrameFlags |= QRhi::EndFrameFlag::SkipPresent;
        reportFatalError(state, ex);
    }
    catch(const std::bad_alloc&) {
        qWarning("RenderThread: Caught std::bad_alloc exception. Ran out of memory during viewport window rendering");
        endFrameFlags |= QRhi::EndFrameFlag::SkipPresent;
        reportFatalError(state, Exception(tr("Ran out of memory during viewport rendering.")));
    }

    // Submit commands and present. This returns quickly on modern APIs
    // (Metal, Vulkan, D3D12) while the GPU processes asynchronously,
    // allowing the CPU to immediately start recording the next target's commands.
    rhi()->endFrame(state.swapChain.get(), endFrameFlags);

    // After every rendered iteration, refresh the frame graph used for picking so that
    // pick results always correspond to what is on screen.
    if(frameGraph && state.lastRenderedFrameGraph != frameGraph) {
        state.lastRenderedFrameGraph = frameGraph;
        state.pickBufferValid = false;
        state.pickObjectIdData.clear();
        state.pickPrimitiveIdData.clear();
        state.pickDepthData.clear();
    }

    // Decide whether to schedule another refinement iteration.
    if(frameGraph && state.rendererConfig && state.renderers.visual && state.renderers.visual->refinementIterationNeeded(*frameGraph, *state.rendererConfig, refinementIteration)) {
        state.refinementIteration++;
        state.needsRender = true;   // Run loop will render next iteration.
    }
    else {
        state.refinementIteration = 0;
        // lastRenderedFrameGraph is kept for picking.
    }

    _canServePickRequests = true;  // The render thread has completed its first onscreen render, so it can now serve pick requests.
}

/******************************************************************************
* Renders a frame graph to a single offscreen target.
* Called on the render thread.
******************************************************************************/
void RenderThread::renderOffscreen(TargetState& state, RenderOffscreenFrameEvent* event)
{
    OVITO_ASSERT(event->rendererConfig);
    state.rendererConfig = std::move(event->rendererConfig);

    bool isQRhiFrameActive = false;
    try {
        // Create a Task::Scope to associate the rendering work with the caller's task for cancellation support.
        Task::Scope taskScope(event->promise.task());
        OVITO_ASSERT(this_task::ui());

        state.lastRenderedFrameGraph.reset();  // discard previous refinement FrameGraph (if any)
        FrameBuffer* frameBuffer = event->frameBuffer.get();

        int refinementIteration = 0;
        do {
            // Begin an offscreen frame.
            QRhiCommandBuffer* cb = nullptr;
            QRhi::FrameOpResult result = rhi()->beginOffscreenFrame(&cb);
            if(result != QRhi::FrameOpSuccess)
                throw Exception(tr("Failed to begin QRhi offscreen frame (error %1).").arg(static_cast<int>(result)));
            isQRhiFrameActive = true;

            // Use the shared rendering code path.
            renderFrameGraph(cb, state.offscreenRenderTarget.get(), state, event->frameGraph.get(), *state.rendererConfig,
                             event->progress, /*isPickingPass=*/false, refinementIteration);

            // Read back the color texture to CPU memory.
            QRhiReadbackResult readbackResult;
            QRhiResourceUpdateBatch* readbackBatch = rhi()->nextResourceUpdateBatch();
            readbackBatch->readBackTexture(QRhiReadbackDescription(state.colorTexture.get()), &readbackResult);
            cb->resourceUpdate(readbackBatch);

            rhi()->endOffscreenFrame();
            isQRhiFrameActive = false;

            // Convert the readback data into a QImage and blit into the FrameBuffer.
            if(readbackResult.data.isEmpty())
                throw Exception(tr("QRhi offscreen rendering produced no image data."));
            if(readbackResult.data.size() != state.offscreenSize.width() * state.offscreenSize.height() * 4)
                throw Exception(tr("QRhi offscreen rendering produced image data of unexpected size."));
            char* imageData = readbackResult.data.data();
            QImage renderedImage(
                reinterpret_cast<uchar*>(imageData),
                state.offscreenSize.width(), state.offscreenSize.height(),
                QImage::Format_RGBA8888,
                [](void* info) {
                    // This cleanup function is called by QImage when it's done with the data.
                    // We can safely free the QByteArray readback buffer here.
                    delete static_cast<QByteArray*>(info);
                },
                new QByteArray(std::move(readbackResult.data))  // Pass ownership of the readback result to the cleanup function.
            );

            // Rescale supersampled image to output size.
            QImage scaledImage =
                (frameBuffer->viewportRect().size() == renderedImage.size())
                ? std::move(renderedImage)
                : std::move(renderedImage).scaled(frameBuffer->viewportRect().size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

            // Transfer rendered image to the output frame buffer. Hold the framebuffer's
            // image mutex so the GUI thread doesn't observe a torn d-pointer while we
            // replace _image, and so the previous QImage's pixel buffer can't be freed
            // out from under a snapshot the GUI thread is about to take.
            {
                QMutexLocker locker(&frameBuffer->imageMutex());
                if(!frameBuffer->image().isNull() && (frameBuffer->viewportRect() != frameBuffer->image().rect() || frameBuffer->image().rect() != scaledImage.rect())) {
                    // Partial update: blit the scaled image into the existing framebuffer image.
                    QPainter painter(&frameBuffer->image());
                    painter.setCompositionMode(QPainter::CompositionMode_Source);
                    painter.drawImage(frameBuffer->viewportRect(), scaledImage);
                }
                else {
                    // Full update: replace the framebuffer image with the newly rendered image.
                    frameBuffer->image() = std::move(scaledImage);
                }
                // Update FrameBuffer after every iteration (intermediate display).
                frameBuffer->update(frameBuffer->viewportRect());
                frameBuffer->commitChanges();
            }

            refinementIteration++;
        }
        while(state.renderers.visual &&
            state.renderers.visual->refinementIterationNeeded(*event->frameGraph, *state.rendererConfig, refinementIteration) &&
            !event->promise.task()->isCanceled());

        // Hold on to the last rendered frame graph to keep cached resources alive until the next animation frame is being rendered.
        state.lastRenderedFrameGraph = event->frameGraph;

        event->promise.setFinished();
    }
    catch(const OperationCanceled&) {
        if(isQRhiFrameActive) {
            // If the operation was canceled while a QRhi frame is active, we need to end it before processing the next event.
            rhi()->endOffscreenFrame();
        }
    }
    catch(...) {
        if(isQRhiFrameActive) {
            // If an exception was thrown while a QRhi frame is active, we need to end it before processing the next event.
            rhi()->endOffscreenFrame();
        }
        event->promise.captureExceptionAndFinish();
    }
}

/******************************************************************************
* Ensures the picking GPU resources are allocated and match the given size.
* Creates or recreates them as needed.
* Called on the render thread.
******************************************************************************/
void RenderThread::ensurePickingResources(const QSize& size)
{
    OVITO_ASSERT(QThread::currentThread() == this);
    OVITO_ASSERT(!size.isEmpty());

    // Check if existing resources already match the requested size.
    if(_pickObjectIdTexture && _pickObjectIdTexture->pixelSize() == size)
        return;

    // Invalidate old resources before (re-)creating.
    _pickTarget.reset();
    _pickRenderPassDesc.reset();
    _pickDepthTexture.reset();
    _pickObjectIdTexture.reset();
    _pickPrimitiveIdTexture.reset();

    // Create two R32UI textures for object ID and primitive ID.
    // Using separate single-channel uint32 textures to match ANARI's objectId/primitiveId channels.
    _pickObjectIdTexture.reset(rhi()->newTexture(QRhiTexture::R32UI, size, 1, QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource));
    if(!_pickObjectIdTexture->create()) {
        qWarning("RenderThread: Failed to create picking objectId texture.");
        _pickObjectIdTexture.reset();
        return;
    }

    _pickPrimitiveIdTexture.reset(rhi()->newTexture(QRhiTexture::R32UI, size, 1, QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource));
    if(!_pickPrimitiveIdTexture->create()) {
        qWarning("RenderThread: Failed to create picking primitiveId texture.");
        _pickObjectIdTexture.reset();
        _pickPrimitiveIdTexture.reset();
        return;
    }

    // Create a D32F depth texture so it can be read back to CPU memory for world-position reconstruction.
    _pickDepthTexture.reset(rhi()->newTexture(QRhiTexture::D32F, size, 1, QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource));
    if(!_pickDepthTexture->create()) {
        qWarning("RenderThread: Failed to create picking depth texture.");
        _pickObjectIdTexture.reset();
        _pickPrimitiveIdTexture.reset();
        _pickDepthTexture.reset();
        return;
    }

    // Create the render target with two color attachments (objectId + primitiveId) and a readable depth texture.
    QRhiColorAttachment objectIdAttachment(_pickObjectIdTexture.get());
    QRhiColorAttachment primitiveIdAttachment(_pickPrimitiveIdTexture.get());
    QRhiTextureRenderTargetDescription desc;
    desc.setColorAttachments({objectIdAttachment, primitiveIdAttachment});
    desc.setDepthTexture(_pickDepthTexture.get());
    _pickTarget.reset(rhi()->newTextureRenderTarget(desc));
    _pickRenderPassDesc.reset(_pickTarget->newCompatibleRenderPassDescriptor());
    _pickTarget->setRenderPassDescriptor(_pickRenderPassDesc.get());
    if(!_pickTarget->create()) {
        qWarning("RenderThread: Failed to create picking render target.");
        _pickObjectIdTexture.reset();
        _pickPrimitiveIdTexture.reset();
        _pickDepthTexture.reset();
        _pickTarget.reset();
        _pickRenderPassDesc.reset();
        return;
    }
}

/******************************************************************************
* Renders the picking pass using the target's lastRenderedFrameGraph.
* Reads back the entire picking texture to pickBufferData.
* Called on the render thread.
******************************************************************************/
void RenderThread::renderPickingPass(TargetState& state, const QSize& size)
{
    OVITO_ASSERT(QThread::currentThread() == this);
    OVITO_ASSERT(state.lastRenderedFrameGraph);
    OVITO_ASSERT(state.pickBufferValid == false);
    OVITO_ASSERT(state.rendererConfig);

    // Ensure picking GPU resources are allocated at the right size.
    ensurePickingResources(size);
    if(!_pickTarget)
        return;

    // Begin an offscreen frame for the picking pass.
    QRhiCommandBuffer* cb = nullptr;
    QRhi::FrameOpResult result = rhi()->beginOffscreenFrame(&cb);
    if(result != QRhi::FrameOpSuccess) {
        qWarning("RenderThread: beginOffscreenFrame() for picking failed with %d", result);
        return;
    }

    // Reset the picking map before the picking pass so it can be rebuilt.
    state.pickingMap.reset();

    try {
        // Record the render pass using the shared code path.
        renderFrameGraph(cb, _pickTarget.get(), state, state.lastRenderedFrameGraph.get(), *state.rendererConfig, TaskProgress::Ignore, /*isPickingPass=*/true);
    }
    catch(const OperationCanceled&) {
        // If the operation was canceled, we still need to end the offscreen frame before processing the next event.
        rhi()->endOffscreenFrame();
        return;
    }
    catch(const std::bad_alloc&) {
        qWarning("RenderThread: Caught std::bad_alloc exception. Ran out of memory during viewport window rendering");
        // If the operation was canceled, we still need to end the offscreen frame before processing the next event.
        rhi()->endOffscreenFrame();
        return;
    }
    catch(const Exception& ex) {
        qWarning("RenderThread: Exception during viewport window picking pass rendering");
        ex.logError();
        // If an exception was thrown while a QRhi frame is active, we need to end it before processing the next event.
        rhi()->endOffscreenFrame();
        return;
    }

    // Read back all three picking textures (instanceId, primitiveId, depth) to CPU memory.
    QRhiReadbackResult objectReadback, primitiveReadback, depthReadback;
    QRhiResourceUpdateBatch* readbackBatch = rhi()->nextResourceUpdateBatch();
    readbackBatch->readBackTexture(QRhiReadbackDescription(_pickObjectIdTexture.get()), &objectReadback);
    readbackBatch->readBackTexture(QRhiReadbackDescription(_pickPrimitiveIdTexture.get()), &primitiveReadback);
    readbackBatch->readBackTexture(QRhiReadbackDescription(_pickDepthTexture.get()), &depthReadback);
    cb->resourceUpdate(readbackBatch);

    // End the offscreen frame. After this call, the readback data is available.
    rhi()->endOffscreenFrame();

    // Store the readback data.
    state.pickObjectIdData    = std::move(objectReadback.data);
    state.pickPrimitiveIdData = std::move(primitiveReadback.data);
    state.pickDepthData       = std::move(depthReadback.data);
    state.pickBufferSize      = size;
    state.pickBufferValid     = true;
}

/******************************************************************************
* Looks up the nearest non-zero object ID in the pick buffer around the
* given position and resolves it into a pick result.
* Called on the render thread.
******************************************************************************/
std::optional<ViewportWindow::PickResult> RenderThread::lookupPickBuffer(const TargetState& state, const QPointF& pos, int radius) const
{
    OVITO_ASSERT(state.pickBufferValid);
    OVITO_ASSERT(!state.pickObjectIdData.isEmpty());
    OVITO_ASSERT(!state.pickPrimitiveIdData.isEmpty());
    OVITO_ASSERT(!state.pickDepthData.isEmpty());
    OVITO_ASSERT(state.lastRenderedFrameGraph);

    return state.pickingMap.lookupPickResult(state.pickObjectIdData, state.pickPrimitiveIdData, state.pickDepthData,
        state.pickBufferSize, pos, radius, state.lastRenderedFrameGraph->projectionParams());
}

/******************************************************************************
* Executes the resource upload and render pass for a frame graph into the
* given render target. This is the shared rendering code path used by both
* viewport (swap chain) and offscreen rendering.
* Called on the render thread.
******************************************************************************/
void RenderThread::renderFrameGraph(QRhiCommandBuffer* cb, QRhiRenderTarget* renderTarget,
    TargetState& state, FrameGraph* frameGraph, const SceneRenderer::Configuration& config, TaskProgress& progress, bool isPickingPass,
    int refinementIteration)
{
    OVITO_ASSERT(QThread::currentThread() == this);
    OVITO_ASSERT(this_task::get());
    OVITO_ASSERT(cb);
    OVITO_ASSERT(renderTarget);
    OVITO_ASSERT(frameGraph);

    // Collect non-fatal warnings from the renderer while the pass is recorded. A picking pass generates no
    // warnings, and the warning store of the visual pass is deliberately left untouched while one is in flight,
    // because it holds the messages the indicator of the viewport currently displays.
    _warnings.clear();

    // The pass sequence itself is shared with the Qt Quick frontend; what is specific to the classic frontend
    // is added through the two callbacks: the warning indicator icon and, in the Basic edition, the watermark.
    FrameGraphRenderPass::Arguments args{
        /* service */ *this,
        /* cb */ *cb,
        /* renderTarget */ *renderTarget,
        /* frameGraph */ *frameGraph,
        /* configuration */ config,
        /* implementations */ state.renderers,
        /* progress */ progress,
        /* isPickingPass */ isPickingPass,
        /* pickingMap */ isPickingPass ? &state.pickingMap : nullptr,
        /* refinementIteration */ refinementIteration,
        /* prepareScenePass */ [&](QRhiResourceUpdateBatch* updates) {

            // Prepare the warning indicator's lazy texture upload and uniform buffer update before beginPass().
            if(!isPickingPass && !_warnings.empty() && state.warningIndicator)
                state.warningIndicator->prepareResourceUpdates(updates, renderTarget, qreal(renderTarget->devicePixelRatio()));

#ifdef OVITO_BUILD_BASIC
            // Create or tear down the watermark renderer depending on what the active renderer requests.
            if(!isPickingPass) {
                if(SceneRenderer::Implementation* implementation = state.renderers.get(false)) {
                    if(implementation->isWatermarked()) {
                        if(!state.watermarkRenderer)
                            state.watermarkRenderer = std::make_unique<WatermarkRenderer>(this, SceneRenderer::watermark());
                    }
                    else {
                        state.watermarkRenderer.reset();
                    }
                }
                if(state.watermarkRenderer)
                    state.watermarkRenderer->prepareResourceUpdates(updates, renderTarget);
            }
#endif
        },
        /* extendScenePass */ [&](QRhiCommandBuffer* passCb, QRhiRenderTarget* passTarget) {
#ifdef OVITO_BUILD_BASIC
            // Draw the tiled watermark overlay, if the active renderer requests one.
            if(!isPickingPass && state.watermarkRenderer)
                state.watermarkRenderer->compositeInPass(passCb, passTarget->renderPassDescriptor());
#endif
            // Draw the warning indicator icon and update the shared warning store. Called once for the scene pass,
            // and once more for the final target when the frame graph was post-processed.
            if(!isPickingPass)
                updateWarningStore(passCb, passTarget, state);
        },
    };
    FrameGraphRenderPass::execute(args);

    _warnings.clear();
}

/******************************************************************************
* Composites the warning indicator icon into the active render pass and
* updates the warning messages on the owning ViewportWindow. Must be called
* from within an active render pass. For visual passes only.
******************************************************************************/
void RenderThread::updateWarningStore(QRhiCommandBuffer* cb, QRhiRenderTarget* renderTarget, TargetState& state)
{
    OVITO_ASSERT(QThread::currentThread() == this);
    if(!_warnings.empty() && state.warningIndicator) {
        QRectF iconArea = state.warningIndicator->compositeInPass(cb, renderTarget->renderPassDescriptor());
        if(auto vpw = state.viewportWindow.lock())
            vpw->setWarnings(std::move(_warnings), iconArea);
    }
    else {
        if(auto vpw = state.viewportWindow.lock())
            vpw->setWarnings({}, {});
    }
}

/******************************************************************************
* Loads a compiled .qsb shader from the Qt resource system.
******************************************************************************/
QShader RenderThread::loadShader(const QString& resourcePath)
{
    QFile f(resourcePath);
    if(!f.open(QIODevice::ReadOnly)) {
        reportWarning(tr("Could not open shader resource: %1").arg(resourcePath));
        return {};
    }
    return QShader::fromSerialized(f.readAll());
}

/******************************************************************************
* Logs a non-fatal warning message from a renderer implementation.
* May be called from the renderFrame() method of a renderer implementation
* on the render thread.
******************************************************************************/
void RenderThread::reportWarning(const QString& message)
{
    OVITO_ASSERT(QThread::currentThread() == this);
    _warnings.push_back(message);

    qWarning() << message;
}

}   // End of namespace
