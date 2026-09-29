# OVITO Modern Workbench UI Implementation Plan

> **Scope**: Spike-driven, phased migration to modern Qt Quick / QML frontend
>
> **Guiding Principle**: Validate high risks first, interaction parity before redesign
>
> **Status**: Proposed Roadmap; Phase 0 audit (partially), Phase 1 rendering spike and Phase 2 deliverables 1–7
> executed — the frontend selection (`--gui=qml`), the shared `gui/base` workbench base class, the layout-derived
> workbench shell and the import path with its empty/busy/cancelling/cancelled/error states are in the tree and verified
> on Linux/OpenGL, see [UI_PHASE0_AUDIT.md](UI_PHASE0_AUDIT.md) and [UI_PHASE1_SPIKE.md](UI_PHASE1_SPIKE.md).
> The comparison against the classic frontend, the duplication it identifies and the abstractions it proposes are
> collected in [UI_FRONTEND_REVIEW.md](UI_FRONTEND_REVIEW.md); the command layer (A1) and the workbench state models
> (A5) proposed there should be built **before** the pipeline and inspector work of Phases 4 and 6.
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
  desktop services). The action/editor inventory and the expanded parity matrix of deliverables 1, 3 and 4 are still pending.

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
  **Metal** on macOS ARM64, and its frame times (512–262144 atoms) were compared against the classic frontend on macOS.
  Results, evidence and the exact reproduction commands are recorded in [UI_PHASE1_SPIKE.md](UI_PHASE1_SPIKE.md); the
  environment recipes and testing traps are collected in [UI_TEST_ENV.md](UI_TEST_ENV.md). Still open: the **D3D12 runtime
  path** (the CI runner now *builds* the frontend on Windows x86_64, but the smoke test has not run there yet), Qt Quick
  Vulkan on a hardware driver (Xvfb lacks DRI3), and mixed-DPI multi-monitor setups.

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
  exit-gate verification on Windows (D3D12 has no test host). The spike verifies the shell with `--qml-layout-check` and `--qml-import-check` in CI; the
  testing recipe is [UI_TEST_ENV.md](UI_TEST_ENV.md). Verified so far: Linux/OpenGL (all checks, including the four
  viewports, the splitter drag and its undo, picking, the import path and the cancellation) and macOS/Metal (the same
  check set, 126 fps with four viewports at 1280x800); macOS screenshots need the screen-recording permission, so the
  shell evidence image is the Linux one.

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
  loading from the deployed layout. The first two are automated in the spike harness (`--qml-layout-check`,
  `--qml-import-check`); the remaining ones are checked by running the frontend in its themes and window sizes (see
  [UI_TEST_ENV.md](UI_TEST_ENV.md)). Record the platform results (Linux, macOS, Windows) in the parity matrix and the
  environment notes; D3D12 runtime evidence is still outstanding from Phase 1 and must be recorded as such rather than
  silently assumed.
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

### Phase 3: Presentation Models & Command Layer
- **Objective**: Expose shared state and editing operations with explicit lifecycle and undo semantics.
- **Deliverables**:
  1. Pipeline presentation using the Phase 0 reuse decision: roles, selection, source/visual-element rows, groups, shared objects, and evaluation status. New adapters, if needed, live under `src/ovito/gui/qml/models/`.
  2. Animation presentation for the scene interval, current time/frame, playback settings, controllers, and keyframe selection. Add scene selection adaptation only where the shared API needs it.
  3. Modifier chooser adaptation preserving discovery, categories/templates, applicability, and insertion semantics from shared services.
  4. A QML-facing command bridge to shared actions and undo infrastructure. Implement discrete transactions and continuous begin/update/commit/cancel edits as defined in design section 5.3; synchronize enablement and shortcuts.
  5. Define selection/status refresh and edit cancellation on target deletion, undo/redo, and dataset replacement. Do not retain row indices as object identity across deferred work.
  6. Session save/open and modified-session close handling using extracted shared operations and QML dialogs. Define scene-change and save-failure behavior before users rely on editing sessions.
- **Exit Gate**: Verify insert/reorder/delete and their undo/redo, one undo step per continuous gesture, cancellation restoring the original value, and deletion/undo restoring coherent selection. Replace a dataset during an edit and confirm no stale writes. Save/reopen a modified scene and verify close cancellation and save failures preserve it.

---

### Phase 4: Pipeline Stack & Basic Property Inspector
- **Objective**: Deliver a usable pipeline editor and the explicitly supported reflected parameter subset.
- **Deliverables**:
  1. `PipelineView.qml`: Compact rows with eye toggles, selection, modifier insertion/deletion, and validated drag reordering. Preserve audited group/shared-object and source/visual-element behavior through shared operations.
  2. `ModifierEditorRegistry`: C++ registry dispatching to specialized QML editors or generic fallback.
  3. `AutoPropertyEditor.qml`: Integer/float, boolean, and color fields plus scalar controller values at the current time. Respect units, bounds, read-only state, and the shared command contract.
  4. Coverage reporting in the Phase 0 field inventory. Unsupported user-editable parameters are clearly identified in the UI, with read-only values where meaningful; they remain parity gaps.
- **Exit Gate**: On a real imported trajectory, add/edit/reorder/delete modifiers and undo the sequence. Verify group drop boundaries, invalid numeric entry, unit conversion, current-time controller edits, and refresh after undo or time changes. Every initial field has a recorded supported or unsupported outcome.

---

### Phase 5: Animation Timeline & Viewport Interaction Parity
- **Objective**: Complete the audited scene-navigation and animation workflows, including keyframe editing.
- **Deliverables**:
  1. Connect the viewport adapter to existing navigation and selection modes. Preserve focus-loss, keyboard/context-menu behavior, logical/device coordinate mapping, and viewport split/maximize interactions.
  2. `TimelineView.qml`: Scrubbing, stepping, playback speed/loop and interval controls using existing time conversions, including non-zero starting frames.
  3. Interactive viewport HUD: Camera projection switcher (Perspective vs Orthographic), View presets (Top, Front, Right).
  4. Controller tracks and keyframe selection, movement, deletion, parameter animation entry points, and auto-key behavior. Route key edits through the transaction contract and preserve audited key-editor operations.
- **Exit Gate**: Execute navigation and animation cases side-by-side with classic on the same scene. Verify picking at fractional display scaling, frame/time mapping, playback cancellation, keyframe edit/undo/cancel, and parameter values at keyed and interpolated times.

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
  5. Complete remaining property editor and command gaps from the coverage inventory. Validate keyboard/accessibility and small-window behavior throughout the workbench.
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

**Phase 2**, scoped above: first the frontend-selection seam and the `gui/base` extractions (deliverables 1–3), because
every later step depends on a real entry point and on a workbench that is not entangled with `gui/desktop`; then the shell,
the import path and the packaging work. Two items from the Phase 0 audit remain open and should be finished alongside,
because they are cheap while the relevant code is being touched: the **action/editor inventory and the expanded parity
matrix** (Phase 0 deliverables 1, 3 and 4, still pending) and the **D3D12 runtime evidence** that the Phase 1 gate left
open.

Phase 1 is closed to the extent this environment allows: the rendering bridge, the picking path and the performance
baseline are documented in [UI_PHASE1_SPIKE.md](UI_PHASE1_SPIKE.md), and the architecture status stays **proposed** only
because of the outstanding D3D12 runtime result. This roadmap describes planned work; no phase is marked complete by this
document revision.
