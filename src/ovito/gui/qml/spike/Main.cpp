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
#include <QDateTime>
#include <QElapsedTimer>
#include <QTimer>

#include <functional>
#include <memory>

namespace Ovito {

namespace {

/// Number of verification checks that did not produce the expected result. The spike exits with a non-zero
/// status when this counter is non-zero, so an automated run (the CI smoke test) fails on a broken viewport
/// instead of only printing a warning into a log nobody reads.
int verificationFailures = 0;

/// How long a picking check waits for the asynchronously rendered picking buffer. This is generous on purpose: a
/// CI runner is much slower than a development machine, and a picking pass includes creating the render thread's
/// graphics device and compiling its pipelines, which can take many seconds there.
constexpr int pickTimeoutMs = 20000;

/// Records a verification check that did not produce the expected result.
void reportVerificationFailure(const QString& message)
{
    verificationFailures++;
    qWarning() << "VERIFY_FAILED" << message;
}

/// Saves the current contents of the workbench window to an image file and quits the application.
void captureWindowAndQuit(QmlMainWindowUI* ui, const QString& file)
{
    QImage image = ui->view() ? ui->view()->grabWindow() : QImage();
    if(image.isNull() || !image.save(file))
        reportVerificationFailure(QStringLiteral("Failed to capture the workbench window to %1").arg(file));
    else
        qInfo() << "Saved workbench window contents to" << file;
    QCoreApplication::exit(verificationFailures == 0 ? 0 : 1);
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

/// The outcome of probing the picking buffer at a set of positions.
struct PickProbe
{
    int tested = 0;
    int hits = 0;
    QPoint center;       ///< The probe location the grid was actually centered on.
    QSizeF itemSize;     ///< The item size the probe was clipped to.
    std::optional<ViewportWindow::PickResult> example;
};

/// Picks at a grid of positions around the given location of a viewport item. The grid is moved into the item
/// when the requested location lies outside of it, so that a probe always covers the viewport - including after
/// the item geometry changed.
PickProbe probePicking(QuickViewportWindow* viewportWindow, const QSizeF& itemSize, const QPoint& center, int extent = 8, int spacing = 4)
{
    PickProbe probe;
    const int marginX = qMin(int(extent), int(itemSize.width()) / 2);
    const int marginY = qMin(int(extent), int(itemSize.height()) / 2);
    probe.center = QPoint(qBound(marginX, center.x(), int(itemSize.width()) - marginX),
                          qBound(marginY, center.y(), int(itemSize.height()) - marginY));
    probe.itemSize = itemSize;
    for(int dy = -extent; dy <= extent; dy += spacing) {
        for(int dx = -extent; dx <= extent; dx += spacing) {
            const QPointF pos = QPointF(probe.center) + QPointF(dx, dy);
            if(pos.x() < 0 || pos.y() < 0 || pos.x() >= itemSize.width() || pos.y() >= itemSize.height())
                continue;
            probe.tested++;
            if(auto result = viewportWindow->pick(pos)) {
                probe.hits++;
                if(!probe.example)
                    probe.example = std::move(result);
            }
        }
    }
    return probe;
}

/// Prints the outcome of a picking probe.
void reportPicking(const PickProbe& probe, const QString& what)
{
    // The center and the item size are part of the report: a probe that hit fewer positions than an earlier one
    // is only comparable when both covered the same viewport geometry.
    if(probe.example) {
        const SceneNode* node = probe.example->sceneNode();
        qInfo() << "PICK_TEST" << what << ":" << probe.hits << "of" << probe.tested << "positions hit"
                << "at" << probe.center << "in item" << probe.itemSize << ";"
                << "example: node" << (node ? node->objectTitle() : QStringLiteral("<none>"))
                << "subobject" << probe.example->subobjectId()
                << "hit location" << probe.example->hitLocation().x() << probe.example->hitLocation().y() << probe.example->hitLocation().z();
    }
    else {
        qInfo() << "PICK_TEST" << what << ":" << probe.hits << "of" << probe.tested << "positions hit"
                << "at" << probe.center << "in item" << probe.itemSize;
    }
}

/// Returns the first viewport item of the QML scene, or null if the scene has none.
QuickViewportItem* firstViewportItem(QmlMainWindowUI* ui)
{
    const QList<QuickViewportItem*> items = viewportItems(ui);
    return items.isEmpty() ? nullptr : items.front();
}

/// Evaluates a condition on the GUI thread repeatedly until it becomes true or the timeout expires.
/// \param done  Receives true when the condition was satisfied and false on timeout.
void pollUntil(QmlMainWindowUI* ui, int intervalMs, int timeoutMs, std::function<bool()> condition, std::function<void(bool)> done)
{
    QQuickWindow* window = ui->view();
    if(!window) {
        done(false);
        return;
    }
    auto* timer = new QTimer(window);
    timer->setInterval(intervalMs);
    QObject::connect(timer, &QTimer::timeout, window,
        [timer, started = QDateTime::currentMSecsSinceEpoch(), timeoutMs, condition = std::move(condition), done = std::move(done)]() mutable {
            if(!condition() && QDateTime::currentMSecsSinceEpoch() - started < timeoutMs)
                return;
            const bool satisfied = condition();
            timer->stop();
            timer->deleteLater();
            done(satisfied);
        });
    timer->start();
}

/// Reports the state a picking check was made under. Without it a failing check only says "no object was picked",
/// which does not distinguish a slow or failed picking pass from a scene with nothing at the probed position.
void reportPickingState(QmlMainWindowUI* ui, const QPoint& probePos)
{
    QuickViewportItem* item = firstViewportItem(ui);
    QuickViewportWindow* viewportWindow = item ? item->viewportWindow() : nullptr;
    if(!item || !viewportWindow) {
        qInfo() << "PICK_TEST state at failure: no viewport item or viewport window";
        return;
    }
    const PickProbe probe = probePicking(viewportWindow, item->size(), probePos);
    const std::optional<ViewportWindow::PickResult> result = viewportWindow->pick(QPointF(probePos));
    qInfo() << "PICK_TEST state at failure: item" << item->size() << "probe center" << probe.center
            << "in item" << probe.itemSize << "hits" << probe.hits
            << "pick()" << (result ? "returned a result" : "returned nothing");
}

/// Waits until the picking buffer has been re-rendered for the current viewport state after the viewport changed
/// (a resize or a hide/show cycle) and reports how long that took, counted from the moment the change was made.
void waitForPickingBuffer(QmlMainWindowUI* ui, const QPoint& probePos, const QString& what, QDateTime changeTime,
                          std::function<void()> continuation, QSizeF previousItemSize = {})
{
    const QDateTime started = std::move(changeTime);
    pollUntil(ui, 10, pickTimeoutMs,
        [ui, probePos, previousItemSize]() {
            if(QuickViewportItem* item = firstViewportItem(ui)) {
                // After a resize the viewport geometry must have been updated before a picking buffer for the new
                // size can exist; without this check the check below would be satisfied by the buffer of the old size.
                if(previousItemSize.isValid() && item->size() == previousItemSize)
                    return false;
                if(QuickViewportWindow* viewportWindow = item->viewportWindow())
                    return probePicking(viewportWindow, item->size(), probePos).hits > 0;
            }
            return false;
        },
        [what, started, continuation, ui, probePos](bool satisfied) {
            if(satisfied)
                qInfo() << "PICK_TEST" << what << ": picking works again after" << started.msecsTo(QDateTime::currentDateTime()) << "ms";
            else {
                reportVerificationFailure(QStringLiteral("no object was picked within %1 s %2").arg(pickTimeoutMs / 1000).arg(what));
                reportPickingState(ui, probePos);
            }
            continuation();
        });
}

/// Verifies object picking at the given position of the first viewport item.
///
/// Picking is served from a buffer that the render thread renders asynchronously, so the first call after the
/// viewport contents changed cannot return a result yet; it starts the picking pass instead. The test therefore
/// measures twice and reports both outcomes, then verifies that a synthetic mouse click selects an object
/// through the regular input mode path.
void runPickTest(QmlMainWindowUI* ui, const QPoint& itemPos, std::function<void()> continuation)
{
    QuickViewportItem* item = firstViewportItem(ui);
    QuickViewportWindow* viewportWindow = item ? item->viewportWindow() : nullptr;
    if(!viewportWindow) {
        reportVerificationFailure(QStringLiteral("no viewport item found"));
        continuation();
        return;
    }

    const QSizeF itemSize = item->size();
    qInfo() << "PICK_TEST" << viewportItems(ui).size() << "viewport items; first item size" << itemSize
            << "test position" << itemPos;

    // First pass: the picking buffer has not been rendered yet, so this starts the picking pass.
    reportPicking(probePicking(viewportWindow, item->size(), itemPos), QStringLiteral("before picking pass"));

    // Measure how long the asynchronously rendered picking buffer takes to become available. The item geometry is
    // re-read for every probe so that a layout pass happening in between cannot distort the measurement.
    const QDateTime probeStart = QDateTime::currentDateTime();
    pollUntil(ui, 25, pickTimeoutMs,
        [viewportWindow, item, itemPos]() {
            return probePicking(viewportWindow, item->size(), itemPos).hits > 0;
        },
        [ui, item, viewportWindow, itemPos, probeStart, continuation](bool satisfied) {
            if(satisfied)
                qInfo() << "PICK_TEST first successful pick after" << probeStart.msecsTo(QDateTime::currentDateTime()) << "ms";
            else {
                reportVerificationFailure(QStringLiteral("the picking buffer did not become available within %1 s").arg(pickTimeoutMs / 1000));
                reportPickingState(ui, itemPos);
            }

            // Second pass: the picking pass has been rendered in the background in the meantime.
            const PickProbe finalProbe = probePicking(viewportWindow, item->size(), itemPos);
            reportPicking(finalProbe, QStringLiteral("after picking pass"));
            if(finalProbe.hits == 0)
                reportVerificationFailure(QStringLiteral("no object was picked at the test position"));

            // Negative control: a corner of the viewport shows only the empty background.
            if(std::optional<ViewportWindow::PickResult> backgroundPick = viewportWindow->pick(QPointF(2, 2))) {
                const SceneNode* node = backgroundPick->sceneNode();
                qInfo() << "PICK_TEST background control: picked" << (node ? node->objectTitle() : QStringLiteral("<none>"));
            }
            else {
                qInfo() << "PICK_TEST background control: nothing picked";
            }

            // End-to-end check: a synthetic click must select an object through the regular input mode path. The
            // pick is repeated right before the click, because the input mode decides on the pick result it is
            // given and a stale result would report a selection without a working pick.
            const QPointF clickPos(itemPos);
            const std::optional<ViewportWindow::PickResult> pickBeforeClick = viewportWindow->pick(clickPos);
            qInfo() << "PICK_TEST the pick before the synthetic click" << (pickBeforeClick ? "hit" : "missed");
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
                            reportVerificationFailure(QStringLiteral("synthetic click selected nothing"));
                    }
                }
            }
            continuation();
        });
}

/// Hides the viewport items for a moment and shows them again. A hidden viewport releases its GPU resources and
/// acquires them again when it reappears; rendering and picking must survive that.
void runHideShowTest(QmlMainWindowUI* ui, const QPoint& probePos, std::function<void()> continuation)
{
    const QList<QuickViewportItem*> items = viewportItems(ui);
    if(items.isEmpty()) {
        reportVerificationFailure(QStringLiteral("hide/show: no viewport item found"));
        continuation();
        return;
    }

    for(QuickViewportItem* item : items)
        item->setVisible(false);

    // While hidden, no picking pass is started and the cached buffer is retained.
    if(QuickViewportItem* item = items.front()) {
        if(QuickViewportWindow* viewportWindow = item->viewportWindow())
            reportPicking(probePicking(viewportWindow, item->size(), probePos), QStringLiteral("while hidden"));
    }

    const QDateTime changeTime = QDateTime::currentDateTime();
    scheduleDelayed(ui, 500, [ui, items, probePos, changeTime, continuation]() {
        for(QuickViewportItem* item : items)
            item->setVisible(true);
        waitForPickingBuffer(ui, probePos, QStringLiteral("after hide/show"), changeTime, continuation);
    });
}

/// Resizes the workbench window. The picking buffer is tied to the viewport device size, so this also verifies
/// that picking recovers after a resize, which may happen at any time in a real session.
void runResizeTest(QmlMainWindowUI* ui, const QSize& newSize, const QPoint& probePos, std::function<void()> continuation)
{
    QQuickWindow* window = ui->view();
    if(!window) {
        continuation();
        return;
    }
    // Remember the viewport geometry. The picking buffer of the old size must not be mistaken for one that
    // belongs to the resized viewport.
    QSizeF previousItemSize;
    if(QuickViewportItem* item = firstViewportItem(ui))
        previousItemSize = item->size();
    qInfo() << "PICK_TEST resize:" << window->size() << "->" << newSize;
    const QDateTime changeTime = QDateTime::currentDateTime();
    window->resize(newSize);

    // The picking buffer is tied to the viewport device size, so it must not be used for a resized viewport.
    if(QuickViewportItem* item = firstViewportItem(ui)) {
        if(QuickViewportWindow* viewportWindow = item->viewportWindow())
            reportPicking(probePicking(viewportWindow, item->size(), probePos), QStringLiteral("immediately after resize"));
    }

    // Poll immediately: the picking buffer is only used once the viewport geometry has been updated, so the
    // measured time is the delay until a picking buffer exists for the resized viewport.
    waitForPickingBuffer(ui, probePos, QStringLiteral("after resize"), changeTime, continuation, previousItemSize);
}

/// Measures the frame rate the viewports achieve when frames are requested continuously, which is what animation
/// playback does. The requests originate from a queued connection because QQuickWindow::frameSwapped is emitted
/// on the render thread, while requesting frames is a GUI-thread operation.
void measureFrameRate(QmlMainWindowUI* ui, int durationMs, std::function<void()> continuation)
{
    QQuickWindow* window = ui->view();
    if(!window) {
        continuation();
        return;
    }
    const QList<QuickViewportItem*> items = viewportItems(ui);

    auto frames = std::make_shared<int>(0);
    auto requestNextFrame = [items]() {
        for(QuickViewportItem* item : items) {
            if(QuickViewportWindow* viewportWindow = item->viewportWindow())
                viewportWindow->requestRerender(false);
        }
    };

    QMetaObject::Connection counter = QObject::connect(window, &QQuickWindow::frameSwapped, window,
        [frames, requestNextFrame]() { (*frames)++; requestNextFrame(); }, Qt::QueuedConnection);

    auto* timer = new QTimer(window);
    timer->setSingleShot(true);
    QObject::connect(timer, &QTimer::timeout, window, [frames, counter, durationMs, items, continuation]() {
        QObject::disconnect(counter);
        const double fps = durationMs > 0 ? (*frames * 1000.0) / durationMs : 0.0;
        qInfo() << "FRAME_STATS" << *frames << "frames in" << durationMs << "ms with" << items.size() << "viewports ->"
                << QString::number(fps, 'f', 1) << "fps (" << QString::number(fps > 0.0 ? 1000.0 / fps : 0.0, 'f', 2) << "ms/frame)";
        // A viewport that renders nothing at all still looks healthy in a screenshot-less run; this is the check
        // that catches it.
        if(*frames == 0)
            reportVerificationFailure(QStringLiteral("no frame was rendered within %1 ms").arg(durationMs));
        continuation();
    });
    timer->start(durationMs);
    requestNextFrame();
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
        parser.addOption(QCommandLineOption(QStringLiteral("qml-frame-stats"),
            tr("Measure the frame rate of the viewport for the given number of milliseconds and print the result."),
            QStringLiteral("MS")));
        parser.addOption(QCommandLineOption(QStringLiteral("qml-window-size"),
            tr("Resize the workbench window to the given size (WxH) before anything is measured."),
            QStringLiteral("WxH")));
        parser.addOption(QCommandLineOption(QStringLiteral("qml-resize"),
            tr("Resize the workbench window to the given size (WxH) and verify that rendering and picking recover."),
            QStringLiteral("WxH")));
        parser.addOption(QCommandLineOption(QStringLiteral("qml-hide-show"),
            tr("Hide the viewport items for a moment and show them again, verifying that rendering and picking recover.")));
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

        // Optional picking verification: perform picking operations and print the outcome.
        std::optional<QPoint> pickPosition;
        const QStringList pickCoordinates = cmdLineParser().value(QStringLiteral("qml-pick")).split(QLatin1Char(','));
        if(pickCoordinates.size() == 2)
            pickPosition = QPoint(pickCoordinates[0].toInt(), pickCoordinates[1].toInt());

        // Optional initial window size: measurements are taken at this size, e.g. to match the viewport area of
        // another frontend for a frame rate comparison.
        const QStringList initialSize = cmdLineParser().value(QStringLiteral("qml-window-size")).split(QLatin1Char('x'));
        if(initialSize.size() == 2)
            mainWinUI->view()->resize(initialSize[0].toInt(), initialSize[1].toInt());

        // Optional capture mode: save the contents of the workbench window to an image file once all
        // verification steps are done.
        const QString captureFile = cmdLineParser().value(QStringLiteral("qml-capture"));
        const bool verifyPicking = pickPosition.has_value();
        const int frameStatsDuration = cmdLineParser().value(QStringLiteral("qml-frame-stats")).toInt();
        const bool hideShowTest = cmdLineParser().isSet(QStringLiteral("qml-hide-show"));
        const int lifecycleCycles = cmdLineParser().value(QStringLiteral("qml-lifecycle-cycles")).toInt();
        QSize resizeSize;
        const QStringList resizeArguments = cmdLineParser().value(QStringLiteral("qml-resize")).split(QLatin1Char('x'));
        if(resizeArguments.size() == 2)
            resizeSize = QSize(resizeArguments[0].toInt(), resizeArguments[1].toInt());

        // Interactive mode: without a capture request or a verification option, keep the window open.
        if(captureFile.isEmpty() && !verifyPicking && frameStatsDuration <= 0 && !hideShowTest && !resizeSize.isValid() && lifecycleCycles <= 0)
            return;

        int delay = cmdLineParser().value(QStringLiteral("qml-capture-delay")).toInt();
        if(delay <= 0)
            delay = 3000;

        _verificationFinished = [ui = mainWinUI, captureFile]() {
            if(captureFile.isEmpty())
                QCoreApplication::exit(verificationFailures == 0 ? 0 : 1);
            else
                captureWindowAndQuit(ui, captureFile);
        };

        // Assemble the verification steps. The delay runs first, so that the imported data set and the initial
        // frame graphs are ready before anything is measured.
        if(verifyPicking) {
            // Pick as early as possible, while the imported scene is likely still being evaluated. The picking
            // pass is an asynchronous offscreen operation; it must neither block the GUI thread nor deadlock with
            // the evaluation of the pipeline that it renders.
            _verificationSteps.push_back([ui = mainWinUI, pickPos = *pickPosition](std::function<void()> next) {
                scheduleDelayed(ui, 100, [ui, pickPos, next = std::move(next)]() {
                    const QDateTime started = QDateTime::currentDateTime();
                    if(QuickViewportItem* item = firstViewportItem(ui)) {
                        if(QuickViewportWindow* viewportWindow = item->viewportWindow()) {
                            const PickProbe probe = probePicking(viewportWindow, item->size(), pickPos);
                            reportPicking(probe, QStringLiteral("during scene evaluation"));
                        }
                    }
                    qInfo() << "PICK_TEST the probe during scene evaluation returned after"
                            << started.msecsTo(QDateTime::currentDateTime()) << "ms";
                    next();
                });
            });
        }
        _verificationSteps.push_back([ui = mainWinUI, delay](std::function<void()> next) {
            scheduleDelayed(ui, delay, std::move(next));
        });
        if(frameStatsDuration > 0) {
            _verificationSteps.push_back([ui = mainWinUI, frameStatsDuration](std::function<void()> next) {
                measureFrameRate(ui, frameStatsDuration, std::move(next));
            });
        }
        if(lifecycleCycles > 0) {
            // Exercise the resource lifecycle (scene graph teardown and recreation) of the viewport items.
            _verificationSteps.push_back([ui = mainWinUI, lifecycleCycles](std::function<void()> next) {
                scheduleLifecycleCycles(ui, lifecycleCycles, std::move(next));
            });
        }
        if(hideShowTest) {
            _verificationSteps.push_back([ui = mainWinUI, probePos = pickPosition.value_or(QPoint(0, 0))](std::function<void()> next) {
                runHideShowTest(ui, probePos, std::move(next));
            });
        }
        if(resizeSize.isValid()) {
            _verificationSteps.push_back([ui = mainWinUI, resizeSize, probePos = pickPosition.value_or(QPoint(0, 0))](std::function<void()> next) {
                runResizeTest(ui, resizeSize, probePos, std::move(next));
            });
        }
        if(verifyPicking) {
            _verificationSteps.push_back([ui = mainWinUI, pickPos = *pickPosition](std::function<void()> next) {
                runPickTest(ui, pickPos, std::move(next));
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
