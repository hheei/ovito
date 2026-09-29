# OVITO Modern Workbench UI Implementation Plan

> **Scope**: Spike-driven, phased migration to modern Qt Quick / QML frontend
>
> **Guiding Principle**: Validate high risks first, interaction parity before redesign
>
> **Status**: Proposed Roadmap; Phase 0 audit (partially), Phase 1 rendering spike and Phase 2 deliverables 1–7
> executed — the frontend selection (`--gui=qml`), the shared `gui/base` workbench base class, the layout-derived
> workbench shell, the import path with its empty/busy/cancelling/cancelled/error states and the shared rendering/
> picking core are in the tree and verified on Linux/OpenGL, Linux/Vulkan, macOS/Metal and Windows/D3D12, see
> [UI_PHASE0_AUDIT.md](UI_PHASE0_AUDIT.md) and [UI_PHASE1_SPIKE.md](UI_PHASE1_SPIKE.md).
> The comparison against the classic frontend, the duplication it identifies and the abstractions it proposes are
> collected in [UI_FRONTEND_REVIEW.md](UI_FRONTEND_REVIEW.md); of those, the command layer (A1, audit decision D26),
> the workbench state models (A5: D27 the recent files list and the shared task progress model, D28 the session
> workflow) and the shared viewport renderer service (A2, D29) are implemented. The remaining review items are A4
> (one offscreen rendering service for render output, AO sampling and picking buffers), A3 (one asynchronous pick API
> used by both frontends) and the small UX gaps, followed by the rest of A5 (the Qt Quick session commands, a selection
> model and a settings facade) **before** the pipeline and inspector work of Phases 4 and 6.
>
> Phase 2 is complete and its exit gate is verified, so the review items above plus the small parity gaps are collected in
> the new **Phase 2.5**. It is deliberately *not* a feature phase: A4, A8 and A9, the finish of the Phase 0 inventory and
> the four small gaps land there, while A3, A6 and A7 are explicitly assigned to Phases 5, 7 and 4 (they change semantics
> that those phases are already touching).
>
> **Design Contract**: [UI_DESIGN.md](UI_DESIGN.md)

---

## 1. Core Engineering Principles

1. **Validate High-Risk Integration Points Before Freezing Interfaces (Spike-Driven)**:
   - Do not commit to unproven assumptions. The greatest technical risk is the 3D viewport rendering bridge (`QQuickRhiItem` vs. `RenderThread` QRhi ownership). This must be rigorously evaluated in an isolated technical spike before locking production architecture.
2. **Strict Architectural Seam (No Desktop Entanglement)**:
   - Dependency rule: `gui/qml -> gui/base -> core`.
   - **`gui/qml` must never depend on `gui/desktop`**. If a feature or logic currently resides in `gui/desktop`, it must be refactored into `gui/base` as a shared service before QML consumes it.
3. **Preserve Computational Behavior**:
   - Reuse numerical pipelines, asynchronous evaluation (`TaskScope`), file readers, and mathematical abstractions. Identify any required `core/rendering` or `core/viewport` integration changes in Phase 1 and verify classic and headless behavior after each change.
4. **Interaction & Feature Parity Before Workflow Redesign**:
   - First priority is 1:1 behavioral equivalence with classic OVITO (modifier stack ergonomics, viewport navigation, timeline behavior).
   - VS Code aesthetic is an inspiration for visual clarity, not an excuse to reinvent user workflow prematurely.
5. **Audit Shared Models Before Adding Adapters**:
   - Evaluate `gui/base` models and actions for direct reuse, composition/proxy adaptation, or extraction of shared operations. New QML adapters belong in `src/ovito/gui/qml/models/`; their existence does not require reimplementing pipeline mutations or modifier discovery.
6. **Project Qt and Platform Baseline**:
   - Use Qt 6.10+ and matching private headers, following root CMake and `AGENTS.md`. Validate Metal on macOS ARM64, D3D12 on Windows x86_64, and Vulkan on Linux x86_64/ARM64. Use CMake presets for configuration, builds, and tests.

---

## 2. Component Migration Matrix (Phase 0 Audit)

| Classic QtWidgets Component | Modern QML Target | Strategy | Migration Seam |
| :--- | :--- | :--- | :--- |
| `MainWindow` (`gui/desktop`) | `WorkbenchWindow.qml` | **REWRITE** | Wraps `ViewportGrid` + right command panel in QML. |
| `ViewportsPanel` (`gui/desktop`) | `ViewportGrid.qml` | **REWRITE** | Preserves audited split/resize/maximize behavior; initial layouts are 1x1, 2x2, and 1+2. |
| `WidgetViewportWindow` (`gui/vpwindow`) | `QuickViewportItem` + renderer | **ADAPT** | `QQuickRhiItem` owns a viewport adapter; `QQuickRhiItemRenderer` handles rendering callbacks. |
| `BaseViewportWindow` (`gui/base`) | `QuickViewportAdapter` | **REUSE VIA COMPOSITION** | Adapter subclasses the shared base and implements its viewport contract. No dual `QObject` inheritance. |
| Pipeline Command Page (`gui/desktop`) | `PipelineView.qml` | **REWRITE VIEW** | Audit `gui/base/PipelineListModel` for binding/adaptation and reuse its editing operations. |
| `AvailableModifiersModel` and actions (`gui/base`) | Modifier chooser and command bridge | **REUSE / ADAPT** | Preserve applicability, templates, enablement, shortcuts, and transactions. |
| `PropertiesEditor` & `ParameterUI` | `ModifierEditorRegistry` | **ADAPT** | Specialized QML editors + generic reflected fallback. |
| `AnimationTrackBar` (`gui/desktop`) | `TimelineView.qml` | **REWRITE** | Connected via new `QmlAnimationModel`. |
| Data Inspector (`gui/desktop`) | `DataInspectorView.qml` | **DEFER** | Targeted for Phase 7 once core parity is achieved. |
| Render settings and output UI | `RenderSettingsView.qml` and output view | **DEFER** | Phase 7 includes settings, rendering, progress, cancellation, and saved output. |
| Session lifecycle (`gui/desktop/MainWindowUI`) | QML session actions and dialogs | **EXTRACT / ADAPT** | Basic import in Phase 2; save/load and unsaved-change handling in Phase 3; remaining file workflows in Phase 7. |

### 2.1 Initial Parity and Acceptance Matrix

Phase 0 expands these rows against the classic frontend for the same build configuration. Record the classic source/action, target behavior, delivery phase, verification case and dataset, and status (`pending`, `passed`, or `gap`). Initially every row is `pending`. Record platform results where relevant. Any deferred applicable feature remains a gap for the complete-parity gate.

| Workflow | Delivery phase | Minimum acceptance evidence |
| :--- | :--- | :--- |
| Viewport rendering and lifecycle | 1, integrated in 2 | Four viewports, picking, resize/HiDPI, hide/show, invalidation/recreation, and teardown pass on target backends. |
| Launch, basic import, and task states | 2 | `ovito`, `ovito --nogui` and `ovito --gui=qml` all start correctly (and a `--gui` name that is not built fails loudly); a real multi-frame trajectory loads through the QML import path *and* from the command line; busy/progress/cancelled/error states are visible and responsive, and a cancelled import leaves no partial data set. |
| Session save/load and close | 3 | Save/reopen preserves scene state; modified-session close offers save/discard/cancel without losing changes on cancellation or save failure. |
| Pipeline edits and selection | 3–4 | Insert/toggle/reorder/delete, group/shared-object behavior, and selection updates agree with classic; invalid drops leave the pipeline unchanged. |
| Parameter edits and undo/redo | 3–4, specialized in 6 | One gesture is one undo step; cancellation restores values; units, bounds, controllers, and read-only state are respected. |
| Navigation and animation | 5 | Input modes, projection, layouts, playback range/speed/loop, parameter animation, and keyframe editing agree with classic. |
| Editor coverage | 4, 6–7 | Every audited user-editable field/action is supported or recorded as a gap; initial priority modifiers pass representative workflows. |
| Rendering settings and output | 7 | Configure renderer/output, render stills and animation, cancel, and save output using available backend capabilities. |
| Remaining file workflows, overlays, inspectors, utilities | 7 | Export/options and audited scene, overlay, data-inspection, and utility workflows pass with appropriate fixtures. |
| Keyboard, accessibility, and layout | 2–7 | Native shortcuts, focus order, accessible control values, error states, both themes, and minimum-size/HiDPI layouts pass in each delivered area. |

This is a seed list, not a declaration that the listed examples exhaust the desktop feature set. Phase 0 assigns all additional audited features to a phase through Phase 7 before claiming a complete migration scope.

---

## 3. Phased Execution Roadmap

```
Phase 0: Repository & Architecture Audit
   │
   ▼
Phase 1: Viewport Technical Spike (QQuickRhiItem Feasibility)
   │
   ▼
Phase 2: Minimal QML Shell & Build Scaffolding
   │
   ▼
Phase 2.5: Shared-Layer Cleanup & Small Parity Gaps
   │
   ▼
Phase 3: Presentation Models & Command Layer
   │
   ▼
Phase 4: Pipeline Stack & Basic Property Inspector
   │
   ▼
Phase 5: Animation Timeline & Viewport Interaction Parity
   │
   ▼
Phase 6: Specialized Property Editors (High-Frequency Modifiers)
   │
   ▼
Phase 7: Remaining Desktop Feature Parity & Regression Testing
   │
   ▼
Phase 8: UX Modernization (Command Palette, Activity Bar)
   │
   ▼
Phase 9: Classic Frontend Retirement (Long-term, optional)
```

---

### Phase 0: Repository & Architecture Audit
- **Objective**: Establish exact component boundaries and identify code that must be promoted to `gui/base`.
- **Deliverables**:
  1. Full mapping of all action handlers, viewport gizmos, and modifier UI entry points.
  2. Audit `gui/base` models/actions and `gui/desktop` services. Record reuse, adapter, and extraction decisions, including application startup and session lifecycle.
  3. Expand the parity matrix with source references, acceptance cases, fixtures, and delivery phases. Inventory all applicable desktop features, not just the initial modifier set.
  4. Audit fields and actions for Slice, CNA, Color Coding, and Polyhedral Template Matching. Classify fallback support, specialized editor requirements, and remaining gaps.
- **Exit Gate**: Each audited workflow has an implementation route and verification case; the initial editor coverage list and model reuse decisions are recorded. Unresolved rendering questions are explicitly assigned to Phase 1.
- **Status (partial)**: The build entry, viewport/rendering interface (QRhi ownership) and shared model/command audits are
  complete and recorded in [UI_PHASE0_AUDIT.md](UI_PHASE0_AUDIT.md), including the decisions implemented in code
  (`RendererService` extraction, `OVITO_BUILD_QML_FRONTEND`, `GuiBase` reuse, non-desktop `UserInterface` guards in two
  desktop services). The action/editor inventory of deliverable 1 is recorded in [UI_PHASE0_AUDIT.md](UI_PHASE0_AUDIT.md)
  section 6 (the 76 action ids grouped by who creates and handles them, the 84 property editors with their 26 control
  classes, the four audited specialized editors, and the panels, dialogs, applets and gizmos), and the expanded parity
  matrix of deliverables 3 and 4 is [UI_PARITY_MATRIX.md](UI_PARITY_MATRIX.md). Both were written in Phase 2.5, which owns
  them as its deliverable 1.

---

### Phase 1: Viewport Technical Spike (Critical Risk First)
- **Objective**: Prove the feasibility of rendering OVITO scenes inside `QQuickRhiItem` without UI freezes, crashes, or deadlocks.
- **Key Investigations**:
  1. **Composition and QRhi Ownership**: Compile a `QQuickRhiItem` owning a `BaseViewportWindow` adapter, with a separate `QQuickRhiItemRenderer`. Determine how OVITO's `FrameGraph` and `SceneRenderer` can use Qt Quick's target and command buffer, and record necessary integration changes.
  2. **Teardown & Deadlock Validation**: Test repeated window closing, scene-graph invalidation/recreation, and hide/show cycles. Check picking during teardown and eliminate circular GUI/render-thread waits.
  3. **Multi-Viewport Concurrency**: Verify that 4 simultaneous viewports (Top, Front, Right, Perspective) render reliably with shared geometry buffers.
  4. **Picking & Interaction**: Verify hardware/software object picking and raycasting against the texture-backed viewport.
  5. **HiDPI & Resize**: Verify crisp subpixel scaling on 4K/Retina displays without framebuffer lag.
  6. **Cross-API Smoke Test**: Validate Linux x86_64/ARM64 (Vulkan), macOS ARM64 (Metal), and Windows x86_64 (D3D12); record unavailable environments as unverified.
  7. **Frame Handoff**: Document prepared-state ownership, synchronization, resource release, and picking-result delivery. Evaluate representative static and animated particle scenes, recording dataset size, frame time, and input responsiveness against classic.
- **Exit Gate**: A minimal executable renders real OVITO scene data through the candidate architecture, and the listed lifecycle, input, and backend checks have recorded results. A visible viewport alone is insufficient. Unverified targets leave the cross-platform gate open. If `QQuickRhiItem` integration fails, revise the design with evidence for an alternative covering all target backends before production integration.
- **Status (rendering bridge, picking and performance validated on three backends)**: `src/ovito/gui/qml` implements
  the `QQuickRhiItem` + `BaseViewportWindow` adapter + `QQuickRhiItemRenderer`/`RendererService` composition and renders
  real particle data, four simultaneous viewports, repeated scene-graph resource rebuild cycles, hide/show and runtime
  resize without a `RenderThread` in the interactive frame. Object picking is implemented as an asynchronous offscreen pass
  on OVITO's `RenderThread` and verified end to end (hit/no-hit controls, 2× HiDPI and Retina, three Qt Quick render loops,
  lifecycle cycles, a 262144-atom scene, and selection through the unmodified `SelectionMode`), including a run with
  assertions enabled. The frontend was exercised with Qt Quick **OpenGL** and **Vulkan** on Linux x86_64 and with
  **Metal** on macOS ARM64, with **Direct3D 12** (hardware and WARP) and **Vulkan** on Windows x86_64, and its frame times
  (512–262144 atoms) were compared against the classic frontend on macOS. Results, evidence and the exact reproduction
  commands are recorded in [UI_PHASE1_SPIKE.md](UI_PHASE1_SPIKE.md); the environment recipes and testing traps are
  collected in [UI_TEST_ENV.md](UI_TEST_ENV.md). Still open: Qt Quick Vulkan on a hardware RADV driver under Linux (Xvfb
  lacks DRI3) and mixed-DPI multi-monitor setups. The Windows round added two fixes to shared code — F13 (an unspecified
  argument evaluation order that MSVC resolved differently and that crashed every import) and F14 (the frontend now
  selects Direct3D 12 itself instead of failing on Qt Quick's D3D11 default, whose shader model OVITO does not bake).

---

### Phase 2: Frontend Selection, Workbench Shell & Import Path
- **Objective**: Turn the Phase 1 prototype into a selectable, self-contained frontend: `ovito --gui=qml` starts the QML
  workbench, a real trajectory can be imported through it, and nothing in the QML module depends on `gui/desktop`.
- **Carried in from Phase 1** (already in the tree, listed so the phase is not planned twice):
  1. The CMake option `OVITO_BUILD_QML_FRONTEND` (default `OFF`) and the `src/ovito/gui/qml` module (`GuiQml`, a standard
     plugin that depends on `GuiBase` only).
  2. The viewport path — `QuickViewportItem` + `QuickViewportWindow` (a `BaseViewportWindow`) + `QuickViewportRenderer`
     (a `QQuickRhiItemRenderer` and `RendererService`) — including asynchronous picking, the verification harness
     (`OvitoQmlSpike`) and the CI smoke test on four platforms.
  3. A prototype shell: `QmlMainWindowUI`, `QmlViewportController`, `WorkbenchWindow.qml`, `Theme.qml`, loaded from a
     hand-written `.qrc`, plus a spike-only application class (`QmlFrontendApplication` in `spike/Main.cpp`) that this
     phase replaces with the real entry point.
- **Deliverables**:
  1. **Frontend selection seam (`--gui=<name>`).** Add a frontend registry to `gui/base` that maps a name to the code that
     builds a workbench, and let `GuiApplication` look up the selected frontend instead of constructing `MainWindowUI`
     directly (the current hard-coded construction in `startupApplication()`). The classic frontend registers
     `qt-widgets` and stays the default, so `ovito` behaves exactly as before; the QML module registers `qml` from its own
     `ApplicationService::applicationInitializing()`, which is the documented plugin hook for exactly this (the class
     registry instantiates services after plugin loading and before `startupApplication()`). `--gui` is registered by
     `GuiApplication::registerCommandLineParameters()`; an unknown name, or `--gui=qml` in a build without
     `OVITO_BUILD_QML_FRONTEND`, prints the available frontends and exits non-zero. A silent fallback to the classic UI is
     forbidden: it would let an automated run pass while testing the wrong frontend.
  2. **Remove the remaining desktop coupling from application startup.** `GuiApplication::initializeUserInterface()` is
     already written against `UserInterface&` for session loading, but reaches `MainWindowUI::importFiles()` and
     `openWorkingDirectory()` through two `dynamic_object_cast<MainWindowUI>` sites. Introduce a small shared interface in
     `gui/base` for command-line file/directory import and for status/error/progress reporting, implement it in both
     workbenches, and keep everything widget-specific (`FileImporterEditor`, the import-mode dialog, recent-directory
     lists, save dialogs) in `gui/desktop`.
  3. **Extract the widget-free part of `MainWindowUI` into `gui/base`** — import orchestration over
     `FileImporter`/`FileImporterClass`, status-bar message plumbing, error reporting, task-progress bookkeeping,
     `checkLoadedDataset`, auto-key mode — so both frontends share one implementation and differ only in presentation.
     Session loading at startup (`.ovito` argument, `defaults.ovito`, empty fallback dataset) already lives in the
     frontend-neutral part of `GuiApplication::initializeUserInterface()` and must keep working unchanged; session
     *saving*, modified-close handling and QML dialogs remain Phase 3.
  4. **Workbench shell.** `WorkbenchWindow.qml` reproducing the classic spatial layout: a viewport area built
     **recursively from `ViewportConfiguration::layoutRootCell()`** (the node's `splitDirection` and `childWeights` drive
     draggable splitters that write back through the undoable transaction path, and `maximizedViewport` drives maximize),
     the right-hand command panel (placeholders here, populated in Phases 3–4), a status bar fed by
     `showStatusBarMessage()`/`clearStatusBarMessage()`, a window title derived from the data set or session file, an
     initial and a minimum window size, a documented keyboard focus order and accessible names for the shell controls as
     specified in design section 3.2, and both themes with the system color scheme followed where the platform provides
     one. This closes decision D14, which Phase 1 explicitly deferred (cell-derived pane placement, maximize state).
  5. **Import path with real states.** A QML import entry point (file dialog plus drag & drop on the window) that drives
     the shared import service, with empty, busy, cancelling, cancelled and error states wired through
     `taskProgressBegin()/taskProgressChanged()/taskProgressEnd()`, `reportError()` and `showMessageBox()`, correct
     behavior for an unsupported or misdetected format, and the command-line import path using the *same* code. A
     cancelled import must leave no half-imported data set behind, and the UI must stay responsive while a large file
     imports (this is the Phase 2 slice of the "launch, basic import, and task states" row of the parity matrix).
     **Known limitation, identical in the classic frontend**: a `ResetScene` import deletes the scene's existing objects
     as its first step (`FileSourceImporter::importFileSet()`), so cancelling an import that already began cannot restore
     the previous scene content; what cancellation guarantees is that the objects the cancelled import created are removed
     again (D20) and that the workbench reports the cancellation. A rollback of the deleted content would need a different
     import transaction in the core and is not part of this plan.
  6. **Resource and packaging flow.** Move from the prototype's hand-written `qml.qrc` to the project's normal resource
     registration, keeping the deliberate decision *not* to use `qt_add_qml_module` (it conflicts with the
     `OVITO_STANDARD_PLUGIN` target and library naming recorded in the Phase 1 report), and verify that QML resources load
     from the deployed layout — the macOS bundle and the Windows plugin directory, not only from the build tree. With
     `OVITO_BUILD_QML_FRONTEND=OFF` the build must contain no QML sources and no QML-linked target.
  7. **Phase 1 leftovers that a real shell needs** (each already recorded in [UI_PHASE0_AUDIT.md](UI_PHASE0_AUDIT.md)):
     ~~**O4** one `RendererService` per window instead of one per viewport item~~ **done** — the items of a window share
     the service (and with it the pipeline and resource caches) and the measurement below confirms the win; ~~**O7/O11**
     a shared core `PickingBufferTarget` and one shared frame-graph render-pass helper over `RendererService*`~~
     **done/resolved** — the pass sequence now lives in `FrameGraphRenderPass` (decision D24), and the duplicated picking
     target disappeared when the Qt Quick frontend moved its picking to `RenderThread::renderPickingFrame()` in Phase 1;
     ~~**O8** a helper that wraps QML-facing calls in a `Task::Scope` bound to the `UserInterface`~~ **done**
     (`GuiTaskScope`); ~~**D14**~~ as described above. **O1** (the `ovitoheadless` QPA plugin does not exist in this tree, which
     is why headless Linux verification needs `xvfb` plus `QT_QPA_PLATFORM=xcb`) and **O2** (the Vulkan
     `VUID-VkApplicationInfo-apiVersion` validation error in `RenderThread`) must be either fixed or explicitly documented
     as constraints in this phase — an undocumented environment requirement is a support burden.
  8. **Verification harness for the shell.** `OvitoQmlSpike` stays the viewport and picking regression net (CI, four
     platforms). The shell gets a smoke check that needs no product-side test API: launch `ovito --gui=qml <file>` under
     `xvfb` (Linux) or the runner's own session (macOS/Windows) and require the process to *still be running* after N
     seconds — a crash or a failed startup exits differently, so a "stays up" assertion catches startup regressions
     without inventing a test-only command line option. macOS has no `timeout`, so that check uses a background process
     plus `sleep`/`kill` and closes the window afterwards (testing rules in [UI_TEST_ENV.md](UI_TEST_ENV.md)).
- **Status (deliverables 1-7 implemented)**: `ovito --gui=qml` starts the Qt Quick workbench, renders a trajectory
  imported from the command line, and plain `ovito` still starts the classic main window. Deliverable 1-3 are the
  frontend registry of decision **D15**, the shared workbench base class of decision **D16** and the shell's own state
  and dialog objects (**D19**, **D22**) in [UI_PHASE0_AUDIT.md](UI_PHASE0_AUDIT.md). Deliverable 4 lays the viewport
  pans out from `ViewportConfiguration::layoutRootCell()` with draggable, undoable handles and maximizing (**D17**).
  Deliverable 5 (the import path) offers empty, busy, cancelling, cancelled and error states: a file dialog
  (`Ctrl+O`), drag & drop onto the window, a status bar with the running operations' progress and a Cancel command, an
  error dialog for a file whose format cannot be detected, and the cleanup that removes a half-imported pipeline after a
  cancellation (**D20**). Deliverable 6 (resources and packaging) is verified by building with
  `OVITO_BUILD_QML_FRONTEND=OFF`, which contains neither QML sources nor a QML target. Deliverable 7 closed the
  carried-over Phase 1 items: the window's viewport items share one renderer service (**D23**, measured below), the
  frame-graph pass sequence exists once (**D24**), the picking target no longer exists twice (**O7**) and **O2** (explicit
  Vulkan API version) and **O1** (documented as an environment constraint) are settled. **Open from this phase**: the
  exit-gate verification on Windows x86_64, which is done now (D3D12 on hardware and through WARP, plus Qt Quick Vulkan,
  each with the full check set and zero failed checks; defects F13 and F14 came out of it). The spike verifies the shell
  with `--qml-layout-check` and `--qml-import-check` in CI; the testing recipe is [UI_TEST_ENV.md](UI_TEST_ENV.md).
  Verified so far: Linux/OpenGL (all checks, including the four viewports, the splitter drag and its undo, picking, the
  import path and the cancellation), macOS/Metal (the same check set, 126 fps with four viewports at 1280x800) and
  Windows/D3D12 (the same check set on a real GPU, 60 fps vsync-limited); macOS screenshots need the screen-recording
  permission, so the shell evidence images are the Linux and Windows ones.

- **Deliverable 7 measurement (frame time of the four viewports, 1280×800, headless, `QSG_NO_VSYNC=1`, medians of three
  runs)**: one service per viewport item versus one per window, Linux/OpenGL/llvmpipe:

  | scene | render loop | per item | per window |
  |---|---|---|---|
  | 512 atoms | threaded | 64.0 fps (15.6 ms) | **114.0 fps (8.8 ms)** |
  | 512 atoms | basic | 118.0 fps (8.5 ms) | **175.5 fps (5.7 ms)** |
  | 32768 atoms | threaded | 19.5 fps (51.3 ms) | **22.5 fps (44.4 ms)** |
  | 32768 atoms | basic | 38.5 fps (26.0 ms) | **44.5 fps (22.5 ms)** |

  Sharing the caches removes most of the fixed per-frame cost of four viewports (each item used to prepare and upload
  the same vertex data, and to compile every pipeline, of its own accord), which is exactly the overhead that the frame
  time of an empty-ish scene is made of.

- **Non-goals of this phase** (so the shell does not swallow the later ones): pipeline and property models, the pipeline
  view and editors, timeline and animation, render settings and output, data inspector, session saving, command palette.
- **Exit Gate**: With `OVITO_BUILD_QML_FRONTEND=ON`, `ovito --gui=qml` starts the QML workbench and loads a real
  trajectory rendered in its viewports, while plain `ovito` and `ovito --nogui` behave as before and
  `OVITO_BUILD_QML_FRONTEND=OFF` still builds classic and headless. Verify: window and viewport layout derived from the
  session's layout cell (including maximize and a dragged splitter that undoes), import of a real multi-frame trajectory
  including failure and cancellation, both themes, resizing and minimum size, keyboard focus order, and QML resource
  loading from the layout the frontend is actually deployed in. Of those, focus order is verified only in part: the shell
  gives the Import Data button the first focus stop and `--qml-parity-check` walks it, while the full Tab order and the
  accessibility names are not automated (the parity matrix carries the row with that status). Resource loading is verified
  way instead of as done: the macOS bundle layout passes (the spike resolves `GuiQml` through
  `@executable_path/../PlugIns/` after the O9 rpath fix) and so does the Windows one-directory build-tree layout
  (`ovito.exe`, `ovito-qml-spike.exe` and every `*.ovito.dll` next to each other, found through `QT_PLUGIN_PATH` and
  `QML_IMPORT_PATH`), while no **installed** tree has been run on any platform even though CMake's build-tree and
  install-tree plugin paths differ. Closing that gap is part of Phase 2.5 deliverable 1. The first two are automated in the spike harness (`--qml-layout-check`,
  `--qml-import-check`); the remaining ones are checked by running the frontend in its themes and window sizes (see
  [UI_TEST_ENV.md](UI_TEST_ENV.md)). Record the platform results (Linux, macOS, Windows) in the parity matrix and the
  environment notes. The D3D12 runtime evidence that Phase 1 left open is **recorded**, not assumed: the full check set
  passes with Qt Quick Direct3D 12 on hardware and through WARP and with Qt Quick Vulkan on Windows x86_64, next to
  Linux OpenGL/Vulkan and macOS Metal (section 3.5 of [UI_PHASE1_SPIKE.md](UI_PHASE1_SPIKE.md)), so this gate is closed.
  The one backend still unverified is Qt Quick Vulkan on a hardware RADV driver under Linux, which `Xvfb`'s missing DRI3
  blocks on this test host; that stays an environment limitation, not a Phase 2 deliverable.
- **Risks**:
  * Two workbench implementations can drift apart. Mitigation: the shared `gui/base` extraction of deliverables 2–3 is
    mandatory before the QML shell grows, and the classic frontend must keep passing the existing checks after each step.
  * Frontend selection can break console/headless modes, session loading or `--noviewports`. Mitigation: the selection
    happens in `startupApplication()` only, and the exit gate re-checks all launch modes.
  * Qt Quick Controls styling differs from the classic Fusion look. Decision needed in this phase: use one
    `QtQuick.Controls` style plus the QML `Theme.qml` palette (recommended, matches the design section on themes) and
    follow the platform color scheme, rather than trying to imitate the widget style per platform.
  * Packaging QML resources for three platforms has already produced one gap (the macOS `@rpath` issue, O9). Mitigation:
    verify from the deployed layout in this phase, not only from the build tree.

### Phase 2.5: Shared-Layer Cleanup & Small Parity Gaps
- **Objective**: Finish the work that the frontend review ([UI_FRONTEND_REVIEW.md](UI_FRONTEND_REVIEW.md)) assigns to the
  *shared* layers, and close the small parity gaps, **before** the presentation models of Phase 3 bind to the shell. These
  are the items that are cheap while the relevant code is being touched and expensive later, when both frontends have
  grown their own version of them.
- **Why this is its own phase**: every later phase adds *features*; this one removes *differences*. Keeping it separate
  keeps the regression surface of a change identifiable — the Windows round showed how much time a single shared-code
  change can cost when it hides inside unrelated work (defect F13), and the classic frontend has to survive every one of
  these changes.
- **Deliverables**:
  1. **Finish the Phase 0 inventory** — **delivered** as
     [UI_PARITY_MATRIX.md](UI_PARITY_MATRIX.md) (the normative per-capability list: state, phase and acceptance case, in 16
     areas, with the fixture catalogue and the remaining verification gaps) plus the action/editor inventory in
     [UI_PHASE0_AUDIT.md](UI_PHASE0_AUDIT.md) section 6. This is the last phase that works on the *difference* between the
     two frontends rather than on features, and the matrix is what tells Phases 3–7 what is left: it is why the phase's
     later deliverables no longer have to argue about scope. What the inventory settled: 43 of the 76 commands reach QML
     and 23 of them act (the other 20 are desktop dialogs or services and are the concrete handler list), the classic
     frontend has **no reflection fallback** for property fields (so A7's scope is the 84 audited editors, not "all
     fields"), Phase 6 needs six editor archetypes rather than 84 ports, and two "gaps" are not gaps at all (the scripting
     commands and the SSH terminal have no implementation in this tree). One real defect surfaced on the way and is fixed
     here (F17: three `getAction()` lookups of never-registered gallery ids trip an assertion in an assert-enabled build).
     The row set includes what Phase 2 could not verify:
     * **Installed layout**: build-tree paths differ from installed paths, so the check was run here on Linux
       (`cmake --install build-native --prefix /tmp/ovito-install`, then both product frontends from that prefix): the
       installed Qt Quick frontend renders the scene and the installed classic frontend loads its 26 plugins and creates its
       viewport windows (recipe and measurements in [UI_TEST_ENV.md](UI_TEST_ENV.md) §2.3). What stays open is a *packaged*
       tree on macOS/Windows; the spike has no install rule because it is a prototype. Running the check also corrected a
       testing recipe that had been recorded as working without ever having worked (the `[viewport]` section of the
       `QSettings` file, §3.2 of the same document).
     * **Windows packaging prerequisites**: a Windows redistributable build needs Boost, a zlib-enabled HDF5 and Perl
       before it configures at all (defect F16), so the matrix names the prerequisites instead of implying the build just
       works.
  2. **Settings facade (the remainder of review item A5)**: one small `gui/base` service that owns the names and defaults
     of the keys the frontends persist (window geometry, renderer selection is already in `ViewportRendererRegistry`,
     per-dialog directory history, UI theme), so neither frontend names storage paths itself. This is deliverable 2 and
     not deliverable 3 on purpose: the window state of deliverable 3 persists through it. The *session* UI stays in
     Phase 3 deliverable 6, and the selection/hover model in Phase 4. **Delivered** as `gui/base/app/GuiSettings` (audit
     decision D30): the facade owns the color-scheme policy, the window state of both kinds of frontend, the file-dialog
     behavior and the first-start flags, the QML shell reads its palette from it through the `guiSettings` context
     property, and `--qml-settings-check` verifies both the reachability from QML and the round trip of every value. One
     visible consequence is worth knowing while reading the screenshots of this document: the shell now renders *light*
     where the platform reports no color scheme (Xvfb), which is what the classic frontend does there.
  3. **The small parity gaps of review section 5** — each one user-visible and cheap. The rule for all of them: an item
     that does not yet have a handler is shipped as a **disabled placeholder whose text names the owning phase**, never
     as an enabled entry that silently does nothing (a QML menu item is not an item and cannot carry a tooltip).
     **Delivered** (audit decision D31): the workbench has an in-window menu bar bound to the shared commands and a
     viewport context menu driven by the new `QmlViewportMenu`; the status line lists one row per running task; the window
     size, position and maximized state are remembered through `GuiSettings`; and the last import is reported by a
     persistent `notice` naming the detected format and the number of source frames (the property was named `importNotice`
     while only imports reported through it; it is the workbench's one place for a report that outlives the status line). `--qml-parity-check` verifies all
     five and runs in the four CI jobs. Two deviations from the classic frontend are deliberate and documented in D31: the
     menu bar is drawn in the window (the native macOS menu bar would require QtWidgets, which this frontend does not
     link) and the context menu offers Show Grid unconditionally (the classic entry exists only in an `OVITO_DEBUG` build).
     * **Viewport context menu** (currently a no-op): the classic menu is
       [ViewportMenu.cpp](../../src/ovito/gui/desktop/viewport/ViewportMenu.cpp). Drive it from the *same* shared commands
       and the viewport API, and split it by what exists today:
       * *Delivered here*: **Show Grid** (`Viewport::setGridVisible`), **Constrain Rotation**, the **View Type** submenu
         (Top/Bottom/Front/Back/Left/Right/Ortho/Perspective, all pure `Viewport` camera API) and **Maximize/Restore**
         (the shared `ACTION_VIEWPORT_MAXIMIZE` command). Each gets a check in the spike harness.
       * *Disabled placeholders naming their phase*: **Adjust View…** and **Preview Mode** (Phase 5, where the render
         preview and the camera form arrive), **Window Layout** (Phase 4, the insert/delete-viewport work, whose classic
         reference is `ViewportsPanel::showSplitterContextMenu`), **Pipeline Visibility** (Phase 4, where the pipeline
         panel gives it a visible counterpart) and **Create Camera** (Phase 4, together with the other scene-node
         operations).
     * **Menu bar and discoverability**: a menu bar mirroring the existing commands, with each item bound to a `Command`
       and a single owner of every shortcut (a menu item must not install a second `QKeySequence`). It may contain only
       commands whose handler exists in `gui/base` or whose frontend action exists in the QML shell — today that is Edit
       (undo/redo/delete), View (viewport modes, maximize, zoom) and File (import through the existing dialog, quit);
       save/save-as, export, settings, render and animation-settings entries are disabled placeholders naming Phase 3,
       Phase 7, Phase 7, Phase 7 and Phase 5 respectively. This also gives macOS the **About and Quit** reachability it
       lacks today, where Qt moves those entries into the global menu: `ACTION_HELP_ABOUT` exists in `gui/base` but its only
       handler is the desktop dialog, so this phase adds a small QML About surface (application name, version, build type,
       links) bound to that command. **Preferences is not delivered here** — `ACTION_SETTINGS_DIALOG` (with the native
       `QKeySequence::Preferences`) has no handler outside the desktop settings dialog either, so it stays a disabled
       placeholder naming Phase 7, and the parity claim is About and Quit only. Verified by extending the spike's command check
       with a menu walk that asserts every item is either enabled with a handler or disabled with an owner.
     * **Per-task progress** (a new capability, not restored parity — the classic status bar shows one aggregate bar and
       cannot cancel a single task either): the shared `TaskProgressModel` already exposes one row per running task, so
       present them (`text`, `value`, `maximum`) and keep the existing Cancel for shell-started operations. Per-task
       cancellation stays out of scope, because a `TaskProgress` carries no handle to its `Task`.
     * **Import diagnostics in the status line** (assigned here by review §3.3 so the promise cannot fall between this
       phase and Phase 7's option UI, which only adds the *options*): after an import, report the detected file format and
       the number of source frames, and keep the notice until the next operation. Acceptance: the spike imports a file
       whose comment line contains `atoms`, which defect F6 hands to the LAMMPS Data importer and which yields an empty
       scene, and asserts that the notice names the format that was used and the frame count — the misdetection becomes
       visible instead of silent.
     * **Window state**: remember window size/position, the last used theme and the pane-layout policy through the settings
       facade of deliverable 2, so a second launch does not look like a first launch.
  4. **Remaining core/frontend couplings (review item A8)**:
     * **A8.1 (done)** — `StandardRenderer` still includes `RenderThread.h` for three static calls (`pickGraphicsApi()`,
       `enumerateAdapters()`, `selectedAdapterName()`); move them behind `RendererService` or a small graphics-API
       utility, which completes the extraction started in Phase 1.
     * **A8.2 (done)** — the three `dynamic_object_cast<MainWindowUI>` sites in application services were resolved with a
       desktop-side lookup instead of a new interface: `MainWindow::activeMainWindow()` (built on the existing
       `MainWindow::visitMainWindows()`) returns the classic workbench when the current `UserInterface` is one, and null
       under the QML or console frontend, so `NewGraphicsSystemService` and `UpdateNotificationService` return early
       rather than opening a widget dialog. A GUI-neutral notification interface was considered and rejected: every
       service instantiates in every build that loads the Gui plugin, and `UpdateNotificationService` reaches deep into
       the desktop window (command panel, update dialog), which no generic message box could replace. Decision D32.
     * **A8.3 (done)** — `--noviewports` is registered by the desktop frontend and read by core
       (`core/dataset/DataSet.cpp`); make it a core-level parameter or let the frontend decide the default viewport
       configuration.
     * **A8.4 (done)** — O1 is settled as a **documented environment constraint, not an implementation task**: the
       `ovitoheadless` QPA plugin does not exist in this tree, Qt's `offscreen` plugin provides no QRhi, and the working
       headless route for the Qt Quick frontend is `xvfb` with `QT_QPA_PLATFORM=xcb` (UI_TEST_ENV.md sections 1 and 3.2).
       What this phase adds is the failure path that constraint implies: when no QRhi-capable platform is available, the
       frontend must detect it (`QQuickWindow::rhi()` stays null) and report an error naming the platform plugin and the
       recipe instead of opening a blank viewport. Acceptance: a spike run with `QT_QPA_PLATFORM=offscreen` takes the error
       path and says so, and the constraint is stated for users, not only for tests.
     * **A8.5 (done)** — `AvailableModifiersModel` and `AvailableOverlaysModel` still register plain `QAction`s; convert them to
       `Command`s so Phase 4's modifier library (and the QML command list) can consume them.
       *Outcome:* they register `ModifierAction`/`OverlayAction` commands (keeping their object names as ids, so
       `findAction(id)` still resolves the `QAction` view that the desktop cards expect) and the models expose
       `categoryCommands()`/`commandAt()`/`commandFromIndex()` plus a `CommandRole` next to the `ActionRole` that returns
       that view. The "Manage templates…" row of each library is no longer a wrapper `QAction` with shortened, italic text
       but the frontend-owned global command itself.
     * *Verification of deliverable 4*: the native build and `ctest --preset native` (6/6), a headless `--nogui` run and a
       classic run under Xvfb; the spike's new `--qml-library-check` (every row of both libraries exposes a registered
       command whose `QAction` view and row flags agree with it) plus the offscreen `--qml-device-check` run that accepts
       the missing-graphics-device path; and the whole spike suite in the release and the assert-enabled build. The insert
       path of the library rows is *not* exercised by the spike: inserting a viewport layer switches the viewport into
       render preview mode, which the Qt Quick viewport does not implement yet (Phase 5). A modifier *is* inserted through
       a library command, by the ambient-occlusion case of `--qml-offscreen-check`, which selects the pipeline through the
       model first (deliverable 6).
  5. **Shared presentation assets (review item A9) — done**: the classic icon set
     (`gui/base/resources/icons/ovito-dark|light`) is published for QML use and the drawn glyphs of the shell are gone, so
     both frontends speak one visual language. `gui/base/app/IconTheme` owns the rule that answers which of the two themes
     is current (it turned out that there is no tint step to share) and how an icon path is resolved, `GuiSettings` applies
     it next to the color scheme, and the Qt Quick frontend publishes the icons through `QmlIcons` (`Icons.url(...)`, the
     `ovito-icon` image provider); the maximize/restore button of a pane, the entries of the menu bar and the Import button
     take their icons from it. The set gained `viewport_restore.svg` in both themes, because the classic frontend never
     needed a way back from a maximized viewport. *Verified* by the new `--qml-icon-check` (both themes resolve the icons of
     the shell and of its commands, the QML singleton reports the theme of the shared layer, every icon image is loaded, and
     a maximized pane switches its button to the restore icon), in the release and the assert-enabled build, plus a classic
     run whose toolbar still shows its icons after the theme wiring moved. Evidence: `docs/design/evidence/phase25_icons.png`.
  6. **Offscreen rendering service (review item A4) — done**: one core-facing service over the four `RenderThread` offscreen
     entry points (`createOffscreenTarget`, `renderOffscreenFrame`, `renderAOFrame`, `renderPickingFrame`), owning target
     creation, the `forPickingOnly`/AO flag, supersampling, readback and reuse, used by the classic viewport grab, the QML
     picking pass, the ambient-occlusion modifier and — in Phase 7 — render output. It does **not** re-introduce a shared
     `PickingBufferTarget`: that duplication (O7) was already removed when the Qt Quick frontend moved to
     `RenderThread::renderPickingFrame()`. Today four call sites own their target, flags and readback, two of them from a
     work thread, and misuse is caught by runtime assertions rather than by the interface.
     The deliverable must therefore **state, per entry point, the thread it runs on, who owns the target and the QRhi
     resources, which executor the completion arrives on, and the reuse/destruction rule across threads** — today only the
     render thread may touch `QRhi`, `createOffscreenTarget` returns a handle whose target is created lazily on the render
     thread, and the AO path awaits its frame from a thread pool. A service that hides those differences without naming
     them would be worse than the duplication it removes, so the interface and the phase's documentation carry the
     protocol instead of leaving it implicit. Acceptance is a concurrency and teardown check **per entry point that the
     spike can reach**: the AO sampling, the picking pass and the render output must each run to completion while the others
     are in flight, a superseded request must neither deadlock nor read a freed target, and releasing the resources with a
     request in flight must fail loudly rather than silently. The classic viewport grab is deliberately **not** exercised by
     the spike (it links no desktop code): it shares `renderImage()` with the render-output path, so what stays unverified
     is that one call site's own timing, not the service. Render *settings* and the output dialog remain Phase 7.
     *Outcome:* the service is `core/rendering/OffscreenRenderTarget` (audit decision D34) — move-only, constructed as
     `OffscreenRenderTarget(UserInterface&, Kind)` with `Kind::Visual` or `Kind::PickingOnly`, exposing `renderImage()`,
     `renderPicking()` and `renderAmbientOcclusion()` and reusing its GPU target until the requested resolution changes.
     The `Kind` replaces the raw `forPickingOnly` flag, and a pass of the wrong kind throws (and asserts) before any GPU
     work is submitted. The interface carries the thread protocol explicitly rather than leaving it implicit: **only the
     main thread may allocate a target** (allocation creates the shared render thread and the graphics device), **a pass
     may be submitted from any thread** once the target exists for that size, and completion arrives on the awaiter's
     executor; `prepare()` is the main-thread step that exists for the one consumer whose pass is submitted from a worker
     thread (the ambient-occlusion sampling). All five consumers moved onto the service — `RenderSettings` render output,
     `WidgetViewportWindow::grabViewportImage()`, the QML picking pass, `AmbientOcclusionModifier` and
     `ColorLegendOverlay` symbol rendering — and a shared `PickingBufferTarget` was deliberately not re-introduced.
     *Verification:* the spike's new `--qml-offscreen-check` first proves the kind contract (a `renderPicking()` on a
     `Visual` target and a `renderImage()` on a `PickingOnly` target are refused) and then drives the three QML-reachable
     offscreen paths **at the same time**: it inserts the ambient-occlusion modifier through its shared library command
     (so the sampling runs on the pipeline's worker thread), starts a picking pass whose result it deliberately does not
     await, resizes the workbench window while the sampling, the picking and a render output are in flight, and then
     re-renders the render output until the ambient-occlusion shading appears in the read-back image — after which a
     fresh pick proves the superseded picking target neither deadlocked nor read a freed target. The step runs in the four
     CI jobs. The desktop-only viewport grab shares `renderImage()` with the render-output path and is therefore not
     exercised separately.
  7. **Measurement-driven viewport optimization pass — done** (each item decided by numbers, not by intuition; method in
     [UI_TEST_ENV.md](UI_TEST_ENV.md) section 9.4 — `QSG_NO_VSYNC=1`, both render loops, medians of three runs at 512 and
     32768 atoms):
     * **Single "viewport canvas" item** — four `QQuickRhiItem`s means four offscreen textures, four pass boundaries,
       four synchronize steps and four composites per window frame; one item drawing all panes (the layout is already
       computed in C++ by `QmlViewportLayout`) could share one resource frame and one pass. Prototype, measure, and keep
       it **only** if it wins at both scene sizes; otherwise record the number and keep four items.
       *Outcome: decided against, and the number is recorded.* Measured instead of prototyped, because the cost a canvas
       item can remove is exactly the cost of the three items it replaces, and that cost is measurable without writing the
       item: with one pane maximized (one item rendering, the same total pane area, the other three items invisible),
       the frame time changes from **8.55 ms to 6.99 ms** at 512 atoms and from **37.04 ms to 11.63 ms** at 32768 atoms
       (medians of three runs, `QSG_NO_VSYNC=1`, threaded loop, four panes at 1280x800). The second pair looks like a
       3.7x win but is not one a canvas item could take: it is the rasterization of three panes that are simply not drawn
       any more. What a single item can remove is the *fixed per-item* share, and the 512-atom pair bounds it at
       (8.55 - 6.99) / 3 = **0.52 ms per item**, i.e. about **1.6 ms per frame** for three saved items - 18% at 512 atoms
       and 4% at 32768 atoms, and nothing measurable at all on the `basic` render loop (5.76 ms with four panes versus
       5.92 ms with one). A 1.6 ms saving is invisible while the frame rate is above the display refresh (both
       configurations are beyond 60 Hz at 512 atoms, and the Metal/D3D12 builds are faster still), and it would buy a new
       item type that has to reproduce input routing, per-pane picking, hover, focus, HiDPI mapping, context menu and
       teardown for four platforms - so the four items stay. The measurement also names what a future optimization should
       target instead: the four *panes* (four rasterizations of the same scene), not the item machinery, which is the
       direction a "reduced quality while dragging" mode would take. Both first-pass numbers (`QSG_NO_VSYNC=1`, threaded
       loop, four panes, medians of three): **8.55 ms** at 512 atoms and **37.04 ms** at 32768 atoms.
     * **Picking pre-warm** — refresh the picking buffer on camera or scene change instead of on the next hover pick,
       which removes the documented one-frame staleness (review section 3.1). Frame time cannot decide this one: its
       acceptance is a **correctness and latency** check — after a camera move or a scene change, the first hover pick at
       a known position must return the object the second pick returns **once the view has settled** (the quiet period of
       the implementation below; a hover within that window is the staleness that stays until Phase 5's asynchronous pick
       API), the added offscreen pass must not push the hover response beyond the measured warm-path latency, and every
       camera change must not turn a static scene into a continuous renderer. If that cannot be shown, the on-demand pick
       path stays and the staleness remains documented.
       *Outcome: implemented and accepted.* Finding defect F20 on the way made this item the round's most instructive
       one: the pre-warm crashed release builds as soon as a picking pass was in flight while the data set was replaced
       (4 of 5 runs of the scene-replacing session check against 0 of 5 without the pre-warm), AddressSanitizer named the
       cause (a picking buffer holding object ID handles of a render thread that was gone), and the fix is described in
       decision D36. The pre-warm stays; `--qml-prewarm-check` verifies that the buffer catches up on its own and that a
       settled viewport does not keep rendering.
       `QuickViewportWindow` no longer tracks staleness with a boolean that any pass
       cleared, but with a counter pair: `renderFrameGraph()` counts every change of the rendered contents, a completed
       pass records the counter it was rendered for, and `isPickingBufferCurrent()` answers whether the cached buffer
       belongs to the current view (a booleans-based flag could mark a buffer current that a pass rendered for a
       superseded view, which is exactly the case that made a hover answer from the previous camera). Whenever the view
       changes, a single-shot 150 ms timer is restarted; when it fires, the buffer is refreshed **if it is behind the
       view**. The delay is the whole trick: during a camera drag frame graphs arrive faster than 150 ms apart, so the
       refreshes happen once after the interaction instead of once per frame (one offscreen pass per interaction, not per
       frame), and a hover that follows an interaction is answered from the current view.
     * **Frame-graph generation** — profile the per-frame, per-pane generation cost before optimizing it; four cameras
       mean four frame graphs, which is expected and may already be dominated by something else.
       *Outcome: measured, nothing to optimize.* With temporary instrumentation (reverted again) around
       `ViewportWindow::generateFrameGraph()`, `QuickViewportWindow::renderFrameGraph()` and the render pass, one frame of
       four viewports decomposes into **0.29 ms per frame graph** (4 x 0.29 = 1.16 ms, 14% of the frame, 512 atoms;
       1.05 ms per graph = 4.19 ms, 11%, at 32768 atoms), **0.05 ms per render pass** on the CPU side (0.19 ms per frame)
       and **nothing measurable** for the hand-off. Generation is therefore a tenth of the frame, not the bottleneck, and
       the remaining ~85% is Qt Quick's scene graph plus the rasterization of four panes - which is what the measurements
       of the item above attribute to the panes rather than to the item machinery. No optimization was recorded here, and
       the numbers are what make the "single canvas" decision above a measurement instead of an assumption.
     * **`--gui` diagnostics** — an unknown frontend name printed a terse list of names and exited 1. It now prints the
       available frontends with their descriptions, guesses what a typo meant (a strict prefix of exactly one name, or the
       unique name within an edit distance of two), and names the way out (`Without the '-gui' parameter, OVITO starts the
       default user interface.`), which is also the answer for a desktop launcher that passes a stale `--gui` argument.
       *Decided against:* an error dialog. This point is reached before a workbench exists, so the dialog would be a
       free-standing modal window, and every unattended run (the CI smoke tests, the verify scripts that check `--gui`
       handling, a batch job) would wait for someone to dismiss it - for a message whose only content is a list and a
       suggestion. The message is written for both audiences instead.
     * *Verification of deliverable 7*: the numbers, the two decided-against items and the pre-warm's acceptance are
       documented in [UI_TEST_ENV.md](UI_TEST_ENV.md) sections 9.4 (frame times) and 9.2.7 (the check, and the release-only
       corruption that defect F20 turned out to be); `--qml-prewarm-check` runs in the four CI jobs.
- **Status**: **deliverables 1–7 are done** — Phase 2.5 is complete except for the exit gate's final pass; every review item A1–A9 is in one of the three states the gate asks for (see the table in [UI_FRONTEND_REVIEW.md](UI_FRONTEND_REVIEW.md) section 7), and each optimization of deliverable 7 is recorded with its measured medians, including the two that were decided against; the picking pre-warm also names the release-only corruption it exposed (D36, defect F20) and its fix. Phase 2 is complete (deliverables 1–7, exit gate
  verified on Linux/OpenGL, Linux/Vulkan, macOS/Metal and Windows/D3D12), so this phase starts from a verified base; the
  audit decisions it produces are recorded as D30 onward in [UI_PHASE0_AUDIT.md](UI_PHASE0_AUDIT.md). Deliverable 1 (the
  action/editor inventory in that document's section 6 plus [UI_PARITY_MATRIX.md](UI_PARITY_MATRIX.md)) is delivered and
  verified: writing it also ran the installed-tree check of §2.3 of UI_TEST_ENV.md, fixed defect F17, and corrected a
  testing recipe that had been recorded as working without ever having worked. Deliverable 2 (the settings facade, D30) is
  delivered as well, with its check running in all four CI jobs. Deliverable 3 (the small parity gaps and the import
  notice, D31), deliverable 4 (the A8 shared-layer cleanup, D32), deliverable 5 (the A9 icon layer, D33) and deliverable 6
  (the A4 offscreen service, D34) are delivered and verified in the release and the assert-enabled build, each with its
  spike check running in the four CI jobs.
- **Non-goals** (each assigned to a phase where its semantics are already being changed, so they are not silently lost):
  * **A3** (one asynchronous pick API) belongs to Phase 5: it changes the `pick()` contract that `SelectionMode`,
    `NavigationModes` and `XFormModes` call on every mouse move, and the classic frontend's blocking behaviour has to be
    decided together with the interaction work.
  * **A6** (import plan / options model) belongs to Phase 7: the missing piece is the option *UI* (format override,
    import-mode dialog), not the import flow, which Phase 2 already shares.
  * **A7** (property model foundation) belongs to Phase 4, immediately before the first inspector, and is the one item of
    this list that must not be deferred past its phase: it is the largest single duplication the migration can avoid.
  * Pipeline/selection models, timeline and keyframes, render settings and output, data inspector and the command list
    (the "command palette" consumption of A1) keep their own phases, and the two entries of that list which used to have
    no owner are assigned explicitly: the **settings dialogs** (Preferences as well as the application-settings pages
    behind `ACTION_SETTINGS_DIALOG`) are Phase 7 deliverable 5, and the **snippet import/export UI** is Phase 4
    deliverable 7.
- **Exit Gate**: every review item A1–A9 is in exactly one of three states, and the mapping is written down in
  [UI_FRONTEND_REVIEW.md](UI_FRONTEND_REVIEW.md) section 7 as well as in the parity matrix: **implemented before this
  phase** (A1's command layer, A2's renderer registry, most of A5 — recent files, task-progress model, session workflow),
  **implemented here** (A4, A8, A9, A5's settings facade) or **assigned to a named later phase** (A3 → Phase 5, A6 →
  Phase 7, A7 and A5's selection model → Phase 4). An item in no state fails the gate; each closed gap has an automated check in `OvitoQmlSpike` (context menu, menu-bar commands including
  undo/redo enablement, per-task list) that also runs in the four CI jobs, or a documented manual check; the classic
  frontend still passes `ctest --preset native`, `--nogui`, and a graphical run in its own session, and the QML frontend
  still passes the full spike set on Linux (and on macOS/Windows whenever a host is available); every optimization
  decision — including the ones decided *against* — is recorded with its measured medians.
- **Risks**:
  * *Small wins with a shared-code blast radius.* Mitigation: land the phase as separate grouped commits per deliverable,
    with the classic-frontend checks after each, exactly as in Phases 1–2; a `gui/base`-only change still needs the
    assert-enabled build when it touches invariants (`WorkbenchUI`, `QmlViewportLayout` and `Command` all had assert-only
    defects that NDEBUG hides).
  * *A menu bar can steal shortcuts.* Keep every `QKeySequence` in the `Command` and let exactly one place install it;
    verify on macOS, where Qt relocates entries into the global menu and can silently drop duplicates.
  * *A context menu can fork `ViewportMenu` semantics.* Drive it from the shared commands and the viewport API, and keep
    the layout mutations in one implementation (Phase 4).
  * *Platform-specific build traps recur.* Three of them have already cost real time: a header-only exported class whose
    implicitly generated destructor MSVC imports instead of exporting it (defect F15 — define special members out of line),
    the Windows redistributable build which will not configure without Boost, a zlib-enabled HDF5 and Perl (defect F16),
    and a conditionally compiled source that joins a target without being moc'd, which needs the stale `*_autogen`
    directory cleared (UI_TEST_ENV.md section 5.5). Deliverable 1's Windows rows start from those prerequisites.

---

### Phase 3: Presentation Models & Command Layer
- **Objective**: Expose shared state and editing operations with explicit lifecycle and undo semantics.
- **Deliverables**:
  1. Pipeline presentation using the Phase 0 reuse decision: roles, selection, source/visual-element rows, groups, shared objects, and evaluation status. New adapters, if needed, live under `src/ovito/gui/qml/models/`.
  2. Animation presentation for the scene interval, current time/frame, playback settings, controllers, and keyframe selection. Add scene selection adaptation only where the shared API needs it.
  3. Modifier chooser adaptation preserving discovery, categories/templates, applicability, and insertion semantics from shared services.
  4. A QML-facing command bridge to shared actions and undo infrastructure (the command layer itself landed early as
     review finding A1 / audit decision D26: the QML workbench reads and triggers the same `Command` objects the classic
     frontend presents as `QAction`s). The menu bar, the undo/redo/import entry points and the per-task list land in
     Phase 2.5, so this deliverable is the **model** side and only that: the command *list* model that a search or
     palette UI consumes later (the palette UI itself is Phase 8), the commands of the panels that exist when this phase
     ends, and the enablement/shortcut synchronization of those panels. Binding the panels that Phase 4 introduces is
     Phase 4 work. Implement discrete transactions and continuous begin/update/commit/cancel edits as defined in design
     section 5.3.
  5. Define selection/status refresh and edit cancellation on target deletion, undo/redo, and dataset replacement. Do not retain row indices as object identity across deferred work.
  6. Session save/open and modified-session close handling using extracted shared operations and QML dialogs. Define scene-change and save-failure behavior before users rely on editing sessions.
- **Exit Gate**: Verify the **model and command APIs**, not a UI workflow: insert/reorder/delete of pipeline items
  through the shared operations and their undo/redo, cancellation restoring the original value, coherent selection after
  deletion and undo, and no stale writes when a dataset is replaced during an edit. Reopen a saved session and verify
  that close cancellation and save failures preserve its content. The interaction acceptance of deliverable 4 — gestures,
  drag & drop, one undo step per continuous drag, editor entry — belongs to Phase 4, where the `PipelineView` and the
  editors actually arrive; Phase 3 verifies through the spike harness and the C++ tests.

---

### Phase 4: Pipeline Stack & Basic Property Inspector
- **Objective**: Deliver a usable pipeline editor and the explicitly supported reflected parameter subset.
- **Deliverables**:
  0. **A7 property model foundation first** (review item A7, assigned here by Phase 2.5): the frontend-neutral property
     *field* enumeration per object kind plus an entry model (label, type, value, unit, range, read-only, resettable,
     animated) with an editor registry keyed by property descriptor. Both frontends consume it — `PropertiesPanel`
     becomes one rendering of the model rather than the owner of the knowledge — and the phase is only enterable through
     this deliverable, because deliverables 1–3 re-derive units, bounds and controller semantics otherwise.
  1. `PipelineView.qml`: Compact rows with eye toggles, selection, modifier insertion/deletion, and validated drag reordering. Preserve audited group/shared-object and source/visual-element behavior through shared operations. It consumes the **shared selection/hover model** (the second half of review item A5), which this deliverable adds to `gui/base` next to the pipeline model, so the classic pipeline list and the QML one share one selection instead of each tracking its own.
  2. `ModifierEditorRegistry`: C++ registry dispatching to specialized QML editors or generic fallback.
  3. `AutoPropertyEditor.qml`: Integer/float, boolean, and color fields plus scalar controller values at the current time. Respect units, bounds, read-only state, and the shared command contract.
  4. Coverage reporting in the Phase 0 field inventory. Unsupported user-editable parameters are clearly identified in the UI, with read-only values where meaningful; they remain parity gaps.
  5. **Viewport insert/delete** (the layout half of `ViewportMenu`'s *Window Layout*, whose QML entry point Phase 2.5
     ships disabled): split horizontal/vertical and remove, driven by the same layout rules and undo transactions the
     shell already uses for splitter drags, with `ViewportsPanel::showSplitterContextMenu` as the reference. The
     per-viewport *window* lifecycle stays with each frontend (A2 of the review document does not propose a shared window
     manager); what this deliverable settles is the rule for when a viewport's window or item is created and destroyed as
     the layout changes, and it verifies it in both frontends.
  6. **Create Camera** (the `View Type` submenu entry whose QML counterpart Phase 2.5 ships disabled): create a camera
     scene node with a `StandardCameraSource` positioned from the clicked viewport's current view, inside a
     `performTransaction()` named "Create camera" exactly as `ViewportMenu::onCreateCamera()` does, so one undo step, and
     refuse it while the viewport already follows a camera node (`viewNode() != nullptr`). Acceptance: create the camera,
     verify the viewport keeps the same image, undo, and verify the camera node is gone and the view is a free camera
     again.
  7. **Snippet import/export UI** (assigned here because both operate on pipeline items): import an `.ovito` snippet as a
     new pipeline and export the selected pipeline item(s) as a snippet, reusing the existing file-importer/exporter
     machinery and the shared file dialogs. Acceptance: round-trip one snippet through export and import, and undo the
     import.
- **Exit Gate**: On a real imported trajectory, add/edit/reorder/delete modifiers and undo the sequence. Verify group drop boundaries, invalid numeric entry, unit conversion, current-time controller edits, and refresh after undo or time changes. Every initial field has a recorded supported or unsupported outcome. The shared A7 model must show the **same** fields, units, bounds and read-only flags that the classic panel shows for the same object, verified side by side on one scene rather than by trusting the QML side. Splitting and removing a viewport must be undoable and must leave no orphaned viewport window or QML item in either frontend.

---

### Phase 5: Animation Timeline & Viewport Interaction Parity
- **Objective**: Complete the audited scene-navigation and animation workflows, including keyframe editing.
- **Deliverables**:
  1. Connect the viewport adapter to existing navigation and selection modes. Preserve focus-loss, keyboard/context-menu behavior, logical/device coordinate mapping, and viewport split/maximize interactions.
  2. `TimelineView.qml`: Scrubbing, stepping, playback speed/loop and interval controls using existing time conversions, including non-zero starting frames.
  3. Interactive viewport HUD: Camera projection switcher (Perspective vs Orthographic), View presets (Top, Front, Right).
  4. Controller tracks and keyframe selection, movement, deletion, parameter animation entry points, and auto-key behavior. Route key edits through the transaction contract and preserve audited key-editor operations.
  5. **A3 — one asynchronous pick API** (review item A3, assigned here because this is the phase that touches the modes
     which pick). Promote `ViewportWindow::pick()` to a thin wrapper over an asynchronous pick and move the
     buffer/refresh policy next to `ObjectPickingBuffer`. The blocking classic wrapper is only allowed if it is provably
     safe: a pick future whose completion is delivered through the GUI event loop (the Qt Quick implementation resumes its
     coroutine on `ObjectExecutor(this)`) must never be awaited on the GUI thread, so this deliverable also states the
     completion thread and the dispatch rule, and adapts `SelectionMode`/`NavigationModes`/`XFormModes` accordingly.
  6. **Adjust View…** (the `ViewportMenu` entry whose QML counterpart Phase 2.5 ships disabled): a QML form editing the
     active viewport's view type, field of view, camera position, camera direction and camera transformation through the
     same `Viewport` setters the classic `AdjustViewDialog` uses (`setViewType`, `setFieldOfView`, `setCameraPosition`,
     `setCameraDirection`, `setCameraTransformation`). Those properties are `PROPERTY_FIELD_NO_UNDO` in core, so the form is
     deliberately *not* undoable — neither is the classic dialog — and the rule is recorded rather than invented.
     Acceptance: change each field and verify the viewport follows and the values survive a view-type switch; verify the
     entry is disabled while the viewport follows a camera node.
  7. **Preview Mode** (the `ViewportMenu` toggle whose QML counterpart Phase 2.5 ships disabled): drive
     `Viewport::renderPreviewMode()`/`setRenderPreviewMode()` and render the non-interactive preview in the viewport,
     including the settings the preview needs (render-output resolution and aspect) and the behaviour while a render is in
     progress. Acceptance: toggle it in a viewport with a known view, verify the flag, that the image switches to the
     render-output aspect and resolution, and that turning it off restores the previous interactive view; verify interaction
     is suspended while it is on.
- **Exit Gate**: Execute navigation and animation cases side-by-side with classic on the same scene. Verify picking at fractional display scaling, frame/time mapping, playback cancellation, keyframe edit/undo/cancel, and parameter values at keyed and interpolated times. For A3: a hover pick in both frontends must complete without blocking the GUI thread and without deadlocking against the completion path (a test that fails fast if the pick result is only delivered through the blocked loop), and both frontends must agree on the pick result for the same position and scene. For deliverables 6–7 the disabled placeholders of Phase 2.5 must become working entries: no menu item this phase owns may stay disabled, and Adjust View and Preview Mode must match their classic counterparts on the same scene.

---

### Phase 6: Specialized Property Editors
- **Objective**: Deliver bespoke QML editors for OVITO's highest-frequency modifiers.
- **Deliverables**:
  1. `SliceModifierEditor.qml`: Normal vector inputs, slice plane distance slider, reverse toggle.
  2. `CnaEditor.qml`: Structure type checklist, cutoff selection, adaptive mode.
  3. `ColorCodingEditor.qml`: Gradient selector, range min/max inputs, property chooser.
  4. `PolyhedralTemplateMatchingEditor.qml`: Structure checkboxes, RMSD cutoff slider.

These lists identify starting controls, not complete editor specifications. Include all additional fields, conditional behavior, data-dependent selectors, actions, and plots found in the Phase 0 audit. Specialized editors use the same validation and transaction paths as fallback controls.

- **Exit Gate**: All audited fields/actions of these four modifiers work on representative inputs, including dependent visibility, available choices, controller values, and undo/redo. Compare pipeline results with classic, using justified numeric tolerances where applicable. Assign remaining editor gaps to Phase 7 explicitly.

---

### Phase 7: Remaining Desktop Feature Parity
- **Objective**: Reach complete feature parity with the classic QtWidgets interface.
- **Deliverables**:
  1. Remaining file/menu workflows: import options and reload, export, scene reset, viewport configuration, and all additional actions from the audit. Recheck session save/load across both frontends.
  2. Overlays & Visual Elements (Simulation Cell display, Coordinate Tripod, Color Legend).
  3. Data Inspector and utilities: begin with particle property tables and complete all applicable inspector/utility workflows identified by Phase 0.
  4. `RenderSettingsView.qml`, render output/progress view, still and animation rendering, cancellation, and saving output through existing rendering services.
  5. Complete remaining property editor and command gaps from the coverage inventory, including the **settings dialogs**:
     the QML equivalent of the application-settings pages behind `ACTION_SETTINGS_DIALOG` (the Preferences entry that
     Phase 2.5 leaves as a disabled placeholder), covering the persisted keys of the `gui/base` settings facade instead of
     introducing a second storage scheme. Validate keyboard/accessibility and small-window behavior throughout the workbench.
  6. Run the complete parity matrix in `ovito` and `ovito --gui=qml` using the same build configuration and datasets, and record platform results.
- **Exit Gate**: Every applicable audited parity row passes, with evidence linked from the matrix; no unresolved parameter, workflow, or platform gaps remain under a complete-parity claim. Regression checks preserve classic and headless operation. If scope is reduced, label the release as partial parity and update both documents rather than marking omitted rows passed.

---

### Phase 8: UX Modernization (Post-Parity Enhancements)
- **Objective**: Introduce advanced workflow accelerators once parity is guaranteed.
- **Deliverables**:
  1. Global Command Palette (`Ctrl+P` fuzzy search for any modifier, action, or preset).
  2. Activity Bar for swift view toggling.
  3. Integrated multi-tab bottom panel (Timeline + Data Inspector + Python Console).
- **Entry Gate**: Phase 7 parity gate passed. Audit shortcut conflicts before assigning Command Palette bindings; preserve platform-native conventions.
- **Exit Gate**: Each enhancement passes its workflow checks without regressing the parity matrix.

---

### Phase 9: Classic Frontend Retirement (Long-Term)
- Deprecate QtWidgets interface only after QML frontend has matured across multiple release cycles and received community consensus.
- This phase authorizes no current removal. A separate retirement proposal must identify compatibility and support implications before implementation.

---

## 4. Verification Strategy

- Add behavioral tests alongside each phase: model mutations and selection, transaction rollback/undo, controller/time conversion, and session round trips. Use QML interaction tests for focus, input, and state transitions where practical.
- Run the repository's required preset build and regression checks after implementation changes: `cmake --preset native`, `cmake --build --preset native`, `ctest --preset native`, and the headless startup check documented in `AGENTS.md`. Shared-service extraction must keep the classic frontend passing throughout migration.
- Run graphical lifecycle and interaction checks on actual target backends. An offscreen/headless pass cannot validate Qt Quick's Vulkan, Metal, or D3D12 integration. Record OS/architecture, Qt version, backend/device, dataset, and result; unavailable environments remain unverified.
- Use the same representative datasets for both frontends: a static particle scene, an animated trajectory, a scene with multiple/grouped/shared modifiers where supported, and a session with animated parameters. Add failure/cancellation fixtures and rendering-output cases as their phases arrive.
- Compare scientific outputs and saved scene state directly. Use image comparisons with documented tolerances where stable, plus interaction checks for behavior that images cannot verify. Record frame times and input responsiveness against classic; Phase 1 sets justified regression limits from measured baselines.

## 5. Immediate Next Step

**Phase 3**, scoped above: the workbench shell and command-layer consumption. Phase 2.5 is complete — all seven
deliverables and the A1–A9 review items are implemented or explicitly assigned to a later phase, and its exit checks
(automated spike checks in the four CI jobs, the classic regression runs) have been run and are recorded in this document,
[UI_PARITY_MATRIX.md](UI_PARITY_MATRIX.md) and [UI_TEST_ENV.md](UI_TEST_ENV.md). What is *not* closed by it: the
asynchronous pick API (A3, Phase 5), the import-options UI (A6, Phase 7), the property model (A7, Phase 4) and the
architect/owner's freeze of the design.

Phase 1 and Phase 2 are closed to the extent this environment allows: the rendering bridge, the picking path and the
performance baseline are documented in [UI_PHASE1_SPIKE.md](UI_PHASE1_SPIKE.md), the shell, import path and packaging in
[UI_PLAN.md](UI_PLAN.md) and the audits in [UI_PHASE0_AUDIT.md](UI_PHASE0_AUDIT.md); the testing recipes and traps are in
[UI_TEST_ENV.md](UI_TEST_ENV.md). The architecture status remains **proposed** until the owner freezes it — the technical
preconditions (rendering bridge, picking, teardown, four platforms, assert-enabled run, performance baseline) are met. This
roadmap describes planned work: Phases 1, 2 and 2.5 are recorded as verified above, and Phases 3–9 have not started.
