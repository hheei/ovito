# OVITO Modern Workbench UI Implementation Plan

> **Scope**: Phased migration to modern Qt Quick / QML frontend  
> **Guiding Principle**: Non-destructive, incremental, side-by-side coexistence  
> **Status**: Approved Roadmap

---

## 1. Principles & Non-Negotiable Rules

1. **Zero Core Disruption**:
   - The numerical engine, pipeline evaluator, file I/O loaders, and `QRhi` render thread under `src/ovito/core/`, `src/ovito/particles/`, and `src/ovito/stdobj/` must remain unchanged and unpolluted by QML details.
2. **Dual-Frontend Coexistence**:
   - The battle-tested QtWidgets frontend (`src/ovito/gui/desktop/`) remains 100% intact and default during the transition.
   - The new QML frontend lives in a dedicated module (`src/ovito/gui/qml/`).
   - The user selects the frontend via command line flag (`ovito --gui=qml`) or CMake build option (`OVITO_BUILD_QML_FRONTEND=ON`).
3. **Milestone-Gated Deliverables**:
   - Each phase delivers an executable, testable milestone that can be verified natively on Linux, macOS, and Windows.
4. **No Code Before Consensus**:
   - Architecture and component API contracts are locked before substantive C++/QML implementation begins.

---

## 2. Phase Breakdown & Milestones

```
  ┌────────────────────────────────────────────────────────────────────────────────┐
  │                               PHASE ROADMAP                                    │
  └────────────────────────────────────────────────────────────────────────────────┘
    Phase 0: Build Infrastructure & Module Setup
        │
        ▼
    Phase 1: 3D Viewport RHI Bridge (`QuickViewportItem`)
        │
        ▼
    Phase 2: Modern Workbench Shell (VS Code Layout & Design Tokens)
        │
        ▼
    Phase 3: Pipeline Stack & Command Palette (`Ctrl+P`)
        │
        ▼
    Phase 4: Animation Timeline & Collapsible Bottom Panel
        │
        ▼
    Phase 5: Dynamic Property Reflection Inspector (Parameter Controls)
        │
        ▼
    Phase 6: Multi-Platform Validation & Polish
```

---

### Phase 0: Build Infrastructure & Module Setup
**Goal**: Integrate QML build dependencies into CMake without breaking existing compilation.

- **Tasks**:
  1. Add CMake option `OVITO_BUILD_QML_FRONTEND` (default `ON` when Qt6Quick is available).
  2. Create source directory tree:
     ```
     src/ovito/gui/qml/
     ├── CMakeLists.txt
     ├── QmlGuiPlugin.cpp / .h
     ├── components/         # Shared atomic QML controls (Buttons, Sliders, Cards)
     ├── shell/              # Workbench layout (ActivityBar, Sidebars, Status)
     ├── viewport/           # 3D Viewport QQuickRhiItem bridge & HUD overlays
     ├── pipeline/           # Pipeline stack view and modifier delegates
     ├── inspector/          # Dynamic property reflection panels
     ├── timeline/           # Animation playback and track scrubber
     └── themes/             # Design tokens (Dark Modern / Light Modern)
     ```
  3. Configure Qt 6 QML module registration via `qt_add_qml_module`.
  4. Implement CLI dispatch flag: `ovito --gui=qml` loads the QML workbench shell; standard `ovito` opens classic QtWidgets.
- **Deliverable / Verification**:
  - `cmake --build --preset native --target ovito` succeeds with zero warnings.
  - Running `ovito --gui=qml --version` prints version info and exits cleanly.

---

### Phase 1: 3D Viewport RHI Bridge (`QuickViewportItem`)
**Goal**: Prove hardware-accelerated 3D scene rendering inside Qt Quick with zero visual artifacts.

- **Tasks**:
  1. Subclass `QQuickRhiItem` to create `QuickViewportItem`.
  2. Implement `QuickViewportRenderer : public QQuickRhiItemRenderer`:
     - Bind OVITO's existing `RenderThread` and `SceneRenderer` to the `QRhiRenderTarget` provided by `QQuickRhiItem`.
     - Implement swapchain synchronization (ensuring GPU commands finish before the scene graph samples the texture).
  3. Implement pointer event dispatch:
     - Map QML mouse/wheel/gesture events into OVITO's `ViewportInputMode` (Orbit, Pan, Zoom, Selection Marquee).
  4. Test with large particle datasets (100k+ atoms) to verify 60+ FPS stability.
- **Deliverable / Verification**:
  - A standalone test window displaying a 3D simulation cell and rotating/zooming smoothly via mouse drag inside a QML scene.

---

### Phase 2: Modern Workbench Shell (VS Code Layout & Tokens)
**Goal**: Establish the full visual frame, design tokens, and multi-viewport layout.

- **Tasks**:
  1. **Theme System (`Theme.qml`)**:
     - Implement VS Code Dark Modern (`#181818`, `#1f1f1f`, `#2b2b2b`, `#0078d4`) and Light Modern palettes.
     - Support runtime theme switching and system preference auto-detection.
  2. **Shell Structure**:
     - `ActivityBar.qml`: Left vertical icon rail (Pipeline, Overlays, Rendering, Data, Settings).
     - `PrimarySidebar.qml`: Resizable pane with accordion section headers.
     - `SecondarySidebar.qml`: Right-side inspector pane with collapse toggle.
     - `StatusBar.qml`: Sleek 24px bottom bar with live state chips.
  3. **Viewport Grid Layout**:
     - Implement responsive splitters allowing 1x1, 2x2, 1+2, and 1x2 viewport configurations.
     - Viewport maximization toggle (double-click viewport or press `1`-`4`).
  4. **Floating Viewport HUD**:
     - Semi-transparent glass pill anchored in the viewport top-left: view mode dropdown, projection toggle, fit-to-view button.
- **Deliverable / Verification**:
  - Full application window renders with the VS Code aesthetic, collapsible panels, and a live 2x2 viewport grid.

---

### Phase 3: Pipeline Stack & Command Palette (`Ctrl+P`)
**Goal**: Enable viewing, toggling, reordering, and adding modifiers via modern declarative UI.

- **Tasks**:
  1. **Pipeline View**:
     - Bind QML `ListView` directly to the existing `PipelineListModel`.
     - Create modern modifier card delegates: icon, title, active checkbox/eye, delete button, drag handle.
     - Implement drag-and-drop reordering with smooth list layout transitions.
  2. **Modifier Command Palette (`Ctrl+P` / `Ctrl+Shift+P`)**:
     - Floating modal search bar with blurred backdrop.
     - Real-time fuzzy query over all registered OVITO modifier classes (CNA, Slice, Cluster Analysis, etc.).
     - Pressing `Enter` instantiates and appends the modifier to the active pipeline.
  3. **Scene Tree Explorer**:
     - Tree view showing data sources, simulation cell, particles, and visual elements.
- **Deliverable / Verification**:
  - User can press `Ctrl+P`, type `slice`, press `Enter`, and see the *Slice Modifier* appear in the pipeline stack with its effect immediately visible in the 3D viewport.

---

### Phase 4: Animation Timeline & Collapsible Bottom Panel
**Goal**: Deliver a fluid animation scrubbing and data exploration experience.

- **Tasks**:
  1. **Timeline Controls**:
     - Play / Pause, Next Frame, Previous Frame, First/Last Frame buttons.
     - FPS setting and playback mode (Loop / Once).
  2. **Scrubber Track**:
     - Smooth draggable thumb with magnetic snapping to keyframes.
     - Numeric frame counter (`Current / Total`).
  3. **Bottom Panel Tab Container**:
     - Tabs for *Timeline*, *Data Table*, *Python Console*, and *Output Log*.
     - Quick toggle hotkey (`Ctrl+J`) with smooth slide animation.
- **Deliverable / Verification**:
  - Scrubbing the timeline smoothly updates the multi-frame trajectory in all active viewports at interactive speeds.

---

### Phase 5: Dynamic Property Reflection Inspector (Parameter Controls)
**Goal**: Enable parameter tuning for all modifiers without writing monolithic manual UI forms.

- **Tasks**:
  1. **C++ Reflection Bridge (`QmlPropertyBridge`)**:
     - Inspects `RefTarget` and exposes property values, limits, steps, and units to QML.
  2. **Atomic Control Library**:
     - `NumericSlider.qml`: Combined drag slider and direct-edit number box.
     - `ToggleSwitch.qml`: Sleek iOS/Fluent toggle switch.
     - `ColorPickerField.qml`: Color chip opening an inline palette/picker.
     - `Vector3Input.qml`: Inline X, Y, Z grouped input fields.
     - `EnumDropdown.qml`: Dropdown menu for enumerated types.
  3. **Inspector Container**:
     - Automatically generates categorized accordion cards based on the selected modifier's reflected fields.
     - Two-way binding: changing a slider immediately marks pipeline cache dirty and updates the 3D view.
- **Deliverable / Verification**:
  - Selecting *Slice Modifier* displays its plane normal, distance slider, and invert checkbox in the inspector; adjusting the distance slider interactively cuts the particles in the viewport.

---

### Phase 6: Multi-Platform Validation & Polish
**Goal**: Ensure tier-1 quality across Linux, macOS, and Windows.

- **Tasks**:
  1. **Vulkan Backend Testing (Linux x86_64 & ARM64)**:
     - Verify on Wayland and X11 without flickering or driver crashes.
  2. **Metal Backend Testing (macOS Apple Silicon)**:
     - Verify Retina display scaling and Metal 3 swapchain integration.
  3. **Direct3D 12 Testing (Windows x64)**:
     - Verify D3D12 device sharing between `RenderThread` and Qt Quick scene graph.
  4. **Performance & Memory Audit**:
     - Ensure idle CPU usage is < 1%.
     - Benchmark frame rate during rapid slider interaction.
- **Deliverable / Verification**:
  - CI passes on all platforms; UI is responsive, robust, and visually cohesive.

---

## 3. Risk Management & Mitigations

| Risk | Probability | Impact | Mitigation Strategy |
| :--- | :---: | :---: | :--- |
| **GPU context sharing conflicts** between `RenderThread` and Qt Quick Scene Graph | Medium | High | `QQuickRhiItem` was specifically designed by the Qt team for this exact pattern; texture-level sharing avoids raw driver context entanglement. |
| **Huge modifier parameter diversity** difficult to reflect generically | Medium | Medium | Implement the generic reflection bridge for 90% of standard fields; provide an escape-hatch mechanism for specialized custom delegates when necessary. |
| **Large dataset UI thread blocking** | Low | High | OVITO's pipeline already computes asynchronously on worker threads via `TaskScope`. The UI thread only receives finished visual geometry. |
| **User resistance to UI change** | Medium | Medium | Keep classic QtWidgets accessible via `--gui=classic` indefinitely until the QML workbench achieves feature parity and user acclaim. |

---

## 4. Next Step Recommendation

With this plan and design formally approved:
- Begin **Phase 0**: Set up the CMake scaffolding and directory layout for `src/ovito/gui/qml/` without touching existing QtWidgets runtime logic.
