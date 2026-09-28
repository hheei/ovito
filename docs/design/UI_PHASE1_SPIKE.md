# Phase 1 — Qt Quick Viewport Spike Report

> **Companion documents**: [UI_DESIGN.md](UI_DESIGN.md), [UI_PLAN.md](UI_PLAN.md), [UI_PHASE0_AUDIT.md](UI_PHASE0_AUDIT.md)
>
> **Status**: The critical rendering risk is validated — an OVITO scene is rendered inside a Qt Quick window by a
> `QQuickRhiItem` renderer that uses the scene graph's own QRhi, with four simultaneous viewports and no
> `RenderThread`. Cross-backend (Metal/D3D12/Vulkan), HiDPI, picking and performance gates remain open, so the
> architecture is **not yet frozen**.

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
target, command buffer or readback is created by OVITO, and no `RenderThread` exists in the QML viewport path.

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
| Object picking | `pick()` returns `std::nullopt`; selection modes are effectively inert. | Phase 1 remaining gate, then Phase 5 |
| Rendering warnings (gizmo overlay warning icon) | Collected by `reportWarning()` and printed via `qWarning()`; no in-scene indicator. | Phase 4 |
| Renderer failure recovery | No analogue of `ViewportsPanel::fatalViewportWindowError()`. | Phase 2 |
| Context menu / drag & drop | Mouse/key events forwarded to `BaseViewportWindow`, but the context-menu request and drop handling are not wired to QML UI. | Phase 3 |
| Session files, error dialogs, message boxes | `reportError()`/`showMessageBox()` log to stderr and the status line. | Phases 3/7 |
| Viewport layout | Fixed 2×2 grid; not derived from `ViewportConfiguration::layoutRootCell()`. | Phase 2 |
| GPU resource sharing between viewports | One `RendererService` per item ⇒ per-item resource cache and pipeline cache. | Phase 2 (O4) |

---

## 2. Verification Environment

| Item | Value |
|------|-------|
| Host | Linux x86_64, 16-core, AMD Ryzen 9 9950X (integrated RADV GPU), Mesa 25.2.8 |
| Qt | 6.10.2 (`.qt/6.10.2/gcc_64`) |
| Build | `cmake --preset native` (RelWithDebInfo, Clang, mold, ccache), `-DOVITO_BUILD_QML_FRONTEND=ON` |
| QPA / RHI for the spike | `QT_QPA_PLATFORM=xcb` under `xvfb-run`, Qt Quick RHI backend **OpenGL** (Mesa llvmpipe, GL 4.5 compat profile) |
| Data set | `lattice.xyz`: 512 argon atoms on an 8×8×8 simple-cubic lattice, a = 3.6 |
| Evidence | `docs/design/evidence/phase1_multiviewport.png` |

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

### 3.2 Viewport rendering

| Scenario | Command (abridged) | Result |
|----------|--------------------|--------|
| Empty dataset, one viewport | `ovito-qml-spike --qml-capture /tmp/spike.png --qml-capture-delay 3000` | Viewport background plus the "Top" viewport label and the red/green/blue orientation tripod are present ⇒ OVITO's frame graph, including the `OverLayer` (gizmo overlay), reaches the Qt Quick scene |
| Real scene, one viewport | `… --qml-capture /tmp/spike2.png --qml-capture-delay 4000 /tmp/lattice.xyz` | 512 shaded icosahedral particles, correct camera fit (`zoomToSceneExtentsWhenReady()` after import) and correct vertical orientation |
| Four viewports | `… --qml-capture /tmp/spike_4vp.png --qml-capture-delay 8000 /tmp/lattice.xyz` | Top, Front, Left and Perspective panes render simultaneously with independent cameras and tripods; see the evidence image |
| Resource lifecycle | `… --qml-capture /tmp/spike_life.png --qml-capture-delay 5000 --qml-lifecycle-cycles 4 /tmp/lattice.xyz` | Four `QQuickWindow::releaseResources()` cycles (scene graph, item nodes and renderers destroyed and rebuilt) followed by a capture: all four viewport regions are **pixel-identical** to the baseline run; only the command panel's text region differs, and that region also differs between two runs with identical parameters, i.e. it is glyph rasterization noise, not viewport state |

All spike runs exited with code 0; no `qWarning()`/exception messages were emitted by `QuickViewportRenderer` and no
resource-cache assert fired. (Caveat: `OVITO_ASSERT` is only active with `OVITO_DEBUG`, which this build configuration
does not define — see "Remaining gate items" below.)

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

---

## 4. Defects Found and Fixed During the Spike

| # | Defect | Fix |
|---|--------|-----|
| F1 | `QuickViewportWindow::renderFrameGraph()` passed `std::move(frameGraph)` and `createConfiguration(*frameGraph)` in the same call; with the implementation's evaluation order the moved-from reference was dereferenced, crashing in `StandardRenderer::createConfiguration()`. | Create the configuration into a local variable first, then move both. |
| F2 | `NewGraphicsSystemService::applicationStarting()` and `UpdateNotificationService::applicationStarting()` dereferenced the result of `dynamic_object_cast<MainWindowUI>()` after an assert only, which crashed during startup for any non-desktop frontend (all plugins, including the desktop GUI plugin, are loaded). | Both services return early when the active `UserInterface` is not a `MainWindowUI`. |
| F3 | The classic frontend logs `VUID-VkApplicationInfo-apiVersion` (value 0) when creating its `QVulkanInstance`. Pre-existing, non-fatal, still open (O2). | Not fixed; recorded in the audit. |

---

## 5. Remaining Phase 1 Exit-Gate Items

| Gate item | Status |
|-----------|--------|
| Minimal executable renders real OVITO scene data through the candidate architecture | **Done** |
| Composition, QRhi ownership, frame handoff documented | **Done** |
| Multi-viewport (4 viewports, shared GPU work) | **Done** (independent renderer per item; cross-item buffer sharing is O4) |
| Teardown, hide/show, scene graph invalidation | **Partially verified** — scene graph release/rebuild cycles are reproducible; hide/show and window destruction were exercised without failure, but with `OVITO_ASSERT` compiled out and without a Debug build. A Debug-configuration run is required before claiming this gate. |
| Deadlock/circular wait elimination | **Done for the QML path** — the QML viewport does not hold a `RenderThread`, so neither `requestPick()` nor window release can deadlock; picking itself is unimplemented (O3). |
| Picking and raycasting | **Not started** (O3) |
| HiDPI / resize | **Not verified** — resizing is forwarded (`geometryChange` → `handleResize` → `requestRerender`) but subpixel/4K behavior is untested. |
| Cross-API smoke test (Vulkan, Metal, D3D12) | **Not verified** — only Qt Quick's OpenGL backend on Linux x86_64 was exercised. macOS ARM64 and Windows x86_64 remain completely unverified; the Linux Vulkan path is blocked by the missing QPA plugin (O1). |
| Performance baseline vs classic (frame time, input responsiveness, data set sizes) | **Not measured.** |
| Document required changes to `core/rendering`/`core/viewport` | **Done** — `RendererService` extraction, see [UI_PHASE0_AUDIT.md](UI_PHASE0_AUDIT.md) section 2. |

Consequently the design's architecture status remains **proposed**, now with the rendering bridge validated on one
backend instead of unvalidated.

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

## 7. Recommended Next Steps

1. **Close the cheap Phase 1 gates first**: implement picking (render a picking pass into the item's render target and
   read back the object ID), run a `Debug` (assert-enabled) build through the lifecycle cycles, and check HiDPI/resize.
2. **Decide and document the cross-backend validation plan**, including whether the `ovitoheadless` QPA plugin (O1) is
   implemented for Linux headless verification or the Linux gate is moved to a Vulkan-capable desktop session.
3. **Move to Phase 2 only after those gates are recorded**, starting with the frontend-neutral application class and
   `--gui=qml`, and deriving the viewport layout from `ViewportConfiguration::layoutRootCell()`.
