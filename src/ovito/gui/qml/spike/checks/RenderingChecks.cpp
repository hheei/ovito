// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

// The checks that exercise the graphics side of the workbench: the picking path, the frame rate, the resource
// lifecycle, resizing, hiding and showing, the ambient-occlusion and render-output offscreen paths and the
// graphics device report. Everything they share is in SpikeHarness.h; the helpers of one check stay in this file.

#include <ovito/gui/qml/spike/SpikeHarness.h>

namespace Ovito::Spike {

/// Verifies object picking at the given position of the first viewport item.
///
/// Picking is served from a buffer that the render thread renders asynchronously, so the first call after the
/// viewport contents changed cannot return a result yet; it starts the picking pass instead. The test therefore
/// measures twice and reports both outcomes, then verifies that a synthetic mouse click selects an object
/// through the regular input mode path.
void runPickTest(QmlMainWindowUI* ui, const QPoint& itemPos, std::function<void()> continuation)
{
    declareCheckPhases({ "viewport item", "picking buffer", "synthetic click" });

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

    reportCheckPhase("viewport item");

    // First pass: the picking buffer has not been rendered yet, so this starts the picking pass.
    reportPicking(probePicking(viewportWindow, item->size(), itemPos), QStringLiteral("before picking pass"));

    // Measure how long the asynchronously rendered picking buffer takes to become available. The item geometry is
    // re-read for every probe so that a layout pass happening in between cannot distort the measurement.
    const QDateTime probeStart = QDateTime::currentDateTime();
    pollUntil(ui, 25, pickTimeoutMs,
        [viewportWindow, item]() {
            // Any object anywhere in the viewport proves that the picking buffer has arrived; see scanPicking().
            return scanPicking(viewportWindow, item->size()).hits > 0;
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
            const PickProbe scan = scanPicking(viewportWindow, item->size());
            reportPicking(scan, QStringLiteral("viewport scan"));
            if(scan.hits == 0)
                reportVerificationFailure(QStringLiteral("no object was picked anywhere in the viewport"));
            else if(finalProbe.hits == 0)
                qInfo() << "PICK_TEST the test position" << itemPos << "does not contain an object in this viewport shape";

            reportCheckPhase("picking buffer");

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
            // The click goes to the viewport item that exists now. The item the poll started with may have been
            // replaced in the meantime, because the workbench rebuilds the panes of its viewport area when the
            // layout changes, and sending an event to a destroyed item would crash the test.
            QuickViewportItem* clickTarget = firstViewportItem(ui);
            if(!clickTarget) {
                reportVerificationFailure(QStringLiteral("synthetic click: the workbench has no viewport item"));
                continuation();
                return;
            }
            const QPointF globalPos = clickTarget->mapToGlobal(clickPos);
            QMouseEvent pressEvent(QEvent::MouseButtonPress, clickPos, globalPos, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
            QCoreApplication::sendEvent(clickTarget, &pressEvent);
            QMouseEvent releaseEvent(QEvent::MouseButtonRelease, clickPos, globalPos, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
            QCoreApplication::sendEvent(clickTarget, &releaseEvent);

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
            reportCheckPhase("synthetic click");
            continuation();
        });
}

/// Hides the viewport items for a moment and shows them again. A hidden viewport releases its GPU resources and
/// acquires them again when it reappears; rendering and picking must survive that.
void runHideShowTest(QmlMainWindowUI* ui, const QPoint& probePos, std::function<void()> continuation)
{
    declareCheckPhases({ "hidden", "shown again" });

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

    reportCheckPhase("hidden");

    const QDateTime changeTime = QDateTime::currentDateTime();
    scheduleDelayed(ui, 500, [ui, items, probePos, changeTime, continuation]() {
        for(QuickViewportItem* item : items)
            item->setVisible(true);
        waitForPickingBuffer(ui, probePos, QStringLiteral("after hide/show"), changeTime, [continuation]() {
            reportCheckPhase("shown again");
            continuation();
        });
    });
}

/// Resizes the workbench window. The picking buffer is tied to the viewport device size, so this also verifies
/// that picking recovers after a resize, which may happen at any time in a real session.
void runResizeTest(QmlMainWindowUI* ui, const QSize& newSize, const QPoint& probePos, std::function<void()> continuation)
{
    declareCheckPhases({ "resized", "picking after the resize" });

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
    reportCheckPhase("resized");

    waitForPickingBuffer(ui, probePos, QStringLiteral("after resize"), changeTime, [continuation]() {
        reportCheckPhase("picking after the resize");
        continuation();
    }, previousItemSize);
}

/******************************************************************************
* Verifies that the frontend notices when its platform plugin provides no graphics device.
*
* Which device is available depends on the platform plugin the run was started with, so this check prescribes no
* outcome. It prescribes that the frontend's answer matches the scene graph of the window, and that the error path
* tells the user which platform plugin is at fault. A run with QT_QPA_PLATFORM=offscreen takes that path: Qt's
* offscreen plugin provides no QRhi, so the viewports of such a run stay empty.
******************************************************************************/
void runDeviceTest(QmlMainWindowUI* ui, std::function<void()> continuation)
{
    // One phase: the frontend answers whether it has a graphics device, and that answer has to match the window.
    declareCheckPhases({ "device report" });

    pollUntil(ui, 100, 10000, [ui]() { return ui->hasGraphicsDevice().has_value(); },
        [ui, continuation = std::move(continuation)](bool answered) {

        QQuickWindow* window = ui->view();
        if(!answered || !window) {
            reportVerificationFailure(QStringLiteral("the frontend did not report whether it has a graphics device"));
            continuation();
            return;
        }

        const bool available = (window->rhi() != nullptr);
        const bool reportedAvailable = *ui->hasGraphicsDevice();
        const QString platformName = QGuiApplication::platformName();

        QString apiName;
        if(available)
            apiName = QString::fromUtf8(QRhi::backendName(window->rhi()->backend()));
        qInfo() << "DEVICE_TEST platform plugin" << platformName << "graphics device"
                << (available ? "available" : "missing") << "reported"
                << (reportedAvailable ? "available" : "missing") << "api" << apiName;

        if(available != reportedAvailable) {
            reportVerificationFailure(QStringLiteral("the frontend reported a graphics device as %1 while the window "
                "has %2").arg(reportedAvailable ? QStringLiteral("available") : QStringLiteral("missing"),
                               available ? QStringLiteral("one") : QStringLiteral("none")));
        }
        if(!available) {
            // The user-visible half of the error path: the workbench must say what is wrong instead of leaving four
            // empty panes, and it must name the platform plugin that failed to provide the device.
            const QString notice = ui->workbenchController() ? ui->workbenchController()->notice() : QString();
            if(!notice.contains(platformName)) {
                reportVerificationFailure(QStringLiteral("the frontend did not report the missing graphics device "
                    "with the platform plugin's name (notice: \"%1\")").arg(notice));
            }
        }
        reportCheckPhase("device report");
        continuation();
    });
}

static /******************************************************************************
* Renders the scene into the given frame buffer through RenderSettings, which is the render output path of both
* frontends: one offscreen render target per viewport of the configuration, assembled into the frame buffer.
******************************************************************************/
std::shared_ptr<Future<void>> startRenderOutput(QmlMainWindowUI* ui, const std::shared_ptr<FrameBuffer>& frameBuffer)
{
    DataSet* dataset = ui->datasetContainer().currentSet();
    RenderSettings* renderSettings = dataset ? dataset->renderSettings() : nullptr;
    OORef<const ViewportConfiguration> viewportConfig = dataset ? dataset->viewportConfig() : nullptr;
    if(!renderSettings || !viewportConfig)
        return {};

    OORef<const AnimationSettings> animationSettings;
    if(Viewport* viewport = viewportConfig->activeViewport()) {
        if(Scene* scene = viewport->scene())
            animationSettings = scene->animationSettings();
    }
    return std::make_shared<Future<void>>(renderSettings->render(*viewportConfig, std::move(animationSettings), frameBuffer));
}

static /******************************************************************************
* Returns whether two images of the same size differ in at least one pixel. The comparison walks a band of scan lines,
* because a rendered image differs in many pixels and the check only has to tell "the same" from "not the same".
******************************************************************************/
bool imagesDiffer(const QImage& a, const QImage& b)
{
    if(a.isNull() || b.isNull() || a.size() != b.size() || a.format() != b.format())
        return true;
    const int step = std::max(1, a.height() / 64);
    for(int y = 0; y < a.height(); y += step) {
        if(std::memcmp(a.constScanLine(y), b.constScanLine(y), a.bytesPerLine()) != 0)
            return true;
    }
    return false;
}

static /******************************************************************************
* Returns the number of modifiers of the given class name in the first pipeline of the scene, or -1 if the scene has
* no such pipeline. Walking the chain needs only classes of the core, so the check works without the particles plugin.
******************************************************************************/
int countPipelineModifiers(QmlMainWindowUI* ui, const QString& className)
{
    const Scene* scene = ui->datasetContainer().activeScene();
    if(!scene || scene->children().empty())
        return -1;
    const Pipeline* pipeline = scene->children().front()->pipeline();
    if(!pipeline)
        return -1;

    int count = 0;
    for(const PipelineNode* node = pipeline->head(); node; ) {
        const ModificationNode* modificationNode = dynamic_object_cast<ModificationNode>(node);
        if(!modificationNode)
            break;
        if(const Modifier* modifier = modificationNode->modifier()) {
            if(modifier->getOOClass().name().contains(className))
                count++;
        }
        node = modificationNode->input();
    }
    return count;
}

/// The state of the offscreen rendering check while it runs across several event loop turns. The check renders the
/// scene twice (before and after the ambient-occlusion sampling recolored the particles) and resizes the viewports in
/// between, so that the sampling, a picking pass and the render output are in flight at the same time.
struct OffscreenCheckState
{
    QmlMainWindowUI* ui = nullptr;
    std::function<void()> continuation;
    PipelineListModel* pipelineListModel = nullptr;
    std::shared_ptr<FrameBuffer> baselineFrameBuffer;
    QImage baselineImage;
    std::shared_ptr<FrameBuffer> frameBuffer;
    std::shared_ptr<Future<void>> renderFuture;
    QSizeF viewportItemSizeBeforeResize;
    int outputsRendered = 0;
};

static /// Timeout of the offscreen passes of the check. Rendering an image is quick, but the ambient-occlusion sampling of
/// the pipeline evaluation may need a few seconds before it recolored the particles.
constexpr int offscreenTimeoutMs = 90000;

static /// Waits until the picking pass that the resize superseded has been rendered again.
void verifyPickingAfterResize(std::shared_ptr<OffscreenCheckState> state)
{
    GuiTaskScope taskScope(*state->ui);

    const QDateTime started = QDateTime::currentDateTime();
    auto resizeApplied = std::make_shared<bool>(false);
    pollUntil(state->ui, 25, pickTimeoutMs,
        [state, resizeApplied]() {
            if(QuickViewportItem* item = firstViewportItem(state->ui)) {
                // The layout has to follow the resize before a picking buffer for the new size can exist.
                if(item->size() != state->viewportItemSizeBeforeResize)
                    *resizeApplied = true;
                if(!*resizeApplied)
                    return false;
                if(QuickViewportWindow* viewportWindow = item->viewportWindow())
                    return scanPicking(viewportWindow, item->size()).hits > 0;
            }
            return false;
        },
        [state, started, resizeApplied](bool satisfied) {
            if(satisfied)
                qInfo() << "OFFSCREEN_TEST picking works again" << started.msecsTo(QDateTime::currentDateTime())
                        << "ms after the resize superseded the picking target";
            else if(!*resizeApplied) {
                // A window manager can refuse to resize the window (a maximized one, or one that already fills the
                // screen of a CI runner); the picking target was then never replaced, so the property this check
                // verifies cannot be observed on this machine. Picking itself is reported all the same.
                QuickViewportItem* item = firstViewportItem(state->ui);
                const PickProbe probe = item ? scanPicking(item->viewportWindow(), item->size()) : PickProbe();
                qInfo() << "OFFSCREEN_TEST the platform did not apply the resize of the workbench, so the superseded"
                        << "picking target was not exercised; picking still works:" << probe.hits << "of" << probe.tested
                        << "probed positions";
            }
            else {
                reportVerificationFailure(QStringLiteral("no object was picked after the resize had superseded the picking target"));
                reportPickingState(state->ui, QPoint(8, 8));
            }
            reportCheckPhase("picking after the resize");
            state->continuation();
        });
}

static /// Renders the scene again and compares the image with the baseline. The image differs once the ambient-occlusion
/// sampling has recolored the particles, so the comparison doubles as the signal that the sampling ran to completion.
void renderOutputAgain(std::shared_ptr<OffscreenCheckState> state)
{
    // The pointer above only lives inside this call: a callback of the event loop has no task context of its own, and
    // starting a render output (which asks the render thread for an offscreen target) needs the user interface.
    GuiTaskScope taskScope(*state->ui);

    DataSet* dataset = state->ui->datasetContainer().currentSet();
    state->frameBuffer = std::make_shared<FrameBuffer>(dataset->renderSettings()->outputImageWidth(),
                                                       dataset->renderSettings()->outputImageHeight());
    state->renderFuture = startRenderOutput(state->ui, state->frameBuffer);
    state->outputsRendered++;

    // A data set without render settings or without a viewport configuration cannot be rendered at all; the check
    // reports that instead of waiting for a future that does not exist.
    if(!state->renderFuture) {
        reportVerificationFailure(QStringLiteral("the render output could not be started: the data set holds no render settings or no viewport configuration"));
        state->continuation();
        return;
    }

    pollUntil(state->ui, 25, offscreenTimeoutMs,
        [state]() { return state->renderFuture->isFinished(); },
        [state](bool finished) {
        // A timeout callback has no task context of its own; waiting for the future and reading the image both need one.
        GuiTaskScope taskScope(*state->ui);

        if(!finished) {
            reportVerificationFailure(QStringLiteral("the render output did not finish within %1 s").arg(offscreenTimeoutMs / 1000));
            state->continuation();
            return;
        }
        try {
            state->renderFuture->waitForFinished();
        }
        catch(const Exception& ex) {
            reportVerificationFailure(QStringLiteral("the render output failed: %1").arg(ex.messages().join(QStringLiteral("; "))));
            state->continuation();
            return;
        }

        const QImage image = state->frameBuffer->image();
        if(image.isNull() || image.size() != state->frameBuffer->size()) {
            reportVerificationFailure(QStringLiteral("the render output produced no image of the requested size"));
            state->continuation();
            return;
        }

        if(!imagesDiffer(image, state->baselineImage)) {
            // The scene renders like before as long as the sampling has not recolored the particles. Give it time.
            if(state->outputsRendered < 30) {
                QTimer::singleShot(250, state->ui->view(), [state]() { renderOutputAgain(state); });
                return;
            }
            reportVerificationFailure(QStringLiteral("the ambient occlusion sampling did not change the rendered image"));
            state->continuation();
            return;
        }

        qInfo() << "OFFSCREEN_TEST the render output" << image.size() << "shows the ambient occlusion shading after"
                << state->outputsRendered << "render pass(es)";
        reportCheckPhase("render output");
        verifyPickingAfterResize(state);
    });
}

static /// Renders the baseline image, inserts the ambient-occlusion modifier through its shared command and starts the passes
/// that render at the same time: the sampling of the pipeline evaluation, a picking pass and a second render output.
void runOffscreenCheckWithSelection(std::shared_ptr<OffscreenCheckState> state)
{
    GuiTaskScope taskScope(*state->ui);

    QmlMainWindowUI* ui = state->ui;
    DataSet* dataset = ui->datasetContainer().currentSet();
    RenderSettings* renderSettings = dataset ? dataset->renderSettings() : nullptr;
    if(!renderSettings) {
        reportVerificationFailure(QStringLiteral("the data set has no render settings, so the render output cannot be checked"));
        state->continuation();
        return;
    }
    if(countPipelineModifiers(ui, QStringLiteral("AmbientOcclusion")) != 0) {
        reportVerificationFailure(QStringLiteral("the pipeline contains an ambient occlusion modifier before the check inserted one"));
        state->continuation();
        return;
    }

    // The baseline: how the scene renders before the sampling recolored the particles.
    state->baselineFrameBuffer = std::make_shared<FrameBuffer>(renderSettings->outputImageWidth(), renderSettings->outputImageHeight());
    state->renderFuture = startRenderOutput(ui, state->baselineFrameBuffer);
    state->outputsRendered++;
    if(!state->renderFuture) {
        reportVerificationFailure(QStringLiteral("the render output could not be started"));
        state->continuation();
        return;
    }

    pollUntil(ui, 25, offscreenTimeoutMs,
        [state]() { return state->renderFuture->isFinished(); },
        [state](bool finished) {
        GuiTaskScope taskScope(*state->ui);

        QmlMainWindowUI* ui = state->ui;
        if(!finished) {
            reportVerificationFailure(QStringLiteral("the baseline render output did not finish within %1 s").arg(offscreenTimeoutMs / 1000));
            state->continuation();
            return;
        }
        try {
            state->renderFuture->waitForFinished();
            state->baselineImage = state->baselineFrameBuffer->image();
        }
        catch(const Exception& ex) {
            reportVerificationFailure(QStringLiteral("the baseline render output failed: %1").arg(ex.messages().join(QStringLiteral("; "))));
            state->continuation();
            return;
        }
        if(state->baselineImage.isNull()) {
            reportVerificationFailure(QStringLiteral("the baseline render output produced no image"));
            state->continuation();
            return;
        }
        reportCheckPhase("baseline");

        // Insert the ambient-occlusion modifier through the shared command of its library entry, the way the frontends
        // do it. Its sampling loop submits one picking-only pass per sample from the pipeline evaluation's worker thread.
        Command* insertCommand = nullptr;
        for(Command* command : ui->actionManager()->commands()) {
            if(command->id().contains(QStringLiteral("AmbientOcclusionModifier"))) {
                insertCommand = command;
                break;
            }
        }
        if(!insertCommand) {
            reportVerificationFailure(QStringLiteral("this build provides no command that inserts the ambient occlusion modifier"));
            state->continuation();
            return;
        }
        if(!insertCommand->isEnabled()) {
            reportVerificationFailure(QStringLiteral("the command \"%1\" is disabled, so the pipeline cannot adopt the modifier").arg(insertCommand->id()));
            state->continuation();
            return;
        }
        insertCommand->trigger();
        if(countPipelineModifiers(ui, QStringLiteral("AmbientOcclusion")) != 1) {
            reportVerificationFailure(QStringLiteral("triggering \"%1\" did not insert the modifier into the pipeline").arg(insertCommand->id()));
            state->continuation();
            return;
        }
        qInfo() << "OFFSCREEN_TEST the ambient occlusion modifier was inserted through the shared command"
                << insertCommand->id();
        reportCheckPhase("sampling");

        // Start a picking pass of the Qt Quick viewport, so that it renders while the sampling runs. The result of
        // this pass is not awaited: the resize below supersedes its target, and the check waits for the pass that
        // follows it (see verifyPickingAfterResize()).
        const QList<QuickViewportItem*> items = viewportItems(ui);
        if(items.isEmpty()) {
            reportVerificationFailure(QStringLiteral("the workbench has no viewport to pick in"));
            state->continuation();
            return;
        }
        state->viewportItemSizeBeforeResize = items.front()->size();
        if(QuickViewportWindow* viewportWindow = items.front()->viewportWindow())
            (void)viewportWindow->pick(QPointF(items.front()->width() * 0.5, items.front()->height() * 0.5));

        // Resize the workbench while the sampling, the picking pass and the render output are in flight: the new size
        // makes the picking service replace its offscreen target while a pass may still be reading the old one.
        // The window is made *smaller*: a window manager refuses to enlarge a window beyond the screen (the CI runners
        // have small virtual displays), and then the picking target would never be replaced at all.
        if(QQuickWindow* window = ui->view()) {
            window->resize(qMax(640, window->width() - 40), qMax(400, window->height() - 30));
            qInfo() << "OFFSCREEN_TEST resized the workbench while the sampling, a picking pass and the render output are in flight";
            reportCheckPhase("resize");
        }

        renderOutputAgain(state);
    });
}

static /// Creates the models the check uses, selects the pipeline they operate on and continues with the check once the
/// pipeline list model has adopted the selection.
void startOffscreenCheckAfterImport(std::shared_ptr<OffscreenCheckState> state)
{
    QmlMainWindowUI* ui = state->ui;

    // The library command inserts the modifier into the pipeline that the pipeline list model has selected, and the
    // model adopts the scene selection one event loop turn after the selection was set. The library model is the one
    // that registers the insert commands, so it is created here even though the check only uses one of them.
    state->pipelineListModel = workbenchModels(ui).pipelineList;
    if(workbenchModels(ui).modifiers->rowCount() == 0)
        reportVerificationFailure(QStringLiteral("the modifier library is empty, so no modifier can be inserted"));

    // The model adopts the selection when the container announces that it *changed*, and the node the import selected
    // is already in the set: setting it again would be no change at all, so it is deselected first.
    if(Scene* scene = ui->datasetContainer().activeScene(); scene && !scene->children().empty()) {
        scene->selection()->clear();
        scene->selection()->setNode(scene->children().front());
    }

    pollUntil(ui, 25, 5000,
        [state]() { return state->pipelineListModel->selectedPipeline() != nullptr; },
        [state](bool selected) {
            if(!selected) {
                reportVerificationFailure(QStringLiteral("the pipeline list model selected no pipeline, so the modifier library could not insert anything"));
                state->continuation();
                return;
            }
            runOffscreenCheckWithSelection(state);
        });
}

/******************************************************************************
* Verifies the shared offscreen rendering service (class OffscreenRenderTarget) end to end: that it refuses a pass
* which does not match the kind of its target, and that the ambient-occlusion sampling, a picking pass and a full
* render output run to completion at the same time, including a picking target that a resize supersedes in flight.
******************************************************************************/
void runOffscreenTest(QmlMainWindowUI* ui, std::function<void()> continuation)
{
    // The phases of the chain this check walks through: the kind check, the baseline image, the sampling, the resize,
    // the render output that shows the sampling's effect and the picking pass the resize superseded.
    declareCheckPhases({ "target kind", "baseline", "sampling", "resize", "render output", "picking after the resize" });

    // Creating the pipeline list model and triggering a command instantiates OVITO objects and opens transactions.
    GuiTaskScope taskScope(*ui);

    DataSet* dataset = ui->datasetContainer().currentSet();
    if(!dataset || !dataset->renderSettings() || !dataset->viewportConfig() || sceneObjectCount(ui) == 0) {
        reportVerificationFailure(QStringLiteral("the offscreen check needs a data set with a scene and render settings"));
        continuation();
        return;
    }

    // Passes that do not match the kind of their target are refused before any GPU work is submitted: the checks run
    // before the frame graph is used, which is why an empty one suffices here. In a build with active assertions the
    // refusal aborts the process instead, which is exactly the loudness the service is supposed to have, so the case is
    // skipped there.
#if !defined(OVITO_DEBUG) && !defined(QT_FORCE_ASSERTS)
    if(SceneRenderer* renderer = dataset->renderSettings()->renderer()) {
        {
            OffscreenRenderTarget target(*ui, OffscreenRenderTarget::Kind::Visual);
            bool refused = false;
            try {
                (void)target.renderPicking(OORef<FrameGraph>(), *renderer, QSize(32, 32));
            }
            catch(const Exception&) {
                refused = true;
            }
            if(!refused)
                reportVerificationFailure(QStringLiteral("the service accepted a picking pass on a target created for color images"));
        }
        {
            OffscreenRenderTarget target(*ui, OffscreenRenderTarget::Kind::PickingOnly);
            bool refused = false;
            try {
                (void)target.renderImage(OORef<FrameGraph>(), *renderer, std::make_shared<FrameBuffer>(64, 64), TaskProgress::Ignore);
            }
            catch(const Exception&) {
                refused = true;
            }
            if(!refused)
                reportVerificationFailure(QStringLiteral("the service accepted a color image on a target that holds only the picking buffers"));
        }
        qInfo() << "OFFSCREEN_TEST a pass that does not match the kind of its target is refused";
    }
    else
        qInfo() << "OFFSCREEN_TEST (skipped) the data set has no active scene renderer";
#else
    qInfo() << "OFFSCREEN_TEST (skipped) a mismatch of the target kind aborts this build, which is the point of the check";
#endif

    reportCheckPhase("target kind");

    auto state = std::make_shared<OffscreenCheckState>();
    state->ui = ui;
    state->continuation = std::move(continuation);

    // The scene the earlier checks left behind may hold no particles at all - the parity check imports a file that the
    // LAMMPS importer misdetects - and the sampling has to recolor particles for the rendered image to change. So the
    // check imports a lattice of its own and waits until the file source has evaluated it.
    const QString dataFile = writeLatticeFile(QDir::tempPath() + QStringLiteral("/ovito-qml-offscreen-check.xyz"), 8, 3.6);
    if(dataFile.isEmpty()) {
        state->continuation();
        return;
    }
    if(QmlWorkbenchController* controller = ui->workbenchController())
        controller->importFiles({QUrl::fromLocalFile(dataFile)});
    else {
        reportVerificationFailure(QStringLiteral("the workbench has no controller that could import the data file"));
        state->continuation();
        return;
    }

    pollUntil(ui, 50, 20000,
        [ui]() {
            const FileSource* fileSource = firstFileSource(ui);
            return fileSource && fileSource->numberOfSourceFrames() >= 1 && sceneObjectCount(ui) == 1;
        },
        [state](bool ready) {
            if(!ready) {
                reportVerificationFailure(QStringLiteral("the data file of the offscreen check was not imported"));
                state->continuation();
                return;
            }
            GuiTaskScope taskScope(*state->ui);
            startOffscreenCheckAfterImport(state);
        });
}

static /// Returns the QML item that renders the given viewport, or null if there is none (any more).
QuickViewportItem* itemForViewport(QmlMainWindowUI* ui, Viewport* viewport)
{
    for(QuickViewportItem* item : viewportItems(ui)) {
        if(QuickViewportWindow* viewportWindow = item->viewportWindow())
            if(viewportWindow->viewport() == viewport)
                return item;
    }
    return nullptr;
}

static /// Indicates whether the picking buffer of the given viewport describes its current contents.
bool pickingBufferIsCurrent(QmlMainWindowUI* ui, Viewport* viewport)
{
    QuickViewportItem* item = itemForViewport(ui, viewport);
    QuickViewportWindow* viewportWindow = item ? item->viewportWindow() : nullptr;
    return viewportWindow && viewportWindow->isPickingBufferCurrent();
}

static /// Describes a pick result for the log.
QString describePick(const std::optional<ViewportWindow::PickResult>& result)
{
    if(!result)
        return QStringLiteral("nothing");
    return QStringLiteral("\"%1\" subobject %2")
        .arg(result->sceneNode() ? result->sceneNode()->objectTitle() : QStringLiteral("<unknown>"))
        .arg(result->subobjectId());
}

static /// Indicates whether two pick results name the same object.
bool samePick(const std::optional<ViewportWindow::PickResult>& a, const std::optional<ViewportWindow::PickResult>& b)
{
    if(a.has_value() != b.has_value())
        return false;
    if(!a)
        return true;
    return a->sceneNode() == b->sceneNode() && a->subobjectId() == b->subobjectId();
}

static /// Counts the frames the window renders within a period without asking for any, i.e. it measures whether a scene that
/// nobody touches keeps being redrawn.
void countIdleFrames(QmlMainWindowUI* ui, int durationMs, std::function<void(int)> done)
{
    QQuickWindow* window = ui->view();
    if(!window) {
        done(-1);
        return;
    }
    auto frames = std::make_shared<int>(0);
    QMetaObject::Connection counter = QObject::connect(window, &QQuickWindow::frameSwapped, window,
        [frames]() { (*frames)++; }, Qt::QueuedConnection);
    auto* timer = new QTimer(window);
    timer->setSingleShot(true);
    QObject::connect(timer, &QTimer::timeout, window, [frames, counter, done]() {
        QObject::disconnect(counter);
        done(*frames);
    });
    timer->start(durationMs);
}

/// Verifies the picking pre-warm (see QuickViewportWindow::pickingPrewarmTimeout()).
///
/// The check moves the camera without picking and expects the picking buffer to catch up on its own: that is the whole
/// point of the pre-warm - the first hover after an interaction is answered from the current view, not from the view
/// before it. It also verifies that a viewport which nobody touches stops rendering, because a pre-warm that refreshed
/// the buffer over and over would be a continuous renderer in disguise.
void runPrewarmTest(QmlMainWindowUI* ui, std::function<void()> continuation)
{
    declareCheckPhases({ "buffer caught up", "camera move", "idle frames" });

    GuiTaskScope taskScope(*ui);

    QuickViewportItem* item = firstViewportItem(ui);
    QuickViewportWindow* viewportWindow = item ? item->viewportWindow() : nullptr;
    Viewport* viewport = viewportWindow ? viewportWindow->viewport() : nullptr;
    if(!viewport) {
        reportVerificationFailure(QStringLiteral("the workbench has no viewport to check the picking pre-warm with"));
        continuation();
        return;
    }

    // Wait for the buffer that belongs to the scene as it was imported, i.e. for the camera fit that the import
    // requested. It has to arrive without a pick being made, which is the first half of the pre-warm.
    pollUntil(ui, 10, 20000,
        [ui, viewport]() { return pickingBufferIsCurrent(ui, viewport); },
        [ui, viewport, continuation](bool current) {

        if(!current) {
            reportVerificationFailure(QStringLiteral("the picking buffer did not catch up with the imported scene within 20 s"));
            continuation();
            return;
        }
        qInfo() << "PREWARM_TEST the picking buffer caught up with the imported scene without a pick";
        reportCheckPhase("buffer caught up");

        // The position of the probe: the center of the viewport item, clamped into it (a probe outside the item
        // reports nothing and would look like a picking failure).
        QuickViewportItem* item = itemForViewport(ui, viewport);
        QuickViewportWindow* viewportWindow = item ? item->viewportWindow() : nullptr;
        if(!item || !viewportWindow) {
            reportVerificationFailure(QStringLiteral("the viewport item disappeared while the pre-warm was checked"));
            continuation();
            return;
        }
        const QPoint probePos(int(item->width() / 2), int(item->height() / 2));
        qInfo() << "PREWARM_TEST picking" << probePos << "in an item of" << item->size();
        const std::optional<ViewportWindow::PickResult> beforeMove = viewportWindow->pick(QPointF(probePos));
        qInfo() << "PREWARM_TEST before the camera move, the probe contains" << describePick(beforeMove);

        // Move the camera and ask for a repaint. Nobody picks while the buffer is behind the view.
        viewport->setCameraPosition(viewport->cameraPosition() + Vector3(1.0, 1.0, 0.0));
        viewport->updateViewport();
        qInfo() << "PREWARM_TEST moved the camera and requested a repaint";

        // The buffer has to notice the new view first ...
        pollUntil(ui, 5, 5000,
            [ui, viewport]() { return !pickingBufferIsCurrent(ui, viewport); },
            [ui, viewport, probePos, continuation](bool stale) {

            if(!stale) {
                reportVerificationFailure(QStringLiteral("the picking buffer did not notice the new view of the camera move"));
                continuation();
                return;
            }

            // ... and then become current again on its own, without any pick asking for it.
            pollUntil(ui, 5, 5000,
                [ui, viewport]() { return pickingBufferIsCurrent(ui, viewport); },
                [ui, viewport, probePos, continuation](bool current) {

                if(!current) {
                    reportVerificationFailure(QStringLiteral("the picking buffer did not refresh itself within 5 s of the camera move"));
                    continuation();
                    return;
                }
                qInfo() << "PREWARM_TEST the picking buffer refreshed itself after the camera move, without a pick";

                QuickViewportItem* item = itemForViewport(ui, viewport);
                QuickViewportWindow* viewportWindow = item ? item->viewportWindow() : nullptr;
                if(!viewportWindow) {
                    reportVerificationFailure(QStringLiteral("the viewport item disappeared before the picks of the pre-warm check"));
                    continuation();
                    return;
                }

                // The first pick of the pre-warmed view has to name the object the second one names: if it were
                // answered from a buffer of the previous view, it would point at whatever used to be under the cursor.
                const std::optional<ViewportWindow::PickResult> firstPick = viewportWindow->pick(QPointF(probePos));
                const std::optional<ViewportWindow::PickResult> secondPick = viewportWindow->pick(QPointF(probePos));
                qInfo() << "PREWARM_TEST after the camera move, the probe contains" << describePick(firstPick);
                if(!samePick(firstPick, secondPick))
                    reportVerificationFailure(QStringLiteral("the first pick after the camera move (%1) differs from the second (%2), so it was answered from the previous view")
                        .arg(describePick(firstPick), describePick(secondPick)));
                else
                    qInfo() << "PREWARM_TEST both picks after the camera move agree on" << describePick(firstPick);
                reportCheckPhase("camera move");

                // A view that nobody touches must not keep rendering - the pre-warm refreshes the picking buffer once
                // and stops, it does not turn the viewport into a continuous renderer.
                countIdleFrames(ui, 1000, [continuation](int frames) {
                    // The allowance covers the tail of the passes of the other panes, which can still be in flight when
                    // the buffer of this one has caught up.
                    if(frames > 4)
                        reportVerificationFailure(QStringLiteral("the settled viewport kept rendering: %1 frame(s) within 1 s")
                            .arg(frames));
                    else
                        qInfo() << "PREWARM_TEST the settled viewport rendered" << frames << "frame(s) within 1 s";
                    reportCheckPhase("idle frames");
                    continuation();
                });
            });
        });
    });
}

/// Measures the frame rate the viewports achieve when frames are requested continuously, which is what animation
/// playback does. The requests originate from a queued connection because QQuickWindow::frameSwapped is emitted
/// on the render thread, while requesting frames is a GUI-thread operation.
void measureFrameRate(QmlMainWindowUI* ui, int durationMs, std::function<void()> continuation)
{
    // One phase: the measurement itself, which reports how many frames the viewports rendered in the period.
    declareCheckPhases({ "measurement" });

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
        reportCheckPhase("measurement");
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
    // One phase: every cycle released and rebuilt the graphics resources when the last one is done.
    declareCheckPhases({ "cycles" });

    auto step = std::make_shared<std::function<void(int)>>();
    *step = [ui, continuation, step](int remaining) {
        if(remaining <= 0) {
            reportCheckPhase("cycles");
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

}   // namespace Ovito::Spike
