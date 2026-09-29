# OVITO Modern Workbench UI — Phase 0 Audit Record

> **Companion documents**: [UI_DESIGN.md](UI_DESIGN.md), [UI_PLAN.md](UI_PLAN.md), [UI_PHASE1_SPIKE.md](UI_PHASE1_SPIKE.md), [UI_FRONTEND_REVIEW.md](UI_FRONTEND_REVIEW.md)
>
> **Status**: Executed for the three audits that gate Phase 1 (build entry, viewport/rendering, shared models).
> The remaining Phase 0 items (full action/editor inventory and the expanded parity matrix) are still pending.

This document records what was actually inspected in the repository (with source references), what was decided,
and which changes to existing code the audit authorized. Where a decision was implemented, the verification that
backs it is listed in [UI_PHASE1_SPIKE.md](UI_PHASE1_SPIKE.md).

---

## 1. Build Entry and Frontend Switch Point

### Findings

1. `src/main/Main.cpp` includes `ovito/gui/desktop/GUI.h` and `ovito/gui/desktop/app/GuiApplication.h` and
   unconditionally constructs `Ovito::OORef<Ovito::GuiApplication>`. There is no runtime frontend selection and no
   frontend-neutral application class.
2. `StandaloneApplication` (`src/ovito/core/app/StandaloneApplication.h/.cpp`) owns the command line parser and
   declares the pure virtual `startupApplication()`; `GuiApplication` (`src/ovito/gui/desktop/app/GuiApplication.h/.cpp`)
   is the **only** GUI implementation. The only frontend-specific options it registers are `--nogui` and
   `--noviewports`, and `startupApplication()` creates a `MainWindowUI` and returns a `MainThreadOperation`.
   A frontend-neutral application class is therefore a larger refactor than switching a single construction site.
3. `StandaloneApplication::initialize()` (src/ovito/core/app/StandaloneApplication.cpp:125) instantiates all
   `ApplicationService` classes after loading every plugin. Desktop services may therefore run in front of any
   frontend, including a future QML one (see section 3.4).
4. `StandaloneApplication::createQtApplicationImpl()` forces `QT_QPA_PLATFORM=ovitoheadless` on Linux when the
   variable is unset, and `cmake/Prerequisites.cmake:290` claims that plugin "is built by OVITO itself".
   **That QPA plugin does not exist anywhere in this tree**: there is no source directory and no build artifact for
   it. `StandaloneApplication.cpp:231-246`, `Application.cpp`, `cmake/Prerequisites.cmake:290` and
   `cmake/OvitoTesting.cmake` reference an unimplemented plugin.
5. `src/main/CMakeLists.txt` builds the single `ovito` executable from `Main.cpp` against `Core` + `Gui` + `Qt6::Core`/`Qt6::Gui`.
   `OVITO_PLUGINS_RELATIVE_PATH` is a **private** definition of the `Core` target, so extra executables do not need it.
6. `cmake/Plugins.cmake` (`OVITO_STANDARD_PLUGIN`) is the module macro: it defines `OVITO_PLUGIN_NAME`, links
   `Core` and `Qt6::Core`/`Qt6::Gui`/`Qt6::GuiPrivate`, enables `AUTOMOC`/`AUTORCC`, and exports/installs into the plugin
   directory. Its `GUI_PLUGIN` option additionally links `Gui` and `Qt6::Widgets` and requires `OVITO_BUILD_APP`, so a
   QtWidgets-free frontend module must **not** be declared as a `GUI_PLUGIN`.
7. `src/ovito/gui/base/CMakeLists.txt` defines the `GuiBase` target: shared actions, `mainwin` models, viewport input
   modes and `BaseViewportWindow`, plus `resources/guibase.qrc` and the `GUIBase.h` precompiled header. Layering is
   `gui/qml -> gui/base -> core`, so the QML module can link `GuiBase` instead of `Gui`.

### Decisions

| # | Decision | Rationale |
|---|----------|-----------|
| D1 | Add `OVITO_BUILD_QML_FRONTEND` (default `OFF`) and a `src/ovito/gui/qml` subdirectory that is added from `src/ovito/gui/CMakeLists.txt` only when the option is on. | The classic build stays byte-identical when the option is off; no `GUI_PLUGIN`/QtWidgets entanglement. |
| D2 | Build `GuiQml` with `OVITO_STANDARD_PLUGIN(GuiQml ...)` and `PLUGIN_DEPENDENCIES GuiBase`, linking `Qt6::Quick` in addition. No `GUI_PLUGIN`. | Keeps the module free of `gui/desktop` and QtWidgets while reusing the plugin build/install machinery and the generated `OVITO_GUIQML_EXPORT` macro. |
| D3 | Defer the frontend-neutral application class and `--gui=qml` to Phase 2. Phase 1 adds a standalone prototype executable (`OvitoQmlSpike`) built from `src/ovito/gui/qml/spike`. | The entry point refactor does not de-risk rendering; keeping the prototype in its own executable keeps the spike honest about not depending on the desktop frontend. **Superseded in Phase 2 by D15**, which selects the frontend through a registry instead of a second application class. |
| D4 | Record the missing `ovitoheadless` QPA plugin as a Phase 2 prerequisite (section 5, O1). | Qt Quick cannot create a QRhi without a platform integration that provides Vulkan/OpenGL; this blocks headless QML verification on Linux servers and CI. |

---

## 2. Viewport and Rendering Interfaces (QRhi Ownership)

### Findings

1. `SceneRenderer::Implementation` (src/ovito/core/rendering/SceneRenderer.h) already stores an **externally supplied**
   `QRhi*`, and rendering is driven entirely through virtuals
   (`renderFrame`, `prepareResourceUpdates`, `performPrePasses`, `prepareIntermediateTarget`, `compositeInPass`,
   `preparePostProcess`, `runPostProcess`, `renderOverLayerOnly`). The only hard dependency on the render thread in this
   interface was the `RenderThread*` argument of `Configuration::createImplementationForVisual/ForPicking`.
2. `RenderThread` is the only provider of onscreen/offscreen render targets and the only place where the QRhi is created
   on the render thread; it also owns `ObjectIdAllocator`, `RendererResourceCache`, the graphics pipeline cache and the
   shader loader. `RenderTarget::requestPick()` blocks the GUI thread, and `WidgetViewportWindow` deliberately keeps the
   shared `RenderThread` alive when releasing its window to avoid a GUI/render-thread deadlock.
3. `StandardRendererImplementation` is the only `SceneRenderer::Implementation` subclass in the tree; it is created by
   `StandardRenderer::createImplementationForVisual/ForPicking`. The renderer-side coupling to the render thread is
   confined to the standard renderer family and measures **100 `rt()->` call sites in 8 files** under
   `src/ovito/core/rendering/standard` (`rt()` was renamed to `service()` by this audit).
4. `QQuickRhiItemRenderer` (`QtQuick/private/qquickrhiitem_p.h`, Qt 6.10) declares the pure virtuals
   `initialize(QRhiCommandBuffer*)`, `synchronize(QQuickRhiItem*)` and `render(QRhiCommandBuffer*)`, and hands out
   `rhi()`, `renderTarget()`, `colorTexture()` and `depthStencilBuffer()` of the item's own texture render target.
   The item always shares the `QQuickWindow`'s QRhi, and `QQuickRhiItemNode::render()` is connected with
   `Qt::DirectConnection` to `QQuickWindow::beforeRendering` and does **not** begin or end a render pass, so a renderer
   performs `beginPass`/`endPass` on `renderTarget()` itself. The item requires a non-software scene graph backend.
5. `RendererResourceCache::acquireResourceFrame()` calls `shared_from_this()`, and its (debug) destructor asserts that no
   resource frames are outstanding, so a renderer service must own the cache in a `std::shared_ptr` and the renderer
   implementation must be destroyed before it.
6. `UserInterface::renderThread()` creates the `RenderThread` lazily and keeps it alive only in non-GUI modes; other
   core subsystems call `RenderThread::createOffscreenTarget()` (render output framebuffer, `AmbientOcclusionModifier`,
   `ColorLegendOverlay`), so `RenderThread` cannot be removed even when viewports no longer use it.

### Decisions

| # | Decision | Rationale |
|---|----------|-----------|
| D5 | Extract an abstract `RendererService` (`src/ovito/core/rendering/RendererService.h`) exposing `rhi()`, `graphicsApi()`, `rhiResourceCache()`, `objectIdAllocator()`, `loadShader()`, `reportWarning()`, `isRendererThread()` and the `ensureGraphicsPipeline`/shared-device helpers; `RenderThread` derives from it, and `SceneRenderer::Device`, `SceneRenderer::Implementation` and `Configuration::createImplementationFor*` take `RendererService*`. | The measured coupling is modest and localized; it removes the last hard dependency of the renderer family on the render thread, which is exactly what blocks using the Qt Quick QRhi. |
| D6 | Phase 1 renders with a custom `QQuickRhiItemRenderer` that *implements* `RendererService` (option A), not by rendering offscreen into a texture that QML then displays (option B). | Sharing textures across two QRhi instances is unreliable; option A renders the frame graph directly into the item's render target with zero readback and zero extra QRhi. |
| D7 | Keep `RenderThread` for all offscreen work and as the classic frontend's service. The interactive QML frame does not use it, but **offscreen picking does** (D11). | No functional change for the classic frontend. The QML viewport's interactive path needs no `RenderThread`, which also removes the `requestPick` GUI-thread blocking deadlock risk from the QML viewport — the picking path added in Phase 1 is asynchronous and never blocks the GUI thread. |
| D8 | Establish the task context (`Promise<void>::create()`, `setIsInteractive()`, `setUserInterface(ui)`, `Task::Scope`) around the frame graph rendering inside the Qt Quick renderer. | `FrameGraph::finalizeForRendering()` asserts `this_task::get()`/`this_task::ui()` and `StandardRenderer` calls `this_task::isInteractive()`; the Qt Quick render thread has no OVITO task by default. |
| D9 | Document `RendererService` as non-thread-safe, callable only from the thread that owns the QRhi (`isRendererThread()`). | The Qt Quick renderer runs on the scene graph thread, while `RenderThread` runs on its own thread; both must satisfy the same contract. |
| D10 | Generalize the offscreen render target flag `forAmbientOcclusion`/`aoSampling` into `forPickingOnly`/`pickingOnly`, and add `RenderThread::renderPickingFrame()` returning `ScopedFuture<ObjectPickingBuffer>` plus the header-only `ObjectPickingBuffer` (`src/ovito/core/rendering/ObjectPickingBuffer.h`) as the render-thread/GUI-thread handoff type. | The QML viewport needs picking results, and the existing `RequestPick` path cannot serve them: it requires a swap chain and blocks the calling thread. The renamed flag expresses what the target actually is — "an offscreen target that receives picking passes only" — and `renderPickingFrame()` reuses the existing `renderPickingPass()`, so the amount of new core code stays small. |
| D11 | Picking in the QML viewport runs as an asynchronous offscreen pass on the shared `RenderThread`; `ViewportWindow::pick()` resolves from a cached `ObjectPickingBuffer` and starts a refresh when that buffer is out of date, instead of waiting for a result. | Verified constraints: Qt Quick has an active QRhi frame during the scene graph sync step (`beginOffscreenFrame()` is rejected there), and a readback submitted inside the interactive frame completes only at frame end, so neither can answer a synchronous `pick()`. Blocking the GUI thread would stall input handling and the scene graph, and `SelectionMode` picks on every mouse move. |

### Follow-up finding: Qt-delivered event handlers run without a `UserInterface`

`TaskManager` wraps main-thread event-loop work in `Task::Scope taskScope(nullptr)` (`TaskManager.cpp:281`, `:365`), i.e.
in a scope task that has **no** `UserInterface`. Any OVITO asynchronous work started from a Qt event handler (a QML
signal, a mouse event, a `QTimer`) therefore runs in a task for which `this_task::ui()` asserts, even though
`this_task::get()` succeeds. The classic frontend hits the same condition and solves it by attaching the context
explicitly *after* the coroutine's first suspension (`ViewportWindow::generateFrameGraph()` at
`ViewportWindow.cpp:207`, `WidgetViewportWindow::grabViewportImage()` at `WidgetViewportWindow.cpp:207`); the QML
viewport does the same in `QuickViewportWindow::renderPickingBuffer()`. This is a general expectation for every QML
frontend entry point that starts asynchronous work, not a picking-specific issue — recorded as **O8**.

---

## 3. Shared Models, Command and Undo Entry Points

### Findings

1. `PipelineListModel` (`src/ovito/gui/base/mainwin/PipelineListModel.h`) is already a `QAbstractListModel` +
   `UserInterfaceComponent<UserInterface>` exposing `TitleRole`, `ItemTypeRole`, `CheckedRole`, `IsCollapsedRole`,
   `DecorationRole`, `ToolTipRole`, `StatusInfoRole`, a `selectionModel()` and
   `Q_INVOKABLE performDragAndDropOperation()` which handles modifier groups, contiguous ranges, dry-run validation and
   `performTransaction("Move modifier")`. It is **not** in `gui/desktop`.
2. `ActionManager` (`src/ovito/gui/base/actions/ActionManager.h:195/209`) is likewise a concrete `QAbstractListModel` +
   `UserInterfaceComponent<UserInterface>` with `findAction(id)`, model roles and `createCommandAction`/
   `createViewportModeAction` helpers, and it registers no actions in its own constructor (the desktop
   `WidgetActionManager` subclass does). It can be instantiated directly by a non-QtWidgets frontend.
3. `ViewportInputManager(QObject* parent, UserInterface&)` (`gui/base/viewport/ViewportInputManager.h:43`) and
   `UndoStack(UserInterface& ui, QObject* parent = nullptr)` (`core/app/undo/UndoStack.h:44`) are constructible without
   QtWidgets. `MainWindowUI::initializeObject()` (`gui/desktop/mainwin/MainWindowUI.cpp`) is the reference wiring:
   set viewport input manager, undo stack and action manager, connect `DataSetContainer::dataSetChanged` to
   `UndoStack::clear`, then create the window.
4. **`UserInterface` is not a `QObject`** (it derives from `OvitoObject`); `MainWindowUI` has no `Q_OBJECT` either.
   Any QML-facing API (properties, `Q_INVOKABLE` methods, signals) therefore cannot be added to the
   `UserInterface` subclass itself.
5. Desktop `ApplicationService`s assume the desktop UI: `NewGraphicsSystemService::applicationStarting()`
   (`gui/desktop/dialogs/NewGraphicsSystemDialog.cpp:153`) and `UpdateNotificationService::applicationStarting()`
   (`gui/desktop/dialogs/UpdateNotificationDialog.cpp:134`) did `dynamic_object_cast<MainWindowUI>(this_task::ui().get())`
   followed by an assert and an unconditional `ui->mainWindow()`. Since all plugins are loaded, these services run for
   any frontend and crashed the QML prototype during `StandaloneApplication::initialize()`.
6. Viewport windows are created per viewport by the frontend:
   `ViewportsPanel::createViewportWindows()` (`gui/desktop/mainwin/ViewportsPanel.cpp:257`) constructs an
   `OORef<WidgetViewportWindow>`, assigns `setSceneRenderer(GuiApplication::instance()->getInteractiveViewportRenderer())`,
   calls `initializeWindow(viewport, ui, this)` and recovers from renderer failure via
   `fatalViewportWindowError()`/`revertToDefaultInteractiveViewportRenderer()`. There is no core-level factory.
   `GuiApplication::getInteractiveViewportRendererName()` reads `OVITO_VIEWPORT_RENDERER` or the
   `rendering/selected_graphics_api` setting (default `"opengl"`).
7. The default dataset layout is built in `DataSet::createDefaultViewportConfiguration()`
   (`core/dataset/DataSet.cpp:54-112`) as four viewports (Top, Front, Left, Perspective) inside a 4-pane
   `ViewportLayoutCell`, and `ViewportConfiguration::viewports()` returns them as a `QList<OORef<Viewport>>`.

### Decisions

| # | Decision | Rationale |
|---|----------|-----------|
| D10 | Reuse `GuiBase` directly (`ViewportInputManager`, `UndoStack`, `ActionManager`) instead of writing QML-specific models for Phase 1; `PipelineListModel` reuse/proxy is the Phase 3 starting point. | The shared models already satisfy the QML consumption contract (models + `Q_INVOKABLE`); duplicating pipeline semantics in QML would create a second implementation of mutation rules. |
| D11 | The QML frontend's `UserInterface` subclass owns a separate `QObject`-derived controller (`QmlViewportController`) that is registered as a QML context property. | `UserInterface` is not a `QObject`, so QML properties/`Q_INVOKABLE` methods/signals live in the controller. |
| D12 | Make the two desktop services return early when the active `UserInterface` is not a `MainWindowUI`. | Both are desktop-only dialogs; skipping them is the correct behavior for any other frontend and removes a crash that the assert-only check would hide in release builds. |
| D13 | The QML viewport item creates and assigns the interactive `StandardRenderer` itself (there is no `GuiApplication` in the QML frontend) and reproduces the per-viewport creation flow. | Preserves classic behavior semantics (one renderer instance providing the settings, per-viewport viewport window) without depending on `gui/desktop`. |
| D15 | The frontend is selected through a registry instead of a second `StandaloneApplication` subclass (which is what D3 originally called a "frontend-neutral application class"): `gui/base/app/GuiFrontend.h` declares a frontend as a factory for the workbench, `GuiFrontendRegistry` collects the factories, and `GuiApplication::startupApplication()` looks up the name given by the new `--gui=<name>` option and lets that frontend create the workbench. The classic frontend registers `qt-widgets` in the `GuiApplication` constructor and remains the default; the Qt Quick frontend registers `qml` from `QmlFrontendService::applicationInitializing()`, i.e. through the documented plugin hook, which runs after plugin loading and before `startupApplication()`. | A frontend-neutral application class would have to reproduce everything that `GuiApplication` already does *frontend-neutrally*: Qt application creation and platform styling, session loading (`initializeUserInterface()` is written against `UserInterface&`), the frontend-neutral parts of the command line, and `reportError`. The one place where the name is actually needed is `startupApplication()`, so the seam belongs there. Registration through an `ApplicationService` needs no new plugin mechanism, and it keeps `gui/qml` out of the executable's dependency graph. |
| D16 | The parts of the workbench that do not depend on the presentation move from `gui/desktop/mainwin/MainWindowUI` into a new `gui/base/app/WorkbenchUI` base class, and the two `dynamic_object_cast<MainWindowUI>` sites in `GuiApplication::initializeUserInterface()` (command-line file and directory import) become `dynamic_object_cast<WorkbenchUI>` calls on public `WorkbenchUI` methods. What moved: the import orchestration over `FileImporter`/`FileImporterClass` (importer creation, priority ordering, import-mode decision, import execution, undo-stack/file-path handling, working directory), the task-progress record list with its 100 ms throttled `progressTasksChanged()` hook, the auto-key mode (`setAutoKeyModeEnabled()`), error reporting (console output in `reportError()`, presentation in `displayErrorMessage()`), and the `fileSourcePipelines()` helper for the multi-pipeline session check. What stayed frontend-specific and is now a documented virtual hook: `createActionManager()`, `displayErrorMessage()`, `progressTasksChanged()`, `openImportDialog()`, `inspectImporterFiles()`, `determineImportMode()`, `runFileImport()`, `importDirectoryChanged()`. The defaults are chosen so that a frontend without dialogs behaves sensibly and never discards work silently: an empty or replaceable scene is reset, anything else is appended, and `openImportDialog()` reports that this frontend cannot select files instead of doing nothing. | The QML frontend must not reimplement import, undo cleanup or progress bookkeeping, and the classic frontend must not lose its dialogs. Splitting "what to ask the user" from "what to do" is the only way both can share one implementation. |
| D17 | The QML viewport area is laid out by `QmlViewportLayout`, which walks `ViewportConfiguration::layoutRootCell()` once and produces both the pane rectangles (one per `ViewportConfiguration::viewports()` entry, in the same pre-order) and the 6-device-pixel splitter handles. Panes and handles are ordinary `QObject`s exposed to QML as a `QVariantList`; their geometry is propagated through per-object change signals so that a drag, a resize, an active-viewport change or a maximize toggle never rebuilds the QML delegates - and with them the viewport items and their GPU resources. A drag is one `UndoableTransaction` (`"Resize viewports"`, `revert()` + `performActions()` per mouse move, one `commit()`), maximizing is not undoable, matching the classic `ViewportsPanel`. An index/invokable-based alternative was implemented and rejected: QML caches the result of an invokable, so a delegate would keep a stale object across a rebuilt list. |
| D18 | **Viewport items are never destroyed synchronously from the code that creates or replaces them.** `QmlViewportController::createViewportItem()` is reached from QML while the scene builds its pane delegates, and `discardViewportItems()` runs on a dataset change; in both cases the item may be handling an event at that moment, and `delete` crashed the process inside Qt's event dispatch. Both paths use `deleteLater()`, each pane delegate owns exactly one item (no reuse across delegates, because the delegate that owned it is destroyed afterwards and would take the item with it), and a pane whose item creation failed retries when its geometry changes. Found and fixed while verifying the Phase 2 shell (defect F11 in the Phase 1 report). |
| D14 | Phase 1 uses a fixed 2x2 QML grid. Deriving the pane arrangement from `ViewportConfiguration::layoutRootCell()` (including the maximized cell) is deferred to Phase 2. | The layout tree is the correct source of truth; the prototype only needs four simultaneous viewports to prove multi-viewport rendering. |

| D19 | The QML shell state and commands live in a separate `QmlWorkbenchController` (context property `workbenchController`), not in `QmlViewportController`: the status line, the task progress of the running operations, the empty state of the scene, the window title, the message dialog the frontend is waiting on, and the import commands. `QmlViewportController` keeps the viewport items and the undo stack. | The two have different lifetimes and different owners: the status line and the progress bar describe the *window*, the viewport items belong to the viewports of one data set and are recreated with it. Splitting them also keeps the "no desktop dependency" rule intact - the shell talks to `WorkbenchUI` (import orchestration) and to the QML scene, never to QtWidgets. |
| D20 | The QML frontend runs an import in a task of its own (`GuiTaskScope`) and hands that task to the shell as the operation the Cancel command cancels; `QmlMainWindowUI::runFileImport()` removes the scene objects the canceled import created before it rethrows, so a canceled import leaves no pipeline whose data source was never filled. | `WorkbenchUI` prescribes the order of events (a ResetScene import deletes the previous objects before it creates the new pipeline), so the previous content cannot be restored - but the half-imported pipeline can and must be removed. The task handle comes from the scope the callback needs anyway (open item O8), and Cancellation is offered exactly where the classic frontend offers it: for the import operation of the progress dialog. Note that OVITO's `importFileSet()` sets up the file source and returns, so a plain import is usually too short to be canceled - the same is true of the classic frontend; a slow operation (large VASP file, slow file system, many files) is cancellable. |
| D21 | The shell uses Qt Quick Controls with the platform-independent `Basic` style (set before the QML scene is created) and colors it from `Theme.qml`; the theme follows the color scheme of the operating system and offers a light and a dark palette. | One style everywhere keeps the workbench looking the same on all three platforms and keeps the palette in one file (UI_DESIGN.md section 7), instead of inheriting the platform widget style and fighting it. `Theme.qml` also carries the sizes the shell lays out with, so a component can be read without a second source of truth. |
| D22 | `GuiApplication::reportError()` gives the message to the active user interface when that is not a `MainWindowUI`, instead of always opening a QtWidgets dialog. | The application object must not assume that the frontend presenting the error is the classic main window: the QML frontend has its own message dialog (Decision D19), and the message box of the frontend is what the *user* sees. The desktop path keeps its own dialog unchanged. |
| D23 | The viewport items of one Qt Quick window share one `QuickRendererService` (a `RendererService` owned by the window) instead of one service per item. | The QRhi instance belongs to the window's scene graph, and so do the caches derived from it: pipelines are only valid for the QRhi that created them. Sharing removes the duplicate work: each item used to compile every shader pipeline of its own and to prepare and upload the same vertex data, which for four viewports is the dominant per-frame cost of a small scene (see the measurement in [UI_PLAN.md](UI_PLAN.md) deliverable 7). The service is created on the GUI thread when an item enters a window and parented to that window, because the GPU resources it owns must not outlive the QRhi instance; the renderers register with it, and it asks them to release their implementations when the window invalidates its scene graph. |
| D24 | The pass sequence of a frame graph lives in `core/rendering/FrameGraphRenderPass` and is used by `RenderThread` and by the Qt Quick renderer alike. | The sequence is a contract with the renderer implementations (resource uploads before `beginPass()`, pre-passes before the scene pass, post-processing after it) and it existed twice, with the second copy already missing the warning indicator. A caller supplies the target, the frame graph, the renderer implementations it keeps per target and two callbacks for what its target adds to the pass (the classic frontend's watermark and warning indicator), so the ordering is written down once. |
| D25 | The pane and handle objects of `QmlViewportLayout` are retired per kind. | A maximized layout keeps the panes but removes the handles; discarding both kinds of object whenever either had changed left the pane list empty while the geometries were still there, which lost the viewport item of every pane (defect F12). Retiring only what is replaced keeps the two lists consistent with the geometries they were derived from. |
| D26 | The commands of the two frontends are frontend-neutral `Command` objects owned by `ActionManager`, and each command's `QAction` is a one-way view of it. Every command is created with `createCommand()`/`createViewportModeCommand()`, needs no `QAction` to be usable, and carries its own `id`, text, shortcut, tooltip, icon, checkable/checked state and handler. `ActionManager` exposes them to QML through a `commandList`/`command(id)`/`triggerCommand(id)` property set, and the QML workbench consumes them as `Shortcut` and button bindings. | The command layer is the answer to review finding A1: 51 of 65 command definitions already lived in `gui/base`, but they were *represented* as `QAction`s, so the QML frontend re-implemented the few commands it had and could not reuse the rest. Keeping the state in the command and mirroring it onto the `QAction` (never the other way round) keeps the classic frontend's menus, toolbars, shortcut table and enabled-state rules working unchanged while giving the QML frontend the same objects - a frontend-specific copy of the shortcut table or of the state rules is exactly the duplication the review was supposed to remove. `ViewportModeAction` therefore survives as a thin `QAction` view of a (neutral) `ViewportModeCommand`, because the eight plugin editors and toolbars that use it need `activateMode()`/`deactivateMode()` and a widget to add to a toolbar. |

---

### Phase 2 status

Deliverables 1-6 are implemented: the frontend is selected with `--gui` (D15), the widget-free parts of the workbench
live in `gui/base` (D16), the shell lays its panes out from the layout tree of the data set with draggable handles,
undoable resizing and maximizing (D17, verified on Linux/OpenGL), the shell state and its dialogs are separate from the
viewport controller (D19, D21, D22), and the import path offers its states - empty, busy, cancelling, cancelled and
error - through the file dialog, drag & drop, the status bar and the message dialog (D20). With
`OVITO_BUILD_QML_FRONTEND=OFF` the build contains no QML sources and no QML target (verified by inspecting the
generated build files and by building the classic frontend from that tree).

Deliverable 7 is implemented as well: the pass sequence of a frame graph lives in one shared
`core/rendering/FrameGraphRenderPass` (D24), the viewport items of a window share one `RendererService` (D23, measured
as a frame-time win), the picking target stays where it is because the QML viewport picks through
`RenderThread::renderPickingFrame()` (O7 resolved by the asynchronous picking design, D11), the missing Vulkan API
version of OVITO's own instances is set (O2) and the missing `ovitoheadless` QPA plugin is documented as an environment
constraint (O1). The runtime verification on macOS/Metal is recorded in [UI_PLAN.md](UI_PLAN.md).

The follow-up work proposed by the frontend comparison ([UI_FRONTEND_REVIEW.md](UI_FRONTEND_REVIEW.md)) has started:
the frontend-neutral command layer of review finding A1 is in the tree (D26), which removes the second command table
the QML frontend would otherwise have grown, and the QML workbench uses the shared Undo/Redo, maximize and viewport-mode
commands instead of its own operations. What remains of that work plan is recorded in the review document itself (A5,
A2, A4, A3 and the small parity gaps); Windows/D3D12 still has no test host (see the Phase 1 report).

## 4. Changes to Existing Code Made Under This Audit

All of these are subject to the regression checks in [UI_PHASE1_SPIKE.md](UI_PHASE1_SPIKE.md) section 4
(native build, `ctest`, headless start, classic frontend start).

1. **`src/ovito/core/rendering/RendererService.h` (new) and the `RendererService` extraction.**
   `RenderThread` now publicly derives from `RendererService` and implements its members as overrides;
   `SceneRenderer::Device`/`Implementation` (with `rendererService()`/`service()`) and
   `Configuration::createImplementationForVisual/ForPicking` take `RendererService*`; the `rt()->` uses inside
   `src/ovito/core/rendering/standard` (100 sites, 8 files) were renamed to `service()->`; `RenderThread` additionally
   exposes `discardCachedResources()` for the shutdown path.
2. **`src/ovito/core/ForwardDecl.h`** declares `RendererService`.
3. **`src/ovito/gui/desktop/dialogs/NewGraphicsSystemDialog.cpp` and `UpdateNotificationDialog.cpp`** return early when
   `dynamic_object_cast<MainWindowUI>(this_task::ui().get())` is null (section 3, finding 5).
4. **Header license conversion**: every file's GPL-3.0/MIT banner was replaced with a two-line
   `SPDX-FileCopyrightText`/`SPDX-License-Identifier` header (1657 files `GPL-3.0-only OR MIT`, `Graph.h`
   `GPL-3.0-or-later`, the two `3rdparty/netcdf_integration` files `GPL-2.0-or-later`). This is a repository-wide
   licensing decision made by the repository owner, not a functional change.
5. **CMake**: `OVITO_BUILD_QML_FRONTEND` option in the root `CMakeLists.txt` and the conditional
   `ADD_SUBDIRECTORY(qml)` in `src/ovito/gui/CMakeLists.txt`.
6. **Frontend selection and workbench extraction (D15, D16).** New in `gui/base`: `app/GuiFrontend.h`,
   `app/GuiFrontendRegistry.{h,cpp}` and `app/WorkbenchUI.{h,cpp}`. `MainWindowUI` (gui/desktop) and `QmlMainWindowUI`
   (gui/qml) now derive from `WorkbenchUI` and implement its presentation hooks; `GuiApplication` gains the `--gui`
   option, registers `qt-widgets`, and resolves the selected frontend through the registry in `startupApplication()`
   (an unavailable or unknown name prints the available frontends and aborts the startup with exit code 1, without the
   modal error dialog, because the request may come from a script); `src/ovito/gui/desktop/app/QtWidgetsFrontend.*` and
   `src/ovito/gui/qml/app/QmlFrontend.*` plus `QmlFrontendService.*` are new. The QML workbench additionally rebuilds
   its viewport items when the current dataset changes (`QmlViewportController::viewportCount` drives the QML grid).
7. **Offscreen picking support** (D10): `src/ovito/core/rendering/ObjectPickingBuffer.h` (new, header-only,
   forward-declared in `ForwardDecl.h`), `RenderThread::renderPickingFrame()` with
   `EventType::RenderPickingFrame`/`RenderPickingFrameEvent`/`handleRenderPickingFrame()`, the `RenderTarget` wrapper for
   it, the `forAmbientOcclusion`→`forPickingOnly` flag rename, `ObjectPickingMap::lookupPickResult()` (factored out of
   `RenderThread::lookupPickBuffer()`, which now delegates to it), and
   `ViewportWindow::generateFrameGraph()` moved from private to protected so that the QML adapter can produce a
   dedicated picking frame graph (a frame graph is consumed by exactly one renderer).

## 5. Open Items (Not Resolved by Phase 0/1)

| # | Item | Owner |
|---|------|-------|
| O1 | **Documented as a constraint in Phase 2** (the phase allowed either fixing or documenting it): the `ovitoheadless` QPA plugin referenced by `StandaloneApplication::createQtApplicationImpl()`, `cmake/Prerequisites.cmake:290` and `cmake/OvitoTesting.cmake` does not exist in this tree. Consequences, all recorded in [UI_TEST_ENV.md](UI_TEST_ENV.md): a GUI-enabled run without a display has to name another platform plugin (`QT_QPA_PLATFORM=xcb` under `xvfb`), because Qt's `offscreen` plugin provides no `QRhi` (`No QRhi found for window ...`, which leaves the viewport of the QML frontend blank rather than failing), and the plugin the code expects cannot be provided by the QPA plugins that ship with the Qt installation. Implementing it means a platform integration with `createPlatformVulkanInstance()` over `VK_EXT_headless_surface`; removing the references would trade the intended behaviour for a clearer error. Until either happens, documentation is the state: an environment requirement, not a defect in the frontend. | Follow-up |
| O2 | ~~`RenderThread` creates its `QVulkanInstance` without an API version.~~ **Fixed in Phase 2**: both the temporary and the real instance now request the newest version the loader offers, capped at 1.3 (`configureVulkanApiVersion()` in `RenderThread.cpp`). A minimal Qt probe reproduced the validation error for an instance that leaves the version at zero (`VUID-VkApplicationInfo-apiVersion ... has value of 0 which is not permitted`) and shows it gone for the instance that sets it, while the loader itself reports 1.3.275 support under the software rasterizer. The earlier `-9`/`VK_ERROR_INCOMPATIBLE_DRIVER` observation was **not** caused by this: the same probe creates an instance without an API version under lavapipe just fine, so that failure came from the platform integration (see O1). | Resolved |
| O3 | ~~Object picking in the QML viewport is not implemented.~~ **Resolved in Phase 1** — picking is implemented as an asynchronous offscreen pass (D11) and verified end to end. Refinements left for Phase 5: pre-warming the buffer on viewport enter/camera change, coalescing hover picks, and revisiting the "use the previous buffer while refreshing" policy. | Phase 5 |
| O7 | ~~The GPU picking-target creation is duplicated between `RenderThread::ensurePickingResources()` and the QML renderer.~~ **Resolved in Phase 1, confirmed in Phase 2**: the Qt Quick frontend no longer creates picking targets at all — it renders its picking pass through `RenderThread::renderPickingFrame()` with the `ObjectPickingBuffer` handoff, so `ensurePickingResources()` has one caller again. What the two paths do share lives in `ObjectPickingMap::lookupPickResult()`. | Resolved |
| O8 | ~~Qt event handlers run in a task without a `UserInterface`~~ **Resolved in Phase 2**: `GuiTaskScope` (gui/base) opens a `MainThreadOperation` bound to the `UserInterface` for the duration of a presentation-layer callback and exposes its task, which is also what lets the shell cancel a running import. Used by the QML entry points that start work: the viewport controller, the viewport layout and the import commands. | Resolved |
| O9 | ~~The prototype executable (`ovito-qml-spike`) cannot locate the OVITO plugin libraries on macOS without `DYLD_LIBRARY_PATH`.~~ **Resolved in Phase 1** — the spike's `INSTALL_RPATH` was built from `OVITO_RELATIVE_PLUGINS_DIRECTORY`, which is bundle-*root* relative, so the resulting `@executable_path/../Ovito.app/Contents/PlugIns` path was doubled. The target now uses the literal bundle-relative paths of `src/main/CMakeLists.txt`; `otool -l` reports `@executable_path/` and `@executable_path/../PlugIns/`, and the spike runs on the macOS test host with only the Qt library directory in `DYLD_LIBRARY_PATH` (Qt itself is still expected there in the build tree, like the main executable). | Resolved |
| O10 | A QtWidgets frontend test run must not be automated in a headless environment without suppressing the interactive import dialog: `MainWindowUI::importFiles()` shows a modal importer dialog (and asks about the import mode when the scene is non-empty), so an unattended run renders an *empty* scene. The recipes and the temporary benchmark instrumentation are recorded in [UI_TEST_ENV.md](UI_TEST_ENV.md) sections 3.1 and 7. | Testing |
| O11 | ~~`QuickViewportRenderer::renderFrameGraph()` (~100 lines) replays the pass sequence of `RenderThread::renderFrameGraph()` a second time.~~ **Resolved in Phase 2** — the sequence is `FrameGraphRenderPass` (D24) and both renderers only describe their target and their additions to the pass. (Original wording kept for the record:) `finalizeForRendering()`, `createImplementationForVisual()`, `renderFrame()`, `prepareIntermediateTarget()`, `prepareResourceUpdates()`, `performPrePasses()`, `beginPass()` with the full-viewport scissor workaround, `compositeInPass()`, `endPass()` and the `OverLayer`/post-process pass. Every change to that ordering in the core render thread must be mirrored by hand in the Qt Quick renderer; the pick radius (4) duplicates the default of the classic `RenderTarget::requestPick()` in the same way. Together with O7 this points at one core helper — a frame-graph renderer over a `RendererService*` plus a `PickingBufferTarget` — shared by both frontends' render paths. | Phase 2 |
| O4 | ~~Each QML viewport item owns its own `RendererService` state (`RendererResourceCache`, `ObjectIdAllocator`, pipeline cache).~~ **Adopted in Phase 2** (D23): the items of a window share one service owned by the window. Measured on Linux/OpenGL with four viewports at 1280×800 and `QSG_NO_VSYNC=1` (medians of three runs): 512 atoms 64.0 → 114.0 fps (threaded loop) and 118.0 → 175.5 fps (basic loop); 32768 atoms 19.5 → 22.5 fps and 38.5 → 44.5 fps. The gain at low atom counts is the fixed per-frame cost of four viewports, which each item used to pay on its own. | Resolved |
| O5 | The full action/editor inventory and the expanded parity matrix required by the remaining Phase 0 deliverables are still pending. | Phase 0 |
| O6 | ~~The frontend-neutral application class and `--gui=qml` (D3) are unimplemented.~~ **Resolved in Phase 2** — `ovito --gui=qml` starts the Qt Quick workbench and `ovito --gui=qt-widgets` (or plain `ovito`) the classic one, selected through the registry of D15. | Resolved |
| O12 | ~~The QML workbench has no file selection UI and no progress display.~~ **Resolved in Phase 2** (deliverable 5): the shell has a file dialog (`Ctrl+O`) and accepts drops onto the window, the status bar shows the progress of the running operations with a Cancel command, failures are reported in a message dialog, and a canceled import removes its half-loaded pipeline (D19-D22). | Resolved |
