# OVITO Modern Workbench UI Design Specification

> **Target Version**: OVITO Next-Gen (Qt 6.10+ / Qt Quick & QML)
>
> **Core Principle**: Interaction Parity First, Visual Modernization in Lockstep
>
> **Visual Inspiration**: VS Code Modern Theme (Clean, focused, dark/light modern)
>
> **Status**: Proposed Design; the Qt Quick viewport rendering bridge is validated on the Linux/OpenGL backend
> (see [UI_PHASE1_SPIKE.md](UI_PHASE1_SPIKE.md)); cross-backend, picking and HiDPI validation are still open
>
> **Execution Plan**: [UI_PLAN.md](UI_PLAN.md)

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
- **Rule 2**: `qml` **MUST NEVER** `#include` headers from `gui/desktop` or link against its `Gui` target. A common executable may select either frontend without introducing that dependency into the QML module.
- **Rule 3**: If business logic currently lives in `gui/desktop` but is needed by `qml`, extract the shared operation into `gui/base` and keep frontend-specific dialogs in their respective frontends. Preserve the classic frontend's behavior during extraction.
- **Rule 4**: Audit models and actions already in `gui/base` before adding QML adapters under `src/ovito/gui/qml/models/`. Choose direct reuse, composition/proxy adaptation, or extraction of shared operations based on actual API gaps; do not duplicate pipeline mutation logic to change its presentation.
- **Rule 5**: Preserve numerical algorithms, file readers, and asynchronous evaluation semantics. Any rendering integration changes in `core/rendering` or `core/viewport` must be identified by the spike and verified against both frontends and headless rendering.

The Qt baseline follows the root CMake configuration and `AGENTS.md`: Qt 6.10+, with matching private headers. Target backends are Metal on macOS ARM64, D3D12 on Windows x86_64, and Vulkan on Linux x86_64/ARM64.

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
| `ViewportsPanel` (Grid container) | `ViewportGrid.qml` | Preserves audited split, resize, and maximize behavior; 1x1, 2x2, and 1+2 are initial layouts. |
| `WidgetViewportWindow` | `QuickViewportItem` + viewport adapter | Composes a `BaseViewportWindow` subclass for input; renders via `QQuickRhiItem`. |
| Pipeline Command Page | `PipelineView.qml` | Uses the shared pipeline model through the adaptation selected in Phase 0. |
| `PropertiesEditor` & `ParameterUI` | `ModifierEditorRegistry` | Specialized QML editors for complex tools + generic fallback. |
| `AnimationTrackBar` / `TimeSlider` | `TimelineView.qml` | Driven by `QmlAnimationModel`; QML declarative track & scrubber. |

### 3.2 Interaction States and Layout Behavior

- **Empty scene**: Offer Import, keep unavailable editing commands disabled, and explain why they are unavailable.
- **Evaluation in progress**: Show task progress and cancellation where supported. Distinguish pending results from ready results; a retained previous frame must not be presented as the completed current frame.
- **Failure or cancellation**: Show an actionable error at the affected pipeline item with details available. Cancellation ends the busy state without discarding the editable scene.
- **Keyboard and accessibility**: Preserve platform-native shortcuts and command enablement. Define focus order between viewport, pipeline, inspector, and timeline; expose accessible names, roles, values, and visible focus. Text editing must take precedence over conflicting global shortcuts.
- **Scientific parameter entry**: Provide direct numeric entry with units, precision, and bounds. Sliders supplement numeric fields; they do not replace them. Invalid input must not silently alter a value.
- **Small windows and HiDPI**: Allow panel resizing and inspector scrolling while keeping navigation and import controls reachable. Phase 2 establishes a documented minimum window size and checks long labels, both themes, and fractional display scaling. Avoid layout animation that interferes with precise pointer input.

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
`BaseViewportWindow` already provides shared input handling, but inherits `QObject` through `ViewportWindow`. `QQuickRhiItem` also inherits `QObject`. A single class must not inherit both; use composition:

```text
GUI thread
QuickViewportItem : QQuickRhiItem
  └── owns QuickViewportAdapter : BaseViewportWindow
        ├── forwards input to existing viewport input modes
        ├── implements viewport size, visibility, cursor, and picking interfaces
        └── participates in existing scene preparation and frame-graph generation

                 prepared state transferred at synchronization
                                    │
                                    ▼
Qt Quick rendering callbacks
QuickViewportRenderer : QQuickRhiItemRenderer
  └── consumes prepared state using Qt Quick's QRhi, command buffer, and target
```

These names describe responsibilities, not frozen interfaces. The adapter forwards press/move/release, double-click, wheel, key, focus-loss, and context-menu behavior. Item visibility, geometry, and scene attachment changes must map to the shared viewport lifecycle. Logical input coordinates and device-pixel render coordinates must remain distinct.

### 4.3 Threading and Resource Contract to Validate

- Keep dataset mutation and input handling on the GUI thread. Reuse OVITO scene preparation and asynchronous evaluation; rendering callbacks consume prepared frame state rather than evaluating pipelines or reading mutable QML state.
- Define the transfer and lifetime of frame graphs, renderer configuration, and picking results. Qt Quick synchronization is the handoff point for item state; validate buffer/resource sharing only within the owning QRhi instance.
- Qt Quick owns its QRhi and command buffer. The renderer must not destroy them or submit work through the classic `RenderThread` using resources owned by Qt Quick.
- Specify resource release and recreation for hide/show, scene-graph invalidation, window closing, and movement between windows. GPU resources must be released on their owning rendering thread.
- Preserve picking semantics while avoiding a circular wait between GUI and rendering threads. The spike must exercise picking during resize, evaluation, and teardown, not only during steady-state rendering.

Phase 1 must record the required changes to the existing renderer, the tested backend matrix, and lifecycle results before these interfaces are finalized. An alternative integration approach requires a revised design covering all target backends.

---

## 5. Presentation Models and Editing Commands

Audit the existing `gui/base` models and actions first. `PipelineListModel` already exposes named roles, a selection model, and an invokable drag-and-drop operation; `AvailableModifiersModel` contains modifier discovery and applicability logic. These are reuse candidates, not proof that every role and operation can be bound unchanged in QML.

New adapters live under `src/ovito/gui/qml/models/`. The `Qml*Model` names below describe presentation contracts; Phase 0 decides whether each needs a new class, a proxy, or direct binding. Shared operations remain the single implementation of pipeline mutations.

### 5.1 Pipeline and Modifier Presentation

- Expose title, item type, enabled state, selection, and evaluation status/details to `PipelineView.qml`.
- Preserve source and visual-element rows, modifier groups, multi-selection, shared modifiers, and valid drop boundaries identified in the parity audit. A pipeline row is not necessarily a movable modifier.
- Route insertion, toggling, reordering, deletion, and grouping through shared model operations or extracted commands. Validate the current target at invocation time; transient row numbers must not identify objects across deferred operations or model rebuilds.
- Synchronize selection and status after undo/redo, asynchronous evaluation, and dataset replacement. Cancel edits whose target has been removed and release old object references when switching datasets.
- Adapt modifier discovery from `AvailableModifiersModel`, including categories, templates, and applicability to the current input; a class registry alone is insufficient.
- Audit scene/pipeline selection separately. Add a scene adapter only if the shared selection API cannot provide the required presentation.

### 5.2 `QmlAnimationModel`

Expose the animation interval, current time/frame, playback state, speed, and loop options using existing animation settings and time conversions. Support non-zero starting frames and distinguish source trajectory frames from the scene animation interval.

Expose editable controller tracks and keyframe selection. Key movement, deletion, parameter animation entry points, and auto-key behavior must use existing controller operations and undo transactions. Keyframe marks alone do not meet animation parity.

### 5.3 Shared Command and Undo Contract

Reuse `gui/base` actions and OVITO's undo infrastructure through a QML-facing bridge. Preserve action enablement, checked state, labels, shortcuts, and error handling; frontend dialogs gather inputs and then invoke the shared operation.

| Edit kind | Required behavior |
| :--- | :--- |
| Discrete edit (toggle, insert, delete, reorder) | One named undo transaction; validate targets and roll back failed mutations. |
| Continuous edit (numeric drag, slider, keyframe move) | Begin on edit start, update within one logical transaction, commit on acceptance/release, cancel and restore on Escape or interrupted interaction. |
| Text entry | Commit valid input on acceptance; define focus-loss behavior consistently with the classic control. Invalid input leaves the stored value unchanged. |
| Dataset replacement or target deletion | End/cancel the active edit before releasing its target; never let a deferred write reach a different object. |

One accepted gesture produces one undo step. Undo/redo must restore affected model state and keep selection coherent; returning a deleted selected modifier through undo must restore the expected selection. Phase 3 verifies these behaviors before QML controls start mutating datasets.

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

Fallback coverage is explicit and incremental. Reflection does not guarantee that every parameter is editable. Phase 0 records each user-editable field of the initial modifier set as supported by fallback, assigned to a specialized editor, or unsupported with a reason and delivery phase.

### 6.3 Initial Fallback Contract

| Parameter kind | Initial support and limits |
| :--- | :--- |
| Plain numeric fields | Integer and floating-point entry using available units, precision, and bounds metadata. |
| Boolean and color fields | Basic controls with shared validation, notification, and undo behavior. |
| Scalar animation controllers | Read/write through the controller at the current animation time; honor auto-key behavior and expose the animation entry point. Never replace the controller reference with a scalar value. |
| Enums, property selectors, vectors/matrices, reference collections, plots, and custom actions | Require audited metadata adapters or specialized editors; do not infer editability from a reflected value alone. |

Use OVITO property descriptors and controller APIs. Specialized and fallback editors share the command/undo contract, refresh on time and object changes, and enforce read-only state and parameter bounds.

An unsupported user-editable parameter must be visibly identified with a reason; show its value read-only where a meaningful representation exists. It remains a parity gap until supported. Phase 4 may ship a partial inspector for development, but Phase 7 cannot claim complete parity with unresolved coverage gaps.

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

## 8. Completion Criteria and Source References

The [implementation plan](UI_PLAN.md) defines phase gates and the initial parity matrix. Phase 0 expands that matrix against the classic frontend's audited feature set for the same build configuration. Complete parity requires evidence for each applicable row; an explicitly deferred feature still prevents a complete-parity claim. Workflow enhancements follow that gate, and retirement of the classic frontend remains a separate future decision.

Source anchors for implementation and audit:

- [ViewportWindow](../../src/ovito/core/viewport/ViewportWindow.h) and [BaseViewportWindow](../../src/ovito/gui/base/viewport/BaseViewportWindow.h): object inheritance and viewport/input contracts.
- [RenderThread](../../src/ovito/core/rendering/RenderThread.h): existing QRhi ownership, target lifecycle, and picking.
- [PipelineListModel](../../src/ovito/gui/base/mainwin/PipelineListModel.h) and [AvailableModifiersModel](../../src/ovito/gui/base/mainwin/AvailableModifiersModel.h): shared models and pipeline operations.
- [FloatParameterUI](../../src/ovito/gui/desktop/properties/FloatParameterUI.cpp) and [NumericalParameterUI](../../src/ovito/gui/desktop/properties/NumericalParameterUI.cpp): reference behavior for controller values, units, bounds, and undo.
- [AnimationTrackBar](../../src/ovito/gui/desktop/widgets/animation/AnimationTrackBar.cpp) and [MainWindowUI](../../src/ovito/gui/desktop/mainwin/MainWindowUI.cpp): reference behavior for key editing and session lifecycle. Desktop sources are audit references, not dependencies of the QML module.
