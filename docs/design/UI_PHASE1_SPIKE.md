# Phase 1 — Qt Quick Viewport Spike Report

> **Companion documents**: [UI_DESIGN.md](UI_DESIGN.md), [UI_PLAN.md](UI_PLAN.md), [UI_PHASE0_AUDIT.md](UI_PHASE0_AUDIT.md)
>
> **Status**: The critical rendering risk is validated — an OVITO scene is rendered inside a Qt Quick window by a
> `QQuickRhiItem` renderer that uses the scene graph's own QRhi, with four simultaneous viewports and no
> `RenderThread` in the interactive frame path. Object picking is implemented as an asynchronous offscreen pass served by
> OVITO's `RenderThread` machinery, verified end to end (selection through `SelectionMode`). Cross-backend
> (Metal/D3D12/Vulkan) and performance baselines remain open, so the architecture is **not yet frozen**.

---

## 1. Architecture Implemented

```
src/ovito/gui/qml/
├── CMakeLists.txt                     OVITO_BUILD_QML_FRONTEND-guarded module "GuiQml" (links GuiBase + Qt6::Quick)
├── QmlFrontend.h                      module precompiled header
├── mainwin/QmlMainWindowUI.{h,cpp}    UserInterface subclass: window, dataset, import, status/error reporting
├── mainwin/QmlViewportController.{h,cpp}  QObject exposed to QML as viewportController (statusMessage, createViewportItem)
├── viewport/QuickViewportWindow.{h,cpp}   BaseViewportWindow adapter (frame graph generation, input, geometry)
├── viewport/QuickViewportItem.{h,cpp}     QQuickRhiItem (scene item, event forwarding, frame handoff)
├── viewport/QuickViewportRenderer.{h,cpp} QQuickRhiItemRenderer + RendererService (renders the frame graph)
├── resources/qml.qrc                  Theme.qml + WorkbenchWindow.qml (hand-written .qrc, no qt_add_qml_module)
└── spike/Main.cpp                     Phase 1 prototype executable "ovito-qml-spike"
```

`Core` gained the abstract `RendererService` (`src/ovito/core/rendering/RendererService.h`), which `RenderThread`
implements for the classic frontend and offscreen rendering, and which `QuickViewportRenderer` implements on top of the
Qt Quick QRhi. See [UI_PHASE0_AUDIT.md](UI_PHASE0_AUDIT.md) sections 2 and 4 for the authoritative description of that
change.

`Core` also gained `ObjectPickingBuffer` (`src/ovito/core/rendering/ObjectPickingBuffer.h`) and a
`RenderThread::renderPickingFrame()` entry point, which let the QML viewport obtain picking buffers without touching the
render thread's internals — see section 1.5.

### 1.1 Why composition (and not one class)

`QQuickRhiItem` and `ViewportWindow` both derive from `QObject`, so a viewport cannot be a single Qt item. The spike uses:

* `QuickViewportItem : QQuickRhiItem` — the QML scene item, no `OVITO_CLASS`, forwards Qt Quick input events, owns the handoff slot;
* `QuickViewportWindow : BaseViewportWindow` — reuses OVITO's viewport input handling, scene preparation, gizmos and
  `generateFrameGraph()` coroutine, and reports the item's geometry/visibility back to the core;
* `QuickViewportRenderer : QQuickRhiItemRenderer, RendererService` — the render-thread side.

### 1.2 Frame handoff protocol (as built)

1. GUI thread: `ViewportWindow::frameGraphReady()` → `QuickViewportWindow::renderFrameGraph(frameGraph)` creates the
   `SceneRenderer::Configuration` and calls `QuickViewportItem::submitFrameGraph()`, which stores the pending frame graph
   and calls `QQuickRhiItem::update()`.
   *Note*: the configuration is created into a local variable **before** the frame graph is moved into the call, because
   the evaluation order of function arguments is unspecified — see finding F1.
2. Scene graph thread, GUI thread blocked: `QuickViewportRenderer::synchronize(item)` takes the pending frame graph and
   configuration from the item. No lock is needed because the GUI thread cannot write while it is blocked in the sync phase.
3. Scene graph thread: `render(cb)` establishes an OVITO task context
   (`Promise<void>::create()` + `setIsInteractive()` + `setUserInterface()`, wrapped in `Task::Scope`), replays
   `RenderThread`'s frame sequence (`finalizeForRendering`, `createImplementationForVisual`, `renderFrame`,
   `prepareIntermediateTarget`, `prepareResourceUpdates`, `performPrePasses`, `beginPass`/`setViewport`/`setScissor`,
   `compositeInPass`, `endPass`, optional post-process + `renderOverLayerOnly`), then emits `frameRendered`.
4. GUI thread (queued): the item calls `QuickViewportWindow::handleShowEvent()` and emits `ViewportWindow::frameCompleted`,
   which drives animation playback and task progress exactly like the classic frontend.

`renderTarget()` is the item's own texture render target (`pixelSize()` matches `viewportWindowDeviceSize()`); no render
target, command buffer or readback is created by OVITO **for the interactive frame**, and no `RenderThread` is needed to
produce it. Offscreen picking is the one exception and uses a `RenderThread` (section 1.5).

### 1.3 Resource lifecycle

* `initialize(cb)` drops the renderer implementation and discards cached graphics pipelines, because the item's render
  pass descriptor (or sample count) may have changed.
* `~QuickViewportRenderer()` releases the implementation and the pending frame graph on the scene graph thread, while the
  QRhi is still alive. The `RendererResourceCache` member is declared **before** the implementation member so that all
  resource frames are released first.
* `QuickViewportWindow::releaseResources()` discards the pending frame graph before calling the base implementation, so a
  hide or window teardown cannot leave a frame graph referencing dead GPU resources.

### 1.4 Known deviations from the classic frontend (not gate items, but deliberate)

| Area | Prototype behavior | Planned phase |
|------|--------------------|---------------|
| Object picking | Implemented as an asynchronous offscreen pass (section 1.5): a pick that arrives before the first buffer exists returns `std::nullopt`, and a pick taken while a refresh is running is answered from the previous buffer (at most one refresh cycle old). | Phase 5 (hover-pick coalescing, pre-warming) |
| Rendering warnings (gizmo overlay warning icon) | Collected by `reportWarning()` and printed via `qWarning()`; no in-scene indicator. | Phase 4 |
| Renderer failure recovery | No analogue of `ViewportsPanel::fatalViewportWindowError()`. | Phase 2 |
| Context menu / drag & drop | Mouse/key events forwarded to `BaseViewportWindow`, but the context-menu request and drop handling are not wired to QML UI. | Phase 3 |
| Session files, error dialogs, message boxes | `reportError()`/`showMessageBox()` log to stderr and the status line. | Phases 3/7 |
| Viewport layout | Fixed 2×2 grid; not derived from `ViewportConfiguration::layoutRootCell()`. | Phase 2 |
| GPU resource sharing between viewports | One `RendererService` per item ⇒ per-item resource cache and pipeline cache. | Phase 2 (O4) |

### 1.5 Picking path (asynchronous, off the interactive frame)

`ViewportWindow::pick()` is a **synchronous contract** (`std::optional<PickResult>` in logical viewport coordinates) which
the classic frontend satisfies by blocking the GUI thread on its render thread. Neither of the two obvious ways to
satisfy it from Qt Quick works, and both were validated to fail before the current design was adopted:

* `QQuickRhiItemRenderer::render(cb)` runs *before* Qt Quick begins its own render pass, and a QRhi readback only
  completes at the end of the frame it was submitted in — a picking pass inside the interactive frame can therefore
  never produce a synchronous result.
* Starting an extra offscreen QRhi frame from the scene graph sync step is rejected by QRhi
  (`Attempted to call beginOffscreenFrame() within a still active frame`), because Qt Quick's frame is already active
  there.
* Blocking the GUI thread — even in a nested event loop — would stall input handling and the Qt Quick scene graph on the
  same thread, while `SelectionMode` requests a pick on *every* mouse move.

Picking therefore runs completely outside the interactive frame, reusing OVITO's existing offscreen rendering path:

1. `QuickViewportWindow::pick()` looks up the cached `ObjectPickingBuffer` locally and never waits. A buffer rendered for
   a different viewport size is discarded, because pixel coordinates would no longer match.
2. If the buffer is missing or was invalidated by a newly generated frame graph, `refreshPickingBuffer()` starts at most
   one background pass (`renderPickingBuffer()`): a coroutine that obtains a **dedicated** frame graph by calling the
   reused `ViewportWindow::generateFrameGraph()` (a frame graph is consumed by exactly one renderer, so it must not be
   shared with the interactive pass), wraps it with `sceneRenderer()->createConfiguration()` and submits it to
   `RenderThread::renderPickingFrame()` on a lazily created offscreen `RenderTarget` created with `forPickingOnly`.
3. On the render thread, `RenderThread::handleRenderPickingFrame()` reuses `renderPickingPass()`: it renders the frame
   graph with `isPickingPass = true` into R32UI object-id/primitive-id and D32F depth targets and resolves them into an
   `ObjectPickingBuffer` (buffers + `ViewProjectionParameters`).
4. `pickingBufferReady()` (queued onto the GUI thread by a `FutureWatcher`) installs the new buffer; `pick()` resolves
   positions through `ObjectPickingMap::lookupPickResult()`.

`ObjectPickingMap::lookupPickResult()` (the concentric-ring search plus record resolution) was factored out of
`RenderThread::lookupPickBuffer()` so that the classic frontend and the QML path share it; the GPU target creation code
is still duplicated and is recorded as follow-up **O7** in the audit.

The resulting deviations from the classic frontend are deliberate trade-offs, not oversights: a pick before the first
buffer has been rendered returns nothing, and a pick taken while a refresh is in flight is answered from the previous
buffer. Both are invisible for interactive use — mouse moves keep the buffer warm — and they are what makes the GUI
thread free of waits. Measured first-pick latency: section 3.4.

---

## 2. Verification Environment

| Item | Value |
|------|-------|
| Host | Linux x86_64, 16-core, AMD Ryzen 9 9950X (integrated RADV GPU), Mesa 25.2.8 |
| Qt | 6.10.2 (`.qt/6.10.2/gcc_64`) |
| Build | `cmake --preset native` (RelWithDebInfo, Clang, mold, ccache), `-DOVITO_BUILD_QML_FRONTEND=ON` |
| QPA / RHI for the spike | `QT_QPA_PLATFORM=xcb` under `xvfb-run`, Qt Quick RHI backend **OpenGL** (Mesa llvmpipe, GL 4.5 compat profile) |
| Data set | `lattice.xyz`: 512 argon atoms on an 8×8×8 simple-cubic lattice, a = 3.6 |
| Evidence | `docs/design/evidence/phase1_multiviewport.png` (four viewports, no selection), `docs/design/evidence/phase1_picking.png` (captured after a successful synthetic click; identical to the former in the viewport region, PSNR = ∞ over x < 1000) |

The machine has no display server, so the window is driven through `Xvfb`. The `offscreen` QPA plugin cannot be used for
this spike: it provides neither `createPlatformVulkanInstance()` nor a GL context, so Qt Quick cannot create a QRhi and
`QQuickRhiItem` reports *"No QRhi found for window, QQuickRhiItem will not be functional"*. The default
`QT_QPA_PLATFORM=ovitoheadless` path taken by `StandaloneApplication` is unavailable because that QPA plugin is missing
from this tree (O1 in the audit).

---

## 3. Results

### 3.1 Build and regression checks

| Check | Command | Result |
|-------|---------|--------|
| Configure | `cmake --preset native -DOVITO_BUILD_QML_FRONTEND=ON` | OK |
| Full build | `cmake --build --preset native -j 16` | OK, 0 errors, 0 new warnings |
| Tests | `ctest --preset native` | 6/6 passed (`tst_core_math`, `tst_containers`, `tst_concurrent`, `tst_concurrent_pool`, `tst_concurrent_loops`, `tst_simulation_cell`) |
| Headless start | `LD_LIBRARY_PATH=$PWD/.qt/6.10.2/gcc_64/lib QT_QPA_PLATFORM=offscreen ./build-native/bin/ovito --nogui` | exit code 0 |
| Classic frontend | `xvfb-run … ./build-native/bin/ovito` (12 s, then SIGTERM) | started, ran, no crash; the only stderr output is the pre-existing Vulkan validation error F3; the X11 screenshot shows the full classic UI, but the Vulkan-presented viewport child windows read back black through `xwd`, so classic viewport **pixels** remain unverified in this environment |
| Configure (assertions enabled) | `cmake --preset native -B build-asserts -DCMAKE_CXX_FLAGS="-DOVITO_DEBUG -DQT_FORCE_ASSERTS" -DOVITO_BUILD_QML_FRONTEND=ON` | OK — this tree ships no Qt debug libraries, so the `debug` preset cannot link (`Debug` maps to Qt debug binaries while `libQt6Cored.so` is absent). `OVITO_DEBUG` + `QT_FORCE_ASSERTS` instead enable `OVITO_ASSERT()`/`Q_ASSERT()` in an otherwise release-style build; verified by the assertion string literals that are present in the objects of `build-asserts` and absent in `build-native` |
| Full build (assertions) | `cmake --build build-asserts -j 12` | OK, 1149/1149 steps, 0 errors |
| Tests (assertions) | `ctest --test-dir build-asserts` (with `LD_LIBRARY_PATH` including `.qt/…/lib`, `build-asserts/lib/ovito/plugins` and `build-asserts/lib/ovito`) | 6/6 passed |
| Headless start (assertions) | `./build-asserts/bin/ovito --nogui` | exit code 0 |
| Classic frontend (assertions) | `xvfb-run … ./build-asserts/bin/ovito` (12 s, then SIGTERM) | ran, no assertion fired |
| Spike (assertions) | `xvfb-run … ./build-asserts/bin/ovito-qml-spike --qml-lifecycle-cycles 4 --qml-pick 300,300 /tmp/lattice.xyz` | no assertion fires after fixing F4; picking, selection and the four-viewport lifecycle behave exactly as in the release build |

### 3.2 Viewport rendering

| Scenario | Command (abridged) | Result |
|----------|--------------------|--------|
| Empty dataset, one viewport | `ovito-qml-spike --qml-capture /tmp/spike.png --qml-capture-delay 3000` | Viewport background plus the "Top" viewport label and the red/green/blue orientation tripod are present ⇒ OVITO's frame graph, including the `OverLayer` (gizmo overlay), reaches the Qt Quick scene |
| Real scene, one viewport | `… --qml-capture /tmp/spike2.png --qml-capture-delay 4000 /tmp/lattice.xyz` | 512 shaded icosahedral particles, correct camera fit (`zoomToSceneExtentsWhenReady()` after import) and correct vertical orientation |
| Four viewports | `… --qml-capture /tmp/spike_4vp.png --qml-capture-delay 8000 /tmp/lattice.xyz` | Top, Front, Left and Perspective panes render simultaneously with independent cameras and tripods; see the evidence image |
| Resource lifecycle | `… --qml-capture /tmp/spike_life.png --qml-capture-delay 5000 --qml-lifecycle-cycles 4 /tmp/lattice.xyz` | Four `QQuickWindow::releaseResources()` cycles (scene graph, item nodes and renderers destroyed and rebuilt) followed by a capture: all four viewport regions are **pixel-identical** to the baseline run; only the command panel's text region differs, and that region also differs between two runs with identical parameters, i.e. it is glyph rasterization noise, not viewport state |

All spike runs exited with code 0; no `qWarning()`/exception messages were emitted by `QuickViewportRenderer` and no
resource-cache assert fired. (`OVITO_ASSERT` is only active with `OVITO_DEBUG`, which the default `native` preset does not
set; the assertion-enabled configuration in section 3.1 exists for that reason.)

### 3.3 Facts established

* OVITO's `StandardRendererImplementation` renders unmodified into a `QQuickRhiItem` texture render target using the
  Qt Quick QRhi; the `RendererService` seam is sufficient (no `RenderThread`, no readback).
* Vertical orientation is correct without `QQuickRhiItem::mirrorVertically`; OVITO's `QRhi::isYUpInFramebuffer()`-based
  decisions and the item's auto render target agree.
* Multiple `QQuickRhiItem`s can render the same scene concurrently on one QRhi; each item needs its own renderer
  implementation and its own resource cache (the render pass descriptors differ per item, which
  `ensureGraphicsPipeline()` already keys on).
* The Qt Quick render thread can host OVITO rendering as long as a task context is established for the frame.
* `QQuickWindow::releaseResources()` is a usable, deterministic stress test for the renderer resource lifecycle.
* **Picking cannot be answered from inside the interactive Qt Quick frame.** Two constraints were verified:
  `beginOffscreenFrame()` is rejected during the scene graph sync step (`Attempted to call beginOffscreenFrame() within a
  still active frame`), and a readback submitted from `render(cb)` completes only at frame end. Both were measured, not
  assumed — the first attempt segfaulted on the former.
* **Qt-delivered event handlers run in a task without a `UserInterface`** (`TaskManager` uses
  `Task::Scope taskScope(nullptr)`), so `this_task::get()` succeeds but `this_task::ui()` asserts. Any QML entry point
  that starts OVITO asynchronous work must attach the context itself after the first suspension, exactly as the classic
  frontend does in `ViewportWindow::generateFrameGraph()` and `WidgetViewportWindow::grabViewportImage()`. This is a
  general QML-frontend trap and is recorded as O8.
* OVITO's offscreen machinery is reusable from a Qt Quick frontend: a new `RenderThread` entry point plus a handoff type
  (`ObjectPickingBuffer`) was enough to serve picking, without touching `renderPickingPass()` or the resolution logic.
* A picking buffer is tied to the pixel size it was rendered at; the QML viewport discards it when the viewport device
  size changes, because picking positions are converted from logical to device pixels.
* Reusing `ViewportWindow::generateFrameGraph()` for the picking pass works without duplicating viewport logic, provided a
  **dedicated** frame graph is generated (one frame graph may be consumed by only one renderer).

---

### 3.4 Object picking (measured)

All runs use the same data set and the same test position (300,300) in the first viewport item (496 × 380 logical
pixels) and scan a 5 × 5 grid of positions spaced 4 logical pixels apart:

| Run | Command additions | First successful pick | Grid hits, 1st pass | Grid hits, 2nd pass | Hit example |
|-----|-------------------|----------------------|--------------------|---------------------|-------------|
| Default render loop | – | 127–149 ms | 0/25 | 19/25 | subobject 335, hit location (16.946, 3.908, 25.368) |
| `basic` render loop | `QSG_RENDER_LOOP=basic` | 110 ms | 0/25 | 19/25 | identical |
| `threaded` render loop | `QSG_RENDER_LOOP=threaded` | 119 ms | 0/25 | 19/25 | identical |
| 2× HiDPI | `QT_SCALE_FACTOR=2` | 120 ms | 0/25 | 14/25 | subobject 335, (17.030, 4.075, 25.324) |
| Assertions + 4 lifecycle cycles | `build-asserts` binary | 120 ms | 0/25 | 19/25 | identical to the default run |

* **0/25 on the first pass** is the expected behavior of the asynchronous design: the first pick finds no buffer, starts the
  picking pass and returns `std::nullopt`. After ~110–150 ms the buffer is available and subsequent picks hit.
* **Identical results across the three Qt Quick render loops** and identical hit locations across runs show that the picking
  path does not depend on the scene graph's threading model (this is expected, because it runs off the interactive frame
  entirely).
* **Negative control**: picking at the corner (2,2) returns nothing, i.e. the empty background is not reported as a hit.
* **End-to-end selection**: a synthetic `QMouseEvent` press/release at the test position is delivered through
  `ViewportInputManager`'s `SelectionMode` and selects the pipeline (`PICK_TEST synthetic click selected "lattice.xyz [XYZ]"`),
  so OVITO's regular input-mode path works unmodified in the QML frontend.
* **HiDPI**: fewer grid positions hit at `QT_SCALE_FACTOR=2` because the pick radius is 4 **device** pixels in both
  frontends, i.e. it covers half the logical distance at 2× scaling. This matches the classic frontend's coordinate handling
  (`WidgetViewportWindow::pick()` also multiplies by `devicePixelRatio()`); the slightly different hit locations stem from the
  different device-pixel grid.
* **Latency caveat**: the measurement covers the *first* pass, which includes creating the shared `RenderThread`, its QRhi and
  the offscreen target. The steady-state cost of a later refresh (which reuses all of them) was not measured separately and
  remains part of the open performance gate.
* Evidence: `docs/design/evidence/phase1_picking.png` is captured *after* the synthetic click; its viewport region is
  pixel-identical to the pre-picking baseline capture.

## 4. Defects Found and Fixed During the Spike

| # | Defect | Fix |
|---|--------|-----|
| F1 | `QuickViewportWindow::renderFrameGraph()` passed `std::move(frameGraph)` and `createConfiguration(*frameGraph)` in the same call; with the implementation's evaluation order the moved-from reference was dereferenced, crashing in `StandardRenderer::createConfiguration()`. | Create the configuration into a local variable first, then move both. |
| F2 | `NewGraphicsSystemService::applicationStarting()` and `UpdateNotificationService::applicationStarting()` dereferenced the result of `dynamic_object_cast<MainWindowUI>()` after an assert only, which crashed during startup for any non-desktop frontend (all plugins, including the desktop GUI plugin, are loaded). | Both services return early when the active `UserInterface` is not a `MainWindowUI`. |
| F3 | The classic frontend logs `VUID-VkApplicationInfo-apiVersion` (value 0) when creating its `QVulkanInstance`. Pre-existing, non-fatal, still open (O2). | Not fixed; recorded in the audit. |
| F4 | The picking coroutine (`QuickViewportWindow::renderPickingBuffer()`) submitted its render-thread work from a Qt event handler, i.e. from a task without a `UserInterface`; with assertions active this aborted in `RenderThread::renderPickingFrame()` (`this_task::ui()`). | Attach the context after the coroutine's first suspension (`setUserInterface()`, `setIsInteractive()`), the pattern `WidgetViewportWindow::grabViewportImage()` already uses. Only found because the assert-enabled configuration exists — see O8 for the general QML-frontend consequence. |

### 4.1 Design corrections (not defects, but recorded so they are not repeated)

* Picking was first attempted from `QQuickRhiItemRenderer::synchronize()`, which segfaults: Qt Quick already has an active
  QRhi frame there. A second attempt kept the pass inside `render(cb)` and finalized the readback in a
  `QRhiReadbackResult::completed` callback, which required a nested GUI-thread event loop, timeouts and cross-thread
  lifetime guards. Both were discarded in favor of the asynchronous design in section 1.5 — the discarded implementation was
  reverted rather than shipped.

---

## 5. Remaining Phase 1 Exit-Gate Items

| Gate item | Status |
|-----------|--------|
| Minimal executable renders real OVITO scene data through the candidate architecture | **Done** |
| Composition, QRhi ownership, frame handoff documented | **Done** |
| Multi-viewport (4 viewports, shared GPU work) | **Done** (independent renderer per item; cross-item buffer sharing is O4) |
| Teardown, hide/show, scene graph invalidation | **Verified with assertions enabled** — four `QQuickWindow::releaseResources()` cycles plus a capture are reproducible in both the release and the `OVITO_DEBUG`/`QT_FORCE_ASSERTS` configuration, where no assertion fires (defect F4 was found this way). Closing the window itself is exercised at exit of every run. A true `Debug` configuration is not possible in this environment (no Qt debug libraries — see section 3.1). |
| Deadlock/circular wait elimination | **Done, and structurally enforced** — the interactive QML frame never touches a `RenderThread`; offscreen picking submits work through `RenderThread::renderPickingFrame()` and is answered from a cached buffer, so no GUI-thread wait exists that could interleave with a render-thread wait. The previously planned design (blocking `pick()`, `RenderThread::requestPick()`) was rejected for this reason. |
| Picking and raycasting | **Done** — asynchronous offscreen picking (section 1.5), verified with hit/no-hit controls, HiDPI, three render loops, lifecycle cycles and an end-to-end synthetic click through `SelectionMode` (section 3.4, O3). |
| HiDPI / resize | **Partially verified** — 2× scaling works with correct coordinate handling (section 3.4). Interactive resizing is forwarded (`geometryChange` → `handleResize` → `requestRerender`) and the picking buffer is dropped when the device size changes, but resizing and mixed-DPI multi-monitor setups were not measured. |
| Cross-API smoke test (Vulkan, Metal, D3D12) | **Not verified** — only Qt Quick's OpenGL backend on Linux x86_64 was exercised (the picking pass, by contrast, runs on OVITO's Vulkan `RenderThread` with lavapipe/RADV). macOS ARM64 and Windows x86_64 remain completely unverified; the Linux Qt Quick Vulkan path is blocked by the missing QPA plugin (O1). |
| Performance baseline vs classic (frame time, input responsiveness, data set sizes) | **Partially measured** — the first-pick latency after a fresh viewport is 110–150 ms (includes `RenderThread`/QRhi creation; section 3.4). Frame times, steady-state refresh cost and larger data sets are still unmeasured. |
| Document required changes to `core/rendering`/`core/viewport` | **Done** — `RendererService` extraction and the offscreen picking entry point, see [UI_PHASE0_AUDIT.md](UI_PHASE0_AUDIT.md) sections 2 and 4. |

Consequently the rendering bridge and the picking path are validated on one Qt Quick backend (OpenGL under `xvfb`),
with an assertion-enabled regression run; the design's architecture status therefore remains **proposed** until the
cross-backend and performance gates above are recorded.

---

## 6. Reproduction

```bash
# Build the prototype (option is OFF by default)
cmake --preset native -DOVITO_BUILD_QML_FRONTEND=ON
cmake --build --preset native --target OvitoQmlSpike -j 16

# 8x8x8 simple-cubic lattice, 512 atoms
python3 - <<'EOF'
n, a = 8, 3.6
lines = [str(n**3), "Lattice test"]
lines += [f"Ar {i*a:.3f} {j*a:.3f} {k*a:.3f}" for i in range(n) for j in range(n) for k in range(n)]
open('/tmp/lattice.xyz','w').write("\n".join(lines)+"\n")
EOF

# Render four viewports and save a screenshot (use --qml-lifecycle-cycles N to stress the resource lifecycle)
LD_LIBRARY_PATH="$PWD/.qt/6.10.2/gcc_64/lib" QT_QPA_PLATFORM=xcb timeout 300 \
  xvfb-run -a --server-args="-screen 0 1280x800x24" \
  ./build-native/bin/ovito-qml-spike --qml-capture /tmp/spike_4vp.png --qml-capture-delay 8000 /tmp/lattice.xyz
```

The prototype reports `Saved workbench window contents to <file>` on success and exits with code 0.

Picking and selection can be verified in the same run (`X,Y` is a position inside the first viewport item):

```bash
LD_LIBRARY_PATH="$PWD/.qt/6.10.2/gcc_64/lib" QT_QPA_PLATFORM=xcb timeout 300 \
  xvfb-run -a --server-args="-screen 0 1280x800x24" \
  ./build-native/bin/ovito-qml-spike --qml-capture /tmp/pick.png --qml-capture-delay 4000 \
    --qml-lifecycle-cycles 4 --qml-pick 300,300 /tmp/lattice.xyz

# The same run with assertions enabled (this tree ships no Qt debug libraries, hence the flags):
cmake --preset native -B build-asserts -DCMAKE_CXX_FLAGS="-DOVITO_DEBUG -DQT_FORCE_ASSERTS" -DOVITO_BUILD_QML_FRONTEND=ON
cmake --build build-asserts -j 12
```

The test prints `PICK_TEST …` lines: the hit count before and after the picking pass, the measured latency of the first
successful pick, a negative control on the empty background, and the object selected by the synthetic click.

## 7. Recommended Next Steps

1. **Close the remaining Phase 1 gates**: the cross-API smoke test (a Qt Quick Vulkan session, plus macOS Metal and Windows
   D3D12 runners) and a frame-time/performance baseline against the classic frontend, including the steady-state cost of a
   picking refresh.
2. **Decide and document the cross-backend validation plan**, including whether the `ovitoheadless` QPA plugin (O1) is
   implemented for Linux headless verification or the Linux gate is moved to a Vulkan-capable desktop session.
3. **Move to Phase 2 only after those gates are recorded**, starting with the frontend-neutral application class and
   `--gui=qml`, and deriving the viewport layout from `ViewportConfiguration::layoutRootCell()`.
