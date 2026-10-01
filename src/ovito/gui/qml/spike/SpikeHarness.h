// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

/**
 * \file
 * \brief The shared machinery of the OvitoQmlSpike verification harness.
 *
 * The harness verifies the Qt Quick frontend by driving it: every check is a step of one sequence (the step table in
 * Main.cpp decides which steps a run asks for and in which order they run), uses the helpers declared here to read and
 * to poke the state of the workbench, and reports a result that does not match through reportVerificationFailure(),
 * which is what makes the spike exit non-zero and the CI job fail.
 *
 * The checks themselves live in checks/ and are grouped by what they verify - the shell, the rendering side and the
 * import path. A helper belongs in this header once two of those files need it; the helper of one check stays in its file.
 */

#ifndef OVITO_QML_SPIKE_HARNESS_H
#define OVITO_QML_SPIKE_HARNESS_H

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

namespace Ovito::Spike {

/// How long a picking check waits for the asynchronously rendered picking buffer. This is generous on purpose: a CI
/// runner is much slower than a development machine, and a picking pass includes creating the render thread's graphics
/// device and compiling its pipelines, which can take many seconds there.
constexpr int pickTimeoutMs = 20000;

/// The result of probing the objects at a position of a viewport.
struct PickProbe
{
    int tested = 0;
    int hits = 0;
    QPoint center;       ///< The probe location the grid was actually centered on.
    QSizeF itemSize;     ///< The item size the probe was clipped to.
    std::optional<ViewportWindow::PickResult> example;
};

/// The models of the pipeline panel and of the modifier and layer libraries. Their constructors register commands with
/// fixed ids, and the action manager refuses a second command with the same id - so a process can hold only one instance
/// of each, which is why the checks share the ones the workbench created (the pipeline list and the modifier library
/// belong to the frontend since Phase 3, the layer library is created here because no frontend presents it yet).
struct WorkbenchModels
{
    PipelineListModel* pipelineList = nullptr;
    AvailableModifiersModel* modifiers = nullptr;
    AvailableOverlaysModel* overlays = nullptr;
};

/// Returns the number of checks that did not produce the expected result.
int spikeVerificationFailures();

/// Records a verification check that did not produce the expected result.
void reportVerificationFailure(const QString& message);

/// Runs the given function after the specified delay.
void scheduleDelayed(QmlMainWindowUI* ui, int delay, std::function<void()> action);

/// Collects the viewport items of the QML scene.
/// Note: the items created by the QML delegate model are not part of the QObject parent chain, so
/// QObject::findChildren() cannot be used here and the item tree must be traversed instead.
QList<QuickViewportItem*> viewportItems(QmlMainWindowUI* ui);

/// Evaluates an expression in the QML context of the workbench root object, i.e. against the context properties the
/// shell is built on (workbenchController, commandManager, taskProgress, ...). Returns an invalid QVariant and logs the
/// error if the expression cannot be evaluated, so an unbound or misspelled property is visible in the log.
QVariant evaluateInQml(QmlMainWindowUI* ui, const QString& expression);

/// Picks at a grid of positions around the given location of a viewport item. The grid is moved into the item
/// when the requested location lies outside of it, so that a probe always covers the viewport - including after
/// the item geometry changed.
PickProbe probePicking(QuickViewportWindow* viewportWindow, const QSizeF& itemSize, const QPoint& center, int extent = 8, int spacing = 4);

/// Picks at a grid of positions covering the whole viewport item, which answers the question "does picking work in
/// this viewport at all". A fixed probe position cannot answer that: the camera fits a scene to the aspect ratio
/// of the viewport, so the same position can lie outside the scene on a viewport of a different shape even though
/// picking works perfectly - which is how the CI smoke test failed on a viewport that is 307 instead of 380 pixels
/// tall, while the synthetic click at the item centre still selected the lattice.
PickProbe scanPicking(QuickViewportWindow* viewportWindow, const QSizeF& itemSize);

/// Prints the outcome of a picking probe.
void reportPicking(const PickProbe& probe, const QString& what);

/// Returns the first viewport item of the QML scene, or null if the scene has none.
QuickViewportItem* firstViewportItem(QmlMainWindowUI* ui);

/// Evaluates a condition on the GUI thread repeatedly until it becomes true or the timeout expires.
/// \param done  Receives true when the condition was satisfied and false on timeout.
void pollUntil(QmlMainWindowUI* ui, int intervalMs, int timeoutMs, std::function<bool()> condition, std::function<void(bool)> done);

/// Reports the state a picking check was made under. Without it a failing check only says "no object was picked",
/// which does not distinguish a slow or failed picking pass from a scene with nothing at the probed position.
void reportPickingState(QmlMainWindowUI* ui, const QPoint& probePos);

/// Waits until the picking buffer has been re-rendered for the current viewport state after the viewport changed
/// (a resize or a hide/show cycle) and reports how long that took, counted from the moment the change was made.
void waitForPickingBuffer(QmlMainWindowUI* ui, const QPoint& probePos, const QString& what, QDateTime changeTime,
                          std::function<void()> continuation, QSizeF previousItemSize = {});

/// Reports where the cameras of the viewports are looking from. The framing of a data set depends on the application
/// of the importer's "zoom to scene extents" request, which is easy to lose when a viewport window does not exist yet
/// at the time the request is made.
void reportCameras(QmlMainWindowUI* ui, const QString& when);

/// A readable snapshot of the pane geometry of the viewport area, used to compare the layout before and after
/// a splitter drag and an undo.
QString layoutSnapshot(QmlViewportLayout* layout, QString* visiblePanes = nullptr);

/// Writes a simple-cubic lattice to an XYZ file and returns the path of the file.
///
/// The comment line must not contain the word "atoms": OVITO's importer autodetection then hands the file to the
/// LAMMPS data importer, which parses an empty scene out of it (see docs/design/UI_TEST_ENV.md).
QString writeLatticeFile(const QString& path, int atomsPerEdge, double latticeConstant);

/// Writes a VASP POSCAR file with the given number of atoms and returns the path of the file.
///
/// Unlike the XYZ importer, the VASP importer loads the data while the file is being imported
/// (POSCARImporter::setupPipeline() evaluates the pipeline), so importing this file makes the import operation long
/// enough to be cancelled - which is what the cancellation check needs.
QString writePoscarFile(const QString& path, int atomsPerEdge);

/// Returns the number of objects in the scene, or -1 if there is no scene.
int sceneObjectCount(QmlMainWindowUI* ui);

/// Returns whether any pipeline of the scene reads the given file, i.e. whether the import of that file left a
/// pipeline behind.
bool sceneReadsFile(QmlMainWindowUI* ui, const QString& fileName);

/// Returns the file source of the first pipeline of the scene, or null.
const FileSource* firstFileSource(QmlMainWindowUI* ui);

/// Compares the workbench's task progress model with the same data as the QML scene sees it. The status line of the
/// shell is bound to the model, so a model that is not reachable from QML (or a broken binding) has to be caught here
/// rather than by looking at a screenshot. Called while an operation runs and after it has finished.
void reportTaskProgress(QmlMainWindowUI* ui, const QString& when);

/// Verifies the import path of the shell: a multi-frame trajectory becomes one pipeline spanning three animation
/// frames, a file of an unsupported format reports an error without changing the scene, and a canceled import leaves no
/// partially loaded pipeline behind.
/// Reads a property that a QML file declares. QObject::property() does not see those properties, QQmlProperty does.
QVariant qmlProperty(QObject* object, const QString& name);

/// Creates the workbench models once per process and returns them again on later calls.
///
/// Their constructors register commands with fixed ids (one per pipeline item, one per library entry), and the action
/// manager refuses a second command with the same id - so a process may hold only one instance of each model. A
/// frontend is in the same position, which is why the models are created once here and shared by the checks.
WorkbenchModels& workbenchModels(QmlMainWindowUI* ui);

/// is what the menu entry and the toolbar button of the classic frontend toggle. The scene must hold an animation with
/// more than one frame; a single-frame scene never starts a playback (SceneAnimationPlayback::startAnimationPlayback).
///
/// This is verified where a trajectory exists - either because the check was given one on the command line (the command
/// check probes it then) or right after the import check has imported one.
void probeAnimationPlaybackCommand(QmlMainWindowUI* ui, Command* playbackCommand);

/******************************************************************************
* Verifies the model side of the pipeline panel (Phase 3, slice S1): the roles a QML view reads, the stable identity of
* a row's object, the selection and its coherence while the pipeline is edited, and the command list model.
*
* The check imports a data set of its own and replaces the data set at the end, so it has to run after the checks that
* rely on the scene they leave behind.
******************************************************************************/
void runPipelineTest(QmlMainWindowUI* ui, std::function<void()> continuation);

/// Verifies the model side of the animation timeline (Phase 3, slice S2). It imports a data set of its own and edits the
/// animation of its scene node, so it belongs after the checks that need the scene they leave behind and before the one
/// that replaces the data set.
void runAnimationTest(QmlMainWindowUI* ui, std::function<void()> continuation);

/// Verifies the automation foundation of the frontend (Phase 3, slice S4): a workbench serves its session only when it
/// was asked to, the command line of this build finds it, reads it within its read-only capabilities and reports its
/// objects, and stopping the server removes the session again.
///
/// The sessions of the check are its own: it points the session directory at a temporary directory of its process, so it
/// neither sees nor disturbs the sessions a developer has running.
void runAutomationTest(QmlMainWindowUI* ui, std::function<void()> continuation);

/// Verifies object picking at the given position of the first viewport item.
///
/// Picking is served from a buffer that the render thread renders asynchronously, so the first call after the
/// viewport contents changed cannot return a result yet; it starts the picking pass instead. The test therefore
/// measures twice and reports both outcomes, then verifies that a synthetic mouse click selects an object
/// through the regular input mode path.
void runPickTest(QmlMainWindowUI* ui, const QPoint& itemPos, std::function<void()> continuation);

/// Hides the viewport items for a moment and shows them again. A hidden viewport releases its GPU resources and
/// acquires them again when it reappears; rendering and picking must survive that.
void runHideShowTest(QmlMainWindowUI* ui, const QPoint& probePos, std::function<void()> continuation);

/// Resizes the workbench window. The picking buffer is tied to the viewport device size, so this also verifies
/// that picking recovers after a resize, which may happen at any time in a real session.
void runResizeTest(QmlMainWindowUI* ui, const QSize& newSize, const QPoint& probePos, std::function<void()> continuation);

/******************************************************************************
* Verifies that the frontend notices when its platform plugin provides no graphics device.
*
* Which device is available depends on the platform plugin the run was started with, so this check prescribes no
* outcome. It prescribes that the frontend's answer matches the scene graph of the window, and that the error path
* tells the user which platform plugin is at fault. A run with QT_QPA_PLATFORM=offscreen takes that path: Qt's
* offscreen plugin provides no QRhi, so the viewports of such a run stay empty.
******************************************************************************/
void runDeviceTest(QmlMainWindowUI* ui, std::function<void()> continuation);

/******************************************************************************
* Verifies that the shell shows the icons of the shared icon set.
*
* Both frontends take their icons from the same pair of icon themes of the shared GUI layer (IconTheme), and the Qt Quick
* frontend reaches them through the image provider of QmlIcons. The check verifies both halves: that the shared rule
* resolves the icons the shell and its commands name, in either color scheme, and that the QML side really displays one.
******************************************************************************/
void runIconTest(QmlMainWindowUI* ui, std::function<void()> continuation);

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
void runSettingsTest(QmlMainWindowUI* ui, std::function<void()> continuation);
void runLibraryTest(QmlMainWindowUI* ui, std::function<void()> continuation);

/// Quick shell has no session commands yet), so this check drives the operations that do not need a file name.
void runSessionTest(QmlMainWindowUI* ui, std::function<void()> continuation);

/// Verifies the picking pre-warm (see QuickViewportWindow::pickingPrewarmTimeout()).
///
/// The check moves the camera without picking and expects the picking buffer to catch up on its own: that is the whole
/// point of the pre-warm - the first hover after an interaction is answered from the current view, not from the view
/// before it. It also verifies that a viewport which nobody touches stops rendering, because a pre-warm that refreshed
/// the buffer over and over would be a continuous renderer in disguise.
void runPrewarmTest(QmlMainWindowUI* ui, std::function<void()> continuation);
void runParityTest(QmlMainWindowUI* ui, std::function<void()> continuation);

/******************************************************************************
* Verifies the shared offscreen rendering service (class OffscreenRenderTarget) end to end: that it refuses a pass
* which does not match the kind of its target, and that the ambient-occlusion sampling, a picking pass and a full
* render output run to completion at the same time, including a picking target that a resize supersedes in flight.
******************************************************************************/
void runOffscreenTest(QmlMainWindowUI* ui, std::function<void()> continuation);
void runImportTest(QmlMainWindowUI* ui, std::function<void()> continuation);

/// Verifies the viewport area of the workbench shell: the panes come from the layout tree of the dataset, dragging a
/// handle resizes them through the undo system, and maximizing a viewport keeps exactly one pane visible.
void runLayoutTest(QmlMainWindowUI* ui, std::function<void()> continuation);

/******************************************************************************
* The commands are the frontend-neutral description of everything the user can invoke, and the classic frontend
* presents them as QActions. This check verifies that the QML workbench sees the same commands with the same state,
* that the state rules of the frontend (the undo stack, the animation playback, the viewport layout) drive the
* commands, and that a command can be invoked through the QML engine.
******************************************************************************/
void runCommandTest(QmlMainWindowUI* ui, std::function<void()> continuation);

/// Measures the frame rate the viewports achieve when frames are requested continuously, which is what animation
/// playback does. The requests originate from a queued connection because QQuickWindow::frameSwapped is emitted
/// on the render thread, while requesting frames is a GUI-thread operation.
void measureFrameRate(QmlMainWindowUI* ui, int durationMs, std::function<void()> continuation);

/**
 * Forces the Qt Quick scene graph to release and rebuild all its graphics resources repeatedly. This destroys
 * and recreates the renderers of the viewport items, including all QRhi resources they hold, which is the
 * resource lifecycle the classic frontend exercises when viewport windows are closed and reopened.
 * The continuation is invoked after the last cycle.
 */
void scheduleLifecycleCycles(QmlMainWindowUI* ui, int cycles, std::function<void()> continuation);

}   // namespace Ovito::Spike

#endif
