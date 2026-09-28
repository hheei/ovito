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
#include <ovito/gui/qml/viewport/QuickViewportItem.h>
#include <ovito/gui/qml/viewport/QuickViewportWindow.h>
#include <ovito/core/app/StandaloneApplication.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/core/dataset/scene/Scene.h>
#include <ovito/core/dataset/scene/SelectionSet.h>
#include <ovito/core/viewport/Viewport.h>
#include <ovito/core/app/Application.h>
#include <ovito/core/utilities/Exception.h>

#include <QMouseEvent>
#include <QElapsedTimer>
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

/// Runs the given function after the specified delay.
void scheduleDelayed(QmlMainWindowUI* ui, int delay, std::function<void()> action)
{
    auto* timer = new QTimer(ui->view());
    timer->setSingleShot(true);
    QObject::connect(timer, &QTimer::timeout, timer, std::move(action));
    timer->start(delay);
}

/// Collects the viewport items of the QML scene.
/// Note: the items created by the QML delegate model are not part of the QObject parent chain, so
/// QObject::findChildren() cannot be used here and the item tree must be traversed instead.
QList<QuickViewportItem*> viewportItems(QmlMainWindowUI* ui)
{
    QList<QuickViewportItem*> items;
    if(QQuickItem* root = ui->view() ? ui->view()->rootObject() : nullptr) {
        std::function<void(QQuickItem*)> collect = [&](QQuickItem* parent) {
            for(QQuickItem* child : parent->childItems()) {
                if(auto* item = qobject_cast<QuickViewportItem*>(child))
                    items.push_back(item);
                collect(child);
            }
        };
        collect(root);
    }
    return items;
}

/// Picks at a grid of positions around the given location and reports the outcome.
void reportPickingResults(QuickViewportWindow* viewportWindow, const QSizeF& itemSize, const QPoint& itemPos, const char* passName)
{
    int hitCount = 0, testedCount = 0;
    std::optional<ViewportWindow::PickResult> firstHit;
    for(int dy = -8; dy <= 8; dy += 4) {
        for(int dx = -8; dx <= 8; dx += 4) {
            const QPointF pos = QPointF(itemPos) + QPointF(dx, dy);
            if(pos.x() < 0 || pos.y() < 0 || pos.x() >= itemSize.width() || pos.y() >= itemSize.height())
                continue;
            testedCount++;
            if(auto result = viewportWindow->pick(pos)) {
                hitCount++;
                if(!firstHit)
                    firstHit = std::move(result);
            }
        }
    }

    if(firstHit) {
        const SceneNode* node = firstHit->sceneNode();
        qInfo() << "PICK_TEST" << passName << ":" << hitCount << "of" << testedCount << "positions hit;"
                << "example: node" << (node ? node->objectTitle() : QStringLiteral("<none>"))
                << "subobject" << firstHit->subobjectId()
                << "hit location" << firstHit->hitLocation().x() << firstHit->hitLocation().y() << firstHit->hitLocation().z();
    }
    else {
        qInfo() << "PICK_TEST" << passName << ":" << hitCount << "of" << testedCount << "positions hit";
    }
}

/// Verifies object picking at the given position of the first viewport item.
///
/// Picking is served from a buffer that the render thread renders asynchronously, so the first call after the
/// viewport contents changed cannot return a result yet; it starts the picking pass instead. The test therefore
/// measures twice and reports both outcomes, then verifies that a synthetic mouse click selects an object
/// through the regular input mode path.
void runPickTest(QmlMainWindowUI* ui, const QPoint& itemPos, std::function<void()> continuation)
{
    const QList<QuickViewportItem*> items = viewportItems(ui);
    if(items.isEmpty()) {
        qWarning() << "PICK_TEST no viewport item found";
        continuation();
        return;
    }

    QuickViewportItem* item = items.front();
    QuickViewportWindow* viewportWindow = item->viewportWindow();
    if(!viewportWindow) {
        qWarning() << "PICK_TEST viewport item has no viewport window";
        continuation();
        return;
    }

    const QSizeF itemSize = item->size();
    qInfo() << "PICK_TEST" << items.size() << "viewport items; first item size" << itemSize
            << "test position" << itemPos;

    // First pass: the picking buffer has not been rendered yet, so this starts the picking pass.
    reportPickingResults(viewportWindow, itemSize, itemPos, "before picking pass");

    // Measure how long the asynchronously rendered picking buffer takes to become available.
    auto probeStart = std::make_shared<QElapsedTimer>();
    probeStart->start();
    auto poll = std::make_shared<std::function<void()>>();
    *poll = [ui, viewportWindow, itemPos, probeStart, poll]() {
        if(viewportWindow->pick(QPointF(itemPos))) {
            qInfo() << "PICK_TEST first successful pick after" << probeStart->elapsed() << "ms";
            return;
        }
        if(probeStart->elapsed() > 5000) {
            qWarning() << "PICK_TEST the picking buffer did not become available within 5 s";
            return;
        }
        scheduleDelayed(ui, 25, *poll);
    };
    scheduleDelayed(ui, 25, *poll);

    // Second pass: the picking pass has been rendered in the background in the meantime.
    scheduleDelayed(ui, 500, [ui, item, viewportWindow, itemSize, itemPos, continuation]() {
        reportPickingResults(viewportWindow, itemSize, itemPos, "after picking pass");

        // Negative control: a corner of the viewport shows only the empty background.
        if(std::optional<ViewportWindow::PickResult> backgroundPick = viewportWindow->pick(QPointF(2, 2))) {
            const SceneNode* node = backgroundPick->sceneNode();
            qInfo() << "PICK_TEST background control: picked" << (node ? node->objectTitle() : QStringLiteral("<none>"));
        }
        else {
            qInfo() << "PICK_TEST background control: nothing picked";
        }

        // End-to-end check: a synthetic click must select an object through the regular input mode path.
        const QPointF clickPos(itemPos);
        const QPointF globalPos = item->mapToGlobal(clickPos);
        QMouseEvent pressEvent(QEvent::MouseButtonPress, clickPos, globalPos, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(item, &pressEvent);
        QMouseEvent releaseEvent(QEvent::MouseButtonRelease, clickPos, globalPos, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        QCoreApplication::sendEvent(item, &releaseEvent);

        if(Viewport* viewport = viewportWindow->viewport()) {
            if(Scene* scene = viewport->scene()) {
                if(SelectionSet* selection = scene->selection()) {
                    if(const SceneNode* selectedNode = selection->firstNode())
                        qInfo() << "PICK_TEST synthetic click selected" << selectedNode->objectTitle();
                    else
                        qWarning() << "PICK_TEST synthetic click selected nothing";
                }
            }
        }
        continuation();
    });
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
        parser.addOption(QCommandLineOption(QStringLiteral("qml-pick"),
            tr("Verify object picking at the given position (x,y) of the first viewport item and print the result."),
            QStringLiteral("X,Y")));
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

        // Optional picking verification: perform picking operations and print the outcome.
        std::optional<QPoint> pickPosition;
        const QStringList pickCoordinates = cmdLineParser().value(QStringLiteral("qml-pick")).split(QLatin1Char(','));
        if(pickCoordinates.size() == 2)
            pickPosition = QPoint(pickCoordinates[0].toInt(), pickCoordinates[1].toInt());

        if(captureFile.isEmpty() && !pickPosition)
            return;

        int delay = cmdLineParser().value(QStringLiteral("qml-capture-delay")).toInt();
        if(delay <= 0)
            delay = 3000;

        // After the optional lifecycle cycles and the delay, verify picking (if requested) and then capture the
        // window contents (if requested) before quitting.
        const QString captureTarget = captureFile;
        auto finish = [ui = mainWinUI, captureTarget]() {
            if(captureTarget.isEmpty())
                QCoreApplication::quit();
            else
                captureWindowAndQuit(ui, captureTarget);
        };
        auto verify = [ui = mainWinUI, pickPosition, finish]() {
            if(pickPosition)
                runPickTest(ui, *pickPosition, finish);
            else
                finish();
        };

        int lifecycleCycles = cmdLineParser().value(QStringLiteral("qml-lifecycle-cycles")).toInt();
        if(lifecycleCycles > 0) {
            // Exercise the resource lifecycle before verifying the window contents.
            scheduleLifecycleCycles(mainWinUI, lifecycleCycles, [ui = mainWinUI, delay, verify]() {
                scheduleDelayed(ui, delay, verify);
            });
        }
        else {
            scheduleDelayed(mainWinUI, delay, verify);
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
