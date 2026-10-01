# OVITO Modern Workbench UI — Phase 0 Audit Record

> **Companion documents**: [UI_DESIGN.md](UI_DESIGN.md), [UI_PLAN.md](UI_PLAN.md), [UI_PHASE1_SPIKE.md](UI_PHASE1_SPIKE.md), [UI_FRONTEND_REVIEW.md](UI_FRONTEND_REVIEW.md)
>
> **Status**: Delivery complete. The three audits that gate Phase 1 (build entry, viewport/rendering, shared models) were
executed, and the Phase 0 deliverables that remained — the full action/editor/control inventory (§6) and the expanded
parity matrix ([UI_PARITY_MATRIX.md](UI_PARITY_MATRIX.md)) — were delivered in Phase 2.5 (deliverable 1). This document is
the decision record of the migration; the open items of §5 are kept up to date as they are closed.

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
| D27 | Frontend-independent workbench data lives in `gui/base` as data types, not in the desktop frontend: `RecentFilesList` (the QSettings-backed list of recently opened sessions and imports) moved from `gui/desktop/mainwin/` to `gui/base/mainwin/`, and a new `TaskProgressModel` (`gui/base/app/`, a `QAbstractListModel` owned by `WorkbenchUI`) presents the running tasks and the progress of the operation the status bar shows. | This is review finding A5: the data of the workbench (the task list, the recent files) was reachable only from the desktop frontend, so the Qt Quick frontend either recomputed it or did without it. The Qt Quick status line and the classic `TaskDisplayWidget` now read the same model instead of each walking the task list, and the recent files are available to a QML file menu instead of being a desktop-only feature. The model also records an OVITO property that is easy to misread: a `TaskProgress` record registers itself with the `UserInterface` of the *task* that created it, so the progress of a pipeline evaluation that a viewport triggered is reported to neither frontend's status bar (the evaluation task has no user interface), while the progress of an import that the frontend itself started is. |
| D38 | **The spike harness registers everything it verifies in one table**: `src/ovito/gui/qml/spike/Main.cpp` holds the application class, a table of verification steps (option, the predicate that decides whether the value asks for the step, the check to run) plus the two value-only options, and `main()`; the machinery the checks share is in `SpikeHarness.h`/`.cpp`, and the checks themselves are grouped by theme in `checks/ShellChecks.cpp`, `checks/RenderingChecks.cpp` and `checks/ImportChecks.cpp`. | The option list, the help text and the *is this a verification run* predicate used to be three hand-kept lists; a check that was missing from the predicate opened the workbench and hung until the caller's timeout killed it, which costs a CI round and reads like a product defect. The checks are grouped by theme rather than one file per check because they share helper clusters (the shell checks share the menu walk and the icon/image readers, the rendering checks share the picking probes) - eleven tiny files would have been the more faithful reading of the plan and the worse layout. Nothing about what is verified changed: the same sequence, the same assertions, verified by the CI-equivalent spike run, `ctest --preset native` and `ovito --nogui`. |
| D37 | **The workbench asks before it closes, and the shell reads the shared file and pipeline state**: `QmlMainWindowUI::canCloseWorkbench()` wraps `WorkbenchUI::askForSaveChanges()` so that a cancelled question keeps the window open instead of throwing (the window's `closeEvent` override and the Quit command consult it), the window title follows `UndoStack::cleanChanged` and marks a modified session, the File menu offers the shared `RecentFilesList` (whose entries the shared save code and the frontend's import path now write, rather than the desktop action), the file selection dialog starts in the directory the shared `GuiSettings` history remembers, and `checkLoadedDataset()` asks which pipeline to keep of a session with several file sources that this frontend used to load silently. | These were the last differences of the class "the user gets a different result, not a different look": closing the workbench lost unsaved changes, the shell showed no dirty state, and a session OVITO Basic cannot display was loaded without a word. Every one of them was *wiring* of code that already existed in the shared layer, and each is verified by extending an existing check (`--qml-session-check`, `--qml-parity-check`) instead of adding one - which is the point of doing this before Phase 3 builds its models on the shell. The one item left as a manual check is the end-to-end multi-pipeline load, because its fixture is a session file that only OVITO Pro writes. |
| D36 | **A picking buffer must not depend on the render service that produced it.** The pre-warm of deliverable 7 (a 150 ms single-shot timer that refreshes the picking buffer when the view has settled) was taken out again first, because it crashed release builds when the data set was replaced, and then restored once the cause was fixed: `ObjectPickingMap` had kept the object-ID handles of the last pass as the keys of its picking records, and `ObjectPickingBuffer` carried that map to the caller, so destroying a pending picking result called `ObjectIdAllocator::free()` on the allocator of a render thread that had already been destroyed (`RenderThread` is only weakly held by the `UserInterface` in GUI mode and goes away with its last `RenderTarget`). The map now stores the plain base object ID, keeps its ID reservations in a separate vector, and gives them up as soon as the pass has been recorded (`releaseObjectIds()`, called from `RenderThread::renderPickingPass()`); `RenderThread::_objectIdAllocator` is declared before the render targets so that it is destroyed last. | Found by AddressSanitizer after a release build crashed in 4 of 5 runs of the scene-replacing session check - the same defect was invisible to valgrind, to the assert-enabled build and to the sanitizer without the right timing, because the failing write is a handle release and not a bounds violation. The fix keeps the property the buffer is documented for: a pick result is a self-contained value that can be queried from any thread long after the pass, with no dependency on the render thread that rendered it. |
| D35 | The picking buffer of the Qt Quick viewport keeps track of *which* view it was rendered for instead of whether a pass is running: `renderFrameGraph()` counts every change of the rendered contents, a completed picking pass records the counter it was rendered for, and `isPickingBufferCurrent()` compares the two (plus the buffer size). A single boolean cannot express this - a pass that started before a change and finished after it would make a buffer of the previous view look current, and the next hover would be answered from the old camera for good - so the pair replaced `_pickingBufferStale`. On top of it, a single-shot 150 ms timer (`pickingPrewarmTimeout()`), restarted by every new frame graph and by a pass that completed while the view changed again, refreshes the buffer once the viewport has settled: during a camera drag the frame graphs arrive faster than the timer fires, so an interaction costs one offscreen pass instead of one per frame, and a hover that follows the interaction is answered from the current view. The residual window (a hover within 150 ms of the last redraw, or while a pass is in flight) is still answered from the previous buffer; the asynchronous pick API of Phase 5 (review item A3) is the fix for it, and the pre-warm is what makes the staleness the exception instead of the rule. The acceptance check is the spike's `--qml-prewarm-check`. |
| D34 | Offscreen rendering has one owner for both frontends: `core/rendering/OffscreenRenderTarget` wraps `RenderThread`'s four offscreen entry points in three calls - `renderImage()` (render output, viewport grabs, symbol images), `renderPicking()` (picking passes) and `renderAmbientOcclusion()` (AO sampling) - and is constructed as `OffscreenRenderTarget(UserInterface&, Kind)` with `Kind::Visual` or `Kind::PickingOnly`, which decides the buffers of the GPU target and makes a pass of the wrong kind throw (and assert) instead of rendering into the wrong buffers. It reuses its target until the requested resolution changes, so a consumer that renders repeatedly (the QML picking pass, the render-output loop, the AO sampling) allocates once. Its thread contract is explicit, which is what the duplicate code never stated: **only the main thread may allocate a target** - allocation is what creates the shared render thread and the graphics device, and `UserInterface::renderThread()` asserts main-thread execution - while **a pass may be submitted from any thread** once the target exists for the requested size, and the completion arrives on the awaiter's executor (`renderAmbientOcclusion()` is the entry point that is submitted from a worker thread, which is why the ambient-occlusion modifier calls `prepare()` on the main thread first and why `ensureTarget()` carries the assertion that caught the violation in the assert-enabled build). All five in-tree consumers moved onto the service (RenderSettings render output, `WidgetViewportWindow::grabViewportImage`, `QuickViewportWindow` picking, `AmbientOcclusionModifier`, `ColorLegendOverlay` symbol rendering), a cached instance deliberately keeps the render thread and the device alive (the QML viewport's picking target) while a one-shot consumer keeps it local; a shared `PickingBufferTarget` was *not* re-introduced because that duplication (open item O7) was already removed by `RenderThread::renderPickingFrame()` plus `ObjectPickingBuffer`. Acceptance is the spike's `--qml-offscreen-check`. |
| D33 | The two frontends share one icon set, and the rule that answers which of its two themes is current lives in the shared layer: `gui/base/app/IconTheme` owns the theme names (`ovito-dark`/`ovito-light`), applies them to the process (`QIcon::setThemeName`, with the light theme as the fallback), resolves a command's icon path (a literal `:` resource or a themed name) and renders an icon at a device-pixel size for QML. The Qt Quick frontend publishes that through `QmlIcons`, a QML singleton `Icons` plus the `ovito-icon` image provider, and the pane's maximize/restore button, the command-backed menu entries and the Import button take their icons from it. | Rationale: the review expected a tint rule, but the classic set has none — it ships two themed SVG sets and picks one by name, so the rule to share is the theme selection plus the path resolution. The decision which theme is current moved out of `GuiApplication` into `GuiSettings`, which already owns the color-scheme policy, so the Qt Quick frontend gets it without consulting the desktop application class. The shell needed one icon the classic set never had (a restore counterpart for `viewport_maximize`, which existed only as a toolbar button of the classic frontend), so `viewport_restore.svg` joins both themes of the shared set instead of the shell drawing its own glyph. |
| D32 | Review items A8.1–A8.5 are implemented as a cleanup of the shared layer: the graphics-API facts (`preferred()`, `enumerateAdapters()`, `selectedAdapterName()`, `adapterSettingsKey()`, `prepareVulkanEnvironment()`, the Vulkan `apiVersion` helper) moved out of `RenderThread` into `core/rendering/GraphicsApi`; `--noviewports` is registered by core `StandaloneApplication`, whose reader `DataSet` is core too; the desktop-only application services find their window through `MainWindow::activeMainWindow()` (null under another frontend) instead of `dynamic_object_cast<MainWindowUI>`; the Qt Quick frontend checks `QQuickWindow::rhi()` after the scene graph came up and reports the platform plugin plus the xvfb recipe instead of showing empty viewports; and the modifier/overlay libraries register `Command`s (`ModifierAction`, `OverlayAction`) instead of plain `QAction`s. | Rationale: each of these couplings was reachable from the desktop frontend only, so the QML frontend would have had either to duplicate it or to work around it — the Phase 1 extraction of `RendererService` is only complete once the standard renderer family no longer includes `RenderThread.h`, and Phase 4's modifier library needs the library rows as commands. A8.4 leaves the missing `ovitoheadless` QPA plugin (O1) a documented environment constraint rather than a plugin to ship. The two deviations from the plan text are that A8.2 needed no new `UserInterface` interface (a desktop lookup says the same thing without pushing desktop concerns into core) and that the two modifier-snippet commands stay desktop-owned (their handlers are dialogs of the desktop frontend), so `PipelineListModel` looks them up tolerantly (defect F19). |
| D31 | The Qt Quick workbench presents the shared commands through an in-window menu bar and a viewport context menu, reports its running operations as one row per task, remembers its window state through `GuiSettings`, and keeps a persistent `notice` on `QmlWorkbenchController` telling which format the last import used and how many source frames it found. The menu bar (`WorkbenchMenuBar.qml`) contains File, Edit, View and Help, and is bound to the commands of the shared command layer through `WorkbenchMenuItem.qml`; an entry whose handler does not exist yet is shipped as a `WorkbenchPlaceholderMenuItem.qml` whose text names the phase that will deliver it, because a QML menu item cannot carry a tooltip. `ViewportContextMenu.qml` is driven by the new `QmlViewportMenu` model (context property `viewportMenu`), which holds the clicked viewport and offers the view type, the construction grid, the camera rotation constraint and maximizing/restoring - the classic `ViewportMenu.cpp` is the reference, except that Show Grid is offered unconditionally (the classic entry sits in an `#ifdef OVITO_DEBUG` block) and that maximize has an entry although the classic frontend only has one in its toolbar. `WorkbenchStatusBar.qml` renders one row per `TaskProgressModel` row (the classic status bar shows one aggregate bar) and keeps the shell-level Cancel for the operation the shell started. The window size, position and maximized state are read from and written back to `GuiSettings` with a 500 ms debounce; the import notice is set by `QmlMainWindowUI::runFileImport()` and completed asynchronously with the number of source frames of the `FileSource` (the continuation future is held in a member of the interface, because a future nobody awaits cancels the continuation it carries). | The shell needs surfaces of its own for the commands that the classic frontend presents in its menu bar, toolbar and context menu; keeping the state in the shared command layer (D26) means a surface only has to present it, and the phase-named placeholders keep the shell honest about what is not implemented yet. An import that was read as an unexpected format leaves an empty scene (defect F6), so the notice is the only hint the user gets - which is why it must survive the viewport interactions that clear the transient status line of `BaseViewportWindow::leaveEvent()`. |
| D30 | The settings the workbenches persist are owned by one facade, `gui/base/app/GuiSettings` (a singleton): the color-scheme policy, the main-window geometry and dock layout, the window rectangle of a frontend without widgets, the behavior of the file dialogs (directory history on/off, preferred dialog type, session-file directory, per-dialog-class directory history) and the flags the first start sets (GPU adapter confirmation). The facade also exposes the resolved dark/light decision to QML, and the shell's `Theme.qml` takes its palette from it, so both frontends follow one color-scheme rule. | This is the remaining part of review finding A5. The keys were spread over `HistoryFileDialog`, `ImportFileDialog`, `NewGraphicsSystemDialog`, `MainWindow`, `MainWindowUI` and `GuiApplication`, so a second frontend had to invent its own convention for every value it wanted to share. Two boundaries are deliberate: (a) a value whose format is a `QWidget` detail stays in that format - a geometry blob also carries the screen and the maximized state, so it is not "simplified" into a rectangle - and (b) a dialog only one frontend has keeps the values only it uses (the export dialog's last directory and filter), while everything the shell owns moves into the facade. Visible consequence: the Qt Quick shell now treats a platform that reports no color scheme (`Qt::ColorScheme::Unknown`, which is what Xvfb reports) as *light*, exactly as the classic frontend always has; the shell was dark there before. \*Proposed by the assistant and implemented in the same change; recorded here for the owner's review.\* |
| D29 | The interactive viewport renderer is a frontend-neutral service: `gui/base/viewport/ViewportRendererRegistry` (a singleton) lists the available renderer implementations, reads and writes the user's selection (`OVITO_VIEWPORT_RENDERER` overriding the `rendering/selected_graphics_api` setting), caches the renderer instances with their saved parameter settings, falls back to the default renderer when the selected implementation is unavailable, and announces changes with `rendererSelectionChanged()`. Both frontends take their renderer from it, and the Qt Quick frontend reverts to the default renderer when a `ViewportWindow::fatalError` arrives (the classic `ViewportsPanel` did this on its own before). | This is review finding A2. The renderer choice was reachable only through `GuiApplication::instance()`, which does not exist in the Qt Quick frontend, so that frontend hardcoded a `StandardRenderer` and had neither a renderer selection nor any recovery when rendering failed. Only this part of A2 was extracted: the review's `ViewportWindowManager` was rejected while implementing it, because the two frontends do not create comparable window objects (the classic `ViewportsPanel` puts a `RenderThread` window into a `QWidget` container, the Qt Quick frontend owns items that the layout model places inside pane delegates) - forcing both through one factory would add a framework rather than remove duplication. Per-viewport window bookkeeping therefore stays where it is; the remaining overlap (creating and destroying a viewport's window as the layout changes) belongs to the insert/delete-viewport work of Phase 4. \*Proposed by the assistant and implemented in the same change; recorded here for the owner's review.\* |
| D28 | The session workflow (save, save-as and 'do you want to save the changes?') is implemented once in `WorkbenchUI` (`saveSessionFile()`, `saveSession()`, `loadSessionFile()`, `isSessionModified()`, `askForSaveChanges()`) with the file dialog behind the `requestSessionFilePath()` hook, and the desktop main window keeps only its `QFileDialog` in that hook. | This completes the session part of review finding A5: the desktop owned the whole workflow, so a Qt Quick frontend would have written a second one. One behavior changes on purpose: a modified session is now also questioned when it has no file path yet (a new, never saved session), where the desktop silently discarded it - the message text for that case already existed in the desktop code but was unreachable, and losing a user's scene without asking is not a defensible default. |

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
commands instead of its own operations. Of the workbench state models of finding A5, the recent files list, the task
progress model (D27) and the session workflow (D28) are shared now; what remains of A5 is a selection model for the
pipeline view (Phase 4) and a settings facade. The viewport renderer is a shared service as well (D29), which gives the
Qt Quick frontend a renderer selection and a recovery path where it previously hardcoded one renderer, and leaves the
per-viewport window bookkeeping with each frontend on purpose. What remains of the work plan is A4 (one offscreen
rendering service for render output, ambient-occlusion sampling and picking buffers), A3 (one asynchronous pick API for
both frontends) and the small parity gaps (among them the Qt Quick session commands that bind to D28). The Windows/D3D12
cross-API gate is closed: the Qt Quick frontend picks and lays out identically on Windows on real hardware and through the
WARP software rasterizer (Phase 1 report, section 3.5), so only the RADV/DRI3 and mixed-DPI cases remain unverified by
environment (see [UI_PARITY_MATRIX.md](UI_PARITY_MATRIX.md) section 6).

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
| O3 | ~~Object picking in the QML viewport is not implemented.~~ **Resolved in Phase 1** — picking is implemented as an asynchronous offscreen pass (D11) and verified end to end. The pre-warm part was also done, in Phase 2.5 (D35): the buffer refreshes itself after a camera or scene change once the view has settled. What remains for Phase 5 (A3) is the pick API itself — hover picks that never answer from a stale buffer, shared by both frontends — plus coalescing hover picks. | Phase 5 |
| O7 | ~~The GPU picking-target creation is duplicated between `RenderThread::ensurePickingResources()` and the QML renderer.~~ **Resolved in Phase 1, confirmed in Phase 2**: the Qt Quick frontend no longer creates picking targets at all — it renders its picking pass through `RenderThread::renderPickingFrame()` with the `ObjectPickingBuffer` handoff, so `ensurePickingResources()` has one caller again. What the two paths do share lives in `ObjectPickingMap::lookupPickResult()`. | Resolved |
| O8 | ~~Qt event handlers run in a task without a `UserInterface`~~ **Resolved in Phase 2**: `GuiTaskScope` (gui/base) opens a `MainThreadOperation` bound to the `UserInterface` for the duration of a presentation-layer callback and exposes its task, which is also what lets the shell cancel a running import. Used by the QML entry points that start work: the viewport controller, the viewport layout and the import commands. | Resolved |
| O9 | ~~The prototype executable (`ovito-qml-spike`) cannot locate the OVITO plugin libraries on macOS without `DYLD_LIBRARY_PATH`.~~ **Resolved in Phase 1** — the spike's `INSTALL_RPATH` was built from `OVITO_RELATIVE_PLUGINS_DIRECTORY`, which is bundle-*root* relative, so the resulting `@executable_path/../Ovito.app/Contents/PlugIns` path was doubled. The target now uses the literal bundle-relative paths of `src/main/CMakeLists.txt`; `otool -l` reports `@executable_path/` and `@executable_path/../PlugIns/`, and the spike runs on the macOS test host with only the Qt library directory in `DYLD_LIBRARY_PATH` (Qt itself is still expected there in the build tree, like the main executable). | Resolved |
| O10 | A QtWidgets frontend test run must not be automated in a headless environment without suppressing the interactive import dialog: `MainWindowUI::importFiles()` shows a modal importer dialog (and asks about the import mode when the scene is non-empty), so an unattended run renders an *empty* scene. The recipes and the temporary benchmark instrumentation are recorded in [UI_TEST_ENV.md](UI_TEST_ENV.md) sections 3.1 and 7. | Testing |
| O11 | ~~`QuickViewportRenderer::renderFrameGraph()` (~100 lines) replays the pass sequence of `RenderThread::renderFrameGraph()` a second time.~~ **Resolved in Phase 2** — the sequence is `FrameGraphRenderPass` (D24) and both renderers only describe their target and their additions to the pass. (Original wording kept for the record:) `finalizeForRendering()`, `createImplementationForVisual()`, `renderFrame()`, `prepareIntermediateTarget()`, `prepareResourceUpdates()`, `performPrePasses()`, `beginPass()` with the full-viewport scissor workaround, `compositeInPass()`, `endPass()` and the `OverLayer`/post-process pass. Every change to that ordering in the core render thread must be mirrored by hand in the Qt Quick renderer; the pick radius (4) duplicates the default of the classic `RenderTarget::requestPick()` in the same way. Together with O7 this points at one core helper — a frame-graph renderer over a `RendererService*` plus a `PickingBufferTarget` — shared by both frontends' render paths. | Phase 2 |
| O4 | ~~Each QML viewport item owns its own `RendererService` state (`RendererResourceCache`, `ObjectIdAllocator`, pipeline cache).~~ **Adopted in Phase 2** (D23): the items of a window share one service owned by the window. Measured on Linux/OpenGL with four viewports at 1280×800 and `QSG_NO_VSYNC=1` (medians of three runs): 512 atoms 64.0 → 114.0 fps (threaded loop) and 118.0 → 175.5 fps (basic loop); 32768 atoms 19.5 → 22.5 fps and 38.5 → 44.5 fps. The gain at low atom counts is the fixed per-frame cost of four viewports, which each item used to pay on its own. | Resolved |
| O5 | ~~The full action/editor inventory and the expanded parity matrix required by the remaining Phase 0 deliverables are still pending.~~ **Resolved in Phase 2.5** (deliverable 1): the inventory is section 6 of this document and the matrix is [UI_PARITY_MATRIX.md](UI_PARITY_MATRIX.md), which carries a state and a phase per capability plus the acceptance case and the fixture list. Writing it also settled scope questions the plan had left open (the 20 commands that still need a QML handler, the fact that A7 has to be measured against the 84 audited editors instead of "all fields", and the six editor archetypes of Phase 6) and found defect F17, which is fixed. | Resolved |
| O6 | ~~The frontend-neutral application class and `--gui=qml` (D3) are unimplemented.~~ **Resolved in Phase 2** — `ovito --gui=qml` starts the Qt Quick workbench and `ovito --gui=qt-widgets` (or plain `ovito`) the classic one, selected through the registry of D15. | Resolved |
| O12 | ~~The QML workbench has no file selection UI and no progress display.~~ **Resolved in Phase 2** (deliverable 5): the shell has a file dialog (`Ctrl+O`) and accepts drops onto the window, the status bar shows the progress of the running operations with a Cancel command, failures are reported in a message dialog, and a canceled import removes its half-loaded pipeline (D19-D22). | Resolved |
| O13 | ~~The prose contract that the executable contract test names does not exist.~~ **Resolved in Phase 2.6** (D52): [AUTOMATION_CONTRACTS.md](AUTOMATION_CONTRACTS.md) is that document and is normative - vocabulary, wire types, identity grammar and lifetime, revision and dispatch rules, the operation catalog, the task/transaction/event schemas, the frozen view-type names, the Python package, worker, bridge and schema-report contracts, the discovery convention and the two error layers of the local transport - with the five CTest suites as its executable half and the exit gate mapped item by item in its §19. Writing it found and fixed one divergence between code and documentation (the implied `task.control` of §6) and one in the probe's documented validation order. | Done |
| O14 | **Resolved in Phase 2.6.** The exit gate asks that external IDs survive an undo/redo and re-resolve afterwards; the contract suite now has `ids_survive_undo_and_redo`, which adds a node through a command, asserts one undo step under the command's label, resolves the node's ID, undoes (the node leaves the scene listing while the ID still resolves to the node the undo stack keeps alive as a tombstone) and redoes (the node is back in the listing under the same ID). The expectation held, so the rule is now a test rather than an assumption. | Done |
| O15 | The decision record numbers two pairs twice: **D10** and **D11** appear once in §2 (the offscreen render-target flag, the asynchronous picking pass) and again in §3 (the reuse of `GuiBase`, the split into `QmlViewportController`), and the D17/D18 rows of §3 carry a decision without a rationale. Renumbering the §3 pair means touching every cross-reference *and* knowing which of the two is meant (the `D11` of the O3 row above is the picking pass), so it is recorded rather than fixed in passing. The Phase 2.6 decisions D39-D49 of §7 are unique. | Follow-up |
| O16 | **The package-location seam of the Python track is a build-time path.** `PythonEnvironmentProbe::defaultScriptFile()` and the spike's worker lookup are baked in as `OVITO_AUTOMATION_PYTHON_DIR` (the source directory), because Phase 2.6 has no installable `ovito` package to look up. Phase 4 has to replace that with the installed-package lookup (and the package's own entry point), which is also what makes the probe meaningful on a user's machine rather than only in the build tree. | Phase 4 |
| O17 | **The topology spike's shared-memory number is an upper bound, and the embedded-runtime half of the comparison is a bound rather than a measurement.** The spike creates one shared-memory segment per evaluation and copies out of the mapping, so a pooled segment with a zero-copy view (the production shape, and the one Phase 4 should implement) is unmeasured; and an embedded runtime was not built at all, so what the spike establishes is that the seam it *would* save is small (≈2 ms of a 13 ms evaluation of 13.7 MB), not how an embedded runtime would perform in general. The spike also ran on one machine (Linux x86_64) with a synthetic payload; the macOS and Windows runs and the structured-property payloads are Phase 4 work. | Phase 4 |
| O19 | **The render half of a capture is unproven.** The endpoint returns a bounded in-memory PNG artifact and the self-test verifies the bytes, the hash and the bounds - but with a generated image, because producing a real one needs a QRhi and this tree has no headless QPA plugin (O1). The *transport* of a capture is therefore certified and the *rendering* behind it is not; the frontend's offscreen check under `xvfb` is today's evidence that a view can be rendered headlessly at all, and joining the two into one capture service (current displayed view versus an explicit scene render, both on the offscreen service of D34) is Phase 5. | Phase 5 |
| O20 | **The endpoint of the local protocol is prototype code.** It serves one connection at a time, has no backpressure beyond the socket buffer, no resume beyond a `since` sequence and no authentication beyond the permissions of the session directory and the socket; it was measured on Linux x86_64 only, while macOS enforces a 104-character socket path limit and Windows uses named pipes without permission bits. **Phase 3 delivered the transport into Core and the CLI that uses it** (D67, D69), so what remains is the production *remote* story: real authentication, backpressure, resume beyond a `since` sequence and the platform-specific limits above, which no phase has designed yet (design §4.3). | Phase 4+ (remote access) |
| O21 | **The data bridge speaks bytes, not OVITO property arrays.** There is no adapter from a `PropertyObject`'s buffer to a `PythonArray` and back, so the ownership and copy rules of D51 are proven for owned byte blocks rather than for a live pipeline buffer; the design's data contract (what a modifier receives, what it may write and what a partial result means) is Phase 4's subject together with the streaming form of a transfer larger than 512 MB and the byte-order question a remote worker would raise. | Phase 4 |
| O18 | **The Phase 2.6 modules have no caller outside their own tests.** No frontend, CLI, transport or Python package uses `AutomationGateway`, `AutomationSession` or `PythonEnvironmentProbe` yet: `session.attachToContainer()`, `setUserInterface()` and the catalog are exercised by the contract suite and the spike, and the gateway is never constructed by the application. This is what "architecture adaptation" means for the phase, but it also means the layer's integration risks (thread affinity under a live frontend, teardown order on session close, an actually attached `DataSetContainer`) are still unproven and belong to the phase that first attaches it. **S1 closed the attached-`DataSetContainer` half** and **S4 the rest:** every workbench now creates its `AutomationSession` and attaches it in `WorkbenchUI::initializeWorkbench()` (D55, D59), and a workbench that serves the session creates the endpoint there too, which is what exercised the layer under a live frontend - the session's revision moving with the frontend's own signals, a local client reading it, and the endpoint being destroyed with the object that owns the session before the session itself (D67, D70, `--qml-automation-check`). | Resolved by Phase 3 |
| O22 | **The classic frontend keeps its own action search.** `gui/desktop/actions/SearchActions.cpp` ranks `QAction`s by a use-count map it stores in raw `QSettings` (`actions/use_counts`), so until it adopts the shared command list model of D57 the two frontends rank the same commands from different data and one `QSettings` key group lives outside the facade of D30. The adoption is Phase 8 work (the command palette) and must not be done in passing, because the desktop's ranking is user-visible. | Phase 8 |
| O24 | **The walk that finds animated parameters, and the formatting of a key's value, are duplicated.** They exist once in the classic `AnimationTrackBar` (a desktop widget, and the only implementation in the tree) and now again in `QmlAnimationModel`, so a change to the labelling rule (`owner->objectTitle() + " - " + field->displayName()`, comma-appending when several owners share a controller) or to the value of a key would have to be made twice. Unifying them means moving the walk into `gui/base` as a service both timelines use, which is exactly the work of handing the timeline over (Phase 5 d2); doing it in Phase 3 would have meant designing that service against one consumer. | Phase 5 d2 |
| O25 | **The automation command line lives in Core.** `AutomationCommandLine` - option parsing, the human-readable formatting and the exit-code policy - is `OVITO_CORE_EXPORT`ed from `src/ovito/core/automation/`, although it is a frontend of the machine-facing layer rather than domain code, so the domain library owns a CLI's text and every plugin links it. Moving it beside the executable (`src/main/`, where `GuiApplication` already calls it) needs a library of its own, because `tst_automation_cli` links only `Core`; until then the placement is deliberate rather than accidental. | Phase 4 (together with the writable verbs) |
| O23 | **Phase 3 proves models, not views.** The QML pipeline view, the animation timeline and the editors arrive in Phases 4-6, so a model verified through the spike harness is not yet proven against a real view's iteration, delegate-recycling and lifetime patterns - the failures that only appear when a view reuses a delegate for a different object (defect F12 of the Phase 1 spike was of this kind). The first of those views is Phase 4 d1 and it has to re-run the S1 check with the view in place. | Phase 4 |

## 6. Action, Editor and Control Inventory (Phase 0 deliverables 1, 3 and 4)

This section is the "which control exists where" map that the parity matrix
([UI_PARITY_MATRIX.md](UI_PARITY_MATRIX.md)) is built on. It records what the classic frontend has, where it lives and in
which layer it already resides, because that decides whether the Qt Quick frontend needs a shared extraction, a new
implementation, or nothing. Every count is reproducible with the command quoted next to it.

### 6.1 Commands: the 76 action ids

`gui/base/actions/ActionManager.h` declares 76 `ACTION_*` ids; they are not 76 commands. Grouping them by who creates them
and who handles them is what decides the QML work:

| # | Group | Count | Created by | Handled by | State in the Qt Quick frontend |
|---|---|---|---|---|---|
| 1 | Shared command, shared handler | 23 | `ActionManager` (gui/base) | gui/base: 12 `connect(getCommand(...))` sites, the undo-stack wiring (3), the playback mirror (1), the auto-key-mode wiring (1), and the 6 viewport-mode commands, which act through `ViewportModeCommand` | works today; exercised by `--qml-command-check` |
| 2 | Shared command, desktop-only handler | 20 | `ActionManager` (gui/base) | `WidgetActionManager` (gui/desktop) | the command exists and is visible, triggering it does nothing: each needs a QML handler or a disabled placeholder |
| 3 | Desktop-created command | 4 | `WidgetActionManager` | same file and the panels | not visible to QML at all: the two XForm modes (Phase 5) and snippet import/export (Phase 4) |
| 4 | Panel command | 19 | `PipelineListModel` (8), `ModifyCommandPage` (1), `OverlayCommandPage` (5), `CommandPanel` (4), `SearchActions` (1) | the panel that creates it | arrives with its panel (Phase 4 pipeline and layer panels, Phase 8 command search) |
| 5 | Vestigial scripting id | 10 | nobody | — | nothing to port (see below) |

* **Group 1 (23)**: `EditUndo`, `EditRedo`, `EditClearUndoStack`, `EditDelete`, `ViewportMaximize`, the four
  `ViewportZoom*Extents*`, `AnimationGoto{Start,End,PreviousFrame,NextFrame}`, `Animation{Start,Stop}Playback`,
  `AnimationTogglePlayback`, `AnimationToggleRecording` and the six viewport-mode commands (`SelectionMode`,
  `ViewportZoom`, `ViewportPan`, `ViewportOrbit`, `ViewportFOV`, `ViewportOrbitPickCenter`). The QML spike verifies undo and
  redo state, `EditDelete` enablement, `ViewportMaximize` through the shared handler, the viewport modes through
  `ViewportModeCommand`, and the playback command starting and stopping playback.
* **Group 2 (20)**: `Quit`, `File{Open,Save,SaveAs,Import,RemoteImport,Export,NewWindow}`,
  `Help{About,ShowOnlineHelp,ShowScriptingReference,SystemInfo,RequestFeature}`, `Settings`, `AnimationSettings`,
  `RenderActiveViewport`, `ConfigureViewportGraphics`, `ClonePipeline`, `RenamePipeline`, `NewPipeline.FileSource`. Each
  handler is a `WidgetActionManager` slot that opens a QtWidgets dialog or calls a desktop service. This is the exact reason
  the QML shell sees 43 of the 76 commands but can act on 23 of them.
* **Group 5 (10)**: `ScriptingRunFile`, `ScriptingGenerateCode` and the eight `ScriptingShowExtensionsGallery*` ids are
  declared and never registered. The File menu adds the two scripting entries only when they exist
  (`MainWindow.cpp:328`, `:330`, both through `findAction`), so a classic build shows no such entries — the Python-based GUI
  integration is not part of this tree. Three other call sites (`AvailableModifiersSelectorWidget::onGetMoreModifiersFromPopup`,
  `AvailableOverlaysSelectorWidget::onGetMoreLayersFromPopup`, `UtilityCommandPage::onOpenUtility`) used the **asserting**
  `getAction()` for gallery ids that are never registered: with `NDEBUG` they open the ovito.org extensions page as a
  fallback, in an assert-enabled build they trip the assertion inside `ActionManager::getAction()`. Recorded as defect F17
  and fixed here by calling `findAction()` at those three sites, which is what `MainWindow.cpp` does.
  `ACTION_NEW_PIPELINE_PYTHON_SOURCE` and `ACTION_NEW_PIPELINE_LAMMPS_SCRIPT_SOURCE` are in the same state and are the
  placeholders of the newer pipeline types.

### 6.2 Property editors and parameter controls

84 classes derive from `PropertiesEditor` (`grep -rl 'public PropertiesEditor' src/ovito --include='*.h'`) and 115
`SET_OVITO_OBJECT_EDITOR` registrations cover 114 distinct object classes; `DefaultPropertiesEditor` is registered for
`RefTarget` itself and is the fallback. By module: particles 31, stdmod 22, gui/desktop 12, stdobj 7, crystalanalysis 6,
grid 2, and one each for mesh, oxdna, correlation and vorotop. Six classes that end in `Modifier` have no editor — four are
abstract bases (`DelegatingModifier`, `MultiDelegatingModifier`, `ReferenceConfigurationModifier`,
`StructureIdentificationModifier`) and are covered through their subclasses, one is a delegate base
(`GenericPropertyModifier`), and one is a concrete object without any editor (see below).

The control vocabulary is 26 `*ParameterUI` classes in `gui/desktop/properties/`: `ParameterUI` and `PropertyParameterUI`
(the bases), `NumericalParameterUI`, `FloatParameterUI`, `IntegerParameterUI`, `IntegerCheckBoxParameterUI`,
`IntegerRadioButtonParameterUI`, `BooleanParameterUI`, `BooleanGroupBoxParameterUI`,
`BooleanRadioButtonParameterUI`, `BooleanActionParameterUI`, `ColorParameterUI`, `StringParameterUI`, `FilenameParameterUI`,
`FontParameterUI`, `VectorParameterUI`, `AffineTransformationParameterUI`, `VariantComboBoxParameterUI`,
`DataObjectReferenceParameterUI`, `RefTargetListParameterUI`, `SubObjectParameterUI`, `ModifierDelegateParameterUI`,
`ModifierDelegateFixedListParameterUI`, `ModifierDelegateVariableListParameterUI`, `PipelineSelectionParameterUI`,
`CustomParameterUI`, plus non-editor helpers (`ObjectStatusDisplay`, `OpenDataInspectorButton`).
`stdobj/gui/widgets/PropertyReferenceParameterUI.h` adds the property chooser.

**The classic frontend has no reflection fallback.** `DefaultPropertiesEditor` only re-opens the sub-editors of reference
fields flagged `PROPERTY_FIELD_OPEN_SUBEDITOR`; it never enumerates scalar fields, and no code under `gui/` iterates
`propertyFields()` to build controls (`grep -rn 'propertyFields()' src/ovito/gui src/ovito/*/gui` returns that loop plus two
special cases in `AnimationTrackBar` and `ColorLegendOverlayEditor`). A parameter is editable in the classic frontend only
because an editor class asks for it, which has two consequences for the migration:

1. The QML field model and generic editor (A7) must be measured against the **audited editors** — the 84 classes and the
   fields they request — not against "all fields of all classes". The latter is a superset of what the classic frontend
   offers, and one concrete case exists where it is: `CoordinationPolyhedraModifier` (derived directly from `Modifier`) has
   no editor, so its `transferParticleProperties` field is not editable in the classic frontend at all.
2. What the editors do *besides* scalar fields is a closed list, and it is the list of specialized QML editors:

| Pattern | Example | QML phase |
|---|---|---|
| Button that calls a core API | Slice "Center in simulation cell", color coding "Export color scale" | 6 |
| Table of sub-objects with per-row color/enable | `StructureListParameterUI` (structure types of CNA/PTM) | 6 |
| Chooser for a delegate or a property class | `ModifierDelegateParameterUI`, `PropertyReferenceParameterUI` | 4, 6 |
| Chooser with a preview and non-scalar values | the color gradient list (image and table gradients included) | 6 |
| Dependent enablement between controls | the CNA cutoff fields are enabled only in "fixed cutoff" mode | 4 (field dependency) |
| Widget showing computed results | `ObjectStatusDisplay` ("N structures identified") | 6 |
| Two-dimensional layout with labels and units | every numerical editor (`FloatParameterUI::label()`) | 4 |
| Visualization tied to the edited object | the `ViewportGizmo` users of §6.4 | 5, 6 |

### 6.3 The four audited specialized editors (deliverable 4)

The Phase 0 deliverable asked for fields and actions of a slice, a structure-identification, a color-coding and a PTM
editor. All four exist and are ordinary layouts of the controls above; the difference between them is which non-field
pattern they use.

| Modifier | Editor | Controls | Coverable by the generic field model | Needs a specialized QML editor |
|---|---|---|---|---|
| `SliceModifier` | `stdmod/gui/SliceModifierEditor` | `FloatParameterUI(distanceController)`, `VectorParameterUI(normalController)` (3 components), reduced-coordinates radio pair with unit conversion, `widthController`, four booleans, "Center in simulation cell" button, `ViewportGizmo` for the plane and a viewport overlay | the numerics (they are **Controller** fields: animatable, with units and bounds) and the booleans | the radio pair with unit conversion, the action button, the plane gizmo and the overlay |
| `CommonNeighborAnalysisModifier` | `particles/gui/modifier/analysis/cna/CommonNeighborAnalysisModifierEditor` | `IntegerRadioButtonParameterUI` with four modes, `FloatParameterUI(cutoffRadiusController)`, the `CutoffRadiusPresetsUI` combo (material presets), two booleans, `ObjectStatusDisplay`, `StructureListParameterUI` table | mode enum, cutoff number, booleans | per-mode enablement of the cutoff controls, the presets combo, the status widget, the structure table |
| `ColorCodingModifier` | `stdmod/gui/ColorCodingModifierEditor` | `ModifierDelegateParameterUI`, `PropertyReferenceParameterUI`, gradient combo with preview, start/end `FloatParameterUI`, auto-adjust boolean, "Export color scale" button, color-legend overlay toggle, image/table gradient loading | the two numbers, the boolean, the property name behind the chooser | both choosers, the gradient chooser with its preview and its image/table variants, the export button |
| `PolyhedralTemplateMatchingModifier` | `particles/gui/modifier/analysis/ptm/PolyhedralTemplateMatchingModifierEditor` | `FloatParameterUI(RMSD cutoff)`, two booleans, five output booleans in a group box, `StructureListParameterUI` table | all eight booleans and the number | the structure table (and group boxes as layout, not as behavior) |

Conclusion for Phase 6: six editor archetypes cover the audited ground — the generic field editor (A7) plus a numeric
field with controller/unit/bounds semantics, a radio group with dependents, a sub-object table with per-row color and
enable, a class/property chooser, a gradient chooser with preview, and an action/status row. Everything else in the 84
editors is a layout of those.

### 6.4 Viewport input modes and gizmos

`BaseViewportWindow` forwards mouse and key input to the mode stack of the `ViewportInputManager`, which is already shared
(`gui/base/viewport/ViewportInputManager.h`). Of the eight modes, six are in `gui/base` and work in both frontends today
(`SelectionMode`, the four `NavigationMode`s Zoom/Pan/Orbit/FOV, `PickOrbitCenterMode`); two are desktop-only and are Phase
5 work (`XFormMode` with its Move/Rotate subclasses, `MoveOverlayInputMode` for dragging overlay labels).

Three classes are `ViewportGizmo` users: `NavigationModes` (rotation feedback, shared), `SliceModifierEditor` (the
draggable slice plane) and `ManualSelectionModifierEditor` (the rubber-band selection). The latter two are Phase 6
specialized work — they are the "visualization tied to the edited object" row of §6.2.

### 6.5 Viewport layers (overlays)

Three overlay types exist and each has a desktop editor: `CoordinateTripodOverlay`, `TextLabelOverlay`
(`gui/desktop/viewport/overlays/`) and `ColorLegendOverlay` (`stdmod/gui/ColorLegendOverlayEditor`). The layer list model
that feeds the panel (`gui/base/mainwin/OverlayListModel.h`, `OverlayListItem.h`) and the layer templates
(`gui/base/mainwin/templates/OverlayTemplates.h`) are already shared, so the QML layer panel is a Phase 4 binding of shared
data plus those three editors.

### 6.6 Panels, dialogs and widgets

`gui/desktop/dialogs/` holds 28 headers, 18 of them dialog classes: `AdjustViewDialog`, `AnimationKeyEditorDialog`,
`AnimationSettingsDialog`, `ApplicationSettingsDialog`, `ClonePipelineDialog`, `ConfigureViewportGraphicsDialog`,
`CopyPipelineItemDialog`, `ExportObjectSnippetDialog`, `FileExporterSettingsDialog`, `FontSelectionDialog`,
`HistoryFileDialog`, `ImportFileDialog`, `ImportObjectSnippetDialog`, `ImportRemoteFileDialog`, `LoadImageFileDialog`,
`MessageDialog`, `ModalPropertiesEditorDialog`, `NewGraphicsSystemDialog`, `RemoteAuthenticationDialog`,
`SaveImageFileDialog`, `SystemInformationDialog`, `UpdateNotificationDialog`, plus the settings and template pages
(`FFmpegSettingsPage`, `GeneralSettingsPage`, `ViewportSettingsPage`, `ModifierTemplatesPage`, `OverlayTemplatesPage`,
`TemplatesPageBase`).

The main window is assembled from `ViewportsPanel`, `DataInspectorPanel`, the command panel with its four pages
(`ModifyCommandPage`, `RenderCommandPage`, `OverlayCommandPage`, `UtilityCommandPage`, driven by `UtilityListModel` and the
selector widgets), `TaskDisplayWidget`, `StatusBar`, `CoordinateDisplayWidget`, `StatusWidget`, the animation widgets
(`AnimationTimeSlider`, `AnimationTrackBar`, `AnimationTimeSpinner`) and `FrameBufferWindow`/`FrameBufferWidget` for render
output. `widgets/general/` contributes 18 reusable QtWidgets (`SpinnerWidget`, `ColorPickerWidget`, `AutocompleteLineEdit`,
`ElidedTextLabel`, `RolloutContainer`, …) that the Qt Quick frontend replaces with Qt Quick Controls rather than ports.
`gui/desktop/widgets/terminal/{TerminalWidget,TerminalBackend}` is not instantiated anywhere in this tree: the integrated
SSH client (`OVITO_BUILD_SSH_CLIENT`, off by default) is not built here, so remote import is a feature the frontends
inherit rather than something the QML shell has to reach parity with now.

### 6.7 Data inspector

The panel is `DataInspectorPanel` with the `DataInspectionApplet` base and five applets: `PropertyInspectionApplet` and
`TypesInspectionApplet` (stdobj), `SimulationCellInspectionApplet` (stdobj), `GlobalAttributesInspectionApplet`
(gui/desktop) and `DislocationInspectionApplet` (crystalanalysis). The panel is fed by the selected object and the applets
declare which classes they can show, so the QML data inspector is Phase 7 work over the same applet contract.

### 6.8 Utilities and templates

The Utilities page has no fixed list: `UtilityListModel` enumerates
`PluginManager::instance().metaclassMembers<UtilityObject>()` (plus the "Get more extensions" item), which is why the QML
counterpart must enumerate the same registry instead of naming utilities. The object, modifier and overlay templates
(`gui/base/mainwin/templates/`) are already shared with the classical command panel, and the modifier chooser
(`AvailableModifiersModel`, `AvailableOverlaysModel`) is shared as well.

### 6.9 What this inventory settles

* The **shared half of the command layer is bigger than the visible half**: 43 command ids reach QML, 23 of them also act.
  The remaining 20 are the concrete list of handlers the shell still needs (or must show as disabled placeholders), and each
  of them is a dialog or a desktop service — never shared logic.
* **A7's scope is the audited editors, not reflection over all fields.** The generic QML editor can be complete for the
  fields the 84 audited editors request; objects without an editor expose nothing in either frontend until someone authors
  an editor.
* **Phase 6 needs six editor archetypes, not 84 ports.** The four audited specialized editors of §6.3 use four of them; the
  rest of the catalog is layout.
* **Two features are not parity gaps**: the scripting ids of group 5 have no implementation in this tree, and the SSH
  terminal/remote import is an optional, currently unbuilt feature.
* Defect **F17** (three `getAction()` lookups of never-registered gallery ids) is fixed as part of this inventory.

---

## 7. Phase 2.6: The Machine-Facing Automation Layer

> **Status**: Phase 2.6 is complete and its exit gate is verified: all ten deliverables are in the tree, the client-facing
> rules are written down in [AUTOMATION_CONTRACTS.md](AUTOMATION_CONTRACTS.md) (which closes O13 and is now the normative
> description the contract suite is the executable half of), and the exit-gate items of [UI_PLAN.md](UI_PLAN.md) §5 are
> answered one by one in that document's §19. This section is the record of the architecture decisions behind the work
> (D39-D52); §5 carries the open items it leaves behind (O13 and O14 are closed, O15 is a pre-existing numbering defect of
> this document, O16-O18 and O21 belong to Phases 3-5) and has no item that the phase itself still owes. The execution
> topology of the Python track is decided and its evidence recorded in
> [AUTOMATION_TOPOLOGY_SPIKE.md](AUTOMATION_TOPOLOGY_SPIKE.md); the local client protocol in
> [AUTOMATION_IPC_SPIKE.md](AUTOMATION_IPC_SPIKE.md); and the schema preview together with the data bridge in
> [AUTOMATION_DATA_BRIDGE.md](AUTOMATION_DATA_BRIDGE.md). The execution topology of the Python
> track is decided and its evidence recorded in [AUTOMATION_TOPOLOGY_SPIKE.md](AUTOMATION_TOPOLOGY_SPIKE.md); the local
> client protocol in [AUTOMATION_IPC_SPIKE.md](AUTOMATION_IPC_SPIKE.md); and the schema preview together with the data
> bridge in [AUTOMATION_DATA_BRIDGE.md](AUTOMATION_DATA_BRIDGE.md).

### Findings

1. **No machine-facing boundary existed.** Every editing path in the repository is presentational: a QtWidgets dialog, a
   `Command` of the shared command layer (`gui/base/actions/Command.h`, D26) or an `ActionManager::createCommand()` caller. A
   `Command` carries what a *menu item* needs — text, shortcut, icon, checkable state, handler — and nothing that a remote
   client needs: no argument schema, no capability, no structured error, no revision. Review item A1 removed the *second*
   command table; it did not create a machine-facing one, and the plan requires both to exist without deriving one from the
   other.
2. **Object identity did not exist.** `PipelineListModel` and the QML models address objects by pointer or by row index.
   Neither can cross a process boundary, and an index means something else as soon as the row set changes, which is the
   opposite of what the shared concepts of §5 require (stable object IDs, a session revision, structured errors, capability
   checks).
3. **The data set had no revision.** `DataSetContainer` announces a changed data set, layout, active viewport, selection
   and animation time as Qt signals, and nothing counted them. A client therefore had no way to state which snapshot it
   planned against, and no way to be told that its snapshot was outdated.
4. **What was missing belongs in `core`.** The consumers of the future gateway are the headless CLI of Phase 3, the
   `ovito` package of Phase 4 and (Phase 8) an MCP adapter; only one of them is a frontend, and Phase 7's batch path is
   not.
5. **Two runtime preconditions of the layer are invisible in a release build** (recorded with the recipe in
   [UI_TEST_ENV.md](UI_TEST_ENV.md) §4.1): `OORef::create()` of a `RefTarget` asks the ambient task whether the creation
   is interactive, and creating a `SceneNode` asks `Application::instance()` for the main thread
   (`this_task::isMainThread()` in `SceneNode::invalidateWorldTransformation()`). A process without either dereferences a
   null pointer in a release build; the assertion-enabled build reports the second one and stays quiet about the first,
   because its `OVITO_ASSERT(this)` sits in a member function whose `this` may be assumed non-null.
6. **A client could change the session without the session being able to say who did it.** The first slice had one
   permission set on the gateway and one anonymous caller: no client identity, no origin, no record of a grant, no task
   per unit of work and no transaction around a command. Every later consumer the plan names — a CLI, an AI plan, a Python
   modifier — needs all five, and the places it needs them are the same places the *scene* is changed, i.e. the gateway.
7. **There was no runtime description of a Python environment anywhere in the repository.** `src/ovito/core` contains no
   `QProcess`, no interpreter probe and no package-version check; the only Python-related code is the legacy Python
   scripting layer of the classic frontend, which the new track may not depend on. The design's own requirement — an
   explicit compatibility check before loading a script, and no silent fallback to another interpreter — therefore had no
   implementation and no vocabulary to be expressed in.
8. **A client had no way to find a running workbench, and none to talk to one.** The design asks a local client to
   discover one live session, read a bounded snapshot of it, follow its task and scene events and ask for an image; the
   repository had no session descriptor, no endpoint, no protocol and no discovery directory, and no process other than
   the application itself could reach a session. The one convention that would have been wrong to invent per client -
   where a session describes itself, how a stale entry is recognised and how the socket is protected from other local
   users - had no implementation and no vocabulary either.
9. **The execution topology was an open question with no measurement behind it.** The design named two candidates
   (embedded runtime, persistent worker) and the criteria for choosing between them (throughput and data transfer on
   representative particle and mesh input, startup, cancellation, crash recovery, frame change), but nothing in the
   repository could produce those numbers: there was no worker protocol, no spawn path and no benchmark fixture. The
   worker script and the spike that now exist are the first Python-facing code of the new track, and they were written to
   *answer* that question rather than to become the production implementation.
10. **Nothing could hand an interpreter a number, and nothing could read a file without running it.** The Python track's
   whole premise is that OVITO's arrays reach a user's function and that a file the user has selected is *inspected*
   before it runs, and neither was expressible in the repository: there was no client that starts an interpreter and
   keeps it, no framing for a payload that is not JSON, no ownership rule for a buffer that belongs to a pipeline, and no
   way to read a decorated function's parameters without importing the module. Writing them turned two assumptions into
   executable rules — the transfer is a copy with a digest at both ends, and only an explicit import mode may execute a
   file — and it found a real defect in passing: the worker's *graceful* shutdown aborted the interpreter
   (`Fatal Python error: _enter_buffered_busy`, because the reader thread holds the buffered-reader lock while the
   interpreter finalizes), so a worker that answered `quit` was reported as one that had to be killed. The spike never saw
   it, because the spike always killed its worker; the bridge's `stop()` asks first, and now the answer is exit code 0.

### Decisions

| # | Decision | Rationale |
|---|----------|-----------|
| D39 | The machine-facing layer is `src/ovito/core/automation/`, compiled into `Core` and owned by whoever creates a session: `AutomationContract` (the versioned vocabulary: contract version `0.1`, the operation kinds, the eleven capability names, the ten error codes and the compatibility rule that only additive minor changes are allowed), `AutomationProtocol` (the wire types — parameter, operation descriptor, request, result, artifact — and their JSON forms), `AutomationObjectId`, `AutomationObjectRegistry`, `AutomationSession` and `AutomationGateway` (the dispatcher and the operation catalog). No transport, no frontend and no QtWidgets type appears in the layer; it is documented as main-thread-only and not thread-safe. The three built-in operation IDs are dot-namespaced (`session.describe`, `scene.list_nodes`, `pipeline.describe`) and reuse no command id. | The plan puts the gateway "above core `Scene`, `Pipeline`, `Modifier`, `Property`, `Task`, `Undo`" and forbids QML, QtWidgets, the CLI and Python from owning duplicate mutation logic. `gui/base` would have been the wrong home: the headless CLI, the Python bridge and the batch path must not depend on a frontend layer, and the layering rule of this project runs core ← gui/base ← gui/qml. The boundary against D26 is deliberate rather than accidental — a `Command` stays the presentation view of an action, a descriptor is the machine view of an operation, and neither is generated from the other, because their audiences differ (a menu needs text and a checked state, a client needs an argument schema and an error code) and deriving one from the other would couple the protocol version to the frontend's release cycle. |
| D40 | **Client-facing identity is numeric, allocated once and never reused, and a property ID is derived from its owner's.** `AutomationObjectId` defines `scenenode:s7`, `pipeline:p42`, `modifier:m108`, `viewport:v2` and `property:m108/distance`; the number comes from a per-kind counter that starts at 1 and no operation resets it, not even `invalidateAll()`. `AutomationObjectRegistry` holds the objects weakly, so an ID whose object is gone resolves to nothing, and the gateway distinguishes `unknown_object` (never issued) from `invalidated_object` (issued, identity gone). When an entry's pointer is reused by a *new* object at the same address, the registry notices the expired reference, forgets the old text and allocates a fresh number. **Deviation from the plan text**: the viewport ID is numeric, not the plan's example `viewport:v-perspective`, because a viewport's view type is neither unique (a layout may hold two perspective viewports) nor stable (the user switches the view of an existing viewport); a semantic alias may be added later as an additional name, but it must not replace the number. | The exit gate asks for IDs that survive a presentation refresh, re-resolve after undo/redo, and invalidate deterministically on deletion or data-set replacement. A QML delegate index fails both halves — it changes when the row set changes and it means another object after a reload — and a semantic ID fails the second, because it names a role rather than an object. Weak storage plus never-reused numbers is what makes "unknown" and "invalidated" two honest answers instead of one guess; the viewport field is where the plan's own example turned out to be unimplementable as written. |
| D41 | **The session revision is coarse on purpose: only a data-set change invalidates IDs, everything else merely advances the revision, and every answer carries it.** `AutomationSession` owns the data set, a monotonic revision counter that starts at 1 and the registry. `attachToContainer(DataSetContainer&)` follows an interactive frontend: `dataSetChanged` replaces the data set, which invalidates every issued ID *and* advances the revision, while `viewportLayoutChanged`, `activeViewportChanged`, `maximizedViewportChanged`, `selectionChangeComplete`, `currentFrameChanged` and `animationIntervalChanged` each advance the revision and nothing else (the changed object those signals carry is deliberately ignored — the session counts *that* the state moved, not what moved it). A request's optional `baseRevision` goes through `acceptsRevision()`: absent means "the current revision", equal is accepted, anything else is `stale_revision` with both revisions in the details. Success *and* failure results name the revision they were computed from, a command that ran advances the revision after its handler returned, and a query never does. Setting the data set that is already current is not a change: no invalidation, no bump. | The exit gate demands revision preconditions and deterministic invalidation, and the classic frontend has no such notion at all — an undo, a layout change or a selection is invisible to an external client, so the gateway has to impose the snapshot discipline itself. The distinction between "the object is gone" (invalidate the ID) and "the state moved" (re-query, keep the ID) is what lets a client keep a long-lived handle across the user's editing while still refusing to act on a stale plan; the coarse rule keeps that distinction cheap, and it is honest because a *finer* revision would have to be maintained by every core class that can change state. The same-pointer case was aligned with the contract test rather than with the code that bumped the revision, because `DataSetContainer` only emits `dataSetChanged` when another data set becomes current, so the bumping branch was reachable only through a direct call. |
| D42 | **The dispatch order and the error vocabulary are the contract, not an implementation detail.** A request is answered in this order: (1) an empty or unknown operation ID → `invalid_request` / `unknown_operation`, the latter naming the requested ID and the known catalog; (2) an operation this build declares but does not implement → `not_supported`, *before* anything client-specific is looked at; (3) the argument schema → `invalid_argument` with one message per offending parameter; (4) the required capabilities → `missing_capability`, naming both the missing and the granted set; (5) `baseRevision` → `stale_revision`; (6) the operation runs, with an exception becoming a structured error (`internal_error`, or `cancelled` for a cancelled operation) instead of reaching the client as a crash; (7) a command that ran advances the session revision and reports it. | An external client has to be able to tell "fix your request" from "ask for permission" from "re-query the session" from "this build cannot do it" without parsing prose, which is what the error-code vocabulary and the details maps are for; the vocabulary is closed (`invalid_request`, `unknown_operation`, `invalid_argument`, `missing_capability`, `stale_revision`, `unknown_object`, `invalidated_object`, `not_supported`, `cancelled`, `internal_error`) so a client can switch on it. Step 2 is deliberately *before* the client-specific steps and is the one place where the implementation goes beyond the plan text, which fixed no order: a later phase can declare an operation with its schema before it implements it, and a client that hears `not_supported` stops asking instead of requesting a capability that would not help. `not_supported` for an unimplemented operation is therefore the mechanism that keeps the catalog honest — the phase never has to pretend an operation works. |
| D43 | **A client connects read-only, and this phase's artifacts are memory only.** `AutomationPermissionSet` grants the four read capabilities at connect time (`AutomationContract::readOnlyDefaults()`, exposed as `grantReadOnlyDefaults()`) and requires an explicit grant for every capability that lets a client change something — `pipeline.write`, `selection.write`, `animation.write`, `python.execute`, `file.write`, `network.access`, `process.execute` and the `task.control` added by D45. `AutomationArtifact` is a bounded in-memory buffer (id, media type, the operation that produced it, dimensions, frame, and an *optional* file path), and the three operations of the first slice are read-only queries: nothing in the layer writes to disk. | The plan makes read-only the initial default for AI and CLI clients and requires separate capabilities and authorization for Python execution, file writes, network access and external processes; it also requires the local-protocol probe to return a bounded image buffer "without arbitrary filesystem writes". Keeping the artifact an in-memory value with a path field that this phase never fills is what makes the PNG capture of Phase 5 a *client-visible* decision later, instead of a permission this phase quietly granted. The capability check sits in the dispatcher rather than in the operations, so an operation cannot forget it. |
| D44 | **Permissions are granted in exactly one audited place, and every grant is activity.** The mutable `permissions()` accessor is gone: capabilities change only through `grantCapability()`, `grantCapabilities()`, `grantReadOnlyDefaults()`, `revokeCapability()` and `clearCapabilities()`, each of which records an `activity` event (capability, client, whether it was granted or revoked) without advancing the session revision, because a permission is not a property of the scene. A client's identity is allocated by the session (`client:cN`, a display name and an `ActivityOrigin`), and `session.describe` reports it together with the granted capabilities. | An AI client, a CLI and a future remote adapter all end up asking the same question — "who may do what, and since when" — and a plain mutable set answers neither. Routing every change through a recording method makes the audit trail a property of the code path rather than of the caller's diligence, and keeping grants out of the revision counter keeps the client's snapshot semantics about the scene. Making the accessor private is what turned the contract test's five direct `permissions().grant()` calls into gateway calls; that test edit is the intended direction, because the test is the acceptance spec of the boundary and the boundary is now narrower. |
| D45 | **Long work is a first-class session object: a task record with progress, cancellation and provenance, an event log, and capability-gated control.** `AutomationTaskRecord` (`task:tN`, never reused) is created for every dispatched *command* — queries are answered in one step and have no task — with its client, origin, requested and granted capabilities, state (`pending`/`running`/`completed`/`failed`/`cancelled`), progress fraction and text, timings, transaction and the full result; the registry keeps a bounded history (64 finished tasks, dropped IDs answer `invalidated_object` rather than `unknown_object`, the record *number* is never reused). The operations are `task.list`, `task.describe`, `task.cancel` (a command requiring the new `task.control`, which additionally refuses another client's task — `invalid_argument` — and treats cancelling a finished task as success with a warning) and `event.list` (`since` exclusive, `limit`, `kinds`). A handler reaches its own task through `this_automation_task` (`progress()`, `recordActivity()`, `isCancellationRequested()`, `throwIfCancellationRequested()`), the thread-local scope mirroring `this_task`; `OperationCanceled` maps to the `cancelled` error and any other exception to `internal_error`. Cancellation is cooperative: a handler that never checks keeps running, which is documented rather than pretended otherwise. | The plan requires task IDs with progress, completion and cancellation for long operations, and an activity stream that distinguishes user, GUI, CLI, AI and Python origins without recording raw input. Both need a *record* per unit of work, and both need it to survive long enough for a client to ask about it later, which is why the history is bounded but the numbers are not reused: a client that was disconnected for a while gets a determinate answer instead of a wrong one. Progress and cancellation are pulled through a thread-local scope rather than pushed through the handler signature, so the eight operations of the first slice did not have to change shape and a future handler that reports nothing remains valid. The control capability is a new one rather than a reuse of `process.execute`, because stopping work a client itself started is not the same permission as starting an external process. |
| D46 | **Every command is a transaction: a failure leaves no partial change, a success is one undo step, and several commands can be one step.** The gateway opens an `AutomationTransaction` around each dispatched command and commits it only when the result is not an error, aborting otherwise; the boundary owns a real `UndoableTransaction` plus the `UndoSuspender` recording scope when the session has a `UserInterface`, so the change lands on the undo stack under the descriptor's `undoLabel` (default: the operation ID) and *is* undone again on failure. Inside a session-level boundary opened by `beginTransaction()`, a failing command rolls back only its own share (`mark()`/`revertTo()`) and the group stays open. The recording scope is released before the boundary settles, because `UndoStack::push` asserts that nothing is being recorded. `transaction.list` reports the records (`transaction:xN`, `open`/`committed`/`aborted`, the joined operation IDs, the open and close revisions, whether it was recorded through a user interface). | The plan asks for atomic operations, explicit transactions for multi-step plans, and "do not promise rollback for computations or file writes that cannot actually be rolled back". The undo system already provides the only honest rollback OVITO has, so the transaction is defined in its terms instead of inventing a parallel one: a command that cannot be recorded (a session without a user interface) still runs, its transaction just reports `undoable: false` and zero recorded operations. Rolling back a *failed* command is the half the classic frontend never needed — a user who cancels a dialog simply does not apply it — and it is what makes an AI plan safe to attempt. Making the group a session-level object rather than a per-client one keeps multi-client semantics simple: whoever holds the boundary decides when it closes, and every command that arrives while it is open joins it. |
| D47 | **Observability is a bounded in-session log plus one signal, not a push protocol.** `AutomationEvent` (kind `session.changed`, `task.started`, `task.progress`, `task.finished`, `activity`; monotonic sequence, UTC timestamp, revision, origin, client/task/transaction/operation IDs, summary and details) is appended to an `AutomationEventLog` that is a `QObject` with a single `eventAppended(sequence)` signal; the default capacity is 256, old entries are dropped while their sequence numbers stay spent, and `events(since, limit)` reads the history. Tasks, transactions and the gateway write through one borrowed `AutomationAuditTrail` helper whose `record()` is a no-op when inactive. Query handlers get no task record, so `progress()` and `recordActivity()` are documented no-ops there. | The design asks for a "bounded semantic activity stream" that a later client can subscribe to, and the local-protocol spike (deliverable 8) has not chosen a transport yet. A ring buffer plus one Qt signal is the smallest thing that satisfies both halves without pretending a transport exists: Phase 3 turns the signal into a subscription and the buffer into its replay, and the bounded capacity is what "bounded" means for a stream that must not become a log file. The `session.changed` cause is prose ("the selection changed") while the operation ID stays on the gateway's own activity event, because a container signal is not an operation. |
| D48 | **The Python package contract is a version envelope plus an authoritative handshake, and the probe reports instead of repairing.** `PythonContract` fixes `ovito.automation.handshake` protocol `1.0`, the package name, the supported CPython range (3.10-3.13), the platform/architecture matrix, and the feature vocabulary (`schema.introspection`, `function.inplace`, `parameter.scalar`, `array.buffer`, `array.shared-memory`, `task.cancellation`, `traceback.mapping`). `PythonHandshake` carries what the interpreter says about itself (including `sys.executable`, `sys.prefix` vs `base_prefix`, `sys.platform`, `platform.machine()` and the ABI cache tag) and what the package says about itself (`found` from an `importlib` metadata lookup, `imported` from a real import, version, protocol version, features, import error). `PythonEnvironmentProbe` runs the interpreter once and validates in a fixed order — interpreter identity, implementation, version envelope, platform, package presence, package import, protocol major version, required features — and returns one status with a message that names the repair and a `details` map for a CLI's JSON mode. It never installs, upgrades or switches anything, and the class-level rule is that the *features* decide compatibility within one major protocol version while `pyproject.toml` is only an admission filter. | The design requires a runtime probe precisely because a manifest cannot prove that an existing environment has not been mixed, and it requires that an incompatible environment "does not silently fall back". Two decisions make that concrete: `found` and `imported` are separate fields, so a package that is installed but raises while importing is reported with its own error instead of being called missing; and `sys.executable` is compared with the executable that was started, so a wrapper or shim that runs user Python somewhere else is an error rather than a detail. The check order is the same discipline as D42's dispatch order: a caller that receives `package_missing` knows the interpreter and the envelope already held. Feature-based compatibility (rather than a version comparison) is what lets a newer package serve an older application, and it is the same additive-minor rule the automation contract uses. |
| D49 | **The Python execution topology is a persistent worker process on the user-selected interpreter, with raw length-framed arrays by default and shared memory as an optional optimisation; an embedded runtime is not selected.** The spike measured, on 500 000 particles plus 200 000 triangles (13.7 MB per evaluation): startup 39.7 ms, control round trip 10 µs, in-process baseline 10.8 ms, `framed` 13.0 ms, `shared-memory` 13.3 ms, `base64` 98.9 ms, `text` 539.2 ms, cancellation 1.3 ms, crash noticed in 4.7 ms and recovered in 43 ms; a numpy-free interpreter costs 226 ms for the same compute and 16 ms to start. The package-level API stays the same either way, so an embedded backend remains additive for a fixed supported ABI. | The design forbids freezing the topology before "one representative particle/mesh benchmark and failure-recovery test pass". They pass, and the numbers say the seam costs about 2 ms (+20 %) on top of computing the same data in process, which is well inside an interactive budget, while JSON carries 41× the cost for 2.3× the bytes. Embedding could save only that small part and would cost a fixed CPython ABI, GIL/thread integration and the crash isolation the worker demonstrates (recovery in one restart, without the application surviving anything). Shared memory is left as an optimisation because the measured per-evaluation segment is not faster than the pipe; the numbers, the caveats (pooled segment not measured, one machine, synthetic payload) and the reproduction recipe are in [AUTOMATION_TOPOLOGY_SPIKE.md](AUTOMATION_TOPOLOGY_SPIKE.md). |
| D50 | **The local client protocol is a per-user local socket carrying JSON Lines control messages, discovered through a session descriptor in a per-user runtime directory, with capabilities per connection and two disjoint error vocabularies.** The descriptor (identity, owning process, socket path, start time, contract version) is a Core value type written `0600` into a `0700` directory by atomic rename and scanned by a client, which classifies an entry as stale when its process is gone; the socket is an **absolute path inside that directory**, because a socket created from a relative name lands in the world-writable temporary directory where another user could own the name; every connection gets its own gateway, its own identity and a capability set the endpoint's policy allows it to grant, and a client that asks for more is told what was refused; the transport envelope (`invalid_message`, `too_large`, `not_connected`, `unsupported_message`, `too_many_clients`, `render_unavailable`, `internal_error`) is disjoint from the contract's error codes, which travel inside a successful reply; the snapshot is a *composition* of contract answers (session, nodes, pipelines, tasks, events) with explicit `truncated` flags; and a capture returns a bounded in-memory artifact, never a file. | The design's §4.3 and §4.5 ask for exactly this and forbid a network listener by default. The spike measured all of it on one machine: connect 0.06 ms, round trip 0.010 ms, snapshot 0.18 ms, dispatch 0.047 ms, event push 0.0016 ms, artifact 213 bytes verified by the client; 24/24 checks pass, including both shutdown modes (graceful removes the descriptor, killed leaves a stale one to prune) and the two cases that mix the vocabularies (bytes that are not a message, a connection refused beyond the client limit). The endpoint and the client are prototypes that Phase 3 replaces, the descriptor already lives in Core, and arrays never travel this way (D49 governs them). Numbers, protocol table and limitations: [AUTOMATION_IPC_SPIKE.md](AUTOMATION_IPC_SPIKE.md). |
| D51 | **The Python seam is a framed byte transfer with copies and digests, and the schema preview never executes the file it reads.** `PythonWorkerProtocol` (operation names, two disjoint error vocabularies, the runtime handshake validated against the package contract), `PythonWorkerProcess` (the Core client: one interpreter chosen by the caller, `-I`, framed payloads, one outstanding request, death reported with the exit code instead of a timeout, cooperative cancellation) and `PythonDataBridge` (`PythonArray`/`PythonArrayBlock` in a fixed type vocabulary, the `arrays` operation, digests in both directions, refusals that name the array) are Core code; `ovito_schema.py` with `PythonSchemaPreview` and `PythonIntrospector` reads a decorated function's schema in an AST-only mode by default and executes the file only in an explicitly requested import mode. | The alternative — letting each later phase invent its own way of calling Python — is how a data contract becomes three incompatible ones: the design's §3.4.1 already fixes the transport (raw length-framed buffers, JSON for control metadata only, shared memory optional) and §5 fixes the ownership and no-execution rules, but neither was executable. Making the rules Core code with two test suites means Phase 4's modifier consumes a checked contract instead of a plan, and it keeps validation in one place: the bridge refuses an unknown type before it is sent, the worker refuses it again on arrival, and neither side ever converts. The AST mode is the default for the reason the design gives — a selected file must not run — and the suite proves the difference with a fixture that writes a marker from its top level. |
| D52 | **The prose contract is normative and lives in `docs/design/AUTOMATION_CONTRACTS.md`, and `task.control` is implied by every capability that permits a change.** The document states the vocabulary, the wire types, the identity grammar and lifetime, the revision and dispatch rules, the catalog, the task, transaction and event schemas, the frozen view-type names, the Python package, worker, bridge and schema-report contracts, the discovery convention and the two transport error layers; the five CTest suites are its executable half, and a disagreement between the two is a defect on one side to be fixed with a test either way. Writing it found one such disagreement - `AutomationContract`'s own documentation of `TaskControl` claimed that a client granted a work capability is granted it with it, and `AutomationPermissionSet` did not do that - so the set now inserts `TaskControl` with every non-read capability, and the rule is stated in the document's §6. | Two people write contract text: the one who implements a rule and the one who implements the client, and until now the second had to read the first's headers. Naming one document as normative (and one suite as its executable half) is what makes "the shared contracts are versioned and documented" of the exit gate checkable rather than a matter of opinion, and it is the same move as D39's: put the boundary somewhere a client can read it. The `task.control` implication belongs to the same question - a permission model that forces every grant site to remember a second capability is one that will be wrong at one of them - and it grants no new authority (another client's task still cannot be cancelled with it), which is why it could be fixed as a clarification of D44 rather than as a new decision about who may do what. |

### Verification

* **Executable contract**: `tests/cpp/core/automation/tst_automation_contracts.cpp` (28 test functions, 30 QtTest cases
  with `initTestCase`/`cleanupTestCase`), registered as a CTest case by `tests/cpp/core/automation/CMakeLists.txt`
  (`ovito_add_cpp_test`), covering the vocabulary round trips and the read/mutation classification, parameter type and
  constraint validation, descriptor argument validation, request/result JSON round trips, the ID grammar, the registry's
  stability and non-reuse rules, the session revision, five gateway cases (the catalog is read-only, malformed and unknown
  requests, capability and revision checks, the read-only operations, and a real data set), the task lifecycle with
  progress, activity and cancellation, task cancellation with its ownership rule, the event log's ordering and bounded
  history, the per-command undo step and its rollback, the transaction group, the recorded capability grants, the
  view-type vocabulary of the session snapshot (one name per settable value, read back through `session.describe`), the
  deterministic dispatch order (`not_supported` before the argument, capability and revision checks), ID survival across
  an undo/redo and across a presentation refresh (`ids_survive_a_presentation_refresh`: a revision bump of the kind
  `attachToContainer()` produces for a viewport or selection change leaves every identity resolving to the object it
  named, while a deletion and a data-set replacement do not), and the rule that neither the event log nor a task record
  carries an argument value. It runs without a Python installation, without a GUI and without a window system.
* **Runtime probe**: `tests/cpp/core/automation/tst_python_environment_probe.cpp` (20 test functions, 22 QtTest cases),
  with the validation order now matching the enumerator order of `PythonContract::ProbeStatus` (`interpreter_mismatch`
  moved ahead of `python_unsupported`, where the probe has always checked it),
  which needs a `python3` on the path and skips the cases that need one without it. It covers the envelope and the feature
  vocabulary, the handshake round trip and the retention of features of a newer protocol, five malformed-handshake cases,
  the interpreter of the machine (which has no `ovito` package, so the verdict is `package_missing`), a synchronous and an
  asynchronous run of the same environment, one fixture package and fixture handshake per compatibility rule
  (`compatible`, `package_incompatible` with the import error, `package_incompatible` on a protocol major mismatch,
  `feature_missing` with the missing names, `python_unsupported` for an old CPython and for a non-CPython implementation,
  `interpreter_mismatch`), the process-level failures (missing executable, an interpreter that prints nothing or garbage,
  a timeout on a hanging interpreter, a missing probe script, nothing configured), cancellation, and the rule that exactly
  one verdict is emitted per start.
* **Schema preview**: `tests/cpp/core/automation/tst_python_schema_preview.cpp` (14 test functions, 17 QtTest cases),
  covering the report value type (the refusal of a format this build does not know, the JSON round trip), the AST mode (a
  modifier read without executing the file, a syntax error with its position, a decorator and a decorator argument computed
  at run time), the explicit import mode (the same fixture with and without its top-level side effect, and a mapped
  traceback of the user's own frames), and the failures of the exchange itself (a missing file, interpreter and script, an
  answer that is not a report, a timeout, a cancellation, exactly one report per start). It needs a `python3` and skips its
  environment cases without one; it runs in `build-asserts` as well.
* **Data bridge**: `tests/cpp/core/automation/tst_python_data_bridge.cpp` (16 test functions, 18 QtTest cases), covering
  the array and block value types and every description they refuse, the framed round trip against a real interpreter
  (scaled values against their expected arithmetic, echoed arrays against the bytes that were sent, and the digests of what
  the worker received), the rule that an exchange does not modify the caller's block, an 8 MB payload no pipe buffer could
  hold, the refusals of a block that cannot be sent, a description the worker does not know (built by hand, so the
  worker's own defence is tested rather than the client's), and the environment failures (a missing interpreter and script,
  a worker that was killed while a request was outstanding reported with its exit code rather than a timeout, the
  handshake, a graceful stop with exit code 0, and a cancelled operation that leaves the worker usable). It needs a
  `python3` and skips its environment cases without one; it runs in `build-asserts` as well. The numbers are recorded in
  [AUTOMATION_DATA_BRIDGE.md](AUTOMATION_DATA_BRIDGE.md).
* **Topology spike**: `ovito-automation-spike` (built from `src/ovito/core/automation/spike/`, no install rule) measured
  the worker seam on both a numpy environment and a numpy-free one; all 93 respectively 89 self-checks pass (identical
  checksums across every transfer mode, recomputed SHA-256 of the received bytes, identical framed and shared-memory
  bytes, decoded JSON and base64 values equal to the raw values, a dead worker reported with its exit code, a cancelled
  operation reported as `cancelled` with the worker still usable afterwards, and one distinct result per frame). The
  numbers are recorded in [AUTOMATION_TOPOLOGY_SPIKE.md](AUTOMATION_TOPOLOGY_SPIKE.md).
* **Discovery contract**: `tests/cpp/core/automation/tst_session_descriptor.cpp` (9 test functions, 10 QtTest cases),
  covering the allocation of a session name and socket path, the JSON round trip and every reason a descriptor is refused
  (missing fields, a relative or over-long endpoint, another discovery version, another major contract version), the
  owner-only permissions of the directory and the file, atomic publication (no temporary sibling left behind), the scan
  that reports foreign files by name and respects its limit, the stale classification of a descriptor whose process is
  gone, and the discovery scope override. It needs no socket, no event loop and no application object.
* **Local protocol spike**: `ovito-automation-ipc-spike` (no install rule) ran its self-test with 24 checks, 24 passed on
  the recorded machine: discovery, handshake and capability policy, snapshot bounds, dispatch, stale revision, unknown
  operation, missing capability, malformed message, unsupported message type, subscription with pushed events, artifact
  verification (bytes, SHA-256, dimensions), `render_unavailable` without an image source, oversized capture,
  `too_many_clients`, disconnect, graceful shutdown that removes the descriptor, and a killed workbench that leaves a
  stale descriptor for `--prune`. The measurements and the limitations are in
  [AUTOMATION_IPC_SPIKE.md](AUTOMATION_IPC_SPIKE.md).
* **The exit gate of the phase** was verified item by item when the phase was closed; the mapping from each exit-gate
  requirement to the test or measurement that answers it is the table in [AUTOMATION_CONTRACTS.md](AUTOMATION_CONTRACTS.md)
  §19. Nothing in it is satisfied by a later phase's feature: the one command in the catalog is `task.cancel`, every other
  operation is a query, and no builtin operation requires a capability that permits a change.
* **Runs at the close of the phase**: `cmake --build --preset native` builds the whole product including both spike
  programs, `ctest --preset native` passes 11/11 (the five automation suites among them, 97 QtTest cases), the
  assertion-enabled tree of [UI_TEST_ENV.md](UI_TEST_ENV.md) §4 passes the same suites — which is where the two missing
  preconditions of finding 5 surfaced — `ovito-automation-spike` reports 93 checks with numpy and 89 without it,
  `ovito-automation-ipc-spike` 24/24, and `ovito --nogui` exits 0 while `ovito --version` reports the release version.
* **What is *not* claimed**: nothing here is user-visible. No frontend, CLI or Python package calls the layer yet (O18), so
  its integration risks - thread affinity under a live frontend, teardown order on session close, an actually attached
  `DataSetContainer` - are unproven by design. The Python probe is reachable from a test and from the spike only, its
  package location is a build-time path rather than an installed package (O16), the worker is the reference implementation
  of the protocol rather than the Phase 4 backend, and the property-array adapter a modifier needs does not exist yet
  (O17, O21). The transport endpoint and its client are prototypes (O20) and the rendering half of a capture is unproven
  (O19). Every user-facing capability of the two tracks is therefore still assigned to Phases 3-8 in
  [UI_PLAN.md](UI_PLAN.md).

---

## 8. Phase 3: Presentation Models and Command Layer

### Findings

1. **The shared models exist; the layer between them and the QML scene does not.** `PipelineListModel` is already a
   `QAbstractListModel` with a role vocabulary, a selection model and undoable mutations (finding 1 of §3), and
   `ActionManager` already exposes `commandList`/`command(id)`/`triggerCommand(id)` to QML (D26) - but
   `src/ovito/gui/qml/models/`, the location [UI_DESIGN.md](UI_DESIGN.md) §5 assigns to the adapters, does not exist;
   the QML pipeline panel and the inspector are placeholder text naming Phase 4; the shared models are instantiated
   only inside the spike harness (`spike/SpikeHarness.cpp:398-405`); and the QML frontend connects exactly three
   command handlers (import, about, quit, `QmlMainWindowUI.cpp:303-330`).
2. **The pipeline model is widgets-shaped in two small places and has no selection API for a QML view.** Its
   `DecorationRole` returns a `QIcon` for the widgets frontend and an icon-name string for QML (two branches of the
   same role, `PipelineListModel.cpp:625-677`); `IsCollapsedRole` is a role QML cannot name, because
   `roleNames()` (`:848-858`) does not list it; and selection is reachable only through the `QItemSelectionModel` the
   model owns and the single `selectedItemChanged()` signal, neither of which a QML view can drive. The model is also
   single-instance-per-process by construction: its constructor registers commands with fixed ids and
   `ActionManager::addCommand` refuses a duplicate (`ActionManager.cpp:166`), which the harness documents and works
   around.
3. **Nothing on the QML side identifies an object.** The models address objects by pointer or by row index. The
   Phase 2.6 layer introduced exactly the missing thing - `AutomationObjectRegistry` (D40), whose IDs survive a
   rebuild, an undo/redo and a presentation refresh and invalidate deterministically on deletion and data-set
   replacement - and nothing under `src/ovito/gui` uses it.
4. **There is no command list model.** `ActionManager` is itself a `QAbstractListModel` publishing
   `ActionRole`/`CommandRole`/`ShortcutRole`/`SearchTextRole`, and the classic frontend already needed a searchable,
   use-count-ranked view of it: `gui/desktop/actions/SearchActions.cpp` builds an `ActionListModel` (a
   `QSortFilterProxyModel`) over `QAction`s with its own use counts in raw `QSettings` (`actions/use_counts`). That is
   the desktop duplicating a model the shared layer should own, and it is built on the one role QML may not use.
5. **The animation presentation does not exist and cannot be copied.** `AnimationSettings` carries the interval, the
   current frame, the playback speed, `loopPlayback`, `framesPerSecond` and the frame labels, and playback itself is
   core's `SceneAnimationPlayback` (a timer that waits for the viewport windows, announcing
   `DataSetContainer::playbackChanged`/`frameChanged`), which the QML spike already drives. What has no counterpart is
   *keyframe selection*: the classic `AnimationTrackBar` keeps it in widget-local state that is rebuilt on every
   controller-list refresh and is not undoable (`AnimationTrackBar.h:93-119`), while [UI_DESIGN.md](UI_DESIGN.md) §5.2
   requires the model to expose editable controller tracks and keyframe selection - "keyframe marks alone do not meet
   animation parity".
6. **The modifier chooser needs no rewrite, but its one Phase 3 acceptance case is unverified.** `AvailableModifiersModel`
   already offers `CommandRole` (what every frontend can present and invoke) next to the widgets-only `ActionRole`, plus
   categories, templates and applicability; the recorded gap of `--qml-library-check` is that
   "insertion ... is not exercised here" (`spike/checks/ShellChecks.cpp:437-441`).
7. **The session workflow is shared but unreachable from QML.** The three menu entries that name Phase 3 are
   placeholders (`WorkbenchMenuBar.qml:73-86`), `QmlMainWindowUI` does not override
   `WorkbenchUI::requestSessionFilePath()` (whose default throws rather than saving), the close path never calls
   `askForSaveChanges()`, and "save fails visibly" has no QML path at all.
8. **The automation layer has five read operations and no transport in the product.** `session.describe`,
   `scene.list_nodes`, `pipeline.describe`, `task.list` and `event.list` exist and are tested; the endpoint, its client
   and the discovery convention are prototypes inside `automation/spike/ipc/` (O20); there is no CLI, and no frontend
   hosts a session, so `attachToContainer()` and the whole layer have no caller outside their tests (O18).
9. **An undone deletion does not restore the selection in the shared model.** `deleteModificationNode()` re-links the
   pipeline and asks for the *preceding* node to be selected (`setNextObjectToSelect(node->input())`,
   `PipelineListModel.cpp:532-557`), and the list update that follows clears `_nextObjectToSelect` and re-selects the
   rows that were selected before it (`:317-333`). An undo puts the object back into the list, but nothing selects it,
   while [UI_DESIGN.md](UI_DESIGN.md) §5.3 requires exactly that ("returning a deleted selected modifier through undo
   restores the expected selection") and the exit gate asks for "coherent selection after deletion and undo". A QML view
   cannot repair it from the outside either: all it learns is the model's `selectedItemChanged()`.
10. **The contract cannot name every row a panel presents.** `AutomationObjectId::Kind` covers scene nodes, pipelines,
    modifiers, viewports and property fields (D40), while the pipeline list's object rows are modifier rows (a
    `ModificationNode`), the data-source row (the pipeline's root, a `PipelineNode` that is *not* a modification node) and
    the visual-element rows (a `DataVis`); the last two have no kind, so a row↔ID mapping exists for part of the list
    only. Both frontends edit them through the item and its object, which is what those rows offer.
11. **A timeline reads keys but cannot create them, and the walk that finds them is a widget's.** The key objects live in
    the controllers of the animated parameters, not in the animation settings, and the only code in the tree that finds
    them is the classic `AnimationTrackBar`, which walks every reference field that does not carry
    `PROPERTY_FIELD_NO_SUB_ANIM` and labels a controller with `owner->objectTitle() + " - " + field->displayName()`. That
    walk is a private implementation detail of a desktop widget, so the QML model duplicates it (with the key-value
    formatting) and the two can drift; and a timeline has no add-key operation at all - keys come from the property
    editors and from auto-key mode (`AnimationKeyEditorDialog` calls `createKey()` itself) - so the presentation of keys
    and the creation of keys are separate capabilities, which the parity matrix's "add, move and delete keys" acceptance
    case obscures. Recorded as O24.
12. **The undo of a key deletion returns the key, not its selection.** `KeyframeController::deleteKeys()` deletes the keys
    through `RefTarget::requestObjectDeletion()`, whose reference removals *are* recorded as ordinary operations (one
    removal per key inside the compound operation of the call), so a deletion is undoable and one call is one undo step -
    but the undo restores the key objects at the frames they were deleted at and nothing restores the selection they had,
    because the selection of the classic frontend is widget-local state rebuilt from the surviving keys. A timeline that
    promises "the selected keys come back" would be promising something the model does not do.
13. **The QML frontend could not save a session at all, and the classic frontend loads a session five times by hand.**
    `QmlMainWindowUI` does not override `requestSessionFilePath()`, so an unnamed session hit the base-class exception, the
    three File-menu entries naming Phase 3 were disabled placeholders, and the Open/Save/Save As commands had no handler;
    on the other side, "`askForSaveChanges()` + `DataSet::createFromFile()` + `checkLoadedDataset()` + `setCurrentSet()`"
    appears in `FileActions.cpp` (twice), `MainWindow.cpp` (twice) and `GuiApplication.cpp` instead of `loadSessionFile()`,
    and the copies skip two things the shared operation does: the recent-files entry and the clean undo stack of a loaded
    session - so a session opened from a recent entry or dropped onto the window could be reported as modified right after
    it was loaded, and the next close asked about changes the user had just discarded.
14. **The classic frontend's "Save As" saves into the current file.** `MainWindowUI::fileSaveAs()` with no file name
    forwards to `saveSession()`, which asks for a file only when the session has none, so pressing "Save Session State
    As..." on a session that already has a file silently overwrites that file instead of asking for a new destination.
    The shared `saveSessionAs()` is the operation the entry means, and the extraction of the session operations is where it
    was found.
15. **The QML file dialog kept the directory it was once asked for.** `QmlWorkbenchController::showImportDialog()` hands
    the dialog the directory "the frontend asked for" and keeps that request forever, so after one request - a dropped
    folder, or the working-directory rule of D66 - every later file dialog, including the one the File menu opens, would
    start in that directory instead of where the user last was. The request is used up by the dialog it was made for. The
    session check's working-directory phase is what exposed it: the parity check, which runs later in the same run, found
    the import dialog pointed at the session check's directory.
16. **The file question of a frontend is synchronous, and the QML file dialog is not.** The shared session workflow asks
    `requestSessionFilePath()` and continues with the answer, and the QML frontend's dialogs (`FileDialog`) are
    asynchronous; the frontend's message box solved the same problem by blocking the caller in a `QEventLoop`, so the
    question is answered by the same means (D64). A shell that only reports the request and never presents a dialog cannot
    be distinguished from one that does by the shared layer, so the check asserts the presentation itself.

### Decisions

| # | Decision | Rationale |
|---|---|---|
| D53 | **Phase 3 delivers the model and command *API*, not the panels.** `src/ovito/gui/qml/models/` gains the adapters of deliverables 1-4 and they are verified through the spike harness and the C++ tests; the views that paint them - the pipeline view with its gestures and drag & drop, the animation timeline, the property editors - stay with Phase 4 and Phase 5, where the parity matrix already places them. | The exit gate says it in one sentence ("Verify the **model and command APIs**, not a UI workflow ... The interaction acceptance of deliverable 4 - gestures, drag & drop, one undo step per continuous drag, editor entry - belongs to Phase 4, where the `PipelineView` and the editors actually arrive"), and the parity matrix already assigns the pipeline list view and the chooser panel to Phase 4 d1/d3. Building a view first would mean designing the model against a consumer that does not exist - the mistake the Phase 0 audit existed to prevent. |
| D54 | **The QML adapters are controllers in `src/ovito/gui/qml/models/` that own the shared model instances, and `gui/base` grows only what is genuinely shared.** `QmlPipelineController` owns the one `PipelineListModel` of the process (a second instance is impossible by construction, finding 2), exposes the model to a QML view, and offers the mutations and the selection of deliverables 1 and 5 as properties and `Q_INVOKABLE`s; the modifier library of deliverable 3 is the same controller's second half (it reads `AvailableModifiersModel` through `DisplayRole`/`CommandRole`/`flags`, never through `ActionRole`, and `insertModifier()` triggers the row's command), because both halves address the same pipeline, share its selection and walk the same registry - a controller of its own would only forward to this one; the animation model of deliverable 2 lives here too. `gui/base` gains exactly three things in this phase: `IsCollapsedRole` in `PipelineListModel::roleNames()`, the shared command list model of D57, and nothing else unless a slice finds a second consumer. | D26 put a QML-shaped surface *on* the shared object because there is exactly one command table to expose; the pipeline, chooser and animation models are widgets-shaped in their selection, refresh and lifetime semantics, so wrapping them keeps QML-only concerns (instance ownership, property and invokable naming, the QML half of `DecorationRole`) out of the shared layer while the data, the mutations and the undo transactions stay single-sourced. The rejected alternatives are a parallel model in `qml/models/` (a second implementation of the pipeline rules, which is what the migration is removing) and adding QML-only API to `gui/base` wholesale (it would make the shared layer's surface a function of one frontend). |
| D55 | **The QML presentation uses the Phase 2.6 object IDs as its only object identity.** The controllers mint and resolve IDs through the session's `AutomationObjectRegistry` (`scenenode:s7`, `pipeline:p42`, `modifier:m108`), a row's identity travels as that ID and never as its row index, and any deferred work re-resolves the ID before it writes. The frontend therefore hosts one `AutomationSession` attached to its `DataSetContainer` - which is also what deliverable 7 needs - and a write whose ID no longer resolves (deleted target, replaced data set, dropped identity) is refused rather than applied to whoever holds the row now. | D40/D41 already solved this requirement for machine clients, in this process, over this object model, and their IDs are the only identity in the tree that survives a row rebuild, a presentation refresh and an undo/redo while invalidating deterministically on deletion and data-set replacement. Reusing them makes the QML panel and the CLI name the same objects by the same names, closes the "an actually attached `DataSetContainer`" half of O18, and costs one registry lookup per row instead of a second identity scheme with its own invalidation rules. |
| D56 | **The animation presentation model owns the keyframe selection and the visible range it needs, and every data-set mutation goes through the existing controller operations.** Selection and range are presentation state: not part of the data set, not undoable, rebuilt from the controller's key list when the list changes. Playback is not model state either: it is core's `SceneAnimationPlayback`, reached through `DataSetContainer::playbackChanged`/`setAnimationPlayback` and the shared animation commands. | [UI_DESIGN.md](UI_DESIGN.md) §5.2 requires keyframe selection and §5.3 requires undo semantics for key movement and deletion, while the classic frontend's selection is widget-local and non-undoable (finding 5) - so there is nothing to reuse and nothing to stay compatible with, and inventing data-set state for it would make a view detail observable to the CLI and to a session file. Keeping playback in core keeps the two frontends' timelines in step for free, which is why the spike could already drive it. |
| D57 | **The command list model is a frontend-neutral proxy over `ActionManager`, in `gui/base`, not a second command table.** It filters and ranks `Command`s (never `QAction`s) by the id/text/shortcut/search-text roles `ActionManager` already publishes, keeps its ranking data (a command's use count) in the shared settings facade of D30 rather than in raw `QSettings`, and offers a `filter` property that a menu search or the Phase 8 palette binds. | `ActionManager` *is* the model, and the desktop has already proved the need for this proxy by building it desktop-locally over `QAction`s with its own `QSettings` keys (finding 4) - the duplication this phase exists to remove. The Phase 8 command palette is the plan's consumer, and the classic frontend can adopt the shared model then without a rewrite (recorded as O22 until it does). |
| D58 | **A continuous edit is one `UndoableTransaction` per gesture, exposed by the controller as begin/update/commit/cancel.** The controller opens the transaction when an interaction starts, the intermediate writes are recorded while it is open, acceptance (release, Enter) commits it, and Escape or an interruption cancels it - which reverts the recorded writes and restores the value the gesture started from. An empty gesture commits nothing, and a gesture whose target disappears or whose data set is replaced while it is open is cancelled before the target is released. Discrete edits remain one named `performTransaction()` per command, which is what the commands of D26 already do. | [UI_DESIGN.md](UI_DESIGN.md) §5.3 fixes exactly these semantics ("One accepted gesture produces one undo step", cancel restores the original value, an interrupted interaction must not leave a half-applied edit), and OVITO's undo system is the mechanism: the tree already uses this pattern in `SpinnerWidget`, `AnimationTrackBar`, `ViewportsPanel`, `XFormModes` and the QML splitter drag of `QmlViewportLayout`. A dedicated wrapper class was considered and rejected - `UndoableTransaction` plus the controller's own state is the whole of it, and another abstraction is the kind the A8 cleanup removed. |
| D59 | **The frontend hosts a session always, and serves one only when it is asked to.** Every workbench creates its `AutomationSession` and attaches it to its `DataSetContainer` (D55 needs the registry), but a session descriptor is published and a local socket is opened only under an explicit start-up option (`--automation-serve`); the per-connection capability policy is the read-only default of D43 unless the option grants more, and a workbench that is not asked to serve is invisible to a client. | D50's discovery convention makes a running workbench findable by anything on the machine, and design §4.5 requires that an untrusted client cannot silently execute Python or write files. An explicit flag is the smallest honest consent in a phase that has neither a Preferences dialog (Phase 7) nor a consent UI (Phase 4/8), and keeping it out of the discovery directory when it is off means `--list` answers what the user chose to publish. The session object is not the security boundary, so it can always exist. |
| D60 | **The CLI JSON mode is a mode of the shipped `ovito` binary, not a new executable.** `ovito --automation <verb> [--json]` runs without a window and without Python, discovers sessions, connects to one and answers from the read-only catalog (`list`, `describe`, `status`, `snapshot`, `events`); the two spike executables stay measurement tools. Phase 4's writable verbs land on the same option. | The plan asks for a "CLI JSON mode" of the product and design §4.5 makes the CLI one adapter of one operation catalog; a separate executable would carry a second copy of the argument parsing, the discovery logic and the error vocabulary, and every packaging path would have to install it. The binary already has the non-GUI start-up path the mode needs (`--nogui`) and the automation layer is linked into it. |
| D61 | **An undone deletion restores the selection in the shared model, not in the controller.** `PipelineListModel` remembers the objects an edit removed (a handful of weak references) and selects one again when a later list update brings it back; the update that does so replaces the list's selection with it and uses the memory up, and a selection change for another reason drops it. | The model owns the rows *and* the selection, so it is the only place that can notice "the object I removed is back" - a controller would have to watch the pipeline itself and race the model's own refresh, and the classic frontend gets the same repair for free. Replacing instead of adding is the part the new check caught: the same update also re-selects the rows that were selected before it, so the returned object would have joined a two-item selection and `selectedItem()` would have answered nothing at all. |
| D62 | **Only the rows the contract can name carry an ID; the rest are addressed by row index within one synchronous call.** A modifier row carries `modifier:mN`; a data-source or visual-element row carries none, because the kind vocabulary has no kind for a pipeline source or a `DataVis` (finding 10). | D55 makes the contract ID *the* identity of the presentation, so an ID handed to a view has to resolve back to that row. A pipeline ID on the data-source row would name the pipeline the list is showing, but then no row would resolve from it and two rows (source and pipeline) would claim one identity - a name that does not round-trip is worse than no name. Row-index writes stay within D55's rule because they never outlive the synchronous call that made them, and the phase that first needs to address a source from outside the frontend extends the vocabulary additively. |
| D63 | **The animation model presents the tracks and keys of the selected objects, but it neither creates keys nor owns playback, and it exposes every track.** The tracks are found by walking the selected objects' animated parameters the way the classic track bar does (duplicated for this phase, O24) and a track contributes key rows as soon as it has one key - the classic's "two keys minimum" is a drawing rule of the widget, not a data rule. Keys are created only by the property editors and by auto-key mode, so the model offers no add-key operation, and playback stays core's `SceneAnimationPlayback` plus the shared command (D56). A deletion is one undo step per call, and its undo returns the keys without their selection, which is the classic behaviour (finding 12). | [UI_DESIGN.md](UI_DESIGN.md) §5.2 asks for editable controller tracks and keyframe selection through the existing controller operations; those operations *are* `moveKeys`/`deleteKeys`/`createKey`, and of the three only the first two belong to a timeline - "parameter-animation entry points and auto-key behavior" are the inspectors' and the toolbar's job, which is why the parity matrix keeps the key-editor button in Phase 5 d3. Exposing one-key tracks follows the same reasoning as the selection rule: a hidden track would make a key that exists invisible and unselectable, and a controller with a single key is exactly the state a first key leaves behind. Promising the restored selection instead would require the model to remember deleted keys and re-select rows that no longer mean the same thing after an undo of an unrelated edit. |
| D64 | **The session file question is a frontend hook with a *synchronous* answer, and the QML frontend presents it with a nested event loop around a `FileDialog`.** `requestSessionFilePath()` gains the request (save or open), so one hook answers both dialogs with the filters, the suggested name and the directory history they need; the desktop implements it with its `QFileDialog` as before, and the QML frontend asks the workbench controller, where the dialog's state lives (`fileDialogVisible`, the mode, the folder, the suggested name, `answerFileDialog()`, `cancelFileDialog()`), and waits for the answer in a `QEventLoop` - exactly what the message box of both frontends already does (`QMessageBox::exec()` versus `presentMessageBox()`). | The shared flow around the question is synchronous: `saveSession()` returns a bool, `askForSaveChanges()` throws `OperationCanceled` and reports `Exception`s, and every caller is a command handler that is written that way. Making the flow callback-based to accommodate an asynchronous dialog would spread async plumbing through the save, open, close, recent-files and quit paths and would lose the exception-based error reporting; a nested loop is the tree's existing answer to "a modal question inside a synchronous flow" and it keeps the *shared* layer free of any QML type. |
| D65 | **The session lifecycle is three shared operations, and no frontend loads a session by hand.** `WorkbenchUI` gains `openSession()` (ask for save changes, ask the frontend for a file, load it), `saveSessionAs()` (ask for a file even if the session already has one, then save) and `canCloseWorkbench()` (ask for save changes, report a failed save, answer "no" when the user cancels), and the five places that repeated "`askForSaveChanges()` + `DataSet::createFromFile()` + `checkLoadedDataset()` + `setCurrentSet()`" call them instead. The classic frontend therefore keeps all of its behaviour and gains the two things the hand-written copies forgot: a loaded session enters the recent-files list and its undo stack is clean. | Deliverable 6 asks for the shared operations, and these five copies differ from `loadSessionFile()` in exactly the ways that are wrong: a session opened from the command line or from a recent entry, or dropped onto the window, was neither remembered nor marked as unmodified, so the next close prompt could ask about changes the user had just discarded. Extracting the operations is also what makes the QML menu entries possible at all - it has none of this code, and writing it a second time is what the migration is removing. |
| D66 | **A directory handed to the import path becomes the working directory instead of a failed import.** `WorkbenchUI::importFiles()` treats a single local directory as "open this working directory": it changes the process working directory, hands it to `importDirectoryChanged()` and opens the frontend's import dialog, which is what the command line does for a directory argument. | The command line already has this rule (`GuiApplication` routes a directory to `openWorkingDirectory()`), so a directory dropped onto the window - the one path the QML frontend has and the desktop does not - behaved differently from a directory named on the command line: it fell into the importer lookup and reported that no importer can read a directory. The rule belongs to the shared import path because both frontends can receive a directory (drag & drop, a future "open folder" entry), and it keeps the "which frontend" question out of it. |

17. **The machine-facing layer of Phase 2.6 had no caller, and its transport was sealed inside a spike.**
    `AutomationLocalEndpoint` and `AutomationLocalClient` lived in `automation/spike/ipc/` and were compiled into the
    `ovito-automation-ipc-spike` executable alone, no frontend created a session-serving endpoint, and `ovito` had no
    mode that spoke to a running workbench. The layer was therefore fully exercised *by its own tests* and never by the
    application - O18 - and "a local client can read a running session" was a property of a prototype rather than of the
    product.
18. **The catalog could not describe what a client sees first, and nothing had ever minted a property ID.** The eight
    builtin operations were the Phase 2.6 set: a client could read the session, the scene nodes, one pipeline, the tasks,
    the transactions and the events, but not the viewports (whose IDs cannot be derived from anything else), not the
    selection, and nothing by ID in general. `AutomationObjectId::Kind::Property` existed in the grammar
    (`property:v1/fieldOfView`) but no code path ever produced one, so the whole property-ID concept - including the
    "what can I change here" question a client asks before writing - was unverified, and `SelectionRead` gated no
    operation at all.
19. **The `ovito` binary cannot report an exit code for a console invocation.** In console mode no Qt application object
    is created (`startupApplication()` creates it only for a GUI frontend), and `Main.cpp` maps that case to `0` or `1`
    by asking whether the task manager is shutting down. A client mode that has to distinguish "answered", "the session
    refused" and "no session was found" therefore has to bring its own `QCoreApplication` and end the event loop with its
    code; blocking on a socket without an application object is not an option either.

| D67 | **The local transport of the automation layer is Core code, and Phase 2.6's prototype executable is a self-test of it.** `AutomationLocalEndpoint` and `AutomationLocalClient` moved from `automation/spike/ipc/` to `automation/transport/`, are `OVITO_CORE_EXPORT`ed, are built only where Qt has local sockets (the wasm build has none), and are documented as main-thread-only with the session they serve; `ovito-automation-ipc-spike` keeps its 24-check self-test but now drives the shared classes, and the descriptor stays where it was (`automation/AutomationSessionDescriptor`, Core). | The wire rules of [AUTOMATION_CONTRACTS.md](AUTOMATION_CONTRACTS.md) §17 were already normative, and they were the only normative rules of the phase whose implementation lived in a program that is never shipped: a client could not use them without copying the prototype, which is exactly what the command line would have had to do. Promoting the code costs one CMake move and one export macro and it turns the spike into the regression test of the shipped thing, which is why the spike binary is kept rather than deleted. |
| D68 | **The read catalog grows by three operations, and `object.describe` is the generic read that mints property IDs.** `viewport.list` (needs `session.read`) lists the viewports a client can address; `selection.describe` (needs `selection.read` alone) answers the selected scene nodes, which gives that capability its purpose; `object.describe` (needs `scene.read`, argument `objectId`) answers any addressable object - scene node, pipeline, modification node, viewport, **property** - and reports a modification node's and a viewport's parameters with their property IDs. The contract minor version moves 0.2 → 0.3, which is additive. | A client that cannot address a viewport, a material parameter or the selection cannot do anything useful with the session, and one operation per kind would have duplicated the shape three times. Reading *by* the property ID that the description *hands out* is what makes the ID concept verifiable end to end: until now `Kind::Property` existed in the grammar but nothing ever produced a value in that form (finding 18). The payload of a parameter whose type has no JSON representation is reported as a null `value` with its `type` name rather than dropped, so a client sees the parameter exists. |
| D69 | **The first automation client is a mode of the shipped binary (`ovito --automation <verb> [--json]`), it is read-only by construction, and it brings its own `QCoreApplication`.** The verbs are `list`, `status`, `describe <object-id>`, `snapshot` and `events`; the capability request is fixed to the four read capabilities; `--json` prints one JSON object per invocation; the exit code is 0 (answered), 1 (the session refused, or the exchange broke) or 2 (the request could not be carried out: no session, no match for `--session`, unknown verb, missing argument). It runs in the application's console mode but creates a plain `QCoreApplication` itself, because that is what the socket needs and what makes the event loop return its exit code (finding 19). | D60 fixed that the CLI is a mode of `ovito` rather than a second program, precisely so that the argument parsing, the discovery convention and the error vocabulary are not duplicated; what it could not fix is *how* a console invocation reports failure, because the binary's console mode has no application object and no exit code of its own (finding 19). Fixing the capability request instead of offering a `--allow-write` flag is what makes this client safe to put in a script or an AI plan: it cannot execute Python or write a file even against a session that would let it, and the `status` answer names the grant so a user can see that. |
| D70 | **A workbench serves its session only when it was asked to, and it keeps the session when it stops serving.** `WorkbenchUI::startAutomationServer()` (D59's implementation) creates the endpoint on the workbench's own session, parented to the object that owns the session, grants the read-only policy of D43, publishes the descriptor and announces the session on the console; `stopAutomationServer()` closes the socket and removes the descriptor. The `--automation-serve` start-up option is read in `initializeWorkbench()` from the application's command line, so both frontends behave the same; its *definition* is `WorkbenchUI::automationServeOption()`, which a frontend application registers (a name a frontend did not register is an unknown option, and Qt 6.10 reports one). | Serving publishes a socket and a file that every process of the user can see, which is a decision for the user (D59) and not a side effect of starting a GUI; reading the option in the shared workbench keeps the QML frontend and the classic frontend from diverging on it. Keeping the session after the socket closes is what lets the presentation keep identifying objects by contract ID while the workbench is no longer reachable from outside, and it is the teardown order that O18 asked for: the endpoint is destroyed with the object that owns the session, before the session itself. |

20. **The smoke list is a concurrency test as well, and it found a pre-existing race in the frame graph builder.**
    One full run of the CI check list died with `SIGSEGV` after the offscreen and parity checks, at the moment the import
    check replaced the data set, and reported no failed check at all - the process was gone. A core dump put the fault in
    `Pipeline::getReplacementVisElement(this=0x0)` called from `FrameGraphBuilder::gatherVisElements`, i.e. a scene node
    whose `pipeline()` had become null was being rendered. The mechanism is in `FrameGraph::buildFromScene`: it collects
    the scene's pipeline nodes as `OORef<SceneNode>`s, then `co_await`s each pipeline evaluation - suspension points at
    which the event loop replaces the data set, and `SceneNode::requestObjectDeletion()` (SceneNode.cpp:129) clears the
    node's *pipeline reference* before the node itself goes away. The strong references in the list keep the node alive,
    so the crash is a null dereference rather than a use-after-free, and it is rare because it needs the replacement to
    fall inside one of those awaits (one crash in about seven full runs of the list). `SceneNode` itself treats a missing
    pipeline as a legitimate transient state (`SceneNode.h` guards `node->pipeline()` in `visitPipelines` and
    `containsPipeline`), so the builder was the outlier: it now skips a node whose pipeline has disappeared, keeping the
    results and the nodes in one pair of lists so the visual element pass stays aligned.
    The same assumption exists in `ParticlePickingHelper`, `ParticleInspectionApplet` and `BondVis`, which read the
    pipeline of a scene node that a pick record or a vis element handed them, but each of them runs in a synchronous
    caller that cannot be suspended between the check and the use, so none of them is a place this race can land - which
    is why only the frame graph path is changed.

21. **The post-phase review found one operational hazard, several small defects, and one of its own claims was wrong.**
    An open-code-review pass over `037df0c50..HEAD` in the tool's delegate mode (the tool selects the files and resolves
    the rules, the reviewing intelligence is the agent's) went through the 42 changed files. The hazard worth a code
    change is the three nested event loops of `QmlWorkbenchController`: the file selection dialog, the message dialog and
    the pipeline question each block their caller in a `QEventLoop`, and the members that remember those loops
    (`_fileDialogLoop`, `_messageBoxLoop`, `_pipelineChoiceLoop`) were written but never read - so a second question asked
    while the first was open would reset the state the first one is blocked on, and because both loops wait on the same
    `*Changed` signal, one answer would release both of them. A request that cannot be answered now says so instead
    (each of the three refuses while its loop is open), which is also what makes the "or null when no dialog is open"
    comment on those members true. The same pass showed that `QmlPipelineController::idOfObject` named four kinds while
    the only code that stores an object in a row (`PipelineListModel::refreshList`) stores a visual element, a modifier
    group, a modification node or a non-modification source node - so its pipeline, scene node and viewport branches were
    unreachable: they are gone, its four repeated row-bound checks became one `itemAt(row)`, and the class comment now
    says which rows have an ID. Three smaller ones were fixed with them: `AutomationCommandLine::humanValue` carried an
    unreachable `QJsonValue` branch (a reply is a `QVariantMap` parsed from JSON), the `events` verb was whichever branch
    fell through the dispatch chain rather than a matched one (a verb added to `verbs()` without a branch would have been
    answered with the event list), and a `status` run whose `selection.describe` was refused replaced that answer with a
    synthetic empty selection while a refused `viewport.list` was left empty - two treatments of one situation that a
    script reading only the payload cannot tell apart, so the selection now carries `unavailable` and the session's own
    error. The review's claim that the spike checks skip their phases silently is **withdrawn**: the shape it counted (13
    sites of `if(!controller) { state->continuation(); return; }`) goes through `pipelineController()`,
    `animationModel()` and `insertModifier()`, and every one of those calls `reportVerificationFailure()` before
    returning null. What is left of that thought is a hardening idea, not a defect - the harness counts failures, not
    completion, so a phase that stopped being reached would not be noticed (the false pass of the first
    `--qml-pipeline-check` had exactly that shape); a per-check phase count is the change that would notice it.

### Verification

D53-D60 are the outcome of reading the shared layer (findings 1-8) before the phase started; findings 9 and 10 and
D61-D62 came out of the first slice's implementation; findings 13-16 came out of S3, findings 17-19 out of S4, and
finding 20 out of re-running S4's full check list. Each slice is verified by the checks it adds, and every check is one
row of `spikeSteps()` in
`src/ovito/gui/qml/spike/Main.cpp` - the single source for the option, its help text, the run order and the decision
whether a run verifies anything at all (the trap recorded in [UI_TEST_ENV.md](UI_TEST_ENV.md) §9.2.8).

| Slice | Deliverables | What will verify it |
|---|---|---|
| S1 | d1, d3, d4, d5 | `--qml-pipeline-check` (new, and the last step of the table because it imports a data set of its own and replaces it): the QML-visible roles and the stable IDs a view reads, insertion through the shared modifier library, reordering, deletion and enablement through the shared operations with their undo/redo - the restored selection of D61 and the refusal of a write through an ID that no longer resolves included - and the command list model of D57 (filtering, order, visibility, `triggerAt()` on a disabled command). The exit gate's gesture clause ("one undo step per continuous drag") is verified in S2, whose animation controller is the first adapter with a gesture surface; the command list model is verified in this check instead of in a C++ test, because `GuiBase` is a plugin whose models need a `UserInterface` to exist at all. |
| S2 | d2 | `--qml-animation-check` (new): the interval, the current frame and the frame<->time conversion of the animation settings, the undo and redo of an interval change, the tracks and key rows of the selected scene node, the key selection (replace, add, by frame, all, clear, jump to a selected key), the two key moves - the discrete one as one undo step, the blocked shift that records nothing, and the continuous drag of D58 (an update replacing the previous one, the clamping at the interval boundary, cancel restoring the start with no undo step, accept as exactly one undo step whose undo returns the frames) - the deletion of keys with its undo and the selection rule of finding 12, the visible range as presentation state, the playback state of core's `SceneAnimationPlayback` through the shared command, and the undoable playback settings. This is also the slice that verifies the exit gate's gesture clause ("one undo step per continuous drag", with cancel restoring the original value). |
| S3 | d6 | The extended `--qml-session-check`: save, save as through the file dialog of the frontend (an answered dialog writes the session where it points and the session adopts that file, a cancelled one changes nothing, and the shell has to have *presented* the dialog it is waiting on), reopen the saved session, `openSession()` through the same dialog, the modified-close question in all three answers, a save into a non-writable location that reports the failure and keeps the file and dirty state, and the working-directory rule of D66. |
| S4 | d7 | The command line of D69 as a CTest suite (`tst_automation_cli`: its read-only capability request, an empty and a stale discovery scope, the exit code of every failing invocation, one JSON object per invocation), the three new read operations in `tst_automation_contracts` (the viewport list and its state, the selection with `selection.read` alone, and `object.describe` for a scene node, a pipeline, a viewport with its parameter IDs, one of those IDs round-tripping to its value, and the unknown/invalidated/malformed refusals), and `--qml-automation-check`: a workbench that was not asked to serve is invisible, serving publishes exactly one session in a private session directory, the `ovito` executable of this build discovers it, reads it (revision, four viewports, the selection, the events) within exactly the read capabilities, describes a viewport with its parameters, and stopping the server leaves the workbench with its session and no descriptor behind. |

**Status: S1 (deliverables 1, 3, 4, 5), S2 (deliverable 2), S3 (deliverable 6) and S4 (deliverable 7) are delivered and
verified, which completes Phase 3.**

* `--qml-pipeline-check` reports 0 failed checks in the native release build and in the assert-enabled `build-asserts`
  build, and the smoke list of `.github/workflows/ci.yml` (`QML_SMOKE_CHECKS`, which now ends with the new step) reports
  0 failed checks as a whole in the release build.
* `ctest --preset native` is 12/12 and the native product builds (`bin/ovito` included), so the shared files the slices
  touched - `PipelineListModel`, `WorkbenchUI`, the new `CommandListModel` and the session workflow - still serve the
  classic frontend.
* Beyond the list above, the check pinned: the role vocabulary the model exposes to QML (`iscollapsed` included), the
  row↔ID round trip for modifier rows, the revision advance and ID invalidation of a data-set replacement, and that the
  command list lists the 97 commands of the workbench, filters them and refuses to trigger a disabled one.
* What S1 did not verify: the panels themselves (D53), an identity for a data-source or visual-element row (D62), and a
  real view's delegate recycling and item lifetime (O23); the gesture clauses moved to S2, which verified them.
* `--qml-animation-check` reports 0 failed checks in the native release build and in the assert-enabled `build-asserts`
  build, and it is the step that now holds the exit gate's continuous-edit clause. Its evidence is the animation of an
  imported lattice: the interval 1..1 becomes 0..20 in one undo step that undo and redo move, three keys created on the
  selected node's transformation controllers appear as three named rows, a drag moves a key 10 -> 14 -> 17, clamps at 20,
  restores 10 when it is cancelled, and commits 10 -> 15 as one undo step, a blocked shift records nothing, deleting two
  of three keys is one undo step whose undo brings the keys back without their selection, and the playback state follows
  the shared command.
* What S2 did not verify: the timeline widget and its markers (Phase 5 d2, D53/O23), the creation of keys from the
  inspector (Phase 5 d3), an animated parameter other than a scene node's transformation (the walk is generic, the check
  used the controllers every scene node has), and the classic track bar's unification with this model (O24).
* S3's evidence: the File menu carries the shared Open/Save/Save As commands, and `--qml-session-check` reports 0 failed
  checks in both builds while it saves a session, answers the file dialog with a new file (Save As writes there and the
  session adopts it), cancels it (nothing written, no file adopted), fails a save into a path whose parent directory does
  not exist (no file, unchanged session file, still modified, the reason in the status line and in an error dialog), opens
  a session through the dialog, makes a directory handed to the import path the working directory, and answers the close
  question three ways through the window's own close event. The classic frontend keeps its behaviour while the five copies
  of the load sequence now call the shared operations, which is how a session opened from a recent entry, a drop or the
  command line gains its recent-files entry and its clean undo stack - and its "Save As" entry now asks for a destination
  instead of overwriting the current file (finding 14).
* What S3 did not verify: the platform's *native* file dialog (the check asks Qt for its own implementation, and the
  native dialog of macOS and Windows needs a manual run), the multi-pipeline question end to end (it needs a session file
  with two file sources, which this build cannot write), and the classic frontend's session commands against a real
  window - the tree has no automated test of the desktop frontend, so the five call sites that now use the shared
  operations are verified by the whole-product build and by the shared operations' own behaviour, not by a click test.
* S4's evidence: the transport is Core code (D67) and the Phase 2.6 spike still passes its 24 checks against it;
  `tst_automation_contracts` verifies the three new read operations (31 test functions, 33 QtTest cases) including a
  viewport parameter's property ID round-tripping to its value; `tst_automation_cli` verifies the command line itself
  against a private discovery scope; and `--qml-automation-check` runs the `ovito` executable of this build against a
  serving workbench of this process and reports 0 failed checks in the native release and in the assert-enabled build:
  nothing is discoverable before the workbench serves, serving publishes exactly one session whose endpoint lies in the
  private session directory, the child process reports the workbench's own revision, four viewports, the selection and 30
  events, is granted exactly `session.read`, `scene.read`, `selection.read` and `file.read`, describes `viewport:v1` as a
  viewport with 8 parameters whose IDs have the property form, and stopping the server leaves no descriptor behind while
  the workbench keeps its session.
* The full-list regression run of S4 (`--qml-window-size 1280x800 --qml-startup-delay 3000 --qml-lifecycle-cycles 2
  --qml-layout-check --qml-command-check --qml-settings-check --qml-session-check --qml-library-check --qml-icon-check
  --qml-offscreen-check --qml-prewarm-check --qml-pick 300,300 --qml-parity-check --qml-import-check
  --qml-automation-check --qml-animation-check --qml-pipeline-check`, which is the CI list) exposed and then verified the
  fix of finding 20: ten runs pass with 0 failed checks and no crash (four before the fix, which included the crash, and
  six after it), and the assertion-enabled build runs `--qml-offscreen-check --qml-prewarm-check --qml-pick
  --qml-import-check` on the changed render path with 0 failed checks and no failed `OVITO_ASSERT`.
* What S4 did not verify: the CLI against a session of a *different* process (the check uses this process' workbench,
  because a check cannot start a second GUI), `viewport.capture` (Phase 5 owns a real rendering capture, O19), a
  transaction or a command from a client (the CLI has no write verb by design, so the write path is verified by the
  contract suite's own commands), the `--automation-serve` option itself (the check calls `startAutomationServer()`
  directly, so the option's plumbing through `initializeWorkbench()` is covered by the shared code path but not by the
  check), and O20's remaining half - a transport for a remote client - which no phase has designed.
* **The post-phase review's fixes are verified.** The native product builds with no new warnings; `ctest --preset
  native` and `ctest --test-dir build-asserts` are 12/12 each; `--qml-pipeline-check`, `--qml-animation-check`,
  `--qml-session-check` and `--qml-automation-check` report 0 failed checks in both the release and the assert-enabled
  build, as does the whole `QML_SMOKE_CHECKS` list in the release build. `tst_automation_cli` passes with
  `OVITO_REQUIRE_TEST_BINARY=1`, the variable the CI jobs that build the application now set, so the suite fails instead
  of skipping when a binary that should exist does not - verified by moving `bin/ovito` aside, where the run then reports
  the failure and exits 1 instead of skipping with 0. The file-dialog guard is what `--qml-session-check` drives, and the
  animation model's row-to-track cache is what `--qml-animation-check` reads through its row roles.
