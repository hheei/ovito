# OVITO Modern Workbench UI Implementation Plan

> **Scope**: Spike-driven, phased migration to modern Qt Quick / QML frontend  
> **Guiding Principle**: Validate high risks first, interaction parity before redesign  
> **Status**: Source-Grounded Roadmap

---

## 1. Core Engineering Principles

1. **Validate High-Risk Integration Points Before Freezing Interfaces (Spike-Driven)**:
   - Do not commit to unproven assumptions. The greatest technical risk is the 3D viewport rendering bridge (`QQuickRhiItem` vs. `RenderThread` QRhi ownership). This must be rigorously evaluated in an isolated technical spike before locking production architecture.
2. **Strict Architectural Seam (No Desktop Entanglement)**:
   - Dependency rule: `gui/qml -> gui/base -> core`.
   - **`gui/qml` must never depend on `gui/desktop`**. If a feature or logic currently resides in `gui/desktop`, it must be refactored into `gui/base` as a shared service before QML consumes it.
3. **Zero Core Disruption**:
   - Numerical pipelines, asynchronous evaluation (`TaskScope`), file readers, and mathematical abstractions in `core/`, `particles/`, and `stdobj/` remain completely untouched.
4. **Interaction & Feature Parity Before Workflow Redesign**:
   - First priority is 1:1 behavioral equivalence with classic OVITO (modifier stack ergonomics, viewport navigation, timeline behavior).
   - VS Code aesthetic is an inspiration for visual clarity, not an excuse to reinvent user workflow prematurely.
5. **No QML-Ready Model Assumptions**:
   - All presentation models (`QmlPipelineModel`, `QmlSceneModel`, `QmlAnimationModel`) are authored fresh in `src/ovito/gui/qml/models/`. Never couple QML directly to legacy QtWidgets models.
6. **Pragmatic Qt Baseline**:
   - Minimum required Qt version is determined strictly by required APIs (initially Qt 6.8+ for mature `QQuickRhiItem` support). Do not lock to 6.10 unless a concrete API makes it mandatory.

---

## 2. Component Migration Matrix (Phase 0 Audit)

| Classic QtWidgets Component | Modern QML Target | Strategy | Migration Seam |
| :--- | :--- | :--- | :--- |
| `MainWindow` (`gui/desktop`) | `WorkbenchWindow.qml` | **REWRITE** | Wraps `ViewportGrid` + right command panel in QML. |
| `ViewportsPanel` (`gui/desktop`) | `ViewportGrid.qml` | **REWRITE** | Manages 1x1, 2x2, 1+2 layouts in pure QML splitters. |
| `WidgetViewportWindow` (`gui/vpwindow`) | `QuickViewportItem` | **ADAPT** | Subclasses `QQuickRhiItem` and `BaseViewportWindow`. |
| `BaseViewportWindow` (`gui/base`) | `BaseViewportWindow` | **REUSE AS-IS** | Provides battle-tested mouse/keyboard navigation modes. |
| Pipeline Command Page (`gui/desktop`) | `PipelineView.qml` | **REWRITE** | Connected via new `QmlPipelineModel`. |
| `PropertiesEditor` & `ParameterUI` | `ModifierEditorRegistry` | **ADAPT** | Specialized QML editors + generic reflected fallback. |
| `AnimationTrackBar` (`gui/desktop`) | `TimelineView.qml` | **REWRITE** | Connected via new `QmlAnimationModel`. |
| Data Inspector (`gui/desktop`) | `DataInspectorView.qml` | **DEFER** | Targeted for Phase 7 once core parity is achieved. |
| Render Settings Dialog | `RenderSettingsView.qml` | **DEFER** | Targeted for Phase 7. |

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
  2. Audit `gui/desktop` to ensure no hidden shared dependencies exist.
  3. Formulate the initial property editor priority list (Slice, CNA, Color Coding, Polyhedral Template Matching).

---

### Phase 1: Viewport Technical Spike (Critical Risk First)
- **Objective**: Prove the feasibility of rendering OVITO scenes inside `QQuickRhiItem` without UI freezes, crashes, or deadlocks.
- **Key Investigations**:
  1. **QRhi Ownership**: Can OVITO's `FrameGraph` and `SceneRenderer` execute using the `QRhiRenderTarget` and command buffer provided by the Qt Quick Scene Graph, or must `RenderThread` be modified/bypassed?
  2. **Teardown & Deadlock Validation**: Test rapid window closing, tab switching, and hide/show cycles to verify that GPU resource destruction does not deadlock with the GUI thread.
  3. **Multi-Viewport Concurrency**: Verify that 4 simultaneous viewports (Top, Front, Right, Perspective) render reliably with shared geometry buffers.
  4. **Picking & Interaction**: Verify hardware/software object picking and raycasting against the texture-backed viewport.
  5. **HiDPI & Resize**: Verify crisp subpixel scaling on 4K/Retina displays without framebuffer lag.
  6. **Cross-API Smoke Test**: Validate on Linux (Vulkan), macOS (Metal), and Windows (D3D12/Vulkan).
- **Exit Gate**: A standalone, minimal executable running an interactive 3D viewport in QML. If `QQuickRhiItem` encounters insurmountable threading blockers, evaluate fallback approaches (e.g. Vulkan texture sharing or `QQuickWindow` composition) before proceeding.

---

### Phase 2: Minimal QML Shell & Build Scaffolding
- **Objective**: Integrate QML build pipeline cleanly into CMake and create the basic window frame.
- **Deliverables**:
  1. CMake option `OVITO_BUILD_QML_FRONTEND=ON`.
  2. CLI runtime flag: `ovito --gui=qml` launches QML shell; standard `ovito` launches classic QtWidgets.
  3. Base directory: `src/ovito/gui/qml/`.
  4. `WorkbenchWindow.qml` reproducing classic OVITO spatial layout (central viewport area + right-hand command panel).
  5. `Theme.qml` encoding VS Code Dark/Light Modern palettes.

---

### Phase 3: Presentation Models & Command Layer
- **Objective**: Create clean, decoupled C++ presentation adapters for QML.
- **Deliverables**:
  1. `src/ovito/gui/qml/models/QmlPipelineModel`: Wraps scene pipeline into a clean `QAbstractListModel` with roles for name, type, enabled state, and error flags.
  2. `src/ovito/gui/qml/models/QmlAnimationModel`: Exposes frame count, current frame, play/pause state.
  3. `src/ovito/gui/qml/models/QmlModifierRegistryModel`: Discovers available modifiers for instantiation.

---

### Phase 4: Pipeline Stack & Basic Property Inspector
- **Objective**: View, toggle, reorder modifiers, and inspect simple reflected parameters.
- **Deliverables**:
  1. `PipelineView.qml`: Card list with eye-toggle checkboxes, drag reordering, and modifier deletion.
  2. `ModifierEditorRegistry`: C++ registry dispatching to specialized QML editors or generic fallback.
  3. `AutoPropertyEditor.qml`: Reflected controls for numeric floats, booleans, and colors.

---

### Phase 5: Animation Timeline & Viewport Interaction Parity
- **Objective**: Achieve 100% parity for scene navigation and animation playback.
- **Deliverables**:
  1. Connect `QuickViewportItem` to `BaseViewportWindow` input modes (Orbit, Pan, Zoom, Selection Marquee).
  2. `TimelineView.qml`: Smooth scrubbing track bar, frame step buttons, play/pause loop.
  3. Interactive viewport HUD: Camera projection switcher (Perspective vs Orthographic), View presets (Top, Front, Right).

---

### Phase 6: Specialized Property Editors
- **Objective**: Deliver bespoke QML editors for OVITO's highest-frequency modifiers.
- **Deliverables**:
  1. `SliceModifierEditor.qml`: Normal vector inputs, slice plane distance slider, reverse toggle.
  2. `CnaEditor.qml`: Structure type checklist, cutoff selection, adaptive mode.
  3. `ColorCodingEditor.qml`: Gradient selector, range min/max inputs, property chooser.
  4. `PolyhedralTemplateMatchingEditor.qml`: Structure checkboxes, RMSD cutoff slider.

---

### Phase 7: Remaining Desktop Feature Parity
- **Objective**: Reach complete feature parity with the classic QtWidgets interface.
- **Deliverables**:
  1. Menu bar actions (File Import/Export, Scene Reset, Viewport Config).
  2. Overlays & Visual Elements (Simulation Cell display, Coordinate Tripod, Color Legend).
  3. Data Inspector panel (spreadsheet table view of particle properties).
  4. Direct regression testing: execute standard workflows side-by-side in `ovito` and `ovito --gui=qml`.

---

### Phase 8: UX Modernization (Post-Parity Enhancements)
- **Objective**: Introduce advanced workflow accelerators once parity is guaranteed.
- **Deliverables**:
  1. Global Command Palette (`Ctrl+P` fuzzy search for any modifier, action, or preset).
  2. Activity Bar for swift view toggling.
  3. Integrated multi-tab bottom panel (Timeline + Data Inspector + Python Console).

---

### Phase 9: Classic Frontend Retirement (Long-Term)
- Deprecate QtWidgets interface only after QML frontend has matured across multiple release cycles and received community consensus.

---

## 4. Immediate Next Step

Proceed directly to **Phase 0 (Repository & Architecture Audit)** and **Phase 1 (Viewport Technical Spike)** in an isolated prototype branch, focusing exclusively on validating `QQuickRhiItem` lifecycle and threading against OVITO's `RenderThread`.
