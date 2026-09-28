# Phase 1 — Qt Quick Viewport Spike Report

> **Companion documents**: [UI_DESIGN.md](UI_DESIGN.md), [UI_PLAN.md](UI_PLAN.md), [UI_PHASE0_AUDIT.md](UI_PHASE0_AUDIT.md)
>
> **Status**: The critical rendering risk is validated — an OVITO scene is rendered inside a Qt Quick window by a
> `QQuickRhiItem` renderer that uses the scene graph's own QRhi, with four simultaneous viewports and no
> `RenderThread` in the interactive frame path. Object picking is implemented as an asynchronous offscreen pass served by
> OVITO's `RenderThread` machinery, verified end to end (selection through `SelectionMode`). The frontend has been
> exercised on **three** Qt Quick backends — OpenGL and Vulkan on Linux x86_64, Metal on macOS ARM64 — including hide/show,
> resize, lifecycle and assertion-enabled runs, and a frame-rate baseline was measured against the classic frontend on
> macOS. On the fourth target, Windows x86_64, the frontend **compiles** (the CI job builds the whole tree with MSVC) but
> the D3D12 runtime path could not be exercised, so the architecture is **not yet frozen**.
>
> Environment recipes, traps and measurement instructions live in [UI_TEST_ENV.md](UI_TEST_ENV.md) — read that before
> re-running any of the checks below.

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

1. `QuickViewportWindow::pick()` looks up the cached `ObjectPickingBuffer` locally and never waits. The buffer is
   only used when it was rendered at the **current viewport device size**; otherwise pixel coordinates would be
   resolved in the geometry of a previous viewport (defect F5). A new pass is also discarded while the viewport
   geometry changes under it, because the projection the frame graph was generated with must belong to the size the
   picking buffer is rendered at.
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
| Evidence | `evidence/phase1_multiviewport.png` (Linux/OpenGL, four viewports), `evidence/phase1_picking.png` (Linux/OpenGL, captured after a successful synthetic click; identical to the former in the viewport region, PSNR = ∞ over x < 1000), `evidence/phase1_macos_metal.png` (macOS/Metal, Retina 2560×1600) |

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

All runs scan a 5 × 5 grid of positions spaced 4 logical pixels around a point of the first viewport item. Every report names
the probe centre and the item size it was clipped to, because the item size depends on the window size and the device
pixel ratio, and a probe that falls outside a small HiDPI item would otherwise measure `0 of 0 positions`:

| Run | Host / Qt Quick backend | Probe | 1st pass | 2nd pass | Example hit |
|-----|-------------------------|-------|----------|----------|-------------|
| Default render loop | Linux/OpenGL (llvmpipe) | (300,300) in 496×380 | 0/25 | 19/25 | subobject 335, (16.946, 3.908, 25.368) |
| `QSG_RENDER_LOOP=basic` | Linux/OpenGL | (300,300) in 496×380 | 0/25 | 19/25 | identical |
| `QSG_RENDER_LOOP=threaded` | Linux/OpenGL | (300,300) in 496×380 | 0/25 | 19/25 | identical |
| Qt Quick Vulkan (lavapipe) | Linux/Vulkan | (300,300) in 496×380 | 0/25 | 19/25 | identical (agrees to 10⁻³ Å) |
| Assertions, 4 lifecycle cycles | Linux/OpenGL, `OVITO_DEBUG` | (300,300) in 496×380 | 0/25 | 19/25 | identical |
| Metal, 512 atoms | macOS ARM64/Metal | (300,300) in 496×381 | 0/25 | 19/25 | subobject 335, (16.9346, 3.88916, 25.3201) |
| Metal, 32768 atoms | macOS ARM64/Metal | (250,250) in 496×381 | 25/25 | 25/25 | subobject 15743, (53.8032, 38.9933, 112.325) |
| Metal, 2× scaling | macOS ARM64/Metal | (200,150) in 336×216 | 11/25 | 11/25 | subobject 343, (16.9375, 7.45384, 25.3556) |
| After a window resize | Linux/OpenGL and macOS/Metal | (300,300) in 406×361 | 12–13/25 | 12–13/25 | subobject 399, (20.69–20.80, 2.98–3.05, 25.38–25.55) |

* **0/25 on the first pass** is the expected behavior of the asynchronous design: the first pick finds no buffer, starts the
  picking pass and returns `std::nullopt`. After ~110–150 ms the buffer is available and subsequent picks hit.
* **Identical results across the three Qt Quick render loops** and across the OpenGL, Vulkan and Metal backends show that the
  picking path does not depend on the scene graph's threading model or on the graphics API (expected, because it runs off the
  interactive frame entirely, and the picking pass is rendered by OVITO's own renderer).
* **Both passes at the same position must agree.** They did not before defect F5 was fixed in the adapter (19/25 versus 13/25
  at the same position, with different atoms reported): the previous viewport's buffer was still being used after a resize.
  The current code answers `nullopt` until a buffer for the new viewport geometry exists, so the consistency of the two
  probes is a regression check that is worth keeping.
* **Negative control**: picking at the corner (2,2) returns nothing, i.e. the empty background is not reported as a hit.
* **End-to-end selection**: a synthetic `QMouseEvent` press/release at the test position is delivered through
  `ViewportInputManager`'s `SelectionMode` and selects the pipeline (`PICK_TEST synthetic click selected "lattice.xyz [XYZ]"`),
  so OVITO's regular input-mode path works unmodified in the QML frontend.
* **HiDPI**: fewer grid positions hit at `QT_SCALE_FACTOR=2` because the pick radius is 4 **device** pixels in both
  frontends, i.e. it covers half the logical distance at 2× scaling. This matches the classic frontend's coordinate handling
  (`WidgetViewportWindow::pick()` also multiplies by `devicePixelRatio()`); the slightly different hit locations stem from the
  different device-pixel grid.
* **Latency**: the first successful pick costs 99 ms (macOS/Metal) to 150 ms (Linux/OpenGL with the software rasterizer),
  which includes creating the shared `RenderThread`, its QRhi and the offscreen target. A refresh on the warm path — after a
  resize invalidated the buffer — completes in 19–27 ms (
  bounded by the 10 ms poll interval of the harness), and a hide/show cycle, which releases and re-acquires the GPU resources,
  recovers after ~500 ms on both hosts. A probe fired *during* scene evaluation returns immediately (0 ms) with no deadlock.
* Evidence: `docs/design/evidence/phase1_picking.png` is captured *after* the synthetic click; its viewport region is
  pixel-identical to the pre-picking baseline capture. `docs/design/evidence/phase1_macos_metal_buddy.png` is the Metal
  capture of the 32768-atom run (1280×800, device pixel ratio 1).

### 3.5 Cross-backend results

Each backend was exercised with the full check set (`--qml-pick`, `--qml-hide-show`, `--qml-resize`,
`--qml-lifecycle-cycles`, `--qml-frame-stats`) and the 512-atom data set:

| Platform | Qt Quick backend | Rendering | Picking / selection | Frame rate (4 viewports) |
|----------|------------------|-----------|---------------------|--------------------------|
| Linux x86_64 (`xvfb`) | OpenGL 4.5 (Mesa llvmpipe, software) | ✔ | ✔ 19/25 grid hits, click selects | 14.4–15.3 ms/frame (65–69 fps, vsync), 7.6 ms (basic loop) |
| Linux x86_64 (`xvfb`) | Vulkan via lavapipe (software ICD) | ✔ | ✔ identical hits and hit locations | 19.7 ms/frame (50.7 fps) |
| macOS ARM64 (Cocoa) | **Metal** | ✔ | ✔ 14/25 grid hits (Retina geometry), click selects | 16.95 ms/frame (59.0 fps, vsync) |
| Windows x86_64 (CI runner) | D3D12 (WARP) | D3D12 (WARP) | not exercised: the job built the tree (1149/1149 targets) but stopped before the smoke test | not measured |

* The picking results are **identical** between the Linux OpenGL and Linux Vulkan runs (same subobject id, same hit
  location to within 10⁻³ Å), which is expected because the picking pass is rendered by OVITO's own renderer through
  either backend.
* The macOS run used the hardware Metal backend of both Qt Quick and OVITO's render thread (used for offscreen picking).
* Qt Quick **Vulkan on RADV** could not be exercised: `Xvfb` has no DRI3, which Vulkan presentation requires. The exact
  error messages and the working lavapipe recipe are in [UI_TEST_ENV.md](UI_TEST_ENV.md) section 2.1.

### 3.6 Performance

Frame times reported by `--qml-frame-stats` (frames are requested continuously, as animation playback does), four
viewports, one per pane:

| Data set | Host, Qt Quick backend, window | Frame time | Frame rate |
|----------|-------------------------------|-----------|------------|
| 512 atoms | macOS ARM64/Metal (Apple M4), 1280×800, dpr 1 | 9.74–9.80 ms | 102.0–102.7 fps |
| 512 atoms | macOS ARM64/Metal, 1920×940 | 8.87 ms | 112.8 fps |
| 4096 atoms | macOS ARM64/Metal, 1280×800 | 10.87 ms | 92.0 fps |
| 32768 atoms | macOS ARM64/Metal, 1280×800 | 10.07 ms | 99.3 fps |
| 262144 atoms | macOS ARM64/Metal, 1280×800 | 16.76 ms | 59.7 fps |
| 262144 atoms | macOS ARM64/Metal, 1920×940 | 16.26 ms | 61.5 fps |
| 512 atoms, `QSG_NO_VSYNC=1` | macOS ARM64/Metal, 1280×800 | 8.60 ms | 116.3 fps |
| 512 atoms | Linux x86_64/OpenGL 4.5 (llvmpipe), 1280×800 | 14.4–15.3 ms | 65–69 fps |
| 512 atoms, `QSG_RENDER_LOOP=basic`, `QSG_NO_VSYNC=1` | Linux x86_64/OpenGL (llvmpipe), 1280×800 | 7.6 ms | 131 fps |
| 4096 atoms | Linux x86_64/OpenGL (llvmpipe), 1280×800 | 19.2 ms | 52.0 fps |
| 32768 atoms | Linux x86_64/OpenGL (llvmpipe), 1280×800 | 48.8 ms | 20.5 fps |
| 262144 atoms | Linux x86_64/OpenGL (llvmpipe), 1280×800 | 285.7 ms | 3.5 fps |
| 512–32768 atoms | macOS ARM64/Metal (first host), 2× Retina (≈3.2 Mpx) | 16.95–17.34 ms | 57.7–59.0 fps |

* On the **M4 the frame time is dominated by the four-viewport overhead** up to 32768 atoms: it stays between 9 and 11 ms
  across a 64× range of particle counts, and it does not grow with the window (8.87 ms at 1920×940 versus 9.80 ms at
  1280×800 for 512 atoms, 16.26 versus 16.76 ms for 262144 atoms). Per frame the frontend generates four frame graphs,
  hands them to four `QQuickRhiItemRenderer`s and composites four textures on the scene-graph thread, and that fixed cost —
  not rasterization — sets the floor. Only 262144 atoms (≈1 M particles across the four viewports) push the render cost
  above it.
* The **Linux numbers come from a software rasterizer** (llvmpipe, 1280×800). They are a lower bound, and their scaling is
  the expected bandwidth-bound one — the opposite of the M4's flat curve, which is why they should not be extrapolated to
  hardware.
* The `basic` (single-threaded) render loop is ~2× faster than the default (threaded) loop under llvmpipe (7.6 versus
  14.4 ms). Frame-graph generation happens per frame in both cases, so this is scene-graph hand-off overhead rather than
  renderer cost; it is carried into Phase 2 together with O4 (one renderer service per window instead of one per item).
* The **Retina measurements of the first host are limited by its display refresh** (57.7–59.0 fps at ≈3.2 Mpx, exactly like
  the classic frontend on that machine); they show that the frontend sustains the refresh at that resolution and nothing
  about headroom. The second host, whose display is not the limiting factor for small scenes, is the one that exposes it.

**Comparison with the classic frontend.** The classic frontend exposes no frame-time counter, so a temporary instrumentation
patch was applied locally and reverted afterwards (the exact diff is recorded in [UI_TEST_ENV.md](UI_TEST_ENV.md) section 7,
which also explains why an unattended classic run is otherwise likely to measure an *empty* scene):

| Frontend | Host, data set, rendered surface | Frame rate |
|----------|----------------------------------|------------|
| Classic QtWidgets (Metal) | first host, 512 atoms, 1386×1156 device px (1.6 Mpx) | 61.0 fps |
| Qt Quick (Metal) | first host, 512 atoms, ≈2000×1600 device px (3.2 Mpx) | 59.0 fps |
| Qt Quick (Metal) | second host, 512 atoms, 1280×800 (1.0 Mpx) | 102 fps |
| Qt Quick (Metal) | second host, 262144 atoms, 1920×940 (1.8 Mpx) | 61.5 fps |

The classic run is bound by the 60 Hz display refresh, so its 61.0 fps is a ceiling and not a cost measurement. What the
comparison does support: at the data set and resolution at which the classic frontend was measured, the Qt Quick frontend
sustains the same refresh rate while rendering twice the pixels, and it exceeds it by a wide margin (102–116 fps) when the
scene is small. A cost-versus-cost comparison would require disabling vsync for the classic path, which OVITO does not
expose; the frame times above are therefore the meaningful absolute numbers, and the classic frontend is kept as a
qualitative reference.

**Input responsiveness (picking).** Measured on Linux/OpenGL unless noted:

| Scenario | Measured latency |
|----------|------------------|
| First pick after window creation (includes creating the render thread and its QRhi) | 99 ms (macOS/Metal) – 150 ms (Linux, software rasterizer) |
| Warm refresh after the buffer was invalidated by a resize | 19–27 ms (bounded by the harness's 10 ms poll interval) |
| Recovery after hide/show (GPU resources released and re-acquired) | 487–522 ms (Linux), 504 ms (macOS/Metal) |
| Probe during scene evaluation | returns immediately (0 ms), no deadlock |
| 262144-atom scene, grid of 25 positions | 25/25 hits, first hit after 25 ms |

### 3.7 Continuous-integration smoke test

`.github/workflows/ci.yml` builds the prototype on all four target platforms and runs it against a generated 512-atom
lattice with `--qml-frame-stats`, `--qml-lifecycle-cycles`, `--qml-hide-show` and `--qml-pick`; the spike exits non-zero
when a check fails, so the step is an assertion instead of a log to grep (the recipes and the CI-specific traps are in
[UI_TEST_ENV.md](UI_TEST_ENV.md) sections 1 and 6). This is the only automated regression net for the frontend, and it
already earned its keep in its first runs:

* **All four jobs failed in the smoke test** while the local runs passed. The checks were right, the reason was not
  visible: the generated data set was handed to the **LAMMPS data importer** because its comment line happened to look
  like a LAMMPS header, which produced an *empty* scene, so there was nothing to pick (see UI_TEST_ENV.md section 1 for
  the one-line reproduction). Neither an import error nor a rendering problem occurred.
* Fixing that exposed defect **F6**: a picking pass that fails leaves no trace at all, so "the picking buffer did not
  become available" was the only message, for a reason that had nothing to do with the buffer. The spike now also prints
  the imported pipelines with their detected format and the viewport state at the moment a check fails.
* The picking checks wait 20 s rather than 5 s: a CI runner creates the render thread's graphics device and compiles its
  pipelines far more slowly than a development machine, so the shorter timeout was measuring the runner, not the code.
* The **checks themselves were wrong on the macOS runner** even after those fixes: the harness probed one fixed position
  `(300,300)`, and the camera fits a scene to the *aspect ratio* of the viewport, so that position missed the scene on the
  runner's 496×307 viewport (19/25 hits on the development machine's 496×380 one) while the synthetic click at the item
  centre selected the lattice in the same run. The check now scans the whole viewport, which in turn needs a scan grid
  *finer than the scene's periodicity*: the first version used a spacing of ~55 px, exactly the particle spacing of a
  fitted 8×8 lattice in a 496 px wide viewport, and reported 0/81 hits on a viewport where a 5 px grid reports 2468/7524.
  Both traps are documented in [UI_TEST_ENV.md](UI_TEST_ENV.md) section 1.1 - the lesson is that a picking check must
  distinguish "picking does not work" from "the probe does not contain an object".

**Windows x86_64.** The Windows job is the reason the smoke test exists, and it paid off differently than expected: it
reached a full build of the tree (1149/1149 targets, including `ovito.exe` and `ovito-qml-spike.exe`) but never got to
the D3D12 smoke test, because the job stopped at the preceding CTest step (five tests aborted with `0xc0000135`,
`STATUS_DLL_NOT_FOUND`, because the step's `PATH` replaced the runner's path instead of extending it - a workflow defect,
not a code defect). Getting that far required the dependency list of section 6 of the testing notes *and* four code fixes
(F7-F10 in section 4), all of them real portability defects that no other platform exposes. CI work was stopped there by
the project maintainer, so the D3D12 runtime verification remains an open gate item rather than a known failure.

Published results of the four jobs: see section 3.5.

---

## 4. Defects Found and Fixed During the Spike

| # | Defect | Fix |
|---|--------|-----|
| F1 | `QuickViewportWindow::renderFrameGraph()` passed `std::move(frameGraph)` and `createConfiguration(*frameGraph)` in the same call; with the implementation's evaluation order the moved-from reference was dereferenced, crashing in `StandardRenderer::createConfiguration()`. | Create the configuration into a local variable first, then move both. |
| F2 | `NewGraphicsSystemService::applicationStarting()` and `UpdateNotificationService::applicationStarting()` dereferenced the result of `dynamic_object_cast<MainWindowUI>()` after an assert only, which crashed during startup for any non-desktop frontend (all plugins, including the desktop GUI plugin, are loaded). | Both services return early when the active `UserInterface` is not a `MainWindowUI`. |
| F3 | The classic frontend logs `VUID-VkApplicationInfo-apiVersion` (value 0) when creating its `QVulkanInstance`. Pre-existing, non-fatal, still open (O2). | Not fixed; recorded in the audit. |
| F4 | The picking coroutine (`QuickViewportWindow::renderPickingBuffer()`) submitted its render-thread work from a Qt event handler, i.e. from a task without a `UserInterface`; with assertions active this aborted in `RenderThread::renderPickingFrame()` (`this_task::ui()`). | Attach the context after the coroutine's first suspension (`setUserInterface()`, `setIsInteractive()`), the pattern `WidgetViewportWindow::grabViewportImage()` already uses. Only found because the assert-enabled configuration exists — see O8 for the general QML-frontend consequence. |
| F5 | After a resize, `QuickViewportWindow::pick()` kept answering from the picking buffer of the **previous** viewport size. `lookupPickResult()` normalizes the pick position with the buffer size it is given, so picks in the resized viewport were silently resolved in the old geometry (found because the harness's probes before and after the picking pass disagreed: 19/25 versus 13/25 at the same position, with different hit atoms). A second, narrower race let a pass be rendered whose frame graph projection came from a different viewport geometry than the buffer. | `pick()` rejects a buffer whose `bufferSize()` differs from the current viewport device size, and `renderPickingBuffer()` drops a pass whose `projectionParams().aspectRatio` does not match the size it would render at. The measured resize recovery time (section 3.4) became a real refresh time because of this; before, it was the instant answer of the stale buffer. |
| F6 | A picking pass that terminated with an **error** was completely silent: the adapter connected only `FutureWatcher::completed`, so a failed pass left no trace and the next `pick()` merely started another attempt. The visible symptom was "picking does not work", with the reason nowhere in the log — which is how the CI smoke test failed on all four platforms at once (section 3.7). | The adapter also connects the watcher's `error` signal, reports the exception once per series of failures (a successful pass clears the flag) and marks the buffer stale so a later pick retries. The spike's checks additionally print the viewport state and the imported datasets when a check fails, because a check that fails without stating under which conditions it ran costs a debugging session. |
| F7 | `src/ovito/core/CMakeLists.txt` deployed the Windows zlib runtime library by globbing conda's names (`zlib.dll`, `zlib1.dll`), but upstream zlib ≥ 1.3.2 names the import library `z.lib` and the DLL `z.dll`; CMake configure aborted with a `FATAL_ERROR` for **any** vcpkg/Conan user. | Derive the DLL name from the import library that `ZLIB::ZLIB` points at (`NAME_WE` of the `.lib` entry, after filtering the `optimized;…;debug;…` list form), and list the DLLs actually present in the error message. |
| F8 | `src/3rdparty/zstd/zlibWrapper/zstd_zlibwrapper.c` does not compile when zlib is a packaged DLL: vcpkg patches `zconf.h` so that `ZEXTERN` becomes `__declspec(dllimport)` unconditionally, and MSVC then rejects the wrapper's definitions of the zlib API (`C2491`). | Define `ZLIB_INTERNAL` for that translation unit only (which turns `ZEXTERN` into `dllexport`); the `gz*.c` sources define it themselves. |
| F9 | `RendererService`'s implicitly declared copy assignment was instantiated by MSVC in every translation unit that includes the header (`C2280`), because the graphics-pipeline cache holds move-only entries (`boost::anys::unique_any`, `std::unique_ptr`). GCC and clang never instantiate it, so it would have surfaced only on Windows. | The service is explicitly non-copyable (deleted copy operations plus an explicit default constructor), which is also the correct semantics for a long-lived GPU-resource owner. |
| F10 | `DataBuffer::copyTo()`/`copyComponentTo()` used a bare `static_assert(false)` inside a discarded `if constexpr` branch, which is ill-formed for Apple clang 15 (legal only from clang 17/GCC 13, via CWG 2518); the macOS build failed in the precompiled header. Pre-existing, but only reachable once the macOS job could configure. | A template-dependent `detail::always_false_v<Iter>` helper. |

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
| Teardown, hide/show, scene graph invalidation | **Verified, with assertions enabled** — `--qml-lifecycle-cycles N` (scene graph release/rebuild), `--qml-hide-show` (items hidden, GPU resources released, then shown again; picking recovers after ~490–520 ms on both hosts) and window teardown at process exit are reproducible in both the release and the `OVITO_DEBUG`/`QT_FORCE_ASSERTS` configuration, where no assertion fires (defect F4 was found this way). A true `Debug` configuration is not possible in this environment (no Qt debug libraries — see section 3.1 and UI_TEST_ENV.md section 4). |
| Deadlock/circular wait elimination | **Done, and structurally enforced** — the interactive QML frame never touches a `RenderThread`; offscreen picking submits work through `RenderThread::renderPickingFrame()` and is answered from a cached buffer, so no GUI-thread wait exists that could interleave with a render-thread wait. The previously planned design (blocking `pick()`, `RenderThread::requestPick()`) was rejected for this reason. |
| Picking and raycasting | **Done** — asynchronous offscreen picking (section 1.5), verified on three Qt Quick backends with hit/no-hit controls, HiDPI/Retina, three render loops, lifecycle cycles, hiding/showing, resizing, a probe during scene evaluation, a 262144-atom scene and an end-to-end synthetic click through `SelectionMode` (sections 3.4–3.6, O3). |
| HiDPI / resize | **Verified for 2× scaling and interactive resize** — `QT_SCALE_FACTOR=2` on Linux and Retina (`devicePixelRatio = 2`) on macOS both work with correct coordinate handling; `--qml-resize WxH` resizes the window at runtime and picking recovers in 19–27 ms once a buffer for the new device size exists (the stale buffer of the previous size is rejected in the meantime, defect F5). Mixed-DPI multi-monitor setups remain unmeasured. |
| Cross-API smoke test (Vulkan, Metal, D3D12) | **Partially verified** — Linux x86_64 with Qt Quick **OpenGL** (llvmpipe) and **Vulkan** (lavapipe ICD), macOS ARM64 with **Metal** (hardware), each with the full check set (section 3.5). Qt Quick Vulkan on the hardware RADV driver is blocked by `Xvfb`'s missing DRI3. On **Windows x86_64 the frontend compiles and links** with MSVC (the CI job builds 1149/1149 targets) but the D3D12/WARP smoke test has not run yet, so that row stays open (section 3.7). |
| Performance baseline vs classic (frame time, input responsiveness, data set sizes) | **Done for the reachable configurations** — frame times for 512/4096/32768/262144 atoms and two render loops (section 3.6), picking latencies for cold/warm/resize/hide-show/during-evaluation, and a same-machine comparison against the classic frontend on macOS/Metal (61.0 fps vs 59.0 fps, vsync-limited, with the Qt Quick frontend rendering twice the pixels). Remaining gap: no vsync-free classic measurement is possible, and the Linux numbers come from software rasterizers. |
| Document required changes to `core/rendering`/`core/viewport` | **Done** — `RendererService` extraction and the offscreen picking entry point, see [UI_PHASE0_AUDIT.md](UI_PHASE0_AUDIT.md) sections 2 and 4. |

Consequently the rendering bridge and the picking path are validated on three Qt Quick backends (OpenGL and Vulkan on
Linux, Metal on macOS), with an assertion-enabled regression run and a measured performance baseline. What keeps the
architecture status at **proposed** is the missing D3D12 runtime evidence (Windows x86_64 is build-verified only) and the
missing `ovitoheadless` QPA plugin for the Linux headless path (O1).

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

1. **Close the last Phase 1 gaps where the environment allows it**: let the Windows CI job reach its D3D12/WARP smoke test
   (the tree already builds there; the job stopped at the preceding CTest step, whose `PATH` handling is fixed but not yet
   re-run) and run Qt Quick Vulkan on a hardware driver, which needs a machine with a real display server or DRI3.
2. **Decide and document the cross-backend validation plan**, including whether the `ovitoheadless` QPA plugin (O1) is
   implemented for Linux headless verification or the Linux gate is moved to a Vulkan-capable desktop session.
3. **Move to Phase 2 only after those gates are recorded**, starting with the frontend-neutral application class and
   `--gui=qml`, and deriving the viewport layout from `ViewportConfiguration::layoutRootCell()`.
