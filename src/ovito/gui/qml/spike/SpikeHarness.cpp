// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

// The shared machinery of the verification harness: the failure report, the helpers that read and poke the state of
// the workbench, and the files a check writes for the importer. The checks themselves live in checks/, grouped by
// what they verify; nothing in this file belongs to a single check.

#include <ovito/gui/qml/spike/SpikeHarness.h>

namespace Ovito::Spike {

/// Number of verification checks that did not produce the expected result. The spike exits with a non-zero
/// status when this counter is non-zero, so an automated run (the CI smoke test) fails on a broken viewport
/// instead of only printing a warning into a log nobody reads.
static int verificationFailures = 0;

/// Returns the number of checks that did not produce the expected result.
int spikeVerificationFailures()
{
    return verificationFailures;
}

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

/// Picks at a grid of positions around the given location of a viewport item. The grid is moved into the item
/// when the requested location lies outside of it, so that a probe always covers the viewport - including after
/// the item geometry changed.
PickProbe probePicking(QuickViewportWindow* viewportWindow, const QSizeF& itemSize, const QPoint& center, int extent, int spacing)
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
                          std::function<void()> continuation, QSizeF previousItemSize)
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
QString layoutSnapshot(QmlViewportLayout* layout, QString* visiblePanes)
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

/// Verifies the import path of the shell: a multi-frame trajectory becomes one pipeline spanning three animation
/// frames, a file of an unsupported format reports an error without changing the scene, and a canceled import leaves no
/// partially loaded pipeline behind.
/// Reads a property that a QML file declares. QObject::property() does not see those properties, QQmlProperty does.
QVariant qmlProperty(QObject* object, const QString& name)
{
    QQmlProperty property(object, name);
    return property.isValid() ? property.read() : QVariant();
}

/// Collects the models of the pipeline panel and of the libraries once and returns them again on later calls.
///
/// Their constructors register commands with fixed ids (one per pipeline item, one per library entry), and the action
/// manager refuses a second command with the same id - so a process may hold only one instance of each model. The
/// pipeline list and the modifier library belong to the frontend since Phase 3 (QmlPipelineController owns them, see
/// audit decision D54); the layer library is created here because the frontend presents no viewport layers yet.
WorkbenchModels& workbenchModels(QmlMainWindowUI* ui)
{
    static WorkbenchModels models;
    if(models.pipelineList == nullptr) {
        models.pipelineList = ui->pipelineController()->pipelineModel();
        models.modifiers = ui->pipelineController()->modifierListModel();
        models.overlays = new AvailableOverlaysModel(ui->view(), *ui, new OverlayListModel(ui->view(), *ui));
    }
    return models;
}

}   // namespace Ovito::Spike
