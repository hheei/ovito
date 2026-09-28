# OVITO Modern Workbench UI Design Specification

> **Target Version**: OVITO Next-Gen (Qt 6.10+ / Qt Quick & QML)  
> **Aesthetic Baseline**: Modern Scientific Workbench inspired by VS Code Dark/Light Modern  
> **Status**: Approved Architectural Design

---

## 1. Design Vision & Philosophy

OVITO is a world-class scientific visualization and analysis platform for atomistic, particle-based, and materials simulation data. While its C++ backend (data pipelines, asynchronous evaluation, QRhi graphics) is state-of-the-art, its legacy QtWidgets interface dates back to early-2010s desktop paradigms.

The **Modern Workbench** redesign transforms OVITO into a sleek, responsive, developer-grade scientific workbench:
- **Clean & Focused**: Minimalist chrome that steps aside to let the 3D simulation data take center stage.
- **VS Code Workbench Paradigm**: Familiar layout with an Activity Bar, Primary Side Bar, Multi-Viewport Editor Grid with floating HUDs, Secondary Inspector Bar, collapsible Bottom Panel, and a blazing-fast Command Palette.
- **Declarative & Fluid**: Powered by Qt Quick / QML with native 60/120 FPS micro-animations, effortless HiDPI scaling, and native dark/light theme switching.
- **Deep 3D Integration via `QQuickRhiItem`**: Hardware-accelerated 3D viewports natively integrated into the Qt Quick scene graph, enabling glassmorphic overlays and crisp vector annotations without window-tearing or clipping bugs.

---

## 2. Workbench Layout Architecture

The overall layout follows the structured workbench topology of modern developer tools:

```
┌─────────────────────────────────────────────────────────────────────────────────────────────────────────┐
│ [Title Bar]  OVITO Workbench — [Project: lammps_dump.data]        [Search Modifiers... (Ctrl+P)]  [_][□][✕] │
├──────┬───────────────────────┬─────────────────────────────────────────────────┬────────────────────────┤
│ Act. │ Primary Side Bar      │ Center Editor Area (3D Viewports)               │ Secondary Side Bar     │
│ Bar  │                       │ ┌───────────────────────────┬─────────────────┐ │ (Properties Inspector) │
│      │ ▾ PIPELINE            │ │ [Perspective] [Preset ▾]  │ [Top]           │ │                        │
│ [📁] │  ● File Source        │ │ (Floating HUD) [📷][⛶][⚙]  │                 │ │ ▾ Common Neighbor    │
│      │  ● Slice Modifier     │ │                           │                 │ │   Analysis           │
│ [⚡] │  ● CNA                │ │      3D Simulation        │    Simulation   │ │   Cutoff Radius      │
│      │  ● Color Coding       │ │          Cell             │       Cell      │ │   [───●────] 3.20 Å  │
│ [📊] │                       │ │                           │                 │ │                        │
│      │ ▾ SCENE OBJECTS       │ ├───────────────────────────┼─────────────────┤ │   Adaptive Cutoff    │
│ [⚙]  │  ◻ Simulation Cell    │ │ [Front]                   │ [Right]         │ │   [✔] Enable         │
│      │  ◼ Particles (150.2k) │ │                           │                 │ │                        │
│      │  ◻ Bonds              │ │                           │                 │ │ ▾ Display            │
│      │                       │ └───────────────────────────┴─────────────────┘ │   [Color Palette  ▾]   │
├──────┴───────────────────────┴─────────────────────────────────────────────────┴────────────────────────┤
│ Collapsible Bottom Panel (Tabs: [Timeline] [Data Inspector] [Python Console] [Output])                  │
│ [ ▶ ][ ⏸ ][ ⏪ ][ ⏩ ] Frame [ 120 / 500 ]  |---|-------●-----------------------------------------------| │
├─────────────────────────────────────────────────────────────────────────────────────────────────────────┤
│ Status Bar: [✔ Pipeline Ready]  Particles: 150,240 | Backend: Vulkan (NVIDIA RTX) | 60 FPS | Layout: 2x2  │
└─────────────────────────────────────────────────────────────────────────────────────────────────────────┘
```

### 2.1 Component Breakdown

#### A. Activity Bar (Far Left / 48px width)
A compact vertical icon bar for switching Primary Side Bar views:
- **Pipeline & Scene Explorer** (`Alt+1`): Modifiers stack, scene tree, and data sources.
- **Visual Elements & Overlays** (`Alt+2`): Coordinate tripods, color legends, text annotations, camera gizmos.
- **Rendering & Raytracing** (`Alt+3`): Standard renderer, OSPRay/VisRTX settings, viewport image render.
- **Data Table / Inspector** (`Alt+4`): Particle properties, global attributes, bonds, and mesh inspection.
- **Settings & Preferences** (Bottom Gear icon): Toolchain settings, graphics API, theme toggles.

#### B. Primary Side Bar (260px - 380px resizable)
Houses collapsible accordion sections:
- **Active Pipeline Stack**: Direct visualization of the `PipelineListModel`. Items feature:
  - Drag-and-drop reordering handles.
  - Quick toggle eye-icon (enable/disable modifier).
  - Branching indicators and cached calculation status dots.
  - Context menu for modifier export/cloning.
- **Scene Hierarchy**: Tree view of loaded simulation cells, meshes, trajectory lines, and particles.

#### C. Center Editor Area (3D Viewports Grid)
- **Viewport Layout Presets**:
  - Single Viewport (Maximized).
  - 2x2 Quad Grid (Top, Front, Right, Perspective) — default engineering layout.
  - 1+2 Split (1 large perspective + 2 orthogonal views).
  - 1x2 Horizontal / Vertical split.
- **Floating HUD (Heads-Up Display)**:
  - Translucent pill floating at top-left of each viewport:
    - Viewport Type selector (`Perspective`, `Top`, `Front`, `Right`, `Custom Camera`).
    - Projection Mode (`Orthographic` vs. `Perspective`).
    - Shading Mode (`Solid`, `Smooth`, `Wireframe`).
    - Quick Action Icons (`Maximize Viewport`, `Reset Camera`, `Render Snapshot`).
- **Interactive Gizmos**:
  - Semi-transparent Interactive Coordinate Tripod (click axis to align camera).
  - In-viewport selection marquee and slicing plane handles.

#### D. Secondary Side Bar / Inspector (300px - 400px resizable, Far Right)
- Displays parameters for the currently selected pipeline item or scene object.
- **Accordion Groups**: Parameters organized into logical collapsible groups (e.g., *Analysis Parameters*, *Selection Criteria*, *Display Styles*).
- Built with modern fluid controls (numeric sliders with direct typing, color pickers with hex/alpha, combo dropdowns).

#### E. Collapsible Bottom Panel
Toggled via `Ctrl+J` or clicking status items. Tabbed container featuring:
1. **Animation Timeline**:
   - Modern playback controls (Play, Pause, Step Next/Prev Frame, Jump Start/End, Loop mode).
   - Scrubbing track bar with draggable current-frame indicator.
   - Keyframe tick marks with visual cues for modified parameters.
2. **Data Inspector (Spreadsheet)**:
   - High-performance virtualized table displaying particle properties (`Position`, `Velocity`, `Structure Type`, `Color`).
   - Filter, sort, and search bar.
3. **Python Interactive Console**:
   - Embedded interactive REPL for ovito Python API scripting with syntax highlighting.
4. **Log & Pipeline Notifications**:
   - Real-time diagnostics, memory footprint, compute task times.

#### F. Status Bar (24px height)
- **Left**: Task execution state (`Idle`, `Evaluating Pipeline...` with spinning progress indicator).
- **Center**: Current selection summary (`Selected: 24,192 / 150,240 atoms`).
- **Right**:
  - Current viewport render backend (`Vulkan 1.3`, `Metal 3`, `Direct3D 12`).
  - Interactive frame rate (`60 FPS`).
  - Viewport layout switcher button (`1x1`, `2x2`, `1+2`).

#### G. Quick Command Palette (`Ctrl+P` / `Ctrl+Shift+P`)
- Modal search box positioned at the top-center of the workbench.
- Fuzzy searches:
  - Modifiers (e.g., typing `cna` immediately suggests *Common Neighbor Analysis*).
  - View presets (e.g., `view: top`, `view: fit`).
  - Render commands (`render: viewport`, `render: high-res`).
  - File operations (`import`, `export`).

---

## 3. Visual System & Design Tokens (VS Code Modern Theme)

### 3.1 Color Palette

The color system strictly mirrors the clean, non-fatiguing tones of modern developer IDEs:

| Token Name | Dark Modern (`ovito-dark`) | Light Modern (`ovito-light`) | Role |
| :--- | :--- | :--- | :--- |
| `surface-workbench` | `#181818` | `#f3f3f3` | Outermost frame and Activity Bar |
| `surface-panel` | `#1f1f1f` | `#ffffff` | Sidebars and Bottom Panel background |
| `surface-viewport-bg` | `#121212` | `#e8e8e8` | 3D Viewport default background |
| `surface-input` | `#2d2d2d` | `#f8f8f8` | Text fields, spin boxes, dropdowns |
| `surface-hover` | `#2a2d2e` | `#e8e8e8` | Hover state for lists and cards |
| `surface-selected` | `#04395e` | `#e4e6f1` | Selected row / active tab |
| `border-subtle` | `#2b2b2b` | `#e5e5e5` | 1px dividers between panes |
| `border-focus` | `#0078d4` | `#005fb8` | Keyboard focus ring |
| `text-primary` | `#cccccc` | `#1f1f1f` | Primary headings, values, labels |
| `text-secondary` | `#858585` | `#616161` | Units, secondary hints, shortcuts |
| `accent-primary` | `#0078d4` | `#005fb8` | Primary action buttons, timeline scrubber |
| `accent-success` | `#89d185` | `#388a34` | Pipeline cached/ready indicator |
| `accent-warning` | `#cca700` | `#bf8803` | Non-fatal compute warnings |
| `accent-error` | `#f14c4c` | `#e51400` | Syntax error, file load failures |

### 3.2 Typography
- **UI Font**: `Inter`, `-apple-system`, `Segoe UI Variable`, `Roboto`, `sans-serif`.
  - Regular UI labels: `12px` / Weight 400.
  - Section headers: `11px` / Weight 600 / All-caps tracking `+0.5px`.
  - Input fields: `12px` / Weight 400.
- **Monospace Font**: `JetBrains Mono`, `Cascadia Code`, `SF Mono`, `monospace`.
  - Terminal, numeric coordinates, data tables: `11px` / Weight 400.

### 3.3 Geometry & Micro-Interactions
- **Borders**: Crisp, 1px non-distracting hairline borders (`#2b2b2b`). No heavy bevels or drop shadows.
- **Corner Radii**:
  - Floating HUD & Popovers: `6px`.
  - Buttons, Inputs, Cards: `4px`.
  - Tabs: Top-only `3px`.
- **Transitions**:
  - Hover background fade: `100ms ease-out`.
  - Sidebar collapse/expand: `180ms cubic-bezier(0.16, 1, 0.3, 1)`.
  - HUD fade-in on viewport mouse-over: `120ms ease-in`.

---

## 4. Technical Architecture: Qt Quick / QML Integration

### 4.1 Hybrid Technology Stack
- **C++ Core Engine (Unchanged)**:
  - `Ovito::Pipeline`, `Ovito::ModificationNode`, `Ovito::SceneRenderer`, `Ovito::RenderThread`.
- **C++ Presentation / ViewModel Layer**:
  - Exposes QML-friendly models, controller QObjects, and property reflection bridges.
- **QML / Qt Quick 6.10 Frontend**:
  - Declarative layouts, animations, visual styling, responsive panes.

```
┌────────────────────────────────────────────────────────┐
│                   QML View Layer                       │
│  - WorkbenchShell.qml (ActivityBar, Sidebars, Status)   │
│  - ViewportGrid.qml   (Multi-viewport container)        │
│  - PipelineView.qml   (ListView with Drag & Drop)       │
│  - PropertyEditor.qml (Dynamic reflection cards)        │
│  - TimelineView.qml   (Declarative keyframe scrubber)   │
└───────────────────────────┬────────────────────────────┘
                            │ QProperty & Signal/Slot
┌───────────────────────────▼────────────────────────────┐
│              C++ ViewModel & Bridge Layer              │
│  - QuickViewportItem (Subclasses QQuickRhiItem)        │
│  - PipelineListModel (Existing QAbstractListModel)     │
│  - QmlPropertyBridge (Reflects RefTarget fields)       │
│  - WorkbenchController (Layout state, commands)        │
└───────────────────────────┬────────────────────────────┘
                            │ Direct C++ API Calls
┌───────────────────────────▼────────────────────────────┐
│                   OVITO C++ Backend                    │
│  - SceneRenderer, RenderThread, QRhi                   │
│  - TaskScope, FutureWatcher, Async Evaluator           │
└────────────────────────────────────────────────────────┘
```

### 4.2 The 3D Viewport Item (`QuickViewportItem`)

The bridge between OVITO's `SceneRenderer` and Qt Quick uses **`QQuickRhiItem`**:
```cpp
class QuickViewportItem : public QQuickRhiItem, public BaseViewportWindow
{
    Q_OBJECT
    Q_PROPERTY(QString viewportTitle READ viewportTitle NOTIFY viewportTitleChanged)
    Q_PROPERTY(bool isPerspective READ isPerspective WRITE setPerspective NOTIFY cameraChanged)
    // ...
public:
    QuickViewportItem();
    QQuickRhiItemRenderer* createRenderer() override;

protected:
    // Forward pointer/touch events to BaseViewportWindow input modes:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
};
```
- **Zero-Copy Rendering**: `QQuickRhiItem` renders directly into a color texture attached to the Qt Quick Scene Graph.
- **Seamless Layering**: Qt Quick controls (HUD buttons, labels, coordinate gizmo) can be positioned directly on top of `QuickViewportItem` using standard QML anchoring.

### 4.3 Dynamic Property Inspection (Property Reflection Bridge)

To avoid rewriting separate parameter UI forms for hundreds of modifiers:
1. Every modifier derives from `RefTarget` and registers its fields with `PropertyFieldDescriptor`.
2. A generic `QmlPropertyModel` queries these descriptors:
   - Name, Label, Description.
   - Value Type (`Double`, `Integer`, `Boolean`, `Color`, `Vector3`, `Enumeration`).
   - Range constraints (`min`, `max`, `step`, `unit`).
3. QML renders these using a type-matched `ComponentSelector`:
   ```qml
   // PropertyFieldDelegate.qml
   Loader {
       sourceComponent: {
           switch (fieldType) {
               case PropertyType.Double:  return spinSliderComponent;
               case PropertyType.Boolean: return toggleSwitchComponent;
               case PropertyType.Color:   return colorPickerComponent;
               case PropertyType.Enum:    return dropdownComponent;
               default:                   return stringFieldComponent;
           }
       }
   }
   ```

---

## 5. Keyboard Navigation & Accessibility

Following modern IDE conventions:
- `Ctrl+P` / `Cmd+P`: Open Command Palette / Quick Search.
- `Ctrl+B` / `Cmd+B`: Toggle Primary Side Bar visibility.
- `Ctrl+J` / `Cmd+J`: Toggle Bottom Panel visibility.
- `Ctrl+Shift+E`: Focus Pipeline Explorer.
- `Space`: Play / Pause animation playback.
- `Left` / `Right`: Previous / Next animation frame.
- `F`: Frame selection / Fit view to simulation box.
- `1`, `2`, `3`, `4`: Maximize corresponding quadrant viewport.

---

## 6. Summary

This design provides a unified, elegant, and ultra-high-performance interface that feels right at home on macOS, Windows 11, and Linux. By leveraging **Qt 6.10**, **QQuickRhiItem**, and a **VS Code-inspired layout**, OVITO retains 100% of its world-class scientific computational muscle while delivering a modern, delightful user experience.
