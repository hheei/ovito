# OVITO Modern Workbench UI — Phase 0 Audit Record

> **Companion documents**: [UI_DESIGN.md](UI_DESIGN.md), [UI_PLAN.md](UI_PLAN.md), [UI_PHASE1_SPIKE.md](UI_PHASE1_SPIKE.md)
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
| D3 | Defer the frontend-neutral application class and `--gui=qml` to Phase 2. Phase 1 adds a standalone prototype executable (`OvitoQmlSpike`) built from `src/ovito/gui/qml/spike`. | The entry point refactor does not de-risk rendering; keeping the prototype in its own executable keeps the spike honest about not depending on the desktop frontend. |
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
| D14 | Phase 1 uses a fixed 2x2 QML grid. Deriving the pane arrangement from `ViewportConfiguration::layoutRootCell()` (including the maximized cell) is deferred to Phase 2. | The layout tree is the correct source of truth; the prototype only needs four simultaneous viewports to prove multi-viewport rendering. |

---

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
6. **Offscreen picking support** (D10): `src/ovito/core/rendering/ObjectPickingBuffer.h` (new, header-only,
   forward-declared in `ForwardDecl.h`), `RenderThread::renderPickingFrame()` with
   `EventType::RenderPickingFrame`/`RenderPickingFrameEvent`/`handleRenderPickingFrame()`, the `RenderTarget` wrapper for
   it, the `forAmbientOcclusion`→`forPickingOnly` flag rename, `ObjectPickingMap::lookupPickResult()` (factored out of
   `RenderThread::lookupPickBuffer()`, which now delegates to it), and
   `ViewportWindow::generateFrameGraph()` moved from private to protected so that the QML adapter can produce a
   dedicated picking frame graph (a frame graph is consumed by exactly one renderer).

---

## 5. Open Items (Not Resolved by Phase 0/1)

| # | Item | Owner |
|---|------|-------|
| O1 | The `ovitoheadless` QPA plugin referenced by `StandaloneApplication::createQtApplicationImpl()`, `cmake/Prerequisites.cmake:290` and `cmake/OvitoTesting.cmake` does not exist. Either implement it (a platform integration providing `createPlatformVulkanInstance()` via `VK_EXT_headless_surface` and fontconfig-based font rendering) or remove the references. Headless Qt Quick rendering on Linux depends on it. | Phase 2 |
| O2 | `RenderThread` creates its `QVulkanInstance` without an API version, which triggers a Vulkan validation error (`VUID-VkApplicationInfo-apiVersion`, observed in the classic frontend run). **Not merely cosmetic**: the validation error was observed with RADV, while the lavapipe ICD rejects the instance outright with `VK_ERROR_INCOMPATIBLE_DRIVER` (`Failed to create Vulkan instance: -9`), i.e. the same bug makes OVITO fail on a driver that enforces the rule. | Phase 1 follow-up |
| O3 | ~~Object picking in the QML viewport is not implemented.~~ **Resolved in Phase 1** — picking is implemented as an asynchronous offscreen pass (D11) and verified end to end. Refinements left for Phase 5: pre-warming the buffer on viewport enter/camera change, coalescing hover picks, and revisiting the "use the previous buffer while refreshing" policy. | Phase 5 |
| O7 | The GPU picking-target creation (≈55 lines: two R32UI textures, the D32F depth texture, the texture render target, the batched readback) is still duplicated between `RenderThread::ensurePickingResources()` and the QML renderer path; the proposal is a shared core `PickingBufferTarget` class. | Phase 2 |
| O8 | Qt event handlers run in a task without a `UserInterface` (see the follow-up finding in section 2), so every QML entry point that starts asynchronous work must attach the context itself. Phase 2 should establish this once — e.g. a helper that wraps a QML-facing call in a `Task::Scope` bound to the `UserInterface` — instead of repeating the pattern per call site. | Phase 2 |
| O9 | ~~The prototype executable (`ovito-qml-spike`) cannot locate the OVITO plugin libraries on macOS without `DYLD_LIBRARY_PATH`.~~ **Resolved in Phase 1** — the spike's `INSTALL_RPATH` was built from `OVITO_RELATIVE_PLUGINS_DIRECTORY`, which is bundle-*root* relative, so the resulting `@executable_path/../Ovito.app/Contents/PlugIns` path was doubled. The target now uses the literal bundle-relative paths of `src/main/CMakeLists.txt`; `otool -l` reports `@executable_path/` and `@executable_path/../PlugIns/`, and the spike runs on the macOS test host with only the Qt library directory in `DYLD_LIBRARY_PATH` (Qt itself is still expected there in the build tree, like the main executable). | Resolved |
| O10 | A QtWidgets frontend test run must not be automated in a headless environment without suppressing the interactive import dialog: `MainWindowUI::importFiles()` shows a modal importer dialog (and asks about the import mode when the scene is non-empty), so an unattended run renders an *empty* scene. The recipes and the temporary benchmark instrumentation are recorded in [UI_TEST_ENV.md](UI_TEST_ENV.md) sections 3.1 and 7. | Testing |
| O4 | Each QML viewport item owns its own `RendererService` state (`RendererResourceCache`, `ObjectIdAllocator`, pipeline cache). Buffers are therefore not shared between the viewports of one window; consider one service per `QQuickWindow` shared by its items. | Phase 2 |
| O5 | The full action/editor inventory and the expanded parity matrix required by the remaining Phase 0 deliverables are still pending. | Phase 0 |
| O6 | The frontend-neutral application class and `--gui=qml` (D3) are unimplemented. | Phase 2 |
