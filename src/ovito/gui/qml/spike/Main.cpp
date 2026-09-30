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

#include <ovito/gui/qml/spike/SpikeHarness.h>

namespace Ovito {

namespace Spike {

namespace {

/// Parses the "X,Y" value of --qml-pick, which several steps read.
std::optional<QPoint> spikePickPosition(const QCommandLineParser& parser)
{
    const QStringList parts = parser.value(QStringLiteral("qml-pick")).split(QLatin1Char(','));
    if(parts.size() != 2 || parts[0].trimmed().isEmpty() || parts[1].trimmed().isEmpty())
        return std::nullopt;
    return QPoint(parts[0].trimmed().toInt(), parts[1].trimmed().toInt());
}

/// Parses the "WxH" value of an option and returns an invalid size when the value is not of that form.
QSize spikeOptionSize(const QCommandLineParser& parser, const QString& option)
{
    const QStringList parts = parser.value(option).split(QLatin1Char('x'));
    if(parts.size() != 2)
        return {};
    return QSize(parts[0].toInt(), parts[1].toInt());
}

/// A command line option of the harness that carries a value but does not run a step of its own.
struct SpikeOption
{
    QCommandLineOption option;
};

/// A verification step of the harness: the option that asks for it, the test that decides whether the value asks for the
/// step at all (`--qml-frame-stats 0` means "no"), and the check to run.
///
/// This table is the single source for the command line options of the parser, for their help text and for the decision
/// whether a run verifies anything or just opens the workbench. A new check only has to be added here - registering it
/// in the parser and in the interactive-mode test separately is the trap that once made a spike run report nothing and
/// hang until the caller's timeout killed it. The table's order is the order the steps run in, and the comments say why
/// each step sits where it sits.
struct SpikeStep
{
    QCommandLineOption option;
    std::function<bool(const QCommandLineParser&)> enabled;
    std::function<void(QmlMainWindowUI*, const QCommandLineParser&, std::function<void()>)> run;
};

/// Returns the verification steps of the harness, in the order they run.
const std::vector<SpikeStep>& spikeSteps()
{
    static const std::vector<SpikeStep> steps{
        // Pick as early as possible, while the imported scene is likely still being evaluated: the picking pass is an asynchronous offscreen operation, so it must neither block the GUI thread nor deadlock with the evaluation of the pipeline that it renders.
        { QCommandLineOption(QStringLiteral("qml-pick"),
              QStringLiteral("Verify object picking at the given position (x,y) of the first viewport item and print the result."), QStringLiteral("X,Y")),
          [](const QCommandLineParser& p) { return spikePickPosition(p).has_value(); },
          [](QmlMainWindowUI* ui, const QCommandLineParser& parser, std::function<void()> next) {
              const QPoint pickPos = *spikePickPosition(parser);
              scheduleDelayed(ui, 100, [ui, pickPos, next = std::move(next)]() {
                  const QDateTime started = QDateTime::currentDateTime();
                  if(QuickViewportItem* item = firstViewportItem(ui)) {
                      if(QuickViewportWindow* viewportWindow = item->viewportWindow()) {
                          const PickProbe probe = probePicking(viewportWindow, item->size(), pickPos);
                          reportPicking(probe, QStringLiteral("during scene evaluation"));
                          reportCameras(ui, QStringLiteral("right after the import"));
                      }
                  }
                  qInfo() << "PICK_TEST the probe during scene evaluation returned after"
                          << started.msecsTo(QDateTime::currentDateTime()) << "ms";
                  next();
              });
          } },

        // The delay runs before anything is measured, so that the imported data set and the first frame graphs are ready.
        { QCommandLineOption(QStringLiteral("qml-startup-delay"),
              QStringLiteral("Time in milliseconds to wait after startup before the verification steps begin."), QStringLiteral("MS")),
          {},
          [](QmlMainWindowUI* ui, const QCommandLineParser& parser, std::function<void()> next) {
              int delay = parser.value(QStringLiteral("qml-startup-delay")).toInt();
              if(delay <= 0)
                  delay = 3000;
              scheduleDelayed(ui, delay, std::move(next));
          } },

        { QCommandLineOption(QStringLiteral("qml-frame-stats"),
              QStringLiteral("Measure the frame rate of the viewport for the given number of milliseconds and print the result."), QStringLiteral("MS")),
          [](const QCommandLineParser& p) { return p.value(QStringLiteral("qml-frame-stats")).toInt() > 0; },
          [](QmlMainWindowUI* ui, const QCommandLineParser& parser, std::function<void()> next) {
              measureFrameRate(ui, parser.value(QStringLiteral("qml-frame-stats")).toInt(), std::move(next));
          } },

        { QCommandLineOption(QStringLiteral("qml-command-check"),
              QStringLiteral("Verify the shared command layer: the commands the QML workbench sees, their state rules and their handlers.")),
          [](const QCommandLineParser& p) { return p.isSet(QStringLiteral("qml-command-check")); },
          [](QmlMainWindowUI* ui, const QCommandLineParser& parser, std::function<void()> next) {
              runCommandTest(ui, std::move(next));
          } },

        { QCommandLineOption(QStringLiteral("qml-layout-check"),
              QStringLiteral("Verify the viewport layout: pane geometry, undoable splitter drags and maximizing a viewport.")),
          [](const QCommandLineParser& p) { return p.isSet(QStringLiteral("qml-layout-check")); },
          [](QmlMainWindowUI* ui, const QCommandLineParser& parser, std::function<void()> next) {
              runLayoutTest(ui, std::move(next));
          } },

        // Release and rebuild the graphics resources of the viewports; one of the stress steps.
        { QCommandLineOption(QStringLiteral("qml-lifecycle-cycles"),
              QStringLiteral("Number of scene graph resource release/rebuild cycles to perform before the window is closed."), QStringLiteral("N")),
          [](const QCommandLineParser& p) { return p.value(QStringLiteral("qml-lifecycle-cycles")).toInt() > 0; },
          [](QmlMainWindowUI* ui, const QCommandLineParser& parser, std::function<void()> next) {
              scheduleLifecycleCycles(ui, parser.value(QStringLiteral("qml-lifecycle-cycles")).toInt(), std::move(next));
          } },

        { QCommandLineOption(QStringLiteral("qml-hide-show"),
              QStringLiteral("Hide the viewport items for a moment and show them again, verifying that rendering and picking recover.")),
          [](const QCommandLineParser& p) { return p.isSet(QStringLiteral("qml-hide-show")); },
          [](QmlMainWindowUI* ui, const QCommandLineParser& parser, std::function<void()> next) {
              runHideShowTest(ui, spikePickPosition(parser).value_or(QPoint(0, 0)), std::move(next));
          } },

        { QCommandLineOption(QStringLiteral("qml-resize"),
              QStringLiteral("Resize the workbench window to the given size (WxH) and verify that rendering and picking recover."), QStringLiteral("WxH")),
          [](const QCommandLineParser& p) { return spikeOptionSize(p, QStringLiteral("qml-resize")).isValid(); },
          [](QmlMainWindowUI* ui, const QCommandLineParser& parser, std::function<void()> next) {
              runResizeTest(ui, spikeOptionSize(parser, QStringLiteral("qml-resize")), spikePickPosition(parser).value_or(QPoint(0, 0)),
                  std::move(next));
          } },

        // Reads the state of the window only, so it can run before everything that changes the scene.
        { QCommandLineOption(QStringLiteral("qml-device-check"),
              QStringLiteral("Verify that the frontend reports a missing graphics device, which depends on the platform plugin.")),
          [](const QCommandLineParser& p) { return p.isSet(QStringLiteral("qml-device-check")); },
          [](QmlMainWindowUI* ui, const QCommandLineParser& parser, std::function<void()> next) {
              runDeviceTest(ui, std::move(next));
          } },

        // Reads the state of the shell only, so it can run before the checks that change the scene.
        { QCommandLineOption(QStringLiteral("qml-icon-check"),
              QStringLiteral("Verify that the shell shows the icons of the shared icon set.")),
          [](const QCommandLineParser& p) { return p.isSet(QStringLiteral("qml-icon-check")); },
          [](QmlMainWindowUI* ui, const QCommandLineParser& parser, std::function<void()> next) {
              runIconTest(ui, std::move(next));
          } },

        { QCommandLineOption(QStringLiteral("qml-pick"),
              QStringLiteral("Verify object picking at the given position (x,y) of the first viewport item and print the result."), QStringLiteral("X,Y")),
          [](const QCommandLineParser& p) { return spikePickPosition(p).has_value(); },
          [](QmlMainWindowUI* ui, const QCommandLineParser& parser, std::function<void()> next) {
              runPickTest(ui, *spikePickPosition(parser), std::move(next));
          } },

        // Before the checks that change the scene: this one only reads and writes the settings store.
        { QCommandLineOption(QStringLiteral("qml-settings-check"),
              QStringLiteral("Verify the settings facade both frontends share: the values round-trip and the shell's theme follows it.")),
          [](const QCommandLineParser& p) { return p.isSet(QStringLiteral("qml-settings-check")); },
          [](QmlMainWindowUI* ui, const QCommandLineParser& parser, std::function<void()> next) {
              runSettingsTest(ui, std::move(next));
          } },

        // Before the checks that change the scene in earnest: the library check inserts a modifier and undoes it again, so it leaves the scene as it found it.
        { QCommandLineOption(QStringLiteral("qml-library-check"),
              QStringLiteral("Verify the modifier and viewport layer libraries, whose entries are commands of the shared command layer.")),
          [](const QCommandLineParser& p) { return p.isSet(QStringLiteral("qml-library-check")); },
          [](QmlMainWindowUI* ui, const QCommandLineParser& parser, std::function<void()> next) {
              runLibraryTest(ui, std::move(next));
          } },

        // Before the import check, which ends with a cancelled import that leaves the scene empty; this check saves and reloads the scene of the workbench.
        { QCommandLineOption(QStringLiteral("qml-session-check"),
              QStringLiteral("Verify the session workflow of the workbench: saving, the modified state, and loading a session back.")),
          [](const QCommandLineParser& p) { return p.isSet(QStringLiteral("qml-session-check")); },
          [](QmlMainWindowUI* ui, const QCommandLineParser& parser, std::function<void()> next) {
              runSessionTest(ui, std::move(next));
          } },

        // Right after the picking check: it needs a scene with a fitted camera, and it moves the camera itself.
        { QCommandLineOption(QStringLiteral("qml-prewarm-check"),
              QStringLiteral("Verify that the picking buffer refreshes itself after a change of the view, without a pick asking for it.")),
          [](const QCommandLineParser& p) { return p.isSet(QStringLiteral("qml-prewarm-check")); },
          [](QmlMainWindowUI* ui, const QCommandLineParser& parser, std::function<void()> next) {
              runPrewarmTest(ui, std::move(next));
          } },

        // Before the import check, which ends with a cancelled import that leaves the scene empty, and after the picking check, because this check clicks in a viewport and changes the camera for a moment.
        { QCommandLineOption(QStringLiteral("qml-parity-check"),
              QStringLiteral("Verify the shell features added for parity: the menus, the viewport context menu, the task rows, the window state and the import notice.")),
          [](const QCommandLineParser& p) { return p.isSet(QStringLiteral("qml-parity-check")); },
          [](QmlMainWindowUI* ui, const QCommandLineParser& parser, std::function<void()> next) {
              runParityTest(ui, std::move(next));
          } },

        // After the parity check and before the import check: it inserts a modifier into the pipeline of the scene and resizes the workbench, so it needs the scene the earlier checks left behind and must not disturb the imports that follow it.
        { QCommandLineOption(QStringLiteral("qml-offscreen-check"),
              QStringLiteral("Verify the shared offscreen rendering service: the ambient-occlusion sampling, a picking pass and a render output at the same time.")),
          [](const QCommandLineParser& p) { return p.isSet(QStringLiteral("qml-offscreen-check")); },
          [](QmlMainWindowUI* ui, const QCommandLineParser& parser, std::function<void()> next) {
              runOffscreenTest(ui, std::move(next));
          } },

        // Last, because it imports data sets of its own.
        { QCommandLineOption(QStringLiteral("qml-import-check"),
              QStringLiteral("Verify the import path of the shell: a multi-frame trajectory, an unsupported file and a cancelled import.")),
          [](const QCommandLineParser& p) { return p.isSet(QStringLiteral("qml-import-check")); },
          [](QmlMainWindowUI* ui, const QCommandLineParser& parser, std::function<void()> next) {
              runImportTest(ui, std::move(next));
          } },
    };
    return steps;
}

/// Returns the options of the harness that are not a step of their own: the initial window size and the time the window
/// is held open for a screenshot after the verification.
const std::vector<SpikeOption>& spikeOptions()
{
    static const std::vector<SpikeOption> options{
        { QCommandLineOption(QStringLiteral("qml-window-size"),
              QStringLiteral("Resize the workbench window to the given size (WxH) before anything is measured."), QStringLiteral("WxH")) },
        { QCommandLineOption(QStringLiteral("qml-hold-ms"),
              QStringLiteral("Keep the workbench window open for the given number of milliseconds after the verification steps, "
               "so that a screenshot can be taken of it, and then quit."), QStringLiteral("MS")) },
    };
    return options;
}

/// Returns whether the command line asks for at least one verification step.
bool isVerificationRun(const QCommandLineParser& parser)
{
    for(const SpikeStep& step : spikeSteps()) {
        if(step.enabled && step.enabled(parser))
            return true;
    }
    return false;
}

}   // anonymous namespace

}   // namespace Spike

using namespace Spike;   // the tables above and the harness behind them

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
        // The options of the harness come from its tables, so that a check cannot be added without its option and
        // without the parser knowing about it (see the comment on SpikeStep). An option that enables two steps - the
        // picking option does - is registered once.
        QStringList registeredOptions;
        for(const SpikeStep& step : spikeSteps()) {
            const QString name = step.option.names().first();
            if(registeredOptions.contains(name))
                continue;
            registeredOptions.push_back(name);
            parser.addOption(step.option);
        }
        for(const SpikeOption& option : spikeOptions())
            parser.addOption(option.option);
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

        // Import the data files specified on the command line through the shared import path of the workbench,
        // which is the same code the real frontend uses.
        std::vector<QUrl> importUrls;
        for(const QString& argument : cmdLineParser().positionalArguments()) {
            if(argument.endsWith(QStringLiteral(".ovito"), Qt::CaseInsensitive)) {
                qWarning() << "Session state files are not supported by the frontend prototype yet:" << argument;
                continue;
            }
            importUrls.push_back(Application::instance()->fileManager().urlFromUserInput(argument));
        }
        // The importer asks the viewports to zoom to the scene extents. That request reaches only viewport windows that
        // exist at that moment, so this records whether the workbench had already created them.
        qInfo() << "BOOTSTRAP the workbench holds" << viewportItems(mainWinUI).size() << "viewport items before the import";

        mainWinUI->handleExceptions([&]() {
            if(!importUrls.empty())
                mainWinUI->importFiles(importUrls);
        });

        // Diagnostic: report what the command line data files turned into. A file that was misdetected by the
        // importer autodetection shows up here, instead of only as a picking check that finds nothing.
        if(QuickViewportItem* item = firstViewportItem(mainWinUI)) {
            if(Viewport* viewport = item->viewportWindow() ? item->viewportWindow()->viewport() : nullptr) {
                if(Scene* scene = viewport->scene()) {
                    for(SceneNode* node : scene->children())
                        qInfo() << "DATASET" << node->objectTitle();
                }
            }
        }

        // Optional initial window size: measurements are taken at this size, e.g. to match the viewport area of
        // another frontend for a frame rate comparison.
        const QSize initialSize = spikeOptionSize(cmdLineParser(), QStringLiteral("qml-window-size"));
        if(initialSize.isValid())
            mainWinUI->view()->resize(initialSize);

        // A screenshot of the workbench is taken from the X server while the window is held open; see
        // docs/design/UI_TEST_ENV.md for the ffmpeg command. It cannot be taken from inside the process, because
        // QQuickWindow::grabWindow() is not usable with the QQuickRhiItem viewports of the workbench.
        const int holdMs = cmdLineParser().value(QStringLiteral("qml-hold-ms")).toInt();

        // Interactive mode: without a verification option, keep the window open and let the user look at it.
        if(!isVerificationRun(cmdLineParser()))
            return;

        _verificationFinished = [ui = mainWinUI, holdMs]() {
            const int status = spikeVerificationFailures() == 0 ? 0 : 1;
            qInfo() << "VERIFICATION_DONE with" << spikeVerificationFailures() << "failed check(s)";
            if(holdMs <= 0) {
                QCoreApplication::exit(status);
                return;
            }
            // Keep the window open, so that the caller can take a screenshot of it, and quit afterwards.
            scheduleDelayed(ui, holdMs, [status]() { QCoreApplication::exit(status); });
        };

        // Assemble the verification steps in the order of the table above.
        for(const SpikeStep& step : spikeSteps()) {
            if(step.enabled && !step.enabled(cmdLineParser()))
                continue;
            _verificationSteps.push_back([ui = mainWinUI, &parser = cmdLineParser(), run = step.run](std::function<void()> next) {
                run(ui, parser, std::move(next));
            });
        }
        runNextVerificationStep();
    }

private:

    /// A single verification step of the prototype. It receives the continuation to invoke when it is done.
    using VerificationStep = std::function<void(std::function<void()>)>;

    /// Runs the next verification step of  _verificationSteps.
    void runNextVerificationStep()
    {
        if(_verificationSteps.empty()) {
            std::function<void()> finished = std::move(_verificationFinished);
            _verificationFinished = {};
            if(finished)
                finished();
            return;
        }
        VerificationStep step = std::move(_verificationSteps.front());
        _verificationSteps.erase(_verificationSteps.begin());
        step([this]() { runNextVerificationStep(); });
    }

    /// The user interface of the Qt Quick frontend. Not owning; the user interface is kept alive by the
    /// main thread operation returned from startupApplication().
    QmlMainWindowUI* mainWinUI = nullptr;

    /// Verification steps to execute one after another before the window contents are captured and the
    /// application quits. The application object outlives the steps, so the sequence needs no extra ownership.
    std::vector<VerificationStep> _verificationSteps;

    /// Invoked when all verification steps have been executed.
    std::function<void()> _verificationFinished;
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
