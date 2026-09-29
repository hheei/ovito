// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/core/Core.h>
#include <ovito/core/app/Application.h>
#include <ovito/core/app/PluginManager.h>
#include <ovito/core/rendering/FrameBuffer.h>
#include <ovito/core/rendering/GraphicsApi.h>
#include <ovito/core/rendering/RenderThread.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include "UserInterface.h"

#include <QOperatingSystemVersion>

namespace Ovito {

#ifndef OVITO_BUILD_MONOLITHIC
#if defined(Q_CC_MSVC)
// Explicit class template instantiations to be exported by the core module:
template class UserInterfaceComponent<UserInterface, true>;
template class UserInterfaceComponent<UserInterface, false>;
#endif
#endif

IMPLEMENT_ABSTRACT_OVITO_CLASS(UserInterface);

/******************************************************************************
* Constructor
******************************************************************************/
UserInterface::UserInterface() : _datasetContainer(OORef<DataSetContainer>::create(*this))
{
}

/******************************************************************************
* Returns a shared render thread, creating one on demand if needed.
******************************************************************************/
std::shared_ptr<RenderThread> UserInterface::renderThread()
{
    OVITO_ASSERT(this_task::isMainThread());

    std::shared_ptr<RenderThread> rt = _renderThread.lock();
    if(!rt) {
        rt = std::make_shared<RenderThread>(*this, GraphicsApi::preferred());
        _renderThread = rt;
        // As a performance optimization when running in headless Python mode, keep the render thread alive as long as the UserInterface exists.
        // This is to speed up consecutive invocations of the Viewport.render_image() method, which would otherwise have to create and destroy the render thread on each call.
        if(Application::guiEnabled() == false) {
            _renderThreadKeepAlive = rt;
        }
    }
    return rt;
}

/******************************************************************************
* Closes the user interface and shuts down the entire application after
* displaying an error message.
******************************************************************************/
void UserInterface::exitWithFatalError(const Exception& ex)
{
    OVITO_ASSERT(this_task::isMainThread());

    // Avoid reentrance.
    if(_exitingDueToFatalError)
        return;

    // Set flag.
    _exitingDueToFatalError = true;

    // Display fatal error message to the user.
    reportError(ex, true);

    // Quit the Qt event loop if it is running.
    if(QCoreApplication::instance() != nullptr) {
        OVITO_ASSERT(QThread::currentThread()->loopLevel() != 0);
        QCoreApplication::exit(1);
    }
    else {
        // No Qt event loop running. Just shut down the application immediately.
        shutdown();
    }
}

/******************************************************************************
* Cancels all running tasks and closes the user interface as soon as possible
* (without asking user to save changes).
******************************************************************************/
bool UserInterface::shutdown()
{
    OVITO_ASSERT(this_task::isMainThread());

    try {
        // Set up a local task context in case we don't have one.
        // The shutdown() method may be called from anywhere.
        MainThreadOperation operation(*this, MainThreadOperation::Kind::Isolated);

        // Close the dataset container. This should release all objects in the current dataset and stop all associated tasks.
        datasetContainer().clearAllReferences();

        // Release render thread.
        _renderThreadKeepAlive.reset();

        return true;
    }
    catch(OperationCanceled) {
        qWarning() << "Warning: Shutdown canceled unexpectedly";
    }
    catch(const Exception& ex) {
        qWarning() << "Warning: Exception caught during shutdown";
        reportError(ex, true);
    }
    return false;
}

/******************************************************************************
* Displays the error message(s) stored in the Exception object to the user.
******************************************************************************/
void UserInterface::reportError(const Exception& ex, bool blocking)
{
    if(!ex.traceback().isEmpty())
        qInfo().noquote() << ex.traceback();
    for(auto msg = ex.messages().crbegin(); msg != ex.messages().crend(); ++msg) {
        qInfo().noquote() << "ERROR:" << *msg;
    }
}

/******************************************************************************
* Creates a frame buffer of the requested size and displays it as a window in the user interface.
******************************************************************************/
std::shared_ptr<FrameBuffer> UserInterface::createAndShowFrameBuffer(int width, int height)
{
    return std::make_shared<FrameBuffer>(width, height);
}

/******************************************************************************
* Flags all viewports for redrawing.
******************************************************************************/
void UserInterface::updateViewports()
{
    OVITO_ASSERT(this_task::isMainThread());

    if(ViewportConfiguration* viewportConfig = datasetContainer().activeViewportConfig()) {
        for(Viewport* vp : viewportConfig->viewports())
            vp->updateViewport();
    }
}

/******************************************************************************
* Zooms all visible viewports to the extents of the scene when all scene
* pipelines have been fully evaluated and the extents are known.
******************************************************************************/
void UserInterface::zoomToSceneExtentsWhenReady()
{
    if(DataSet* dataset = datasetContainer().currentSet()) {
        if(ViewportConfiguration* viewportConfig = dataset->viewportConfig())
            viewportConfig->zoomToSceneExtentsWhenReady();
    }
}

/******************************************************************************
* Queries the system's information and graphics capabilities.
******************************************************************************/
QString UserInterface::generateSystemReport()
{
    QString text;
    QTextStream stream(&text, QIODevice::WriteOnly | QIODevice::Text);
    stream << "======= System =======\n";
    stream << "Current date: " << QDateTime::currentDateTime().toString() << "\n";
    stream << "Application: " << Application::applicationName() << " " << Application::applicationVersionString() << "\n";
#if defined(Q_OS_LINUX)
    // Get 'uname' output.
    QProcess unameProcess;
    unameProcess.start("uname", QStringList() << "-m" << "-i" << "-o" << "-r" << "-v", QIODevice::ReadOnly);
    unameProcess.waitForFinished();
    QByteArray unameOutput = unameProcess.readAllStandardOutput();
    unameOutput.replace('\n', ' ');
    stream << "uname output: " << unameOutput << "\n";
    // Get 'lsb_release' output.
    QProcess lsbProcess;
    lsbProcess.start("lsb_release", QStringList() << "-s" << "-i" << "-d" << "-r", QIODevice::ReadOnly);
    lsbProcess.waitForFinished();
    QByteArray lsbOutput = lsbProcess.readAllStandardOutput();
    lsbOutput.replace('\n', ' ');
    stream << "LSB output: " << lsbOutput << "\n";
#else
    stream << "Operating system: " << QOperatingSystemVersion::current().name() << " (" << QOperatingSystemVersion::current().majorVersion() << "." << QOperatingSystemVersion::current().minorVersion() << ")" << "\n";
#endif
    stream << "Processor architecture: " << QSysInfo::currentCpuArchitecture() << "\n";
    stream << "Qt version: " << QT_VERSION_STR << " (" << QSysInfo::buildCpuArchitecture() << ")\n";
#ifdef OVITO_DISABLE_THREADING
    stream << "Multi-threading: disabled\n";
#endif
    stream << "Command line: " << QCoreApplication::arguments().join(' ') << "\n";
    stream << "Python file path: " << PluginManager::instance().pythonDir() << "\n";
    // Let the plugin classes optionally add their specific info to the system report, e.g. the GPU driver information.
    for(Plugin* plugin : PluginManager::instance().plugins()) {
        for(OvitoClassPtr clazz : plugin->classes()) {
            clazz->querySystemInformation(stream, *this);
        }
    }
    return text;
}

}   // End of namespace
