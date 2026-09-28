# OVITO Modern Workbench UI Implementation Plan

> **Scope**: Spike-driven, phased migration to modern Qt Quick / QML frontend
>
> **Guiding Principle**: Validate high risks first, interaction parity before redesign
>
> **Status**: Proposed Roadmap; Phase 0 audit (partially) and Phase 1 rendering spike executed, see
> [UI_PHASE0_AUDIT.md](UI_PHASE0_AUDIT.md) and [UI_PHASE1_SPIKE.md](UI_PHASE1_SPIKE.md)
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
| Launch, basic import, and task states | 2 | Both launch modes work; a real trajectory loads; progress, failure, and supported cancellation remain responsive. |
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
- **Status (rendering bridge and picking validated)**: `src/ovito/gui/qml` implements the `QQuickRhiItem` +
  `BaseViewportWindow` adapter + `QQuickRhiItemRenderer`/`RendererService` composition and renders real particle data, four
  simultaneous viewports and repeated scene-graph resource rebuild cycles without a `RenderThread` in the interactive frame.
  Object picking is implemented as an asynchronous offscreen pass on OVITO's `RenderThread` and verified end to end
  (hit/no-hit controls, 2× HiDPI, all three Qt Quick render loops, lifecycle cycles, and selection through the unmodified
  `SelectionMode`), including a run with assertions enabled. Results, evidence and the exact reproduction commands are
  recorded in [UI_PHASE1_SPIKE.md](UI_PHASE1_SPIKE.md). Still open: interactive resize and mixed-DPI setups, frame-time
  performance baselines, and every non-OpenGL backend (including Linux Vulkan, which is blocked by the missing
  `ovitoheadless` QPA plugin).

---

### Phase 2: Minimal QML Shell & Build Scaffolding
- **Objective**: Integrate QML build pipeline cleanly into CMake and create the basic window frame.
- **Deliverables**:
  1. CMake option `OVITO_BUILD_QML_FRONTEND=ON`.
  2. CLI runtime flag: `ovito --gui=qml` launches QML shell; standard `ovito` launches classic QtWidgets.
  3. Base directory: `src/ovito/gui/qml/`.
  4. `WorkbenchWindow.qml` reproducing classic OVITO spatial layout (central viewport area + right-hand command panel).
  5. `Theme.qml` encoding VS Code Dark/Light Modern palettes.
  6. Select the frontend in application startup without making the QML module depend on `gui/desktop`; define behavior when the QML build option is disabled. Load and package QML resources through the existing build/deployment flow.
  7. Basic local-file/trajectory import through shared backend services, with empty, busy, error, and cancellation states. Importing a real dataset must be possible before building editors.
  8. Resizable panels, an initial minimum window size, keyboard focus order, and accessible control names following design section 3.2.
- **Exit Gate**: With QML enabled, both `ovito` and `ovito --gui=qml` launch and a real trajectory renders in the QML shell. With QML disabled, classic and headless builds still work. Verify both themes, resizing, import failure/cancellation, and packaged QML loading on target platforms.

---

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

Start **Phase 0** by recording shared-model reuse decisions, the expanded parity matrix, and the initial parameter coverage inventory. Then run **Phase 1** in an isolated prototype branch using the composition architecture from design section 4. Resolve and document the rendering handoff and lifecycle before production integration. This roadmap describes planned work; no phase is marked complete by this document revision.
