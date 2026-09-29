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
#include <ovito/gui/base/app/TaskProgressModel.h>
#include <ovito/gui/base/app/GuiSettings.h>
#include <ovito/gui/base/app/IconTheme.h>

#include <ovito/gui/base/mainwin/RecentFilesList.h>
#include <ovito/gui/base/mainwin/PipelineListModel.h>
#include <ovito/gui/base/mainwin/OverlayListModel.h>
#include <ovito/gui/base/mainwin/AvailableModifiersModel.h>
#include <ovito/gui/base/mainwin/AvailableOverlaysModel.h>
#include <ovito/gui/qml/viewport/QuickViewportItem.h>
#include <ovito/gui/qml/viewport/QmlViewportMenu.h>
#include <ovito/gui/qml/viewport/QuickViewportWindow.h>
#include <ovito/core/app/StandaloneApplication.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/core/dataset/io/FileSource.h>
#include <ovito/core/dataset/io/FileSourceImporter.h>
#include <ovito/core/dataset/scene/Pipeline.h>
#include <ovito/core/dataset/animation/AnimationSettings.h>
#include <ovito/core/dataset/scene/Scene.h>
#include <ovito/core/dataset/scene/SelectionSet.h>
#include <ovito/core/dataset/scene/SceneNode.h>
#include <ovito/core/viewport/ViewportConfiguration.h>
#include <ovito/core/viewport/Viewport.h>
#include <ovito/core/viewport/ViewportSettings.h>
#include <ovito/core/rendering/FrameBuffer.h>
#include <ovito/core/rendering/OffscreenRenderTarget.h>
#include <ovito/core/rendering/RenderSettings.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <ovito/core/app/Application.h>
#include <ovito/core/utilities/Exception.h>

#include <QDir>
#include <QFile>
#include <QMouseEvent>
#include <QTextStream>
#include <QDateTime>
#include <QElapsedTimer>
#include <ovito/core/app/undo/UndoStack.h>
#include <ovito/core/app/UserInterface.h>
#include <ovito/gui/base/actions/ActionManager.h>
#include <ovito/gui/base/actions/Command.h>
#include <ovito/gui/base/viewport/ViewportInputManager.h>
#include <ovito/gui/base/viewport/ViewportInputMode.h>
#include <ovito/gui/base/app/GuiTaskScope.h>
#include <ovito/core/utilities/concurrent/TaskProgress.h>
#include <QtQml/qqml.h>
#include <QtQml/qqmlengine.h>
#include <QtQml/qqmlexpression.h>
#include <QtQml/qqmlproperty.h>
#include <QTimer>

#include <functional>
#include <memory>
#include <cstring>
#include <ranges>

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

/// Evaluates an expression in the QML context of the workbench root object, i.e. against the context properties the
/// shell is built on (workbenchController, commandManager, taskProgress, ...). Returns an invalid QVariant and logs the
/// error if the expression cannot be evaluated, so an unbound or misspelled property is visible in the log.
QVariant evaluateInQml(QmlMainWindowUI* ui, const QString& expression)
{
    QQuickView* view = ui->view();
    QQuickItem* rootObject = view ? view->rootObject() : nullptr;
    if(!rootObject) {
        reportVerificationFailure(QStringLiteral("the workbench has no QML scene"));
        return {};
    }
    QQmlExpression evaluator(qmlContext(rootObject), rootObject, expression);
    QVariant result = evaluator.evaluate();
    if(evaluator.hasError()) {
        reportVerificationFailure(QStringLiteral("the QML expression \"%1\" failed: %2").arg(expression, evaluator.error().toString()));
        return {};
    }
    return result;
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

/// Picks at a grid of positions covering the whole viewport item, which answers the question "does picking work in
/// this viewport at all". A fixed probe position cannot answer that: the camera fits a scene to the aspect ratio
/// of the viewport, so the same position can lie outside the scene on a viewport of a different shape even though
/// picking works perfectly - which is how the CI smoke test failed on a viewport that is 307 instead of 380 pixels
/// tall, while the synthetic click at the item centre still selected the lattice.
PickProbe scanPicking(QuickViewportWindow* viewportWindow, const QSizeF& itemSize)
{
    PickProbe probe;
    probe.center = QPoint(int(itemSize.width() / 2), int(itemSize.height() / 2));
    probe.itemSize = itemSize;
    // The grid must be fine enough that it cannot fall between the objects: pick() looks for an object within
    // four device pixels of the probe position, so a spacing of five pixels cannot miss an object that covers
    // more than a point. A coarse grid aliases with the periodicity of a crystal: a 55 pixel grid - the spacing
    // of a fitted 8x8 lattice in a 496 pixel wide viewport - landed exactly in the gaps and hit nothing at all.
    constexpr qreal spacing = 5;
    for(qreal y = spacing / 2; y < itemSize.height(); y += spacing) {
        for(qreal x = spacing / 2; x < itemSize.width(); x += spacing) {
            probe.tested++;
            if(auto result = viewportWindow->pick(QPointF(x, y))) {
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
    // The scan distinguishes "picking does not work here" from "the test position does not contain an object".
    reportPicking(scanPicking(viewportWindow, item->size()), QStringLiteral("viewport scan at failure"));
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
                    return scanPicking(viewportWindow, item->size()).hits > 0;
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

/// Reports where the cameras of the viewports are looking from. The framing of a data set depends on the application
/// of the importer's "zoom to scene extents" request, which is easy to lose when a viewport window does not exist yet
/// at the time the request is made.
void reportCameras(QmlMainWindowUI* ui, const QString& when)
{
    for(QuickViewportItem* item : viewportItems(ui)) {
        QuickViewportWindow* viewportWindow = item->viewportWindow();
        if(!viewportWindow || !viewportWindow->viewport())
            continue;
        const Vector3 cameraPosition = viewportWindow->viewport()->cameraTransformation().translation();
        qInfo() << "CAMERA" << when << "item" << item->size() << "position" << cameraPosition << "distance" << cameraPosition.length();
    }
}

/// A readable snapshot of the pane geometry of the viewport area, used to compare the layout before and after
/// a splitter drag and an undo.
QString layoutSnapshot(QmlViewportLayout* layout, QString* visiblePanes = nullptr)
{
    QStringList parts;
    int visibleCount = 0;
    for(const QVariant& entry : layout->panes()) {
        auto* pane = qobject_cast<QmlViewportPane*>(entry.value<QObject*>());
        if(!pane)
            continue;
        if(pane->isVisible())
            visibleCount++;
        parts << QStringLiteral("%1[%2,%3 %4x%5]%6")
            .arg(pane->viewportIndex()).arg(pane->x()).arg(pane->y()).arg(pane->width()).arg(pane->height())
            .arg(pane->isVisible() ? QString() : QStringLiteral(" hidden"));
    }
    if(visiblePanes)
        *visiblePanes = QString::number(visibleCount);
    return parts.join(QLatin1Char(' '));
}

/// Writes a simple-cubic lattice to an XYZ file and returns the path of the file.
///
/// The comment line must not contain the word "atoms": OVITO's importer autodetection then hands the file to the
/// LAMMPS data importer, which parses an empty scene out of it (see docs/design/UI_TEST_ENV.md).
QString writeLatticeFile(const QString& path, int atomsPerEdge, double latticeConstant)
{
    QFile file(path);
    if(!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        reportVerificationFailure(QStringLiteral("cannot write the data file %1").arg(path));
        return {};
    }
    QTextStream stream(&file);
    stream << atomsPerEdge * atomsPerEdge * atomsPerEdge << "\n";
    stream << "simple cubic lattice, a=" << latticeConstant << "\n";
    for(int i = 0; i < atomsPerEdge; i++) {
        for(int j = 0; j < atomsPerEdge; j++) {
            for(int k = 0; k < atomsPerEdge; k++)
                stream << "Ar " << i * latticeConstant << ' ' << j * latticeConstant << ' ' << k * latticeConstant << "\n";
        }
    }
    return path;
}

/// Writes a VASP POSCAR file with the given number of atoms and returns the path of the file.
///
/// Unlike the XYZ importer, the VASP importer loads the data while the file is being imported
/// (POSCARImporter::setupPipeline() evaluates the pipeline), so importing this file makes the import operation long
/// enough to be cancelled - which is what the cancellation check needs.
QString writePoscarFile(const QString& path, int atomsPerEdge)
{
    QFile file(path);
    if(!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        reportVerificationFailure(QStringLiteral("cannot write the data file %1").arg(path));
        return {};
    }
    const double latticeConstant = 50.0;
    QTextStream stream(&file);
    stream << "simple cubic lattice\n";
    stream << "1.0\n";
    stream << latticeConstant << " 0 0\n0 " << latticeConstant << " 0\n0 0 " << latticeConstant << "\n";
    stream << "Ar\n";
    stream << atomsPerEdge * atomsPerEdge * atomsPerEdge << "\n";
    stream << "Direct\n";
    for(int i = 0; i < atomsPerEdge; i++) {
        for(int j = 0; j < atomsPerEdge; j++) {
            for(int k = 0; k < atomsPerEdge; k++)
                stream << (i + 0.5) / atomsPerEdge << ' ' << (j + 0.5) / atomsPerEdge << ' ' << (k + 0.5) / atomsPerEdge << "\n";
        }
    }
    return path;
}

/// Returns the number of objects in the scene, or -1 if there is no scene.
int sceneObjectCount(QmlMainWindowUI* ui)
{
    const Scene* scene = ui->datasetContainer().activeScene();
    return scene ? static_cast<int>(scene->children().size()) : -1;
}

/// Returns whether any pipeline of the scene reads the given file, i.e. whether the import of that file left a
/// pipeline behind.
bool sceneReadsFile(QmlMainWindowUI* ui, const QString& fileName)
{
    const Scene* scene = ui->datasetContainer().activeScene();
    if(!scene)
        return false;
    for(const SceneNode* node : scene->children()) {
        const Pipeline* pipeline = node->pipeline();
        const FileSource* fileSource = pipeline ? dynamic_object_cast<FileSource>(pipeline->source()) : nullptr;
        if(!fileSource)
            continue;
        for(const QUrl& url : fileSource->sourceUrls()) {
            if(QFileInfo(url.toLocalFile()).fileName() == fileName)
                return true;
        }
    }
    return false;
}

/// Returns the file source of the first pipeline of the scene, or null.
const FileSource* firstFileSource(QmlMainWindowUI* ui)
{
    const Scene* scene = ui->datasetContainer().activeScene();
    if(!scene || scene->children().size() != 1)
        return nullptr;
    const Pipeline* pipeline = scene->children().front()->pipeline();
    return pipeline ? dynamic_object_cast<FileSource>(pipeline->source()) : nullptr;
}

/// Compares the workbench's task progress model with the same data as the QML scene sees it. The status line of the
/// shell is bound to the model, so a model that is not reachable from QML (or a broken binding) has to be caught here
/// rather than by looking at a screenshot. Called while an operation runs and after it has finished.
void reportTaskProgress(QmlMainWindowUI* ui, const QString& when)
{
    TaskProgressModel* model = ui->taskProgressModel();
    if(!model) {
        reportVerificationFailure(QStringLiteral("the workbench has no task progress model"));
        return;
    }

    qInfo() << "TASK_TEST" << when << ":" << model->rowCount() << "task(s), busy =" << model->isBusy()
            << ", displayed" << model->activeText() << model->activeValue() << "of" << model->activeMaximum();

    // The QML side must see the same model. Both values are read from the model object the scene binds to.
    const QVariant qmlBusy = evaluateInQml(ui, QStringLiteral("taskProgress.busy"));
    const QVariant qmlCount = evaluateInQml(ui, QStringLiteral("taskProgress.count"));
    const QVariant qmlText = evaluateInQml(ui, QStringLiteral("taskProgress.text"));
    if(qmlBusy.toBool() != model->isBusy())
        reportVerificationFailure(QStringLiteral("the QML scene sees a busy state of %1 while the task model reports %2")
            .arg(qmlBusy.toBool()).arg(model->isBusy()));
    if(qmlCount.toInt() != model->rowCount())
        reportVerificationFailure(QStringLiteral("the QML scene sees %1 tasks while the task model has %2 rows")
            .arg(qmlCount.toInt()).arg(model->rowCount()));
    if(qmlText.toString() != model->activeText())
        reportVerificationFailure(QStringLiteral("the QML scene displays the task \"%1\" while the model reports \"%2\"")
            .arg(qmlText.toString(), model->activeText()));
}

/// Step 3 of the import check: imports a large file and cancels the operation as soon as it is running, verifying that
/// the shell reports the cancellation and that the data set contains no partially loaded pipeline afterwards.
void verifyCancelledImport(QmlMainWindowUI* ui, std::function<void()> continuation)
{
    QmlWorkbenchController* controller = ui->workbenchController();
    const int objectsBefore = sceneObjectCount(ui);

    const QString poscarFile = QDir::tempPath() + QStringLiteral("/ovito-qml-import-test/large.poscar");
    if(writePoscarFile(poscarFile, 140).isEmpty()) {
        continuation();
        return;
    }

    // The import blocks the main thread while it loads the data, but it keeps processing events, so a timer observes the
    // running operation and requests its cancellation - as soon as the task progress model reports the running import,
    // and at the latest after 200 ms, which the check below relies on: the file has to be large enough that the import
    // is still running then (a larger file is the knob to turn if that ever stops holding).
    //
    // Whether the model reported the import by then is logged rather than asserted: how soon an importer begins to
    // report progress depends on the machine and the file format, which made this check fail on the slower CI runners
    // although the import was cancelled correctly there. The per-task display of the model is verified deterministically
    // by the parity check, which drives the model itself; what this check asserts is what is deterministic here - the
    // canceled import reports the cancellation, leaves no partially loaded pipeline and leaves the model idle.
    constexpr int cancelAfterMs = 200;
    auto observedProgress = std::make_shared<bool>(false);
    QElapsedTimer elapsed;
    elapsed.start();
    auto* timer = new QTimer(ui->view());
    QObject::connect(timer, &QTimer::timeout, controller, [controller, timer, ui, elapsed, observedProgress]() {
        if(TaskProgressModel* model = ui->taskProgressModel(); model && model->isBusy()) {
            if(!*observedProgress)
                reportTaskProgress(ui, QStringLiteral("while the import is running"));
            *observedProgress = true;
        }
        if(controller->cancellable() && (elapsed.elapsed() >= cancelAfterMs || *observedProgress)) {
            timer->stop();
            qInfo() << "IMPORT_TEST requesting the cancellation of the running import";
            controller->cancelCurrentOperation();
        }
    });
    timer->start(10);

    controller->importFiles(QVariantList{ QUrl::fromLocalFile(poscarFile) });
    const qint64 duration = elapsed.elapsed();
    timer->stop();
    timer->deleteLater();

    qInfo() << "IMPORT_TEST canceled import: took" << duration << "ms," << sceneObjectCount(ui) << "object(s) in the scene"
            << "(was" << objectsBefore << "before), status" << controller->statusMessage()
            << ", reported by the task model:" << *observedProgress;
    // The cancelled import must not leave its own pipeline behind. (A ResetScene import deletes the previous objects
    // before it creates the new pipeline, so they are gone by then - the same order of events the classic frontend's
    // import follows; what matters is that no pipeline with a data source that was never filled remains.)
    if(sceneReadsFile(ui, QFileInfo(poscarFile).fileName()))
        reportVerificationFailure(QStringLiteral("the canceled import left a partially loaded pipeline in the scene"));
    if(!controller->statusMessage().startsWith(QStringLiteral("Import cancelled")))
        reportVerificationFailure(QStringLiteral("the canceled import did not report the cancelled state (status: %1)").arg(controller->statusMessage()));

    // The workbench refreshes the model at most every 100 ms, so the row of the canceled import can still be there right
    // after the import has returned. It has to disappear once the operation has wound down - a progress record that
    // stayed behind would keep the status line busy forever.
    pollUntil(ui, 50, 2000, [ui]() {
        TaskProgressModel* model = ui->taskProgressModel();
        return !model || !model->isBusy();
    }, [ui, continuation = std::move(continuation)](bool idle) {
        if(!idle)
            reportVerificationFailure(QStringLiteral("the task progress model still reports the canceled import"));
        reportTaskProgress(ui, QStringLiteral("after the cancelled import"));
        continuation();
    });
}

/// Step 2 of the import check: imports a file of an unsupported format, which must report an error and leave the scene
/// alone, and answers the error dialog the shell shows for it.
///
/// The frontend presents that dialog from the event loop and waits for it, so the run continues only after the dialog
/// has been answered - which is exactly what this verifies.
void verifyUnsupportedFileImport(QmlMainWindowUI* ui, const QString& directory, std::function<void()> continuation)
{
    QmlWorkbenchController* controller = ui->workbenchController();
    const QString unsupportedFile = directory + QStringLiteral("/unsupported.dat");
    {
        QFile file(unsupportedFile);
        if(!file.open(QIODevice::WriteOnly)) {
            reportVerificationFailure(QStringLiteral("cannot write the unsupported data file"));
            continuation();
            return;
        }
        file.write(QByteArray(4096, '\x01'));
    }

    const int objectsBefore = sceneObjectCount(ui);
    controller->importFiles(QVariantList{ QUrl::fromLocalFile(unsupportedFile) });
    qInfo() << "IMPORT_TEST unsupported format:" << sceneObjectCount(ui) << "object(s) in the scene, status"
            << controller->statusMessage();
    if(sceneObjectCount(ui) != objectsBefore)
        reportVerificationFailure(QStringLiteral("the failed import changed the scene"));
    if(controller->statusMessage().isEmpty())
        reportVerificationFailure(QStringLiteral("the failed import did not report anything"));

    pollUntil(ui, 50, 10000, [controller]() { return controller->messageBoxVisible(); }, [ui, controller, continuation](bool appeared) {
        if(appeared) {
            qInfo() << "IMPORT_TEST error dialog:" << controller->messageBoxTitle() << "/"
                    << controller->messageBoxText().split(QLatin1Char('\n')).first();
            controller->answerMessageBox(static_cast<int>(UserInterface::MessageBoxButton::Ok));
        }
        else {
            reportVerificationFailure(QStringLiteral("the failed import did not show an error dialog"));
        }
        verifyCancelledImport(ui, std::move(continuation));
    });
}

/// Probes the "Play Animation" command: the checkable command has to start and stop the playback of the data set, which
/// is what the menu entry and the toolbar button of the classic frontend toggle. The scene must hold an animation with
/// more than one frame; a single-frame scene never starts a playback (SceneAnimationPlayback::startAnimationPlayback).
///
/// This is verified where a trajectory exists - either because the check was given one on the command line (the command
/// check probes it then) or right after the import check has imported one.
void probeAnimationPlaybackCommand(QmlMainWindowUI* ui, Command* playbackCommand)
{
    OVITO_ASSERT(playbackCommand);
    playbackCommand->setChecked(true);
    if(!ui->datasetContainer().isPlaybackActive())
        reportVerificationFailure(QStringLiteral("playback command: checking the command did not start the playback"));
    playbackCommand->setChecked(false);
    if(ui->datasetContainer().isPlaybackActive())
        reportVerificationFailure(QStringLiteral("playback command: unchecking the command did not stop the playback"));
    qInfo() << "COMMAND_TEST the playback command controls the animation playback";
}

/// Verifies the import path of the shell: a multi-frame trajectory becomes one pipeline spanning three animation
/// frames, a file of an unsupported format reports an error without changing the scene, and a canceled import leaves no
/// partially loaded pipeline behind.
/// Reads a property that a QML file declares. QObject::property() does not see those properties, QQmlProperty does.
QVariant qmlProperty(QObject* object, const QString& name)
{
    QQmlProperty property(object, name);
    return property.isValid() ? property.read() : QVariant();
}

/// Verifies the entries of a QML menu against the rule the shell's menus follow: an entry presents a command of the
/// shared command layer, or it is one of the frontend side actions its own check exercises, or it is disabled and names
/// the phase that will deliver it. An entry that none of this applies to would look clickable while doing nothing, so
/// it fails the check.
/// \param allowFrontendActions  True for a menu whose entries call into the frontend instead of triggering a command,
///                              such as the viewport context menu (its actions are driven by verifyContextMenu()).
void verifyMenuEntries(QObject* menu, const QString& what, bool allowFrontendActions)
{
    if(menu == nullptr) {
        reportVerificationFailure(QStringLiteral("the workbench has no %1").arg(what));
        return;
    }

    int entries = 0;
    int commands = 0;
    int frontendActions = 0;
    int pending = 0;
    std::function<void(QObject*)> walk = [&](QObject* parent) {
        for(QObject* child : parent->children()) {
            // The entries are the Qt Quick Controls menu items; a separator carries neither a label nor a state.
            if(child->inherits("QQuickMenuItem") && !child->inherits("QQuickMenuSeparator")) {
                const QString text = child->property("text").toString();
                const bool enabled = child->property("enabled").toBool();
                Command* command = qmlProperty(child, QStringLiteral("command")).value<Command*>();
                const QString ownerPhase = qmlProperty(child, QStringLiteral("ownerPhase")).toString();
                entries++;
                QString wiring;
                if(text.isEmpty()) {
                    reportVerificationFailure(QStringLiteral("the %1 holds an entry without a label").arg(what));
                    wiring = QStringLiteral("<without a label>");
                }
                else if(command != nullptr) {
                    commands++;
                    wiring = command->id();
                }
                else if(enabled && allowFrontendActions) {
                    frontendActions++;
                    wiring = QStringLiteral("<action of the frontend>");
                }
                else if(!enabled && !ownerPhase.isEmpty()) {
                    pending++;
                    wiring = QStringLiteral("<delivered by> ") + ownerPhase;
                }
                else {
                    wiring = QStringLiteral("<nothing>");
                    reportVerificationFailure(QStringLiteral("the entry \"%1\" of the %2 is %3 without belonging to a command of the shared layer%4")
                        .arg(text, what, enabled ? QStringLiteral("enabled") : QStringLiteral("disabled"),
                             ownerPhase.isEmpty() ? QString() : QStringLiteral(" (it names %1)").arg(ownerPhase)));
                }
                qInfo() << "PARITY_TEST  entry" << text << (enabled ? "[enabled]" : "[disabled]") << wiring;
            }
            walk(child);
        }
    };
    walk(menu);

    qInfo() << "PARITY_TEST the" << what << "holds" << entries << "entries:" << commands << "of the shared command layer,"
            << frontendActions << "of the frontend and" << pending << "that name the phase which will deliver them";
    if(entries < 3)
        reportVerificationFailure(QStringLiteral("the %1 lists only %2 entries").arg(what).arg(entries));
}

/// The About command of the shared command layer is presented by whichever frontend runs: the classic one opens a widget
/// dialog, so the shell brings its own dialog, and this verifies that the command reaches it and that it names the
/// application.
void verifyAboutDialog(QmlMainWindowUI* ui, std::function<void()> continuation)
{
    QmlWorkbenchController* controller = ui->workbenchController();
    QQuickItem* rootObject = ui->view() ? ui->view()->rootObject() : nullptr;
    QObject* dialog = rootObject ? rootObject->findChild<QObject*>(QStringLiteral("aboutDialog")) : nullptr;
    if(controller == nullptr || dialog == nullptr) {
        reportVerificationFailure(QStringLiteral("the shell has no About dialog"));
        continuation();
        return;
    }

    controller->showAboutDialog();
    pollUntil(ui, 50, 2000, [dialog]() { return dialog->property("visible").toBool(); }, [dialog, continuation](bool opened) {
        const QString title = dialog->property("title").toString();
        qInfo() << "PARITY_TEST the About dialog:" << title;
        if(!opened)
            reportVerificationFailure(QStringLiteral("the About command did not open the About dialog of the shell"));
        else if(!title.contains(Application::applicationName()))
            reportVerificationFailure(QStringLiteral("the About dialog does not name the application"));
        QMetaObject::invokeMethod(dialog, "close");
        continuation();
    });
}

/// Opens the viewport context menu the way a right-click on the title label of a pane does, and exercises what it
/// offers. Everything it changes is restored afterwards, because the following steps render the same scene.
void verifyContextMenu(QmlMainWindowUI* ui, std::function<void()> continuation)
{
    QmlViewportMenu* menu = ui->viewportMenu();
    QQuickItem* rootObject = ui->view() ? ui->view()->rootObject() : nullptr;
    QObject* menuPopup = rootObject ? rootObject->findChild<QObject*>(QStringLiteral("viewportContextMenu")) : nullptr;
    if(menu == nullptr || menuPopup == nullptr) {
        reportVerificationFailure(QStringLiteral("the viewport context menu is not available"));
        continuation();
        return;
    }

    // The title label of a viewport belongs to the frame graph the viewport rendered last, and the viewport items are
    // rebuilt when the data set or the layout changes, so wait for a viewport that can actually be clicked instead of
    // reading the state a moment too early.
    pollUntil(ui, 100, 5000, [ui]() {
        QuickViewportItem* item = firstViewportItem(ui);
        QuickViewportWindow* viewportWindow = item ? item->viewportWindow() : nullptr;
        return viewportWindow && !viewportWindow->contextMenuArea().isEmpty();
    }, [ui, menu, menuPopup, continuation](bool ready) {
        QuickViewportItem* item = firstViewportItem(ui);
        QuickViewportWindow* viewportWindow = item ? item->viewportWindow() : nullptr;
        Viewport* viewport = viewportWindow ? viewportWindow->viewport() : nullptr;
        ViewportConfiguration* config = ui->datasetContainer().activeViewportConfig();
        if(!ready || viewport == nullptr || config == nullptr) {
            reportVerificationFailure(QStringLiteral("no viewport of the workbench offers a title label to click on"));
            continuation();
            return;
        }

        const QRectF captionArea = viewportWindow->contextMenuArea();
        // The caption area belongs to the rendered frame graph, which is laid out in device pixels, while the viewport item
        // receives its mouse events in device independent coordinates.
        const qreal devicePixelRatio = item->window() ? item->window()->devicePixelRatio() : 1.0;
        const QPointF captionCenter = captionArea.center() / devicePixelRatio;

        Viewport* previousActiveViewport = config->activeViewport();
        const QString previousViewportTitle = viewport->objectTitle();
        const QPointF globalPos = item->mapToGlobal(captionCenter);
        QMouseEvent pressEvent(QEvent::MouseButtonPress, captionCenter, globalPos, Qt::RightButton, Qt::RightButton, Qt::NoModifier);
        QCoreApplication::sendEvent(item, &pressEvent);

        if(!menu->isOpen())
            reportVerificationFailure(QStringLiteral("a right-click on the title label of a viewport did not open its context menu"));
        if(menu->item() != item)
            reportVerificationFailure(QStringLiteral("the context menu belongs to another viewport than the one that was clicked"));
        if(!menuPopup->property("visible").toBool())
            reportVerificationFailure(QStringLiteral("the context menu of the viewport did not appear"));
        if(config->activeViewport() != viewport)
            reportVerificationFailure(QStringLiteral("clicking a viewport did not make it the active one"));

        verifyMenuEntries(menuPopup, QStringLiteral("viewport context menu"), true);

        // Show Grid, the view type and the constraint on the camera rotation are viewport state; the section level setting
        // of the rotation constraint is shared with the classic frontend, so it has to be saved and put back.
        const bool gridVisible = viewport->isGridVisible();
        menu->setGridVisible(!gridVisible);
        if(viewport->isGridVisible() != !gridVisible)
            reportVerificationFailure(QStringLiteral("the context menu did not change the visibility of the construction grid"));

        ViewportSettings& viewportSettings = ViewportSettings::getSettings();
        const bool constrainRotation = viewportSettings.constrainCameraRotation();
        menu->setConstrainRotation(!constrainRotation);
        if(viewportSettings.constrainCameraRotation() != !constrainRotation)
            reportVerificationFailure(QStringLiteral("the context menu did not change the camera rotation constraint"));

        const int defaultMaximizedType = viewportSettings.defaultMaximizedViewportType();
        const Viewport::ViewType viewType = viewport->viewType();
        menu->setViewType(Viewport::VIEW_FRONT);
        if(viewport->viewType() != Viewport::VIEW_FRONT)
            reportVerificationFailure(QStringLiteral("the context menu did not switch the viewport to another view type"));
        menu->setViewType(viewType);

        const bool wasMaximized = config->maximizedViewport() != nullptr;
        menu->toggleMaximize();
        if((config->maximizedViewport() != nullptr) == wasMaximized)
            reportVerificationFailure(QStringLiteral("the context menu did not maximize the viewport"));
        menu->toggleMaximize();
        if((config->maximizedViewport() != nullptr) != wasMaximized)
            reportVerificationFailure(QStringLiteral("the context menu left the viewport maximized"));

        viewportSettings.setConstrainCameraRotation(constrainRotation);
        viewportSettings.setDefaultMaximizedViewportType(static_cast<Viewport::ViewType>(defaultMaximizedType));
        viewportSettings.save();
        QMetaObject::invokeMethod(menuPopup, "close");
        menu->close();
        config->setActiveViewport(previousActiveViewport);

        qInfo() << "PARITY_TEST the viewport context menu of the" << previousViewportTitle
                << "viewport offers the view type, the construction grid, the rotation constraint and maximizing, and all of it was restored";

        continuation();
    });
}

/// The status line lists one row per running task, fed by the shared task progress model. No operation of this prototype
/// runs long enough to be observed together with another one, so two progress records stand in for two concurrent
/// operations here - which is also the case the classic status bar (one aggregate bar) cannot show.
void verifyTaskRows(QmlMainWindowUI* ui, std::function<void()> continuation)
{
    TaskProgressModel* model = ui->taskProgressModel();
    QQuickItem* rootObject = ui->view() ? ui->view()->rootObject() : nullptr;
    QObject* rows = rootObject ? rootObject->findChild<QObject*>(QStringLiteral("taskProgressRows")) : nullptr;
    if(model == nullptr || rows == nullptr) {
        reportVerificationFailure(QStringLiteral("the status line does not present the running tasks"));
        continuation();
        return;
    }

    // Reporting progress touches OVITO objects, which needs the task context of this user interface: a Qt timer
    // callback runs in no task of its own (see open item O8 of the audit).
    GuiTaskScope taskScope(*ui);

    // The records are shared, because they have to outlive this function: the model is read again once the status line
    // has caught up with it.
    auto firstTask = std::make_shared<TaskProgress>(ui);
    auto secondTask = std::make_shared<TaskProgress>(ui);
    firstTask->setText(QStringLiteral("Verifying the status line"));
    firstTask->setMaximum(4);
    firstTask->setValue(1);
    secondTask->setText(QStringLiteral("Verifying the task list"));
    secondTask->setMaximum(2);
    secondTask->setValue(1);

    pollUntil(ui, 100, 3000, [model]() { return model->rowCount() == 2; }, [model, rows, firstTask, secondTask, continuation](bool listed) {
        const int qmlRows = rows->property("count").toInt();
        qInfo() << "PARITY_TEST the status line shows" << qmlRows << "row(s) for" << model->rowCount()
                << "running task(s), busy" << model->isBusy() << "text" << model->activeText();
        if(!listed)
            reportVerificationFailure(QStringLiteral("the task progress model did not list the two running tasks"));
        else if(qmlRows != 2)
            reportVerificationFailure(QStringLiteral("the status line shows %1 row(s) for two running tasks").arg(qmlRows));
        else if(!model->isBusy() || model->activeText().isEmpty())
            reportVerificationFailure(QStringLiteral("the aggregate state of the running tasks is empty"));
        continuation();
    });
}

/// The size and position of the workbench window are remembered in the shared settings store: a resize is written back
/// after a short delay, and the frontend can apply the remembered state again on the next start.
void verifyWindowState(QmlMainWindowUI* ui, std::function<void()> continuation)
{
    QQuickWindow* window = ui->view();
    if(window == nullptr) {
        reportVerificationFailure(QStringLiteral("the shell has no window"));
        continuation();
        return;
    }

    const QRect storedGeometry = GuiSettings::instance().workbenchWindowGeometry();
    const bool storedMaximized = GuiSettings::instance().isWorkbenchWindowMaximized();

    // A window manager can adjust a requested size - the CI runners have small virtual displays and macOS clamped the
    // height of the window that was asked for below - so the check asks for a modest size and then works with what the
    // window actually became: the store is compared against the window, not against the request.
    const QSize requested(900, 600);
    window->resize(requested);
    pollUntil(ui, 100, 3000, [window]() { return GuiSettings::instance().workbenchWindowGeometry().size() == window->size(); },
        [ui, window, requested, storedGeometry, storedMaximized, continuation](bool saved) {
            const QSize actualSize = window->size();
            if(!saved)
                reportVerificationFailure(QStringLiteral("resizing the workbench window was not written to the settings store"));
            else if(actualSize != requested)
                qInfo() << "PARITY_TEST the platform adjusted the requested window size" << requested << "to" << actualSize;

            // The other direction: a remembered geometry is applied by the frontend.
            const QRect remembered(QPoint(60, 40), requested);
            GuiSettings::instance().setWorkbenchWindowGeometry(remembered);
            const bool applied = ui->applyStoredWindowState();
            qInfo() << "PARITY_TEST the shell restored the remembered window state:" << applied << window->size();
            if(!applied)
                reportVerificationFailure(QStringLiteral("the shell did not apply the remembered window state"));
            else if(actualSize == requested && window->size() != remembered.size())
                // Only asserted while the platform is known to honour this size, which the resize above established.
                reportVerificationFailure(QStringLiteral("the remembered window size was not applied (expected %1x%2, got %3x%4)")
                    .arg(remembered.width()).arg(remembered.height()).arg(window->width()).arg(window->height()));
            else if(window->size() != remembered.size())
                qInfo() << "PARITY_TEST the platform adjusted the remembered window size to" << window->size();

            // Leave the settings store as it was found - a developer machine runs the spike against its real settings.
            GuiSettings::instance().setWorkbenchWindowGeometry(storedGeometry);
            GuiSettings::instance().setWorkbenchWindowMaximized(storedMaximized);
            window->resize(1280, 800);
            continuation();
        });
}

/// An import reports which format the file was understood as and how many source frames it holds. The format is known
/// right away, while the number of frames is discovered when the file source is evaluated, so the notice grows into its
/// final form while the pipeline is being processed.
void verifyImportNotice(QmlMainWindowUI* ui, std::function<void()> continuation)
{
    QmlWorkbenchController* controller = ui->workbenchController();
    if(controller == nullptr) {
        reportVerificationFailure(QStringLiteral("the workbench has no shell controller"));
        continuation();
        return;
    }

    const QString directory = QDir::tempPath() + QStringLiteral("/ovito-qml-parity-test");
    QDir().mkpath(directory);

    // The file that OVITO's importer autodetection hands to the LAMMPS Data importer although it is an XYZ file: a
    // comment line mentioning atoms is all the LAMMPS importer needs (defect F6 of the spike report). Such a file
    // imports without an error but leaves an empty scene, so the notice reporting the format it was read as is the only
    // hint the user gets.
    const QString path = directory + QStringLiteral("/misdetected.xyz");
    {
        QFile file(path);
        if(!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            reportVerificationFailure(QStringLiteral("the import check could not write its test file"));
            continuation();
            return;
        }
        QTextStream stream(&file);
        stream << "8\n";
        // The comment line is the trigger: the LAMMPS Data importer accepts any line within the first 20 lines that
        // mentions atoms and starts with a number, and it takes precedence over the XYZ importer.
        stream << "8 atoms\n";
        for(int atom = 0; atom < 8; atom++)
            stream << "Ar 0 0 0\n";
    }

    controller->importFiles({QUrl::fromLocalFile(path)});
    pollUntil(ui, 100, 10000, [controller]() { return controller->notice().contains(QStringLiteral("source frame")); },
        [ui, controller, continuation](bool reported) {
            const QString message = controller->notice();
            const FileSource* fileSource = firstFileSource(ui);
            const QString formatUsed = fileSource && fileSource->importer() ? fileSource->importer()->objectTitle() : QString();
            qInfo() << "PARITY_TEST the import notice reads:" << message
                    << "| the file was read as" << formatUsed << "and left" << sceneObjectCount(ui) << "object(s) in the scene";
            if(!reported)
                reportVerificationFailure(QStringLiteral("the notice did not report the number of source frames"));
            else if(formatUsed.isEmpty() || !message.contains(formatUsed))
                reportVerificationFailure(QStringLiteral("the notice does not name the format the file was imported as (\"%1\")").arg(formatUsed));
            // Informational: the trap of defect F6 is that this file leaves no object behind, which the notice now makes
            // visible instead of the scene simply looking empty.
            else if(sceneObjectCount(ui) == 0)
                qInfo() << "PARITY_TEST the notice is the only visible outcome of that import: the scene holds no object";
            continuation();
        });
}

/******************************************************************************
* Verifies the modifier and viewport layer libraries, whose entries are Commands of the shared command layer.
*
* The entries used to be plain QActions that were registered as such; they are now commands registered with the
* ActionManager, which gives every frontend access to them and keeps the QAction as the view of the widgets frontend.
* This check builds both library models, walks their rows and verifies that each row offers a registered command and
* the QAction that presents it.
******************************************************************************/
/// The models of the pipeline panel and of the modifier and layer libraries.
struct WorkbenchModels {
    PipelineListModel* pipelineList = nullptr;
    AvailableModifiersModel* modifiers = nullptr;
    AvailableOverlaysModel* overlays = nullptr;
};

/// Creates the workbench models once per process and returns them again on later calls.
///
/// Their constructors register commands with fixed ids (one per pipeline item, one per library entry), and the action
/// manager refuses a second command with the same id - so a process may hold only one instance of each model. A
/// frontend is in the same position, which is why the models are created once here and shared by the checks.
WorkbenchModels& workbenchModels(QmlMainWindowUI* ui)
{
    static WorkbenchModels models;
    if(models.pipelineList == nullptr) {
        models.pipelineList = new PipelineListModel(*ui, ui->view());
        models.modifiers = new AvailableModifiersModel(ui->view(), *ui, models.pipelineList);
        models.overlays = new AvailableOverlaysModel(ui->view(), *ui, new OverlayListModel(ui->view(), *ui));
    }
    return models;
}

void runLibraryTest(QmlMainWindowUI* ui, std::function<void()> continuation)
{
    // Creating the models instantiates OVITO objects, which requires a task context when this runs from a Qt callback.
    GuiTaskScope taskScope(*ui);

    auto* modifiersModel = workbenchModels(ui).modifiers;
    auto* overlaysModel = workbenchModels(ui).overlays;

    int entries = 0;
    int categories = 0;
    for(AvailableModifiersModel* model : { modifiersModel }) {
        for(int category = 0; category < model->rowCount(); category++) {
            categories++;
            const QModelIndex categoryIndex = model->index(category, 0);
            for(int row = 0; row < model->rowCount(categoryIndex); row++) {
                const QModelIndex index = model->index(row, 0, categoryIndex);
                entries++;
                Command* command = model->data(index, AvailableModifiersModel::CommandRole).value<Command*>();
                if(command == nullptr) {
                    reportVerificationFailure(QStringLiteral("the modifier library row %1/%2 has no command").arg(category).arg(row));
                    continue;
                }
                if(command->id().isEmpty() || command->text().isEmpty())
                    reportVerificationFailure(QStringLiteral("the modifier library command of row %1/%2 is incomplete").arg(category).arg(row));
                if(ui->actionManager()->findCommand(command->id()) != command)
                    reportVerificationFailure(QStringLiteral("the modifier library command \"%1\" is not registered with the action manager").arg(command->id()));
                QAction* actionView = model->data(index, AvailableModifiersModel::ActionRole).value<QAction*>();
                if(actionView == nullptr || actionView != ui->actionManager()->actionView(command))
                    reportVerificationFailure(QStringLiteral("the modifier library row \"%1\" does not present the QAction view of its command").arg(command->id()));
            }
        }
    }
    qInfo() << "LIBRARY_TEST modifier library:" << categories << "categories," << entries << "commands";

    int overlayEntries = 0;
    int overlayCategories = 0;
    // The viewport layer library enumerates the layer classes of the plugins that are loaded in this run.
    for(int category = 0; category < overlaysModel->rowCount(); category++) {
        overlayCategories++;
        const QModelIndex categoryIndex = overlaysModel->index(category, 0);
        for(int row = 0; row < overlaysModel->rowCount(categoryIndex); row++) {
            const QModelIndex index = overlaysModel->index(row, 0, categoryIndex);
            overlayEntries++;
            Command* command = overlaysModel->data(index, AvailableOverlaysModel::CommandRole).value<Command*>();
            if(command == nullptr || command->id().isEmpty())
                reportVerificationFailure(QStringLiteral("the viewport layer library row %1/%2 has no command").arg(category).arg(row));
            else if(ui->actionManager()->findCommand(command->id()) != command)
                reportVerificationFailure(QStringLiteral("the viewport layer command \"%1\" is not registered with the action manager").arg(command->id()));
        }
    }
    qInfo() << "LIBRARY_TEST viewport layer library:" << overlayCategories << "categories," << overlayEntries << "commands";

    if(entries == 0)
        reportVerificationFailure(QStringLiteral("the modifier library is empty, so nothing was verified"));

    // The flags of a row follow the enabled state of its command, which is what the views of the libraries present.
    if(Command* first = modifiersModel->commandAt(0, 0)) {
        const QModelIndex firstIndex = modifiersModel->index(0, 0, modifiersModel->index(0, 0));
        const bool enabled = modifiersModel->flags(firstIndex).testFlag(Qt::ItemIsEnabled);
        if(enabled != first->isEnabled())
            reportVerificationFailure(QStringLiteral("the row flags of \"%1\" do not follow the state of its command").arg(first->id()));
    }

    // The insert commands of both libraries run inside performTransaction and are reached through Command::triggered, as
    // the classic frontend does when the user picks an entry. That path is not exercised here: inserting a viewport
    // layer switches the viewport into render preview mode, which the Qt Quick viewport does not implement yet (it is
    // an open item of Phase 5), and inserting a modifier needs a selected pipeline in the pipeline list model, which
    // only a selection operation of the frontend establishes.

    // Note: the library models are parented to the window and their commands stay registered with the action manager,
    // exactly as the frontends use them; unregistering them again would mean reimplementing the tool that owns them.
    continuation();
}

/// One icon image the QML scene of the workbench currently displays.
struct IconImage
{
    QUrl source;
    int status = 0;
};

/// QQuickImageBase::Status: the image is loaded and ready to be displayed.
constexpr int ImageStatusReady = 1;

/// Returns the icon images of the shell, i.e. the plain QML Image items whose source is an icon of the given image
/// provider. The source and the load status are read through the meta object of the item, which keeps this independent of
/// Qt Quick's private headers.
static QVector<IconImage> shellIconImages(QmlMainWindowUI* ui, const QString& host)
{
    QVector<IconImage> images;
    QQuickItem* root = ui->view() ? ui->view()->rootObject() : nullptr;
    if(!root)
        return images;

    std::function<void(QQuickItem*)> collect = [&](QQuickItem* parent) {
        for(QQuickItem* child : parent->childItems()) {
            if(child->inherits("QQuickImage")) {
                const QUrl source = child->property("source").toUrl();
                if(source.host() == host)
                    images.push_back({source, child->property("status").toInt()});
            }
            collect(child);
        }
    };
    collect(root);
    return images;
}

/******************************************************************************
* Verifies that the shell shows the icons of the shared icon set.
*
* Both frontends take their icons from the same pair of icon themes of the shared GUI layer (IconTheme), and the Qt Quick
* frontend reaches them through the image provider of QmlIcons. The check verifies both halves: that the shared rule
* resolves the icons the shell and its commands name, in either color scheme, and that the QML side really displays one.
******************************************************************************/
void runIconTest(QmlMainWindowUI* ui, std::function<void()> continuation)
{
    // 1. The shared rule resolves the icons of the icon set itself and those the commands of the shell carry.
    const QString themeName = IconTheme::currentThemeName();
    const QString expectedTheme = IconTheme::themeName(GuiSettings::instance().usingDarkTheme());
    if(themeName != expectedTheme)
        reportVerificationFailure(QStringLiteral("the icon theme of the process is \"%1\" while the color scheme calls for \"%2\"")
            .arg(themeName, expectedTheme));

    QStringList iconPaths = {QStringLiteral("viewport_maximize"), QStringLiteral("viewport_restore")};
    for(const QString& commandId : {QStringLiteral("FileImport"), QStringLiteral("EditDelete"), QStringLiteral("ViewportMaximize")}) {
        if(Command* command = ui->actionManager() ? ui->actionManager()->findCommand(commandId) : nullptr) {
            if(!command->iconPath().isEmpty())
                iconPaths.push_back(command->iconPath());
        }
    }
    for(const QString& iconPath : iconPaths) {
        const QImage image = IconTheme::image(iconPath, QSize(16, 16), 1);
        if(image.isNull() || image.size() != QSize(16, 16))
            reportVerificationFailure(QStringLiteral("the shared icon set does not resolve \"%1\" (%2x%3 px)")
                .arg(iconPath).arg(image.size().width()).arg(image.size().height()));
    }

    // 2. The other theme of the set resolves them as well: both frontends follow the color scheme, so both themes have
    // to hold the icons the shell uses.
    IconTheme::apply(true);
    for(const QString& iconPath : iconPaths) {
        if(IconTheme::image(iconPath, QSize(16, 16), 1).isNull())
            reportVerificationFailure(QStringLiteral("the dark icon theme does not hold \"%1\"").arg(iconPath));
    }
    IconTheme::apply(GuiSettings::instance().usingDarkTheme());

    // 3. The QML side: the singleton reports the theme of the shared layer and builds URLs of its image provider.
    const QVariant reportedTheme = evaluateInQml(ui, QStringLiteral("Icons.themeName"));
    if(reportedTheme.toString() != themeName)
        reportVerificationFailure(QStringLiteral("QML sees the icon theme \"%1\" while the process uses \"%2\"")
            .arg(reportedTheme.toString(), themeName));
    const QUrl restoreUrl = evaluateInQml(ui, QStringLiteral("Icons.url(\"viewport_restore\", Icons.themeName).toString()")).toString();
    const QUrl maximizeUrl = evaluateInQml(ui, QStringLiteral("Icons.url(\"viewport_maximize\", Icons.themeName).toString()")).toString();

    // 4. The icons of the shell are displayed: the maximize button of every pane shows the icon of the state of its pane,
    //    and no icon of the shell failed to load.
    QVector<IconImage> images = shellIconImages(ui, restoreUrl.host());
    int paneButtons = 0;
    for(const IconImage& image : images) {
        if(image.status != ImageStatusReady)
            reportVerificationFailure(QStringLiteral("the shell could not load its icon \"%1\"").arg(image.source.toString()));
        if(image.source == restoreUrl || image.source == maximizeUrl)
            paneButtons++;
    }
    if(images.isEmpty())
        reportVerificationFailure(QStringLiteral("the shell displays no icon of the shared icon set"));
    if(paneButtons == 0)
        reportVerificationFailure(QStringLiteral("the maximize button of no pane shows an icon of the shared icon set"));

    // 5. Maximizing a viewport switches the button of that pane to the restore icon, which is the asset this frontend
    //    needed and the classic icon set did not have; the layout is put back before the check is done.
    if(!evaluateInQml(ui, QStringLiteral("viewportLayout.maximizable")).toBool()) {
        reportVerificationFailure(QStringLiteral("no viewport of the workbench can be maximized"));
        continuation();
        return;
    }
    evaluateInQml(ui, QStringLiteral("viewportLayout.toggleMaximize(viewportLayout.activeViewportIndex)"));

    pollUntil(ui, 50, 5000,
        [ui, restoreUrl]() {
            return std::ranges::any_of(shellIconImages(ui, restoreUrl.host()),
                [&restoreUrl](const IconImage& image) { return image.source == restoreUrl && image.status == ImageStatusReady; });
        },
        [ui, continuation = std::move(continuation), restoreUrl, themeName, iconPaths = iconPaths, paneButtons](bool restored) {

        if(!restored) {
            reportVerificationFailure(QStringLiteral("the maximize button of a maximized pane does not show the restore icon \"%1\"")
                .arg(restoreUrl.toString()));
        }
        evaluateInQml(ui, QStringLiteral("viewportLayout.toggleMaximize(viewportLayout.activeViewportIndex)"));

        // 6. An icon reaches the scene graph as an Image, which caches its pixmap by URL - so the URL has to depend on
        //    the theme, or the shell would keep the icons of the old color scheme when the user switches between light
        //    and dark. (Whether the operating system reports such a switch is out of the hands of the shell; only the
        //    URL contract can be verified here.)
        const QString otherTheme = IconTheme::themeName(!GuiSettings::instance().usingDarkTheme());
        const QUrl otherThemeUrl = evaluateInQml(ui, QStringLiteral("Icons.url(\"viewport_restore\", \"%1\").toString()").arg(otherTheme)).toString();
        if(otherThemeUrl == restoreUrl)
            reportVerificationFailure(QStringLiteral("the URL of an icon does not depend on the theme, so an Image cannot refresh its pixmap when the color scheme changes"));

        qInfo() << "ICON_TEST theme" << themeName << "resolved" << iconPaths.size() << "icons," << "pane buttons"
                << paneButtons << "of which switched to the restore icon:" << restored
                << "| the URL of an icon follows the theme:" << (otherThemeUrl != restoreUrl);
        continuation();
    });
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
        continuation();
    });
}

/******************************************************************************
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

/******************************************************************************
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

/******************************************************************************
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

/// Timeout of the offscreen passes of the check. Rendering an image is quick, but the ambient-occlusion sampling of
/// the pipeline evaluation may need a few seconds before it recolored the particles.
constexpr int offscreenTimeoutMs = 90000;

/// Waits until the picking pass that the resize superseded has been rendered again.
void verifyPickingAfterResize(std::shared_ptr<OffscreenCheckState> state)
{
    GuiTaskScope taskScope(*state->ui);

    const QDateTime started = QDateTime::currentDateTime();
    pollUntil(state->ui, 25, pickTimeoutMs,
        [state]() {
            if(QuickViewportItem* item = firstViewportItem(state->ui)) {
                // The layout has to follow the resize before a picking buffer for the new size can exist.
                if(item->size() == state->viewportItemSizeBeforeResize)
                    return false;
                if(QuickViewportWindow* viewportWindow = item->viewportWindow())
                    return scanPicking(viewportWindow, item->size()).hits > 0;
            }
            return false;
        },
        [state, started](bool satisfied) {
            if(satisfied)
                qInfo() << "OFFSCREEN_TEST picking works again" << started.msecsTo(QDateTime::currentDateTime())
                        << "ms after the resize superseded the picking target";
            else {
                reportVerificationFailure(QStringLiteral("no object was picked after the resize had superseded the picking target"));
                reportPickingState(state->ui, QPoint(8, 8));
            }
            state->continuation();
        });
}

/// Renders the scene again and compares the image with the baseline. The image differs once the ambient-occlusion
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
        verifyPickingAfterResize(state);
    });
}

/// Renders the baseline image, inserts the ambient-occlusion modifier through its shared command and starts the passes
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
        if(QQuickWindow* window = ui->view()) {
            window->resize(window->width() + 40, window->height() + 30);
            qInfo() << "OFFSCREEN_TEST resized the workbench while the sampling, a picking pass and the render output are in flight";
        }

        renderOutputAgain(state);
    });
}

/// Creates the models the check uses, selects the pipeline they operate on and continues with the check once the
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

/// Returns the QML item that renders the given viewport, or null if there is none (any more).
QuickViewportItem* itemForViewport(QmlMainWindowUI* ui, Viewport* viewport)
{
    for(QuickViewportItem* item : viewportItems(ui)) {
        if(QuickViewportWindow* viewportWindow = item->viewportWindow())
            if(viewportWindow->viewport() == viewport)
                return item;
    }
    return nullptr;
}

/// Indicates whether the picking buffer of the given viewport describes its current contents.
bool pickingBufferIsCurrent(QmlMainWindowUI* ui, Viewport* viewport)
{
    QuickViewportItem* item = itemForViewport(ui, viewport);
    QuickViewportWindow* viewportWindow = item ? item->viewportWindow() : nullptr;
    return viewportWindow && viewportWindow->isPickingBufferCurrent();
}

/// Describes a pick result for the log.
QString describePick(const std::optional<ViewportWindow::PickResult>& result)
{
    if(!result)
        return QStringLiteral("nothing");
    return QStringLiteral("\"%1\" subobject %2")
        .arg(result->sceneNode() ? result->sceneNode()->objectTitle() : QStringLiteral("<unknown>"))
        .arg(result->subobjectId());
}

/// Indicates whether two pick results name the same object.
bool samePick(const std::optional<ViewportWindow::PickResult>& a, const std::optional<ViewportWindow::PickResult>& b)
{
    if(a.has_value() != b.has_value())
        return false;
    if(!a)
        return true;
    return a->sceneNode() == b->sceneNode() && a->subobjectId() == b->subobjectId();
}

/// Counts the frames the window renders within a period without asking for any, i.e. it measures whether a scene that
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
                    continuation();
                });
            });
        });
    });
}

/// Verifies the features the shell gained for parity with the classic frontend: the menus of the workbench present the
/// shared commands, the viewport has a context menu, the status line lists the running tasks, the window state is
/// remembered and an import reports what it did with the file.
/// Verifies the keyboard focus chain of the shell: a keyboard user starts at the import control, and the chain continues
/// into the viewports - the pane that takes the focus is the one keyboard events of the viewport input modes arrive at.
/// Verifies the keyboard focus order of the shell: a keyboard user starts at the import control, and the chain continues
/// into the viewports - the pane that takes the focus is the one keyboard events of the viewport input modes arrive at.
///
/// The order itself can only be read from a window that has the keyboard focus: a window manager that never gave it to
/// the window (the headless macOS runner of the CI does not) leaves the tab chain degenerate, which says nothing about
/// the shell. The structural precondition - the import control and the viewports are part of the tab focus chain - is
/// therefore always verified, and the order is verified as well whenever the chain can be walked.
void verifyFocusOrder(QmlMainWindowUI* ui)
{
    QQuickView* view = ui->view();
    QQuickWindow* window = view;
    QQuickItem* rootObject = view ? view->rootObject() : nullptr;
    QQuickItem* importButton = rootObject ? rootObject->findChild<QQuickItem*>(QStringLiteral("importButton")) : nullptr;
    if(!rootObject || !importButton) {
        reportVerificationFailure(QStringLiteral("the shell has no import control to put the keyboard focus on"));
        return;
    }

    QVector<QQuickItem*> tabbable;
    std::function<void(QQuickItem*)> collectTabbable = [&collectTabbable, &tabbable](QQuickItem* item) {
        if(item->activeFocusOnTab())
            tabbable.push_back(item);
        const QList<QQuickItem*> children = item->childItems();
        for(QQuickItem* child : children)
            collectTabbable(child);
    };
    collectTabbable(rootObject);
    const bool importControlIsTabbable = tabbable.contains(importButton);
    const bool viewportIsTabbable = std::ranges::any_of(tabbable, [](QQuickItem* item) {
        return qobject_cast<QuickViewportItem*>(item) != nullptr;
    });

    // nextItemInFocusChain() follows the chain the scene declares, which is the order Tab walks.
    QQuickItem* first = window->contentItem()->nextItemInFocusChain();
    QVector<QQuickItem*> visited;
    for(QQuickItem* item = first; item && visited.size() < 32 && !visited.contains(item); item = item->nextItemInFocusChain())
        visited.push_back(item);
    const bool reachesViewport = std::ranges::any_of(visited, [](QQuickItem* item) {
        for(QQuickItem* parent = item; parent; parent = parent->parentItem())
            if(qobject_cast<QuickViewportItem*>(parent))
                return true;
        return false;
    });
    const bool chainIsWalkable = visited.size() > 1;

    qInfo() << "PARITY_TEST the keyboard focus chain holds" << visited.size() << "stop(s); the first one is the import"
            << "control:" << (first == importButton) << "and it reaches a viewport:" << reachesViewport
            << "| tabbable items of the shell:" << tabbable.size() << "; the import control:" << importControlIsTabbable
            << "and a viewport among them:" << viewportIsTabbable;
    if(!importControlIsTabbable)
        reportVerificationFailure(QStringLiteral("the import control of the shell cannot be reached with the Tab key"));
    if(!viewportIsTabbable)
        reportVerificationFailure(QStringLiteral("no viewport of the shell can be reached with the Tab key"));
    if(chainIsWalkable) {
        if(first != importButton)
            reportVerificationFailure(QStringLiteral("the first keyboard focus stop of the shell is not the import control"));
        if(!reachesViewport)
            reportVerificationFailure(QStringLiteral("the keyboard focus chain of the shell does not reach any viewport"));
    }
    else
        qInfo() << "PARITY_TEST the focus chain of the window cannot be walked in this environment (the window did not get"
                << "the keyboard focus), so the order is verified by the tabbable items above";
}

void runParityTest(QmlMainWindowUI* ui, std::function<void()> continuation)
{
    QQuickItem* rootObject = ui->view() ? ui->view()->rootObject() : nullptr;
    verifyMenuEntries(rootObject ? rootObject->findChild<QObject*>(QStringLiteral("workbenchMenuBar")) : nullptr,
        QStringLiteral("menu bar of the workbench"), false);
    verifyFocusOrder(ui);

    verifyContextMenu(ui, [ui, continuation]() {
        verifyAboutDialog(ui, [ui, continuation]() {
            verifyTaskRows(ui, [ui, continuation]() {
                verifyWindowState(ui, [ui, continuation]() {
                    verifyImportNotice(ui, std::move(continuation));
                });
            });
        });
    });
}

void runImportTest(QmlMainWindowUI* ui, std::function<void()> continuation)
{
    QmlWorkbenchController* controller = ui->workbenchController();
    if(controller == nullptr) {
        reportVerificationFailure(QStringLiteral("the workbench has no shell controller"));
        continuation();
        return;
    }

    const QString directory = QDir::tempPath() + QStringLiteral("/ovito-qml-import-test");
    QDir().mkpath(directory);

    // ---------------------------------------------------------------------------------------------
    // 1. A trajectory of three files becomes a single pipeline spanning three animation frames.
    // ---------------------------------------------------------------------------------------------
    QVariantList trajectoryUrls;
    for(int frame = 0; frame < 3; frame++) {
        const QString path = writeLatticeFile(QStringLiteral("%1/frame_%2.xyz").arg(directory).arg(frame), 4, 3.6 + 0.05 * frame);
        if(path.isEmpty()) {
            continuation();
            return;
        }
        trajectoryUrls.push_back(QUrl::fromLocalFile(path));
    }

    const int objectsBefore = sceneObjectCount(ui);
    const bool hadDataBefore = controller->hasData();
    qInfo() << "IMPORT_TEST before:" << objectsBefore << "object(s) in the scene, hasData" << hadDataBefore;
    controller->importFiles(trajectoryUrls);

    // The importer sets up a file source and returns; the list of frames of a multi-file trajectory is discovered when
    // the pipeline is evaluated for the first time, which the viewports trigger. So the result of the import has to be
    // polled for instead of being read right away.
    pollUntil(ui, 100, 20000, [ui]() {
        const FileSource* fileSource = firstFileSource(ui);
        return fileSource && fileSource->numberOfSourceFrames() == 3;
    }, [ui, controller, directory, continuation](bool ready) {
        const FileSource* fileSource = firstFileSource(ui);
        const int frameCount = fileSource ? fileSource->numberOfSourceFrames() : -1;
        const Scene* scene = ui->datasetContainer().activeScene();
        const AnimationSettings* animation = scene ? scene->animationSettings() : nullptr;
        qInfo() << "IMPORT_TEST trajectory:" << sceneObjectCount(ui) << "object(s) in the scene, first pipeline has"
                << frameCount << "frame(s), animation interval"
                << (animation ? animation->firstFrame() : -1) << ".." << (animation ? animation->lastFrame() : -1);
        if(frameCount != 3)
            reportVerificationFailure(QStringLiteral("importing three files did not produce one pipeline with three animation frames (got %1)").arg(frameCount));
        if(animation && animation->numberOfFrames() != 3)
            reportVerificationFailure(QStringLiteral("the animation interval does not span the three imported frames"));
        if(!controller->hasData())
            reportVerificationFailure(QStringLiteral("the empty state was not left after the import"));
        // The imported trajectory is the natural place to probe the playback command: a static scene cannot play.
        if(Command* playbackCommand = ui->actionManager() ? ui->actionManager()->findCommand(QStringLiteral("AnimationTogglePlayback")) : nullptr)
            probeAnimationPlaybackCommand(ui, playbackCommand);

        verifyUnsupportedFileImport(ui, directory, std::move(continuation));
    });
    return;
}

/// Verifies the viewport area of the workbench shell: the panes come from the layout tree of the dataset, dragging a
/// handle resizes them through the undo system, and maximizing a viewport keeps exactly one pane visible.
void runLayoutTest(QmlMainWindowUI* ui, std::function<void()> continuation)
{
    QmlViewportLayout* layout = ui->viewportLayout();
    QQuickView* view = ui->view();
    QQuickWindow* window = view;
    // The viewport area is the item the layout model places the panes in; its coordinates are the ones the handles
    // report, which is why the mouse events are translated into window coordinates through it.
    QQuickItem* hostItem = view && view->rootObject() ?
        view->rootObject()->findChild<QQuickItem*>(QStringLiteral("viewportHost")) : nullptr;
    if(!layout || !window || !hostItem) {
        reportVerificationFailure(QStringLiteral("layout check: the workbench exposes no viewport area"));
        continuation();
        return;
    }

    reportCameras(ui, QStringLiteral("in the layout check"));

    const QVariantList panes = layout->panes();
    const QVariantList splitters = layout->splitters();
    qInfo() << "LAYOUT_TEST the viewport area holds" << panes.size() << "panes and" << splitters.size() << "handles"
            << "in an area of" << hostItem->size();
    for(QuickViewportItem* item : viewportItems(ui)) {
        qInfo() << "LAYOUT_ITEM" << item->size() << item->mapToScene(QPointF(0, 0)) << "visible=" << item->isVisible()
                << "viewportWindow=" << (item->viewportWindow() != nullptr);
    }
    for(int index = 0; index < panes.size(); index++) {
        if(auto* pane = qobject_cast<QmlViewportPane*>(panes[index].value<QObject*>())) {
            qInfo() << "LAYOUT_PANE" << index << QStringLiteral("rect=(%1,%2 %3x%4)").arg(pane->x()).arg(pane->y()).arg(pane->width()).arg(pane->height())
                    << "active=" << pane->isActive() << "maximized=" << pane->isMaximized() << "visible=" << pane->isVisible();
        }
    }
    for(int index = 0; index < splitters.size(); index++) {
        if(auto* splitter = qobject_cast<QmlViewportSplitter*>(splitters[index].value<QObject*>())) {
            qInfo() << "LAYOUT_SPLITTER" << index << (splitter->isHorizontal() ? "horizontal" : "vertical")
                    << QStringLiteral("rect=(%1,%2 %3x%4)").arg(splitter->x()).arg(splitter->y()).arg(splitter->width()).arg(splitter->height());
        }
    }

    // The panes must add up to the layout tree of the dataset and must not overlap the handles.
    if(panes.size() < 2 || splitters.isEmpty()) {
        reportVerificationFailure(QStringLiteral("layout check: the default viewport layout should have several panes and handles"));
        continuation();
        return;
    }

    const QString before = layoutSnapshot(layout);

    // Drag the first handle with synthetic mouse events, i.e. through the QML mouse area a user would grab - not by
    // calling the layout model directly, so that the QML wiring is part of what is verified.
    auto* handle = qobject_cast<QmlViewportSplitter*>(splitters.front().value<QObject*>());
    const QPointF handleCenter(handle->x() + 0.5 * handle->width(), handle->y() + 0.5 * handle->height());
    const QPointF dragOffset = handle->isHorizontal() ? QPointF(100, 0) : QPointF(0, 100);
    const auto sendMouseEvent = [ui, window, hostItem](QEvent::Type type, Qt::MouseButton button, Qt::MouseButtons buttons, const QPointF& panelPosition) {
        const QPointF windowPosition = hostItem->mapToScene(panelPosition);
        QMouseEvent event(type, windowPosition, window->mapToGlobal(windowPosition), button, buttons, Qt::NoModifier);
        QCoreApplication::sendEvent(window, &event);
    };
    sendMouseEvent(QEvent::MouseButtonPress, Qt::LeftButton, Qt::LeftButton, handleCenter);
    sendMouseEvent(QEvent::MouseMove, Qt::NoButton, Qt::LeftButton, handleCenter + 0.5 * dragOffset);
    sendMouseEvent(QEvent::MouseMove, Qt::NoButton, Qt::LeftButton, handleCenter + dragOffset);
    sendMouseEvent(QEvent::MouseButtonRelease, Qt::LeftButton, Qt::NoButton, handleCenter + dragOffset);

    const QString afterDrag = layoutSnapshot(layout);
    if(afterDrag == before)
        reportVerificationFailure(QStringLiteral("layout check: dragging a handle did not change the pane sizes"));

    // One drag is one undoable operation, and undoing it has to restore the exact pane sizes.
    UndoStack* undoStack = ui->undoStack();
    if(!undoStack || !undoStack->canUndo())
        reportVerificationFailure(QStringLiteral("layout check: the drag was not recorded as an undoable operation"));
    else
        qInfo() << "LAYOUT_UNDO the drag produced the undo step" << undoStack->undoText();

    ui->actionManager()->triggerCommand(ACTION_EDIT_UNDO);
    if(layoutSnapshot(layout) != before)
        reportVerificationFailure(QStringLiteral("layout check: undo did not restore the pane sizes of the drag"));
    ui->actionManager()->triggerCommand(ACTION_EDIT_REDO);
    if(layoutSnapshot(layout) != afterDrag)
        reportVerificationFailure(QStringLiteral("layout check: redo did not restore the dragged pane sizes"));
    ui->actionManager()->triggerCommand(ACTION_EDIT_UNDO);
    if(layoutSnapshot(layout) != before)
        reportVerificationFailure(QStringLiteral("layout check: a second undo did not return to the layout from before the drag"));
    qInfo() << "LAYOUT_UNDO the drag was undone and redone with the pane sizes restored";

    // Maximizing keeps exactly one pane visible and restores the layout afterwards.
    const int activeIndex = layout->activeViewportIndex();
    QString visiblePanes;
    layout->toggleMaximize(activeIndex);
    layoutSnapshot(layout, &visiblePanes);
    if(visiblePanes != QStringLiteral("1"))
        reportVerificationFailure(QStringLiteral("layout check: maximizing left %1 panes visible").arg(visiblePanes));
    else
        qInfo() << "LAYOUT_MAXIMIZE viewport" << activeIndex << "fills the viewport area while the other panes are hidden";
    layout->toggleMaximize(activeIndex);
    if(layoutSnapshot(layout) != before)
        reportVerificationFailure(QStringLiteral("layout check: restoring the layout did not bring the pane sizes back"));

    continuation();
}


/******************************************************************************
* Verifies the shared command layer of the two frontends.

*
* The commands are the frontend-neutral description of everything the user can invoke, and the classic frontend
* presents them as QActions. This check verifies that the QML workbench sees the same commands with the same state,
* that the state rules of the frontend (the undo stack, the animation playback, the viewport layout) drive the
* commands, and that a command can be invoked through the QML engine.
******************************************************************************/
void runCommandTest(QmlMainWindowUI* ui, std::function<void()> continuation)
{
    ActionManager* actionManager = ui->actionManager();
    QQuickView* view = ui->view();
    if(!actionManager || !view || !view->rootObject()) {
        reportVerificationFailure(QStringLiteral("command check: the workbench exposes no command layer"));
        continuation();
        return;
    }

    // 1. Every command the classic frontend knows about is available here, and its QAction view carries the state of
    //    the command, which is what the menus and toolbars of the classic frontend display.
    const QStringList ids = {
        QStringLiteral("EditUndo"), QStringLiteral("EditRedo"), QStringLiteral("EditDelete"),
        QStringLiteral("ViewportMaximize"), QStringLiteral("ViewportZoomSceneExtents"),
        QStringLiteral("ViewportPan"), QStringLiteral("SelectionMode"), QStringLiteral("AnimationTogglePlayback")
    };
    for(const QString& id : ids) {
        Command* command = actionManager->findCommand(id);
        if(!command) {
            reportVerificationFailure(QStringLiteral("command check: the command %1 does not exist").arg(id));
            continue;
        }
        QAction* actionView = actionManager->actionView(command);
        if(!actionView) {
            reportVerificationFailure(QStringLiteral("command check: the command %1 has no QAction view").arg(id));
            continue;
        }
        if(actionView->text() != command->text() || actionView->isEnabled() != command->isEnabled()
           || actionView->isCheckable() != command->isCheckable() || actionView->isChecked() != command->isChecked()) {
            reportVerificationFailure(QStringLiteral("command check: the QAction view of %1 does not mirror the command").arg(id));
        }
    }
    qInfo() << "COMMAND_TEST the command layer provides" << actionManager->commands().size() << "commands,"
            << "including all" << ids.size() << "commands of this check";

    // 2. The QML side of the workbench sees the same objects, and can read the state of a command. The expressions are
    //    evaluated by the QML engine, so the context property, the registered type and the properties are all covered.
    Command* undoCommand = actionManager->findCommand(QStringLiteral("EditUndo"));
    const QVariant qmlUndoText = evaluateInQml(ui, QStringLiteral("workbench.undoCommand.text"));
    // The title of the Undo command carries the name of the operation it would revert, so only compare the prefix.
    if(!qmlUndoText.toString().startsWith(QStringLiteral("Undo")))
        reportVerificationFailure(QStringLiteral("command check: QML reads \"%1\" as the title of the Undo command").arg(qmlUndoText.toString()));
    const QVariant qmlCommandCount = evaluateInQml(ui, QStringLiteral("commandManager.commandList.length"));
    if(qmlCommandCount.toInt() != actionManager->commands().size())
        reportVerificationFailure(QStringLiteral("command check: QML sees %1 of %2 commands").arg(qmlCommandCount.toInt()).arg(actionManager->commands().size()));
    qInfo() << "COMMAND_TEST QML reads the Undo command as" << qmlUndoText.toString() << "and sees" << qmlCommandCount.toInt() << "commands";

    // 3. Invoking a command through the QML engine has to run the handler the two frontends share.
    ViewportConfiguration* viewportConfig = ui->datasetContainer().activeViewportConfig();
    if(viewportConfig) {
        evaluateInQml(ui, QStringLiteral("commandManager.triggerCommand('ViewportMaximize')"));
        if(viewportConfig->maximizedViewport() != viewportConfig->activeViewport())
            reportVerificationFailure(QStringLiteral("command check: invoking a command through QML did not run its handler"));
        evaluateInQml(ui, QStringLiteral("commandManager.triggerCommand('ViewportMaximize')"));
        if(viewportConfig->maximizedViewport())
            reportVerificationFailure(QStringLiteral("command check: invoking a command through QML did not restore the layout"));
        qInfo() << "COMMAND_TEST QML can invoke a command and run its handler";
    }

    // 4. The state rules of the frontend drive the commands. The undo stack is the most important one: its state has to
    //    reach the Undo/Redo commands (and their QAction views) without the frontend wiring them up again.
    Scene* scene = ui->datasetContainer().activeScene();
    SceneNode* node = scene && !scene->children().empty() ? scene->children().front() : nullptr;
    if(!node) {
        reportVerificationFailure(QStringLiteral("command check: the scene has no object to rename"));
    }
    else {
        const QString originalTitle = node->objectTitle();
        GuiTaskScope taskScope(*ui);
        ui->performTransaction(QStringLiteral("Rename pipeline"), [&]() {
            node->setSceneNodeName(QStringLiteral("renamed by the command check"));
        });
        if(node->objectTitle() != QStringLiteral("renamed by the command check"))
            reportVerificationFailure(QStringLiteral("command check: the test transaction did not rename the object"));
        Command* redoCommand = actionManager->findCommand(QStringLiteral("EditRedo"));
        if(!undoCommand || !undoCommand->isEnabled() || !actionManager->actionView(undoCommand)->isEnabled())
            reportVerificationFailure(QStringLiteral("command check: the Undo command did not become enabled after an operation"));
        else
            qInfo() << "COMMAND_TEST the Undo command is" << undoCommand->text();
        actionManager->triggerCommand(QStringLiteral("EditUndo"));
        if(node->objectTitle() != originalTitle)
            reportVerificationFailure(QStringLiteral("command check: the Undo command did not revert the operation"));
        if(!redoCommand || !redoCommand->isEnabled())
            reportVerificationFailure(QStringLiteral("command check: the Redo command did not become enabled after an undo"));
        actionManager->triggerCommand(QStringLiteral("EditRedo"));
        if(node->objectTitle() != QStringLiteral("renamed by the command check"))
            reportVerificationFailure(QStringLiteral("command check: the Redo command did not reapply the operation"));
        actionManager->triggerCommand(QStringLiteral("EditUndo"));
        qInfo() << "COMMAND_TEST the shared undo stack drives the Undo/Redo commands and their QAction views";
    }

    // 5. A checkable command that mirrors program state: starting and stopping the animation playback goes through the
    //    command, which is what the menu entry of the classic frontend toggles. A playback needs an animation with more
    //    than one frame, so the probe is skipped for a static scene instead of importing data here: this step must leave
    //    the scene as it found it, and the playback is probed where a trajectory is imported anyway (runImportTest()).
    {
        const AnimationSettings* animSettings = ui->datasetContainer().activeAnimationSettings();
        Command* playbackCommand = actionManager->findCommand(QStringLiteral("AnimationTogglePlayback"));
        if(!animSettings || animSettings->isSingleFrame()) {
            qInfo() << "COMMAND_TEST (skipped) the scene holds no animation, so the playback command cannot be probed";
        }
        else if(playbackCommand) {
            probeAnimationPlaybackCommand(ui, playbackCommand);
        }
    }

    // 6. Viewport input modes are commands as well, including the rule that an exclusive mode stays active.
    ViewportInputManager* inputManager = ui->viewportInputManager();
    Command* panCommand = actionManager->findCommand(QStringLiteral("ViewportPan"));
    Command* selectionCommand = actionManager->findCommand(QStringLiteral("SelectionMode"));
    if(!inputManager || !panCommand || !selectionCommand) {
        reportVerificationFailure(QStringLiteral("command check: the viewport input modes are not available as commands"));
    }
    else {
        panCommand->trigger();
        if(inputManager->activeMode() != inputManager->panMode() || !panCommand->isChecked())
            reportVerificationFailure(QStringLiteral("command check: invoking the pan command did not activate the pan mode"));
        panCommand->trigger();
        if(inputManager->activeMode() == inputManager->panMode() || panCommand->isChecked())
            reportVerificationFailure(QStringLiteral("command check: invoking the pan command again did not deactivate the mode"));
        selectionCommand->trigger();
        if(inputManager->activeMode() != inputManager->selectionMode() || !selectionCommand->isChecked())
            reportVerificationFailure(QStringLiteral("command check: invoking the selection command did not activate the selection mode"));
        // The selection mode is exclusive: the user cannot turn it off, so it stays active and stays checked.
        selectionCommand->trigger();
        if(inputManager->activeMode() != inputManager->selectionMode() || !selectionCommand->isChecked())
            reportVerificationFailure(QStringLiteral("command check: the exclusive selection mode was deactivated"));
        qInfo() << "COMMAND_TEST the viewport input modes are commands, including the exclusive selection mode";
    }

    // 7. Maximizing the active viewport is the same command the classic frontend has in its toolbar.
    if(Command* maximizeCommand = actionManager->findCommand(QStringLiteral("ViewportMaximize"))) {
        ViewportConfiguration* config = ui->datasetContainer().activeViewportConfig();
        if(config) {
            maximizeCommand->trigger();
            if(config->maximizedViewport() != config->activeViewport() || !maximizeCommand->isChecked())
                reportVerificationFailure(QStringLiteral("command check: the maximize command did not maximize the active viewport"));
            maximizeCommand->trigger();
            if(config->maximizedViewport() || maximizeCommand->isChecked())
                reportVerificationFailure(QStringLiteral("command check: the maximize command did not restore the layout"));
            qInfo() << "COMMAND_TEST the maximize command toggles the maximized viewport";
        }
    }

    continuation();
}

/******************************************************************************
* Verifies the settings facade the two workbenches share.
*
* Both frontends persist the same things - the color scheme, the window state, how the file dialogs behave - and have to
* agree on the key, the default and the meaning of each value, or the classic settings dialog and the Qt Quick shell drift
* apart. This check covers that the QML scene really reaches the facade, that the shell's theme takes its palette from it,
* and that every accessor round-trips through the settings store.
*
* Every value this check writes is written back before it returns, so a verification run does not change how the
* developer's frontends behave; only the history of a file dialog class that exists solely for this check is left behind.
* Run the spike with an isolated XDG_CONFIG_HOME (docs/design/UI_TEST_ENV.md) for a fully clean run.
******************************************************************************/
void runSettingsTest(QmlMainWindowUI* ui, std::function<void()> continuation)
{
    GuiSettings& settings = GuiSettings::instance();

    // 1. The QML scene reaches the same facade object, and the theme of the shell resolves to it. Both values are read
    //    through the QML engine, which covers the context property, the property bindings and the palette binding.
    const bool cppDarkTheme = settings.usingDarkTheme();
    const QVariant qmlDarkTheme = evaluateInQml(ui, QStringLiteral("guiSettings.usingDarkTheme"));
    const QVariant qmlThemeDark = evaluateInQml(ui, QStringLiteral("workbench.darkTheme"));
    if(qmlDarkTheme.toBool() != cppDarkTheme)
        reportVerificationFailure(QStringLiteral("settings check: QML reads %1 as the color scheme of the shared settings while C++ reads %2")
            .arg(qmlDarkTheme.toBool()).arg(cppDarkTheme));
    if(qmlThemeDark.toBool() != cppDarkTheme)
        reportVerificationFailure(QStringLiteral("settings check: the shell's theme resolved to %1 while the shared settings report %2 - the theme does not follow the shared policy")
            .arg(qmlThemeDark.toBool()).arg(cppDarkTheme));

    // 2. The window placement of a frontend without widgets. The Qt Quick shell restores its window from these, which the
    //    classic frontend does not use (it stores QWidget geometry blobs instead).
    auto expect = [](const QString& what, const QVariant& actual, const QVariant& expected) {
        if(actual != expected)
            reportVerificationFailure(QStringLiteral("settings check: %1 is '%2' after writing '%3'")
                .arg(what, actual.toString(), expected.toString()));
    };

    const QRect originalWindowGeometry = settings.workbenchWindowGeometry();
    settings.setWorkbenchWindowGeometry(QRect(120, 80, 1024, 768));
    expect(QStringLiteral("the remembered window rectangle"), settings.workbenchWindowGeometry(), QRect(120, 80, 1024, 768));
    settings.setWorkbenchWindowGeometry(originalWindowGeometry);
    expect(QStringLiteral("the window rectangle after restoring the original"), settings.workbenchWindowGeometry(), originalWindowGeometry);

    const bool originalMaximized = settings.isWorkbenchWindowMaximized();
    settings.setWorkbenchWindowMaximized(!originalMaximized);
    expect(QStringLiteral("the maximized flag"), settings.isWorkbenchWindowMaximized(), !originalMaximized);
    settings.setWorkbenchWindowMaximized(originalMaximized);
    expect(QStringLiteral("the maximized flag after restoring the original"), settings.isWorkbenchWindowMaximized(), originalMaximized);

    // 3. The opaque blobs of the classic main window. They are not converted into rectangles on purpose: a QWidget
    //    geometry blob also carries the screen and the maximized state of the window.
    const QByteArray originalGeometry = settings.mainWindowGeometry();
    const QByteArray originalState = settings.mainWindowState();
    settings.setMainWindowGeometry(QByteArrayLiteral("spike-geometry-blob"));
    settings.setMainWindowState(QByteArrayLiteral("spike-state-blob"));
    expect(QStringLiteral("the main window geometry blob"), settings.mainWindowGeometry(), QByteArrayLiteral("spike-geometry-blob"));
    expect(QStringLiteral("the main window layout blob"), settings.mainWindowState(), QByteArrayLiteral("spike-state-blob"));
    settings.setMainWindowGeometry(originalGeometry);
    settings.setMainWindowState(originalState);

    // 4. The behavior of the file dialogs, which both frontends follow.
    const bool originalKeepHistory = settings.keepDirectoryHistory();
    const bool originalPreferQt = settings.preferQtFileDialog();
    const QString originalSessionDirectory = settings.sessionFileDirectory();
    settings.setKeepDirectoryHistory(!originalKeepHistory);
    settings.setPreferQtFileDialog(!originalPreferQt);
    settings.setSessionFileDirectory(QStringLiteral("/tmp/ovito-settings-check"));
    expect(QStringLiteral("the directory-history flag"), settings.keepDirectoryHistory(), !originalKeepHistory);
    expect(QStringLiteral("the preferred file dialog"), settings.preferQtFileDialog(), !originalPreferQt);
    expect(QStringLiteral("the session file directory"), settings.sessionFileDirectory(), QStringLiteral("/tmp/ovito-settings-check"));
    settings.setKeepDirectoryHistory(originalKeepHistory);
    settings.setPreferQtFileDialog(originalPreferQt);
    settings.setSessionFileDirectory(originalSessionDirectory);
    expect(QStringLiteral("the directory-history flag after restoring the original"), settings.keepDirectoryHistory(), originalKeepHistory);
    expect(QStringLiteral("the preferred file dialog after restoring the original"), settings.preferQtFileDialog(), originalPreferQt);
    expect(QStringLiteral("the session file directory after restoring the original"), settings.sessionFileDirectory(), originalSessionDirectory);

    // The directory history of one kind of file dialog: the most recent directory moves to the front, and an empty
    // directory name is ignored. This uses a dialog class of its own, so no real history is touched.
    const QString dialogClass = QStringLiteral("spike-settings-check");
    settings.rememberDirectory(dialogClass, QStringLiteral("/tmp/ovito-check-a"));
    settings.rememberDirectory(dialogClass, QStringLiteral("/tmp/ovito-check-b"));
    expect(QStringLiteral("the directory history"), settings.recentDirectories(dialogClass), QStringList{QStringLiteral("/tmp/ovito-check-b")});
    settings.rememberDirectory(dialogClass, QStringLiteral("/tmp/ovito-check-a"));
    expect(QStringLiteral("the directory history after returning to the first directory"), settings.recentDirectories(dialogClass), QStringList{QStringLiteral("/tmp/ovito-check-a")});

    // 5. The flags the first start sets: the classic frontend shows its GPU adapter dialog while this one is not set.
    const bool originalSetupDone = settings.graphicsAdapterSetupDone();
    settings.setGraphicsAdapterSetupDone(true);
    expect(QStringLiteral("the graphics-adapter setup flag"), settings.graphicsAdapterSetupDone(), true);
    settings.setGraphicsAdapterSetupDone(false);
    expect(QStringLiteral("the graphics-adapter setup flag after clearing it"), settings.graphicsAdapterSetupDone(), false);
    settings.setGraphicsAdapterSetupDone(originalSetupDone);

#ifdef OVITO_BUILD_PROFESSIONAL
    // The multi-file import mode is a setting of the professional edition only; elsewhere it is fixed.
    const FileImporter::MultiFileImportMode originalImportMode = settings.multiFileImportMode();
    settings.setMultiFileImportMode(FileImporter::ImportAsSeparateObjects);
    expect(QStringLiteral("the multi-file import mode"), static_cast<int>(settings.multiFileImportMode()), static_cast<int>(FileImporter::ImportAsSeparateObjects));
    settings.setMultiFileImportMode(originalImportMode);
#endif

    // The color-scheme policy itself: following the system is compulsory on Linux and macOS and a user decision
    // elsewhere, which is where the check can exercise the stored flag.
#if defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
    if(!settings.followsSystemColorScheme())
        reportVerificationFailure(QStringLiteral("settings check: following the system color scheme is not compulsory on this platform"));
    qInfo() << "SETTINGS_TEST the color scheme always follows the system on this platform, dark theme is" << cppDarkTheme;
#else
    const bool originalFollowSystem = settings.followsSystemColorScheme();
    settings.setFollowsSystemColorScheme(!originalFollowSystem);
    expect(QStringLiteral("the automatic color-scheme flag"), settings.followsSystemColorScheme(), !originalFollowSystem);
    settings.setFollowsSystemColorScheme(originalFollowSystem);
    qInfo() << "SETTINGS_TEST the automatic color-scheme flag round-trips, dark theme is" << cppDarkTheme;
#endif

    qInfo() << "SETTINGS_TEST the shared settings facade round-trips the window state, the file dialog behaviour and"
            << "the first-start flags; the shell's theme follows it";
    continuation();
}

/// Verifies the session workflow of the workbench, which both frontends share (WorkbenchUI::saveSessionFile(),
/// saveSession(), loadSessionFile() and isSessionModified()): saving a session writes a file and clears the modified
/// state, a change to the scene marks the session as modified, and loading the session brings the saved content back.
///
/// The file dialog in front of the workflow is frontend specific (the classic main window shows a QFileDialog, the Qt
/// Quick shell has no session commands yet), so this check drives the operations that do not need a file name.
void runSessionTest(QmlMainWindowUI* ui, std::function<void()> continuation)
{
    const QString sessionFile = QDir::tempPath() + QStringLiteral("/ovito-qml-session-test/session.ovito");
    QDir().mkpath(QFileInfo(sessionFile).absolutePath());
    QFile::remove(sessionFile);

    // The check saves the scene of the workbench, so there has to be one: an earlier step may have canceled an import
    // and left the scene empty.
    {
        GuiTaskScope taskScope(*ui);
        if(Scene* scene = ui->datasetContainer().activeScene(); !scene || scene->children().empty()) {
            const QString dataFile = QDir::tempPath() + QStringLiteral("/ovito-qml-session-test/lattice.xyz");
            if(!writeLatticeFile(dataFile, 8, 3.6).isEmpty())
                ui->workbenchController()->importFiles(QVariantList{ QUrl::fromLocalFile(dataFile) });
        }
    }

    Scene* scene = ui->datasetContainer().activeScene();
    SceneNode* node = scene && !scene->children().empty() ? scene->children().front() : nullptr;
    if(!node) {
        reportVerificationFailure(QStringLiteral("session check: the scene holds no object to save"));
        continuation();
        return;
    }

    GuiTaskScope taskScope(*ui);
    ui->handleExceptions([&]() {
        // 1. Saving a session writes the file, remembers it and clears the modified state.
        ui->saveSessionFile(sessionFile);
        if(!QFileInfo::exists(sessionFile))
            reportVerificationFailure(QStringLiteral("session check: saving wrote no file"));
        if(ui->sessionFilePath() != QFileInfo(sessionFile).absoluteFilePath())
            reportVerificationFailure(QStringLiteral("session check: the workbench does not remember the file it saved (%1)").arg(ui->sessionFilePath()));
        if(ui->isSessionModified())
            reportVerificationFailure(QStringLiteral("session check: a freshly saved session is reported as modified"));
        qInfo() << "SESSION_TEST saved the scene to" << QFileInfo(sessionFile).fileName()
                << QStringLiteral("(%1 bytes)").arg(QFileInfo(sessionFile).size());

        // 2. A change marks the session as modified, and saving it again (into the remembered file) clears that.
        ui->performTransaction(QStringLiteral("Rename pipeline"), [&]() { node->setSceneNodeName(QStringLiteral("renamed after saving")); });
        if(!ui->isSessionModified())
            reportVerificationFailure(QStringLiteral("session check: a change to the scene did not mark the session as modified"));
        ui->saveSession();
        if(ui->isSessionModified())
            reportVerificationFailure(QStringLiteral("session check: the modified session is still reported as modified after saving it again"));

        // 3. Loading the session file has to replace the current scene by the saved one. Change the scene again without
        //    saving it first, so that the reload has something to undo.
        Scene* currentScene = ui->datasetContainer().activeScene();
        SceneNode* currentNode = currentScene && !currentScene->children().empty() ? currentScene->children().front() : nullptr;
        if(currentNode) {
            ui->performTransaction(QStringLiteral("Rename pipeline"), [&]() { currentNode->setSceneNodeName(QStringLiteral("renamed without saving")); });
        }

        const bool loaded = ui->loadSessionFile(QUrl::fromLocalFile(sessionFile));
        if(!loaded) {
            reportVerificationFailure(QStringLiteral("session check: the frontend rejected the session file it had written"));
        }
        else {
            Scene* reloadedScene = ui->datasetContainer().activeScene();
            SceneNode* reloadedNode = reloadedScene && !reloadedScene->children().empty() ? reloadedScene->children().front() : nullptr;
            if(!reloadedNode)
                reportVerificationFailure(QStringLiteral("session check: loading the session file produced an empty scene"));
            else if(reloadedNode->objectTitle() != QStringLiteral("renamed after saving"))
                reportVerificationFailure(QStringLiteral("session check: loading the session file did not restore the saved scene (title: %1)").arg(reloadedNode->objectTitle()));
            if(ui->isSessionModified())
                reportVerificationFailure(QStringLiteral("session check: a freshly loaded session is reported as modified"));
            if(ui->sessionFilePath() != QFileInfo(sessionFile).absoluteFilePath())
                reportVerificationFailure(QStringLiteral("session check: the loaded session does not remember its file"));
        }

        // 4. The session file ends up in the recently opened files, which is the list the frontends offer to the user.
        const auto& recentEntries = RecentFilesList::instance().entries();
        const bool isMostRecent = !recentEntries.isEmpty() && recentEntries.front().urls.size() == 1
            && recentEntries.front().urls.front() == QUrl::fromLocalFile(sessionFile);
        if(!isMostRecent)
            reportVerificationFailure(QStringLiteral("session check: the session file is not the most recently opened file"));
        qInfo() << "SESSION_TEST the session workflow saved, modified and reloaded the scene;"
                << RecentFilesList::instance().entries().size() << "recent file(s)";

        // 5. A session without a file name needs a file dialog, which this frontend does not provide yet. That has to be
        //    reported instead of failing silently - the classic frontend overrides requestSessionFilePath() with its
        //    QFileDialog.
        OORef<DataSet> dataset = ui->datasetContainer().currentSet();
        const QString rememberedPath = ui->sessionFilePath();
        if(dataset)
            dataset->setFilePath({});
        bool reportedMissingDialog = false;
        try {
            ui->saveSession();
        }
        catch(const Exception&) {
            reportedMissingDialog = true;
        }
        if(!reportedMissingDialog)
            reportVerificationFailure(QStringLiteral("session check: saving a session without a file name did not report the missing file dialog"));
        else
            qInfo() << "SESSION_TEST a session without a file name reports the missing file dialog";
        if(dataset)
            dataset->setFilePath(rememberedPath);
    });

    continuation();
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
        parser.addOption(QCommandLineOption(QStringLiteral("qml-hold-ms"),
            tr("Keep the workbench window open for the given number of milliseconds after the verification steps, "
               "so that a screenshot can be taken of it, and then quit."), QStringLiteral("MS")));
        parser.addOption(QCommandLineOption(QStringLiteral("qml-startup-delay"),
            tr("Time in milliseconds to wait after startup before the verification steps begin."), QStringLiteral("MS")));
        parser.addOption(QCommandLineOption(QStringLiteral("qml-lifecycle-cycles"),
            tr("Number of scene graph resource release/rebuild cycles to perform before the window is closed."), QStringLiteral("N")));
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
        parser.addOption(QCommandLineOption(QStringLiteral("qml-session-check"),
            tr("Verify the session workflow of the workbench: saving, the modified state, and loading a session back.")));
        parser.addOption(QCommandLineOption(QStringLiteral("qml-settings-check"),
            tr("Verify the settings facade both frontends share: the values round-trip and the shell's theme follows it.")));
        parser.addOption(QCommandLineOption(QStringLiteral("qml-command-check"),
            tr("Verify the shared command layer: the commands the QML workbench sees, their state rules and their handlers.")));
        parser.addOption(QCommandLineOption(QStringLiteral("qml-layout-check"),
            tr("Verify the viewport layout: pane geometry, undoable splitter drags and maximizing a viewport.")));
        parser.addOption(QCommandLineOption(QStringLiteral("qml-import-check"),
            tr("Verify the import path of the shell: a multi-frame trajectory, an unsupported file and a cancelled import.")));
        parser.addOption(QCommandLineOption(QStringLiteral("qml-library-check"),
            tr("Verify the modifier and viewport layer libraries, whose entries are commands of the shared command layer.")));
        parser.addOption(QCommandLineOption(QStringLiteral("qml-icon-check"),
            QStringLiteral("Verify that the shell shows the icons of the shared icon set.")));
        parser.addOption(QCommandLineOption(QStringLiteral("qml-offscreen-check"),
            tr("Verify the shared offscreen rendering service: the ambient-occlusion sampling, a picking pass and a render output at the same time.")));
        parser.addOption(QCommandLineOption(QStringLiteral("qml-prewarm-check"),
            tr("Verify that the picking buffer refreshes itself after a change of the view, without a pick asking for it.")));
        parser.addOption(QCommandLineOption(QStringLiteral("qml-device-check"),
            tr("Verify that the frontend reports a missing graphics device, which depends on the platform plugin.")));
        parser.addOption(QCommandLineOption(QStringLiteral("qml-parity-check"),
            tr("Verify the shell features added for parity: the menus, the viewport context menu, the task rows, the window state and the import notice.")));
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
        const bool verifyPicking = pickPosition.has_value();
        const int frameStatsDuration = cmdLineParser().value(QStringLiteral("qml-frame-stats")).toInt();
        const bool hideShowTest = cmdLineParser().isSet(QStringLiteral("qml-hide-show"));
        const bool layoutCheck = cmdLineParser().isSet(QStringLiteral("qml-layout-check"));
        const bool commandCheck = cmdLineParser().isSet(QStringLiteral("qml-command-check"));
        const bool settingsCheck = cmdLineParser().isSet(QStringLiteral("qml-settings-check"));
        const bool sessionCheck = cmdLineParser().isSet(QStringLiteral("qml-session-check"));
        const bool importCheck = cmdLineParser().isSet(QStringLiteral("qml-import-check"));
        const bool parityCheck = cmdLineParser().isSet(QStringLiteral("qml-parity-check"));
        const bool deviceCheck = cmdLineParser().isSet(QStringLiteral("qml-device-check"));
        const bool iconCheck = cmdLineParser().isSet(QStringLiteral("qml-icon-check"));
        const bool libraryCheck = cmdLineParser().isSet(QStringLiteral("qml-library-check"));
        const bool offscreenCheck = cmdLineParser().isSet(QStringLiteral("qml-offscreen-check"));
        const bool prewarmCheck = cmdLineParser().isSet(QStringLiteral("qml-prewarm-check"));
        const int lifecycleCycles = cmdLineParser().value(QStringLiteral("qml-lifecycle-cycles")).toInt();
        QSize resizeSize;
        const QStringList resizeArguments = cmdLineParser().value(QStringLiteral("qml-resize")).split(QLatin1Char('x'));
        if(resizeArguments.size() == 2)
            resizeSize = QSize(resizeArguments[0].toInt(), resizeArguments[1].toInt());

        // A screenshot of the workbench is taken from the X server while the window is held open; see
        // docs/design/UI_TEST_ENV.md for the ffmpeg command. It cannot be taken from inside the process, because
        // QQuickWindow::grabWindow() is not usable with the QQuickRhiItem viewports of the workbench.
        const int holdMs = cmdLineParser().value(QStringLiteral("qml-hold-ms")).toInt();

        // Interactive mode: without a verification option, keep the window open.
        if(!verifyPicking && frameStatsDuration <= 0 && !hideShowTest && !resizeSize.isValid() && lifecycleCycles <= 0 && !layoutCheck && !importCheck && !commandCheck && !sessionCheck && !settingsCheck && !parityCheck && !deviceCheck && !libraryCheck && !iconCheck && !offscreenCheck && !prewarmCheck)
            return;

        int delay = cmdLineParser().value(QStringLiteral("qml-startup-delay")).toInt();
        if(delay <= 0)
            delay = 3000;

        _verificationFinished = [ui = mainWinUI, holdMs]() {
            qInfo() << "VERIFICATION_DONE with" << verificationFailures << "failed check(s)";
            if(holdMs <= 0) {
                QCoreApplication::exit(verificationFailures == 0 ? 0 : 1);
                return;
            }
            // Keep the window open, so that the caller can take a screenshot of it, and quit afterwards.
            scheduleDelayed(ui, holdMs, []() { QCoreApplication::exit(verificationFailures == 0 ? 0 : 1); });
        };

        // Assemble the verification steps. The delay runs first, so that the imported data set and the initial
        // frame graphs are ready before anything is measured. The layout check comes before the steps that release and
        // rebuild the graphics resources of the viewports, because those are the stress steps.
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
                    reportCameras(ui, QStringLiteral("right after the import"));
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
        if(commandCheck) {
            _verificationSteps.push_back([ui = mainWinUI](std::function<void()> next) {
                runCommandTest(ui, std::move(next));
            });
        }
        if(layoutCheck) {
            _verificationSteps.push_back([ui = mainWinUI](std::function<void()> next) {
                runLayoutTest(ui, std::move(next));
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
        if(deviceCheck) {
            // First of all checks: it only reads the state of the window and reports which platform plugin provides
            // which graphics device.
            _verificationSteps.push_back([ui = mainWinUI](std::function<void()> next) {
                runDeviceTest(ui, std::move(next));
            });
        }
        if(iconCheck) {
            // Reads the state of the shell only, so it can run before the checks that change the scene.
            _verificationSteps.push_back([ui = mainWinUI](std::function<void()> next) {
                runIconTest(ui, std::move(next));
            });
        }
        if(verifyPicking) {
            _verificationSteps.push_back([ui = mainWinUI, pickPos = *pickPosition](std::function<void()> next) {
                runPickTest(ui, pickPos, std::move(next));
            });
        }
        if(settingsCheck) {
            // Before the checks that change the scene: this one only reads and writes the settings store.
            _verificationSteps.push_back([ui = mainWinUI](std::function<void()> next) {
                runSettingsTest(ui, std::move(next));
            });
        }
        if(libraryCheck) {
            // Before the checks that change the scene in earnest: the trigger case below inserts a modifier and undoes
            // it again, so the scene is left as it was found.
            _verificationSteps.push_back([ui = mainWinUI](std::function<void()> next) {
                runLibraryTest(ui, std::move(next));
            });
        }
        if(sessionCheck) {
            // Before the import check, which ends with a canceled import that leaves the scene empty; this check saves
            // and reloads the scene of the workbench.
            _verificationSteps.push_back([ui = mainWinUI](std::function<void()> next) {
                runSessionTest(ui, std::move(next));
            });
        }
        if(prewarmCheck) {
            // Right after the picking check: it needs a scene with a fitted camera, and it moves the camera itself.
            _verificationSteps.push_back([ui = mainWinUI](std::function<void()> next) {
                runPrewarmTest(ui, std::move(next));
            });
        }
        if(parityCheck) {
            // Before the import check, which ends with a cancelled import that leaves the scene empty, and after the
            // picking check, because this check clicks in a viewport and changes the camera for a moment.
            _verificationSteps.push_back([ui = mainWinUI](std::function<void()> next) {
                runParityTest(ui, std::move(next));
            });
        }
        if(offscreenCheck) {
            // After the parity check and before the import check: it inserts a modifier into the pipeline of the scene
            // and resizes the workbench, so it needs the scene the earlier checks left behind and must not disturb the
            // imports that follow it.
            _verificationSteps.push_back([ui = mainWinUI](std::function<void()> next) {
                runOffscreenTest(ui, std::move(next));
            });
        }
        if(importCheck) {
            // Last, because it imports data sets of its own.
            _verificationSteps.push_back([ui = mainWinUI](std::function<void()> next) {
                runImportTest(ui, std::move(next));
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
