// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

/**
 * \file
 * \brief Entry point of the Phase 1 prototype application of the Qt Quick / QML frontend.
 *
 * This application is a technical spike: it opens a Qt Quick window showing the OVITO workbench prototype
 * and renders the current scene with a QQuickRhiItem that uses the QRhi instance of the Qt Quick scene graph.
 * It is not the final frontend entry point; selecting the frontend at runtime (`ovito --gui=qml`) is Phase 2
 * of the migration plan (see docs/design/UI_PLAN.md).
 */

#include <ovito/gui/qml/QmlFrontend.h>
#include <ovito/gui/qml/mainwin/QmlMainWindowUI.h>
#include <ovito/core/app/StandaloneApplication.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/core/app/Application.h>
#include <ovito/core/utilities/Exception.h>

#include <QTimer>

#include <functional>
#include <memory>

namespace Ovito {

namespace {

/// Saves the current contents of the workbench window to an image file and quits the application.
void captureWindowAndQuit(QmlMainWindowUI* ui, const QString& file)
{
    QImage image = ui->view() ? ui->view()->grabWindow() : QImage();
    if(image.isNull() || !image.save(file))
        qWarning() << "Failed to capture the workbench window to" << file;
    else
        qInfo() << "Saved workbench window contents to" << file;
    QCoreApplication::quit();
}

/// Saves the contents of the workbench window to an image file after the given delay and quits the application.
void scheduleCapture(QmlMainWindowUI* ui, const QString& file, int delay)
{
    auto* timer = new QTimer(ui->view());
    timer->setSingleShot(true);
    QObject::connect(timer, &QTimer::timeout, timer, [ui, file]() { captureWindowAndQuit(ui, file); });
    timer->start(delay);
}

/**
 * Forces the Qt Quick scene graph to release and rebuild all its graphics resources repeatedly. This destroys
 * and recreates the renderers of the viewport items, including all QRhi resources they hold, which is the
 * resource lifecycle the classic frontend exercises when viewport windows are closed and reopened.
 * The continuation is invoked after the last cycle.
 */
void scheduleLifecycleCycles(QmlMainWindowUI* ui, int cycles, std::function<void()> continuation)
{
    auto step = std::make_shared<std::function<void(int)>>();
    *step = [ui, continuation, step](int remaining) {
        if(remaining <= 0) {
            continuation();
            return;
        }
        if(ui->view())
            ui->view()->releaseResources();
        auto* timer = new QTimer(ui->view());
        timer->setSingleShot(true);
        QObject::connect(timer, &QTimer::timeout, timer, [step, remaining]() { (*step)(remaining - 1); });
        timer->start(300);
    };
    (*step)(cycles);
}

}   // End of anonymous namespace

/**
 * \brief The application object of the Qt Quick frontend prototype.
 */
class QmlFrontendApplication : public StandaloneApplication
{
public:

    /// Inherit constructor of the base class.
    using StandaloneApplication::StandaloneApplication;

protected:

    /// Create the global instance of the right QCoreApplication derived class.
    QCoreApplication* createQtApplicationImpl(bool supportGui, int& argc, char** argv) override
    {
        if(!supportGui)
            return StandaloneApplication::createQtApplicationImpl(supportGui, argc, argv);

        // Qt Quick requires a QGuiApplication. Unlike the classic frontend, we do not force a particular
        // QPA platform plugin here, so that the windowing system of the user's choice is used.
        auto* qtApp = new QGuiApplication(argc, argv);
        qtApp->installEventFilter(this);
        return qtApp;
    }

    /// Defines the program's command line parameters.
    void registerCommandLineParameters(QCommandLineParser& parser) override
    {
        StandaloneApplication::registerCommandLineParameters(parser);
        // Note: the classic frontend defines this option as well. It will move to the frontend-neutral
        // application class when the runtime frontend selection is implemented (Phase 2).
        parser.addOption(QCommandLineOption(QStringLiteral("noviewports"),
            tr("Do not create any viewports (for debugging purposes only).")));
        parser.addOption(QCommandLineOption(QStringLiteral("qml-capture"),
            tr("Render the workbench window and save it to the given image file, then quit."), QStringLiteral("FILE")));
        parser.addOption(QCommandLineOption(QStringLiteral("qml-capture-delay"),
            tr("Time in milliseconds to wait before capturing the window contents."), QStringLiteral("MS")));
        parser.addOption(QCommandLineOption(QStringLiteral("qml-lifecycle-cycles"),
            tr("Number of scene graph resource release/rebuild cycles to perform before capturing the window."), QStringLiteral("N")));
    }

    /// Prepares the application to start running.
    MainThreadOperation startupApplication() override
    {
        OVITO_ASSERT(this_task::isMainThread());
        OVITO_ASSERT(this_task::get());

        if(Application::guiEnabled()) {
            // Set up the Qt event loop.
            createQtApplication(true);

            if(Application::runMode() == Application::AppMode) {
                OORef<QmlMainWindowUI> mainWindowUI = OORef<QmlMainWindowUI>::create();
                mainWinUI = mainWindowUI.get();
                mainWindowUI->initializeWindow();
                return MainThreadOperation(*mainWindowUI, MainThreadOperation::Kind::Isolated);
            }
        }

        // Run the application without a user interface.
        return MainThreadOperation(*this, MainThreadOperation::Kind::Isolated);
    }

    /// Is called at program startup once the event loop is running.
    void postStartupInitialization() override
    {
        StandaloneApplication::postStartupInitialization();

        if(!mainWinUI)
            return;

        // Import the data files specified on the command line.
        for(const QString& argument : cmdLineParser().positionalArguments()) {
            if(argument.endsWith(QStringLiteral(".ovito"), Qt::CaseInsensitive)) {
                qWarning() << "Session state files are not supported by the frontend prototype yet:" << argument;
                continue;
            }
            mainWinUI->importFile(Application::instance()->fileManager().urlFromUserInput(argument));
        }

        // Optional capture mode: save the contents of the workbench window to an image file and quit.
        // This is used for automated verification of the prototype in headless environments.
        const QString captureFile = cmdLineParser().value(QStringLiteral("qml-capture"));
        if(!captureFile.isEmpty()) {
            int delay = cmdLineParser().value(QStringLiteral("qml-capture-delay")).toInt();
            if(delay <= 0)
                delay = 3000;
            int lifecycleCycles = cmdLineParser().value(QStringLiteral("qml-lifecycle-cycles")).toInt();
            if(lifecycleCycles > 0) {
                // Exercise the resource lifecycle before capturing the window contents.
                scheduleLifecycleCycles(mainWinUI, lifecycleCycles, [ui = mainWinUI, captureFile, delay]() {
                    scheduleCapture(ui, captureFile, delay);
                });
            }
            else {
                scheduleCapture(mainWinUI, captureFile, delay);
            }
        }
    }

private:

    /// The user interface of the Qt Quick frontend. Not owning; the user interface is kept alive by the
    /// main thread operation returned from startupApplication().
    QmlMainWindowUI* mainWinUI = nullptr;
};

}   // End of namespace

// Register the Qt resource files of the statically linked frontend modules.
// Following the Qt documentation, this needs to be placed outside of any C++ namespace.
static void registerQtResources()
{
#ifdef OVITO_BUILD_MONOLITHIC
    Q_INIT_RESOURCE(qml);
    Q_INIT_RESOURCE(guibase);
#endif
}

/**
 * This is the main entry point of the Qt Quick frontend prototype.
 */
int main(int argc, char** argv)
{
    registerQtResources();

    Ovito::OORef<Ovito::QmlFrontendApplication> app = Ovito::OORef<Ovito::QmlFrontendApplication>::create();

    int exitCode = 1;
    if(app->initialize(argc, argv)) {
        if(QCoreApplication::instance()) {
            try {
                exitCode = QCoreApplication::exec();
            }
            catch(const Ovito::Exception& ex) {
                qWarning() << ex.message();
            }
        }
        else {
            exitCode = 0;
        }
    }
    app->shutdown();

    return exitCode;
}
