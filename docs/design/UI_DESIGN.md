# OVITO Modern Workbench UI Design Specification

> **Target Version**: OVITO Next-Gen (Qt 6.8+ / Qt Quick & QML)  
> **Core Principle**: Interaction Parity First, Visual Modernization in Lockstep  
> **Visual Inspiration**: VS Code Modern Theme (Clean, focused, dark/light modern)  
> **Status**: Source-Grounded Architectural Design

---

## 1. Design Vision & Guiding Principles

OVITO is a precision scientific visualization and data analysis platform for atomistic, molecular, and particle simulations. Its computational core is exceptionally robust. The goal of this UI modernization is **not to reinvent OVITO**, but to provide a modernized, responsive, and maintainable frontend while strictly respecting existing user workflows.

### 1.1 The Three-Tier Design Hierarchy

```
  ┌────────────────────────────────────────────────────────────────────────┐
  │  Level 1: Interaction Parity (Top Priority — Phase 1-7)                 │
  │  - Identical pipeline ergonomics (modifier stack, eye toggles, order)  │
  │  - Identical viewport navigation (Orbit, Pan, Zoom, Perspective/Ortho) │
  │  - Identical animation timeline controls and keyframe behavior         │
  │  - Identical keyboard shortcuts and command semantics                  │
  ├────────────────────────────────────────────────────────────────────────┤
  │  Level 2: Visual Modernization (Phase 2-7)                             │
  │  - Crisp HiDPI rendering with vector icons and modern typography       │
  │  - VS Code Modern-inspired color palettes (Dark Modern & Light Modern) │
  │  - Refined margins, subtle hairline dividers (1px), clean controls     │
  ├────────────────────────────────────────────────────────────────────────┤
  │  Level 3: Workflow Enhancements (Phase 8+ / Defer until Parity Met)     │
  │  - Global Command Palette (`Ctrl+P` fuzzy search for modifiers)        │
  │  - Activity Bar for extended tooling                                   │
  │  - Integrated multi-view dockable bottom panel                         │
  └────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Architectural Boundaries & Dependency Rules

### 2.1 Strict Dependency Direction
To prevent architectural rot and ensure true frontend independence, the new QML frontend must strictly obey this dependency boundary:

```
         ┌───────────────────────────────────────┐
         │            src/ovito/core/            │
         │ (Computational backend, math, render) │
         └───────────────────▲───────────────────┘
                             │
         ┌───────────────────┴───────────────────┐
         │          src/ovito/gui/base/          │
         │  (BaseViewportWindow, generic input)  │
         └─────────▲───────────────────▲─────────┘
                   │                   │
                   │                   │  (ALLOWED)
                   │                   │
         ┌─────────┴─────────┐       ┌─┴─────────────────────┐
         │src/ovito/gui/     │       │src/ovito/gui/qml/     │
         │desktop/           │       │(Modern QML frontend)  │
         │(Classic QtWidgets)│       └───────────────────────┘
         └───────────────────┘                   ▲
                   ▲                             │
                   └─────────── ✗ ───────────────┘
                     STRICTLY FORBIDDEN DEPENDENCY:
                     qml MUST NOT depend on desktop!
```

- **Rule 1**: `qml` may depend on `gui/base` and `core`.
- **Rule 2**: `qml` **MUST NEVER** `#include` headers from `gui/desktop` or link against `ovito_desktop`.
- **Rule 3**: If a service or logic currently lives in `gui/desktop` but is needed by `qml`, it must first be extracted and refactored into `gui/base` as a shared abstraction.
- **Rule 4**: All QML-facing presentation models must be authored independently under `src/ovito/gui/qml/models/`. Never assume an existing desktop model is directly QML-ready.

---

## 3. Workbench Layout Architecture

The layout preserves classic OVITO spatial semantics while dressing them in clean, modern workbench chrome:

```
┌─────────────────────────────────────────────────────────────────────────────────────────────────────────┐
│ [Menu / Toolbar]  File   Edit   View   Modifiers   Rendering   Help               [Window Controls]     │
├─────────────────────────────────────────────────┬───────────────────────────────────────────────────────┤
│ Center Viewport Area (1x1, 2x2, or 1+2 Grid)    │ Right Command & Property Panel                        │
│ ┌───────────────────────────┬─────────────────┐ │ ┌───────────────────────────────────────────────────┐ │
│ │ [Perspective ▾] [📷][⛶]    │ [Top ▾]         │ │ │ Tabs: [Modify]  [Render]  [Overlays]  [Utilities] │ │
│ │                           │                 │ │ ├───────────────────────────────────────────────────┤ │
│ │      3D Simulation        │    Simulation   │ │ │ ▾ Pipeline Stack                                  │ │
│ │          Cell             │       Cell      │ │ │   [✔] Slice Modifier                              │ │
│ │                           │                 │ │ │   [✔] Common Neighbor Analysis (CNA)              │ │
│ │                           │                 │ │ │   [✔] Color Coding                                │ │
│ ├───────────────────────────┼─────────────────┤ │ │   ●   File Source (lammps.data)                   │ │
│ │ [Front ▾]                 │ [Right ▾]       │ │ │   [+ Add Modifier...]  [Trash]  [Snippet]         │ │
│ │                           │                 │ │ ├───────────────────────────────────────────────────┤ │
│ │                           │                 │ │ │ ▾ Modifier Parameters (Specialized or Reflected)  │ │
│ │                           │                 │ │ │   Cutoff Radius:   [───●────] 3.20 Å              │ │
│ │                           │                 │ │ │   [✔] Adaptive Cutoff                             │ │
│ └───────────────────────────┴─────────────────┘ │ └───────────────────────────────────────────────────┘ │
├─────────────────────────────────────────────────┴───────────────────────────────────────────────────────┤
│ Animation Timeline Bar                                                                                  │
│ [ ▶ ][ ⏸ ][ ⏪ ][ ⏩ ] Frame: [ 120 / 500 ]  |---|-------●-------------------------------------------| │
├─────────────────────────────────────────────────────────────────────────────────────────────────────────┤
│ Status Bar: [✔ Pipeline Ready]  Particles: 150,240 | Backend: Vulkan (RHI) | 60 FPS                      │
└─────────────────────────────────────────────────────────────────────────────────────────────────────────┘
```

### 3.1 Spatial Parity Mapping

| Classic OVITO Component | Modern QML Equivalent | Architecture Strategy |
| :--- | :--- | :--- |
| `MainWindow` (QtWidgets) | `WorkbenchWindow.qml` | Rewrite in QML shell; retains classic menu bar + side panel topology. |
| `ViewportsPanel` (Grid container) | `ViewportGrid.qml` | Re-implements 1x1, 2x2, 1+2 splitters with fluid QML transitions. |
| `WidgetViewportWindow` | `QuickViewportItem` | Adapts `BaseViewportWindow` input handling; renders via `QQuickRhiItem`. |
| Pipeline Command Page | `PipelineView.qml` | Driven by a dedicated `QmlPipelineModel`. |
| `PropertiesEditor` & `ParameterUI` | `ModifierEditorRegistry` | Specialized QML editors for complex tools + generic fallback. |
| `AnimationTrackBar` / `TimeSlider` | `TimelineView.qml` | Driven by `QmlAnimationModel`; QML declarative track & scrubber. |

---

## 4. 3D Viewport Integration Architecture

### 4.1 RHI Ownership Challenge (The Critical Spike)
In classic OVITO:
- Each `WidgetViewportWindow` wraps a native `QWindow`.
- All viewports in a window share a single `RenderThread` which owns the `QRhi` instance.
- **Teardown hazard**: Render targets must be released before the native window surface is destroyed, otherwise the GUI thread and `RenderThread` deadlock.

In Qt Quick:
- `QQuickRhiItem` renders directly into a texture sampled by the Qt Quick Scene Graph.
- The **Scene Graph owns the QRhi** and the rendering thread lifecycle.

```
Classic OVITO:                   Qt Quick Candidate:
WidgetViewportWindow             QuickViewportItem
      │                                │
      ▼                                ▼
   QWindow                      QQuickRhiItem
      │                                │
      ▼                                ▼
 RenderTarget                    QRhiRenderTarget (texture-backed)
      │                                │
      ▼                                ▼
OVITO RenderThread owns QRhi    Qt Quick Scene Graph owns QRhi
```

### 4.2 Integration Seam via `BaseViewportWindow`
`QuickViewportItem` leverages `BaseViewportWindow` (from `gui/base`), which already decouples generic mouse/keyboard navigation logic from `QWidget`:
```cpp
class QuickViewportItem : public QQuickRhiItem, public BaseViewportWindow
{
    Q_OBJECT
    // Pointer and wheel events map directly into BaseViewportWindow methods:
    void mousePressEvent(QMouseEvent* event) override { BaseViewportWindow::mousePressEvent(event); }
    void mouseMoveEvent(QMouseEvent* event) override { BaseViewportWindow::mouseMoveEvent(event); }
    void mouseReleaseEvent(QMouseEvent* event) override { BaseViewportWindow::mouseReleaseEvent(event); }
    void wheelEvent(QWheelEvent* event) override { BaseViewportWindow::wheelEvent(event); }
};
```
*Note: The exact threading bridge and swapchain synchronization must be validated in Phase 1 (Technical Spike) before finalizing this class.*

---

## 5. Presentation Layer & Models (Zero Assumption)

No existing QtWidgets model is assumed to be QML-ready. Independent presentation adapters are authored under `src/ovito/gui/qml/models/`:

### 5.1 `QmlPipelineModel`
```
OVITO Pipeline / ModificationNode
              │
              ▼
QmlPipelineModel : public QAbstractListModel
  ├── Roles: title, itemType, isEnabled, isSelected, statusIcon, hasError
  ├── Methods: moveItem(from, to), toggleItem(index), deleteItem(index)
              │
              ▼
QML ListView (PipelineView.qml)
```

### 5.2 `QmlAnimationModel`
Exposes trajectory frame counts, current time/frame, playback state (playing, paused), keyframe marks, and playback speed/loop options.

---

## 6. Property Inspector: Specialized Registry + Reflected Fallback

### 6.1 Why 100% Automatic Reflection Fails
QtWidgets implementation reveals that parameter UI involves extensive dynamic logic:
- Conditional visibility (e.g. checkbox revealing subsequent slider).
- Data-dependent choices (dropdown populated by input file attributes).
- Custom action buttons (e.g., "Re-center", "Invert Normal").
- 2D/3D matrix and affine transformation manipulators.
- Specialized curve and histogram plots (e.g. RDF, Bond Length Distribution).

### 6.2 The Hybrid Architecture
```
Modifier Selected in Pipeline
              │
              ▼
   ModifierEditorRegistry
              │
    Is specialized QML editor registered?
          ├── YES ──► Load specialized editor (e.g., SliceModifierEditor.qml, CnaEditor.qml)
          │
          └── NO  ──► Fallback to AutoPropertyEditor.qml (Reflected basic sliders/toggles)
```

This guarantees 100% parameter coverage on Day 1 via reflection, while providing an unconstrained path to deliver bespoke, pixel-perfect UX for OVITO's most critical modifiers.

---

## 7. Visual Design Tokens (VS Code Modern Inspiration)

Tokens are codified in `Theme.qml`:

| Token | Dark Modern | Light Modern | Role |
| :--- | :--- | :--- | :--- |
| `surfaceWorkbench` | `#181818` | `#f3f3f3` | Window frame background |
| `surfacePanel` | `#1f1f1f` | `#ffffff` | Command panel & sidebar background |
| `surfaceInput` | `#2d2d2d` | `#f8f8f8` | Text fields, spin boxes |
| `surfaceSelected` | `#04395e` | `#e4e6f1` | Selected modifier item |
| `borderSubtle` | `#2b2b2b` | `#e5e5e5` | 1px hairline structural dividers |
| `borderFocus` | `#0078d4` | `#005fb8` | Keyboard focus ring |
| `accentPrimary` | `#0078d4` | `#005fb8` | Active toggles, scrubber thumb |
| `textPrimary` | `#cccccc` | `#1f1f1f` | Primary text |
| `textSecondary` | `#858585` | `#616161` | Units, hints, inactive labels |

---

## 8. Summary

This design specification anchors the new frontend in **strict interaction parity with classic OVITO**, establishes **impenetrable architectural boundaries** preventing entanglement with `gui/desktop`, tackles the **QRhi ownership spike upfront**, and deploys a **pragmatic hybrid property inspector** that handles real-world scientific tool complexity.
