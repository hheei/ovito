# OVITO UI Parity Matrix

**Status**: written as deliverable 1 of Phase 2.5 ([UI_PLAN.md](UI_PLAN.md)), together with the action/editor inventory in
[UI_PHASE0_AUDIT.md](UI_PHASE0_AUDIT.md) section 6. This file is the **normative** list of what the Qt Quick frontend has,
what it will get and in which phase, and what is deliberately not ported; the phase sections of UI_PLAN.md and the
narrative of [UI_FRONTEND_REVIEW.md](UI_FRONTEND_REVIEW.md) have to stay consistent with it, and a row without a state is a
defect of this file rather than an open question. The Python pipeline and live-session automation rows below are the
normative phase mapping introduced by Phase 2.6; an architecture adaptation row does not mean that the user-facing
capability is already implemented.

## 1. How to read this matrix

Every row names four things: the **capability**, its **classic reference** (source file or class, so the claim is
checkable), the **state in the Qt Quick frontend**, and the **acceptance case** — a check that exists today
(`OvitoQmlSpike` option or CTest), a check scheduled with the phase, or an explicit "manual" with the fixture to use (§5).

States:

| State | Meaning |
|---|---|
| ✅ | implemented in the Qt Quick frontend (the acceptance column names the check that already passes) |
| ◐ *Phase N* | the phase's architecture adaptation is in the tree and covered by the named check, but the user-facing capability of the row is still scheduled for that phase |
| ▶ *Phase N* | scheduled, either in the phase that owns the feature or, for the small gaps, in Phase 2.5 deliverable 3 |
| ✖ | deliberately not ported, with the reason in the same row (collected again in §4) |

Rows are grouped by area. Counts behind them come from the inventory in [UI_PHASE0_AUDIT.md](UI_PHASE0_AUDIT.md) section 6
(76 action ids, 84 property editors, 26 `*ParameterUI` classes, 28 dialog headers, 5 inspection applets).

## 2. What "verified" means here, and where it does not

Two claims of the phase plan are narrowed here rather than repeated:

* **Deployed layout**: verified on three layouts — the macOS `.app` bundle, the Windows one-directory build tree (`ovito.exe`, the `*.ovito.dll` files and the plugins next to each other, found through `QT_PLUGIN_PATH`/`QML_IMPORT_PATH`) and, since Phase 2.5 deliverable 1, a **Linux install prefix** (`cmake --install build-native --prefix /tmp/ovito-install`): the installed Qt Quick frontend renders the scene and the installed classic frontend starts, loads its 26 plugins and creates its viewport windows before taking its known Xvfb renderer path (recipe and measurements in [UI_TEST_ENV.md](UI_TEST_ENV.md) §2.3). What stays unverified is a *packaged* tree on macOS/Windows, and the spike has no install rule because it is a prototype.
* **Windows**: the Windows build needs Boost, a zlib-enabled HDF5 and Perl before it configures (defect F16 in
  [UI_PHASE1_SPIKE.md](UI_PHASE1_SPIKE.md)). That is a prerequisite of the platform row, not a frontend property.

Not verified by environment and deliberately not scheduled as a phase item: RADV (Vulkan on a real driver under Xvfb is
blocked by missing DRI3 presentation), mixed-DPI multi-monitor setups, and macOS screenshots from an SSH session. §6 lists
them together with what would be needed.

## 3. The matrix

### 3.1 Application bootstrap, windows and frontend selection

| Capability | Classic reference | Qt Quick | Phase | Acceptance case |
|---|---|---|---|---|
| Frontend selection at runtime | `GuiApplication::startupApplication` (the only GUI `startupApplication`) | ✅ `GuiFrontend` registry plus `--gui NAME`, default `qt-widgets` | 2 (done) | `ovito --gui=bogus` exits 1 and prints both available names (`qml`, `qt-widgets`), a `Did you mean …?` line for a near miss and — without `--gui` — the hint that the default user interface is missing from this build |
| Session and data files from the command line | `GuiApplication::initializeUserInterface` (positional arguments, `defaults.ovito`) | ✅ shared: the loading path only needs a `UserInterface` | 2 (done) | `ovito --gui=qml data.xyz` imports it; a `.ovito` argument loads the session |
| `defaults.ovito` auto-load | `GuiApplication::initializeUserInterface` via `QStandardPaths` | ✅ same shared path | 2 (done) | start with a `defaults.ovito` in the app data directory |
| Window title and dirty marker | `DataSetContainer::filePathChanged` plus `UndoStack::cleanChanged` | ✅ `QmlWorkbenchController::windowTitle` | 2 (done) | `--qml-session-check` asserts the title follows save/load |
| Window state persistence (size, position) | `QSettings` plus `QMainWindow::saveGeometry` | ✅ `GuiSettings` (`app/window/geometry`, `app/window/maximized`); the shell applies the remembered state before showing its window and writes it back with a 500 ms debounce | 2.5 (done) | `--qml-parity-check`: a resize reaches the settings store and a remembered geometry is applied again |
| Minimum window size | `MainWindow` minimum size | ✅ 640x400 minimum on the `QQuickView` | 2 (done) | resize below the minimum, the layout stays usable |
| Unsaved-changes prompt when closing | `MainWindow::closeEvent` → `askForSaveChanges` | ▶ Phase 3 d6 (the shared `WorkbenchUI::askForSaveChanges` exists, the close event still needs wiring) | 3 | close with a modified session: Yes saves, No discards, Cancel keeps the window |
| About and Quit reachable on macOS | help menu and the native Quit | ✅ About is a small QML dialog bound to the shared `HelpAbout` command, Quit to the shared `Quit` command (Preferences stays a disabled entry) | 2.5 (done) | `--qml-parity-check` walks the menu bar, opens the About dialog and checks that `Quit` acts; the disabled `Settings` entry names Phase 7 in its text (a placeholder is a menu item, which cannot carry a tooltip) |
| Second window (`File → New Window`) | `on_FileNewWindow_triggered` | ✖ not ported: this frontend opens one workbench window per process, and a second one needs a multi-window architecture (a candidate for Phase 8) | — | `--qml-parity-check`: the File menu has no `New Window` entry |
| The classic frontend stays the default | — | ✅ unaffected | 2 (done) | `ctest --preset native`, `ovito --nogui`, and a classic Xvfb run after every Phase 2.5 change |

### 3.2 Menus, commands and shortcuts

| Capability | Classic reference | Qt Quick | Phase | Acceptance case |
|---|---|---|---|---|
| One command table for both frontends | `ActionManager` (76 ids, 43 commands created there) | ✅ finding A1: `Command` is the state owner, each `QAction` a one-way view | 2 (done) | `--qml-command-check` compares `commandManager.commandList.length` with the manager's commands |
| Commands that act in the QML shell | 23 of the 43 shared commands | ✅ | 2 (done) | the command check exercises undo, redo, delete, maximize, the viewport modes and playback |
| Commands that still need a QML handler or placeholder | the 20 `WidgetActionManager` slots | ✅ the menu bar of the shell shows the shared commands and names the phase of every entry whose handler is not there yet; handlers arrive with their area (3 file/session, 4 pipeline, 5 animation, 7 render/settings) | 2.5 | menu walk: every entry is either enabled **and** acting, or disabled with its phase named in the menu text |
| Menu bar | `MainWindow::buildMenus` (File, Edit and Help; the viewport commands live in its toolbar) | ✅ File/Edit/View/Help drawn in the window and bound to the shared commands; unimplemented entries are disabled and name their phase. Deliberate deviations: no native macOS menu bar (it needs QtWidgets), no accelerators in the items (the shell owns each shortcut once) and a View menu the classic frontend does not have | 2.5 (done) | `--qml-parity-check` walks every entry: enabled entries present a command, disabled ones name a phase |
| Shortcut ownership (one owner per `QKeySequence`) | `QAction` shortcuts | ✅ the shell installs the sequence of every command once through a central `Shortcut` object, and the menu items show no accelerator (see the same row in section 3.13) | 2.5 (done) | no two commands share a sequence; `--qml-command-check` triggers the commands and the menu walk asserts the items carry no shortcut |
| Quick command search | `SearchActions` (`ACTION_COMMAND_QUICK_SEARCH`) | ▶ Phase 8 (command palette; the preview belongs to the palette phase) | 8 | — |
| Toolbar and action cards | `MainWindow` toolbar, `ActionCardsPopup` | ▶ Phase 8 (the shell uses plain buttons; card-based discovery is an enhancement) | 8 | — |
| Item context menus (pipeline, layers) | `ModifyCommandPage`, `OverlayCommandPage` item menus | ▶ Phase 4 with those panels | 4 | right-click a pipeline item: the group-4 commands appear with correct enablement |

### 3.3 Import

| Capability | Classic reference | Qt Quick | Phase | Acceptance case |
|---|---|---|---|---|
| File dialog import | `ImportFileDialog` | ✅ Qt Quick `FileDialog` (Ctrl+O and the header button) | 2 (done) | `--qml-import-check` imports a three-file trajectory |
| Drag & drop of files onto the window | `MainWindow::dropEvent` | ✅ window `DropArea` (same `WorkbenchUI::importFiles` path) | 2 (done) | manual: drop an `.xyz` onto the window (§5); the drop overlay appears while dragging |
| Multi-file and trajectory import | `ImportFileDialog` multi-select | ✅ | 2 (done) | the import check asserts one pipeline with three source frames |
| Import mode (replace vs. add) | modal import-mode question | ✅ deliberate policy instead of a question: `ResetScene` when the scene is empty, `AddToScene` otherwise (`WorkbenchUI::determineImportMode`) | 2 (done) | import into an empty and into a non-empty scene; the policy is documented as the deviation |
| Importer-specific options | `FileImporterEditor::inspectNewFile` inside `MainWindowUI::importFiles` | ▶ Phase 7 (finding A6: the *flow* is shared, the option UI is missing) | 7 | import a file whose defaults matter (LAMMPS data) and compare with the classic dialog |
| Detected format and frame count in the status line | importer name in the pipeline title, `FileSourceEditor` | ✅ the shell reports the format it used and the source frame count in the status line | 2.5 (done) | `lattice_*.xyz` with `atoms` in its comment (defect F6) → the notice names the format that was used and the frame count |
| Unsupported file | `MainWindow::reportError` | ✅ QML message dialog, scene unchanged | 2 (done) | `--qml-import-check` imports an unsupported file |
| Cancelling an import | `ProgressDialog` Cancel (cancels by dropping the task reference) | ✅ shell Cancel cancels the shell task; the objects a canceled import created are removed again | 2 (done) | the import check cancels a 512000-atom POSCAR import |
| Opening a directory as working directory | `openWorkingDirectory` | ▶ Phase 3 (shared method exists; needs a menu entry) | 3 | start with a directory argument and pick a file from it |
| Recent files and MRU directories | `RecentFilesList` (shared in `gui/base` since A5) plus `HistoryFileDialog` | ▶ Phase 3 d6 for the menu; the list itself is shared | 3 | `--qml-session-check` asserts the saved session becomes the newest entry |
| Remote (SSH) import | `ImportRemoteFileDialog`, `TerminalWidget`/`TerminalBackend` | ✖ not ported now: `OVITO_BUILD_SSH_CLIENT` is off, the terminal widget is not instantiated anywhere in this tree, and the shell has no menu entry for it | — | — |

### 3.4 Pipeline panel

| Capability | Classic reference | Qt Quick | Phase | Acceptance case |
|---|---|---|---|---|
| Pipeline list (rows, visibility toggles, status text) | `ModifyCommandPage` plus the shared `PipelineListModel` | ▶ Phase 4 d1 | 4 | model-level: roles, object ids, selection, status text for a reporting modifier |
| Modifier insertion through the chooser | `AvailableModifiersModel` plus the selector widget (both shared) | ▶ Phase 4 d3 | 4 | insert a representative modifier per plugin, undo and redo |
| Reordering by drag & drop with validation | `PipelineListModel::performDragAndDropOperation` (group and range handling, dry-run validation) | ▶ Phase 4 d1 | 4 | reorder across groups, invalid drop rejected, `performTransaction("Move modifier")` |
| Delete, rename, copy to, clone, make unique | commands of group 4 plus `WidgetActionManager` | ▶ Phase 4 d1 | 4 | each command, then undo: the object and the selection return |
| Modifier groups | `ModifierGroupEditor` | ▶ Phase 4 d1 | 4 | group three modifiers, ungroup, undo |
| Per-pipeline visual element grouping | `PipelineGroupVisElements` | ▶ Phase 4 d1 | 4 | two pipelines sharing a vis element, toggle it |
| Multiple-pipelines decision on load | `MainWindowUI::checkLoadedDataset` | ▶ Phase 4 (scene-node operations) | 4 | import two pipelines in a non-professional build: keep one, the other is deleted in one undo step |
| File source editor (frame list, reload, playback rate) | `FileSourceEditor`, `FileSourcePlaybackRateEditor` | ▶ Phase 4 d0 field model; the specialized editor with the frame table is Phase 7 (Phase 6 delivers the four audited modifier editors only) | 4/7 | change the source frame list, reload a changed file, edit the rate |
| Pipeline ↔ viewport selection sync | `SelectionSet` wiring in the panels | ▶ Phase 4 with the shared selection model (rest of A5) | 4 | select in one, the other follows; in both directions |
| Snippet import and export | `ExportObjectSnippetDialog`, `ImportObjectSnippetDialog`, commands of group 3 | ▶ Phase 4 d7 | 4 | round-trip one snippet and undo the import |
| Pipeline templates | `ModifierTemplates` (shared), `ModifierTemplatesPage` | ▶ Phase 4 for insertion, Phase 7 for the management page | 4/7 | insert from a template; manage templates in the settings dialog |

### 3.5 Property inspector

The scope of this area is defined by the audited editors, not by reflection over all fields — see
[UI_PHASE0_AUDIT.md](UI_PHASE0_AUDIT.md) section 6.2.

| Capability | Classic reference | Qt Quick | Phase | Acceptance case |
|---|---|---|---|---|
| Field model (finding A7) | `PropertiesEditor` plus `propertyFields()` | ▶ Phase 4 d0 (mandatory entry point of the phase) | 4 | both frontends show the same field set, units, bounds and read-only flags for one object, side by side |
| Generic field editor | (does not exist in the classic frontend; it uses explicit editors) | ▶ Phase 4 d3 | 4 | the fields audited in the inventory render, in the classic order |
| Numeric fields: units, bounds, reset | `NumericalParameterUI`, `SpinnerWidget`, the reset action for `PROPERTY_FIELD_RESETTABLE` | ▶ Phase 4 d3 | 4 | unit display, clamping at the bounds, reset action, precise entry |
| Numeric fields: animation controller | `FloatParameterUI`/`VectorParameterUI` on `…Controller` fields plus the key-editor button | ▶ Phase 5 d3 (the value is the one at the current animation time) | 5 | animate the slice distance; the value follows the animation and the key list matches |
| Boolean, color, string and enum fields | `BooleanParameterUI`, `ColorParameterUI`, `StringParameterUI`, `VariantComboBoxParameterUI` | ▶ Phase 4 d3 | 4 | one object per field kind |
| Vector and affine-transformation fields | `VectorParameterUI`, `AffineTransformationParameterUI` | ▶ Phase 4 d3 | 4 | edit the simulation cell and an affine transformation |
| Sub-object tables (structures, types, vis elements) | `RefTargetListParameterUI`, `StructureListParameterUI` | ▶ Phase 6 | 6 | the CNA/PTM structure table with per-row color, enable and name; add/remove a row |
| Delegate and property choosers | `ModifierDelegateParameterUI`, `PropertyReferenceParameterUI` | ▶ Phase 4 (chooser field) and 6 (its use in the specialized editors) | 4/6 | choose the delegate of a compute-property modifier and the property of the color coding modifier |
| Specialized editors of the 84 audited classes | [UI_PHASE0_AUDIT.md](UI_PHASE0_AUDIT.md) §6.3 lists the six archetypes | ▶ Phase 6 | 6 | per editor: field set and behaviour against the classic dialog, on the fixture of §5 |
| Editor dispatch registry (class → editor, fallback) | `PropertiesEditor::create` | ▶ Phase 4 d2 | 4 | a class with a specialized editor uses it, every other class falls back to the generic one |
| Modifier delegate editors (compute property, select, color by type …) | `ModifierDelegate*ParameterUI` family | ▶ Phase 6 | 6 | each delegate editor of the particles plugin |
| Modal property editing | `ModalPropertiesEditorDialog` | ✖ not ported: the QML shell edits in the inspector panel, which is what the modal dialog works around | — | — |

### 3.6 Viewport layers, utilities and templates

| Capability | Classic reference | Qt Quick | Phase | Acceptance case |
|---|---|---|---|---|
| Layer list (add, remove, reorder, rename) | `OverlayCommandPage`, shared `OverlayListModel` | ▶ Phase 4 (the five layer commands are created by the desktop page and have to move into a shared model) | 4 | insert, reorder, delete, rename, each with undo |
| Layer editors: coordinate tripod, text label, color legend | the three `PropertiesEditor` classes of the overlays | ▶ Phase 6 | 6 | one acceptance case per editor (fields, live preview in the viewport) |
| Layer rendering | core `ViewportOverlay` implementations | ✅ shared through the frame graph | 1 (done) | the tripod/label/legend appear in the captures |
| Layer templates | `OverlayTemplates` (shared) | ▶ Phase 4 | 4 | insert a layer from a template |
| Utilities list | `UtilityListModel` enumerating `metaclassMembers<UtilityObject>()` | ▶ Phase 4/7 (data inspector, tables, camera) | 4 | the list matches the registry; opening a utility shows its editor |
| Utility discovery ("Action cards") | `ActionCardsPopup` | ▶ Phase 8 | 8 | — |

### 3.7 Viewport: layout, interaction, context menu

| Capability | Classic reference | Qt Quick | Phase | Acceptance case |
|---|---|---|---|---|
| Panes derived from the layout tree, splitter drag with one undo step, maximize | `ViewportsPanel`, `getViewportRectangles` | ✅ Phase 2 d4 | 2 (done) | `--qml-layout-check` (pane rectangles, one `Resize viewports` step, undo/redo, maximize) |
| Maximize/restore | `ACTION_VIEWPORT_MAXIMIZE` | ✅ through the shared command | 2 (done) | the layout check; the pane also becomes active |
| Active viewport switching on click | `BaseViewportWindow::mousePressEvent` | ✅ shared | 1 (done) | click in a pane, the active viewport follows |
| Navigation modes (zoom, pan, orbit, FOV, pick orbit center) | `NavigationModes` (gui/base) | ✅ shared, driven by the four shortcut entries and the modes themselves | 1 (done) | mode state checks plus manual wheel/drag |
| Selection by click, asynchronous hover pick | `SelectionMode` (gui/base), blocking `RenderTarget::requestPick` | ✅ same mode, answered from a cached `ObjectPickingBuffer` | 1 (done) | `--qml-pick` grid, negative control, synthetic click through the selection mode |
| Hover information (coordinates, picked object) in the status bar | `StatusWidget`, `CoordinateDisplayWidget` | ▶ Phase 5 (the status line currently reports operations, not the pointer) | 5 | hover over a particle: node and location appear, as in the classic status bar |
| Rubber-band/manual selection | `ManualSelectionModifierEditor` gizmo | ▶ Phase 6 | 6 | draw a selection region in a pane |
| Move/rotate modes (XForm) | `XFormModes` (desktop) | ▶ Phase 5 | 5 | move and rotate an object, one undo step each |
| Dragging overlay labels | `MoveOverlayInputMode` (desktop) | ▶ Phase 5/6 | 5 | drag a text label and undo it |
| Viewport context menu | `ViewportMenu` (12 entries) | ✅ `QmlViewportMenu` plus `ViewportContextMenu.qml` deliver View Type, Show Grid, Constrain Rotation and Maximize/Restore; Adjust View, Preview Mode, Window Layout, Pipeline Visibility, Create Camera and Configure Viewport Graphics are disabled placeholders naming their phase. Deviation: Show Grid is offered in release builds too, where the classic entry is inside an `#ifdef OVITO_DEBUG` block | 2.5 (done) | `--qml-parity-check` right-clicks the title label of a pane, drives all four delivered actions and restores them |
| Split and remove viewport ("Window Layout") | `ViewportsPanel::showSplitterContextMenu` | ▶ Phase 4 d5 | 4 | split horizontally/vertically, remove, each with undo and with correct window/item teardown |
| Create camera | `ViewportMenu::onCreateCamera` | ▶ Phase 4 d6 | 4 | create, verify the view is unchanged, undo removes the node |
| Per-viewport graphics settings | `ConfigureViewportGraphicsDialog` over the shared registry (A2) | ▶ Phase 7 (the registry is shared, the dialog is not) | 7 | switch the renderer from the shell; an unavailable renderer falls back with one warning |
| Preview mode, Adjust View | `ViewportMenu` | ▶ Phase 5 d6/d7 | 5 | as specified in the plan (preview aspect/behaviour, camera form fields) |
| Pipeline visibility from the viewport menu | `ViewportMenu` | ▶ Phase 4 | 4 | toggle a pipeline's visibility from the pane, undoable |

### 3.8 Animation and keyframes

| Capability | Classic reference | Qt Quick | Phase | Acceptance case |
|---|---|---|---|---|
| Play/pause/stop, frame stepping | animation toolbar over `AnimationGoto*`/`Playback` commands | ✅ the commands are shared and act | 2 (done) | `--qml-command-check` starts and stops playback on a multi-frame scene |
| Timeline (current frame, total frames, scrubbing) | `AnimationTimeSlider`, `AnimationTrackBar` | ▶ Phase 5 d2 | 5 | drag the slider; the frame and the scene follow |
| Keyframe markers and their editing (select, move, delete) | `AnimationTrackBar` plus `AnimationKeyEditorDialog` | ▶ Phase 5 d1/d3 | 5 | add, move and delete keys with undo; the dialog's list matches the track |
| Parameter animation from the inspector | numeric editor plus the key-editor button | ▶ Phase 5 d3 | 5 | animate a modifier parameter; the key list and the animated value agree |
| Animation settings (interval, auto-adjust, FPS, loop, playback rate) | `AnimationSettingsDialog` | ▶ Phase 5 d2 | 5 | change the interval and the rate; auto-adjust on a trajectory |
| Auto key mode | `ACTION_ANIMATION_TOGGLE_RECORDING` with wiring in `WorkbenchUI` | ✅ wiring shared and the menu bar presents the toggle | 2.5 (done) | toggle it, change a property, a key appears |

### 3.9 Render settings and output

| Capability | Classic reference | Qt Quick | Phase | Acceptance case |
|---|---|---|---|---|
| Render settings (resolution, image size, background, output aspect) | `RenderSettingsEditor` | ▶ Phase 7 d1 | 7 | set the size and render; the image matches the classic output byte for byte |
| Renderer-specific parameters | `StandardRendererEditor`, `BaseSceneRendererEditor` | ▶ Phase 7 | 7 | change a renderer parameter and re-render |
| Render output (image, movie) | `ACTION_RENDER_ACTIVE_VIEWPORT`, `FrameBufferWindow`, `SaveImageFileDialog`, `FFmpegSettingsPage` | ▶ Phase 7 (video needs OVITO's ffmpeg support, which is **not available in this build** — the matrix records it as unverifiable here) | 7 | render a PNG and compare with the classic output; video export documented as unavailable in this build |
| Render preview inside the viewport | Phase 5 d7 (Preview Mode) | ▶ Phase 5 | 5 | toggle the preview; the pane shows the render aspect |
| Viewport image grab | `WidgetViewportWindow::grabViewportImage` | ▶ Phase 5 shared capture service/CLI (§3.15); Phase 7 desktop save workflow | 5/7 | capture a displayed pane with frame/dimension metadata; compare with the classic grab and verify authorized file output |
| Ambient occlusion modifier | `AmbientOcclusionModifierEditor` | ▶ Phase 6 (editor) — the offscreen sampling path itself is core | 6 | enable AO on a representative scene |

### 3.10 Export paths

| Capability | Classic reference | Qt Quick | Phase | Acceptance case |
|---|---|---|---|---|
| Export particle and mesh data | `FileExporterSettingsDialog` plus the exporter editors (POSCAR, LAMMPS, GSD, attribute file, …) | ▶ Phase 7 | 7 | export one file per format and diff with the classic output |
| Export data tables and plots | `DataTablePlotExporterEditor` | ▶ Phase 7 | 7 | export a histogram as data and as an image |
| Export a snippet | `ExportObjectSnippetDialog` | ▶ Phase 4 d7 | 4 | see 3.4 |
| "Generate script" / run script file | vestigial `ACTION_SCRIPTING_*` ids (never registered) | ✖ not ported: no implementation exists in this tree | — | — |

### 3.11 Session, settings and window state

| Capability | Classic reference | Qt Quick | Phase | Acceptance case |
|---|---|---|---|---|
| Save/open session, dirty state, unsaved-change question | `WorkbenchUI` session workflow (shared since D28) plus `MainWindowUI` dialogs | ✅ closing the window or quitting asks through `askForSaveChanges()` (`canCloseWorkbench()`, a cancelled close keeps the window open), the title marks a modified session, and a `.ovito` file handed to the import path opens as a session | 2.5 (done) | `--qml-session-check` (save, reopen, discard changes, missing file dialog, the question answered three ways, the title marker, `openRecentFile` and a `.ovito` import) |
| Session file with several pipelines | `MainWindowUI::checkLoadedDataset` asks which pipeline to keep | ✅ the same question in a QML chooser; a cancelled question aborts the load | 2.5 (done, end to end still manual) | the chooser is exercised by `--qml-parity-check` (asked and answered in both ways); the full path needs a session file with two file sources, see §6 |
| Application settings dialog (general, viewport, ffmpeg, templates) | `ApplicationSettingsDialog` with its pages | ▶ Phase 7 d5 | 7 | each page's values persist and are read back through the shared facade |
| Settings storage: one owner for key names and defaults | scattered `QSettings` uses | ✅ `gui/base/app/GuiSettings` owns the key names, defaults and validation of the workbench shell (D30) | 2.5 (done) | a grep for the literal keys finds them in the facade only |
| Recently used directories per dialog | `HistoryFileDialog` over `QSettings` | ✅ the import dialog reopens in the directory of the last import, which the frontend remembers through the facade | 2.5 (done) | `--qml-parity-check` asserts the directory the dialog would open in |
| Recently opened files (`File → Open Recent`) | `RecentFilesList` plus `MainWindow::updateRecentFilesMenu` | ✅ the shared list in a submenu of the File menu, with session entries reopening the session and data entries re-importing through the remembered importer and format; a file that cannot be opened is dropped from the list | 2.5 (done) | `--qml-parity-check` compares the submenu with the shared list; `--qml-session-check` opens a session entry |
| Saving into a non-writable location fails visibly | `MainWindowUI::fileSave` error path | ▶ Phase 3 d6 | 3 | save to a read-only directory: the error dialog appears, the dirty state stays |

### 3.12 Status bar, task progress and errors

| Capability | Classic reference | Qt Quick | Phase | Acceptance case |
|---|---|---|---|---|
| Status line for operations and notices | `TaskDisplayWidget`, status bar messages | ✅ `QmlWorkbenchController::statusMessage` | 2 (done) | the import check reports the unsupported file through it |
| Progress of running operations, with Cancel | `ProgressDialog` (200 ms delay, cancel by dropping the task reference) | ✅ shell task cancel; a cancelled import removes its objects | 2 (done) | the import check's cancelled 512000-atom import |
| One progress row per running task | classic shows a single aggregate bar | ✅ the shared `TaskProgressModel` reports one row per task and the status line renders all of them (see the same row in section 3.12) | 2.5 (done) | two concurrent tasks appear as two rows while both run; `--qml-parity-check` fabricates two records and asserts two rows |
| Error reporting (deferred, modal, with details) | `MainWindow::reportError` plus `MessageDialog` | ✅ shared `WorkbenchUI::reportError`; `GuiApplication` routes errors to the active `UserInterface` | 2 (done) | unsupported-file import shows the first line in the status bar and the rest in the dialog |
| Warnings (renderer fallback, missing QRhi) | status bar and dialogs | ✅ renderer fallback warns once; a missing graphics device is reported with the platform plugin and the xvfb recipe, and the spike asserts that report (D32, A8.4) | 2 (done) / 2.5 | `OVITO_VIEWPORT_RENDERER=anari`; `QT_QPA_PLATFORM=offscreen` takes the error path |
| Progress of viewport-triggered pipeline work | neither frontend shows it (the task has no `UserInterface`) | ✅ same behaviour as the classic frontend | — | recorded, not a gap |

### 3.13 Data inspector

| Capability | Classic reference | Qt Quick | Phase | Acceptance case |
|---|---|---|---|---|
| Data inspector panel with its applets | `DataInspectorPanel` plus the 5 applets (property, types, simulation cell, global attributes, dislocations) | ▶ Phase 7 | 7 | select a particle type: the property table matches the classic content |
| Opening the inspector from an editor | `OpenDataInspectorButton` | ▶ Phase 7 | 7 | click the button in an editor and land in the right applet |

### 3.14 Appearance: theme, icons, fonts, cursors

| Capability | Classic reference | Qt Quick | Phase | Acceptance case |
|---|---|---|---|---|
| Light and dark appearance following the OS | `OvitoStyle` plus `GuiApplication::usingDarkTheme()` | ✅ the shell's `Theme.qml` resolves the scheme through the shared `GuiSettings`, so both frontends follow one rule | 2.5 (done) | with the platform reporting a dark scheme the shell is dark and with a light one it is light; a platform that reports none counts as light in both frontends |
| Same icon set as the classic frontend | `gui/base/resources/icons` (shared assets) | ✅ the shell takes every icon from the shared set through `Icons` (`QmlIcons`): the maximize/restore button of a pane, the entries of its menu bar and the Import button, in the icon theme that matches the color scheme | 2.5 (done) | `--qml-icon-check`: the shell resolves the icons of the set and of its commands in both themes, the QML singleton reports the theme of the shared layer, every icon image of the shell is loaded, and the button of a pane switches to the restore icon when the pane is maximized |
| Theme and font selection | `GeneralSettingsPage`, `FontSelectionDialog` | ▶ Phase 7 (settings dialog) | 7 | switch the theme and the font size; the shell follows |
| Viewport mode cursors | `gui/base/resources/cursor` (shared) | ✅ through the shared modes | 1 (done) | each mode shows its cursor |

### 3.15 Python pipeline and live-session automation

These capabilities are independent of the classic frontend's absent scripting-console implementation. Phase 2.6 establishes
the shared contracts and compatibility probes and has delivered its adaptation set — the gateway, the identity/revision
contract, the permission model and the task/event/transaction layer, the Python package contract with its runtime probe, the
execution-topology spike, the local protocol with its discovery descriptor, and the Python seam (schema preview and data
bridge) of the ◐ rows below - whose client-facing rules are normative in
[AUTOMATION_CONTRACTS.md](AUTOMATION_CONTRACTS.md) - audit D39-D52 in [UI_PHASE0_AUDIT.md](UI_PHASE0_AUDIT.md) §7; the phase
in the table owns the first user-facing implementation.

| Capability | Existing reference | Qt Quick / shared state | Phase | Acceptance case |
|---|---|---|---|---|
| Automation Gateway and machine-facing operation catalog | `gui/base::Command`, core pipeline/task/undo APIs | ◐ Phase 2.6 (D39, D42, D43): `core/automation/AutomationGateway` with the contract vocabulary and a read-only catalog (`session.describe`, `scene.list_nodes`, `pipeline.describe`), separate from the UI commands; the client/transport side is Phase 3 | 2.6 adaptation (done), Phase 3 foundation | CTest `tst_automation_contracts` rejects malformed parameters, unknown and unimplemented operations and unsupported capabilities; UI commands remain usable by both frontends |
| Stable object IDs and session revisions | core object model and `PipelineListModel` | ◐ Phase 2.6 (D40, D41): numeric IDs that are never reused (`pipeline:p42`, `modifier:m108`, `viewport:v2`, `property:m108/distance`), `unknown_object` vs. `invalidated_object`, and a `baseRevision` precondition that answers `stale_revision`; ID survival across undo/redo is still untested (audit §5, O14) | 2.6 adaptation | CTest `tst_automation_contracts`: object deletion and data-set replacement produce deterministic invalidated/unknown-object and stale-revision errors, and a replaced object at the same address gets a fresh ID |
| Task lifecycle, transaction boundaries and semantic activity provenance | `TaskScope`, `UndoStack`, `WorkbenchUI` | ◐ Phase 2.6 (D44-D47): client identity and origin, `task.list`/`describe`/`cancel` with a task record per command (progress, cooperative cancellation, capability-gated `task.control`), a failing command that leaves no partial change, a committed command that is exactly one undo step, and a bounded in-session event log; a blocking `await` and a subscription are Phase 3 transport work | 2.6 adaptation, Phase 3 foundation | CTest `tst_automation_contracts`: task events expose progress/completion/error/cancel with origin and revision, a cancelled operation is reported as `cancelled`, a failed command changes nothing, and a capability that was never granted cannot be used. No raw input is recorded |
| Lightweight `ovito` package and selected Python environment probe | no implemented `src/ovito/pyscript` runtime in this checkout | ◐ Phase 2.6 (D48, D51): `core/automation/python/` carries the handshake protocol `1.0`, the CPython 3.10-3.13 range, the platform/architecture matrix, the feature vocabulary and `PythonEnvironmentProbe`, which validates one environment in a fixed order and never installs or falls back, plus the runtime half of the package: `PythonWorkerProcess` starts the selected interpreter and speaks the framed worker protocol, and `PythonDataBridge` moves named numeric arrays over it with checked digests in both directions ([AUTOMATION_DATA_BRIDGE.md](AUTOMATION_DATA_BRIDGE.md)); the package itself (`pyproject.toml`, decorator, entry point) and the installed-package lookup are Phase 4 (O16, O21) | 2.6 adaptation (done), Phase 4 package | CTest `tst_python_environment_probe` (one deterministic status per environment), `tst_python_data_bridge` (a round trip that scales, echoes byte for byte, leaves the caller's block untouched and reports a dead interpreter with its exit code) |
| Python environment selection and saved default | shared `GuiSettings` facade; no existing Python selector | ▶ Phase 4 lightweight selector in the modifier workflow; Phase 7 Preferences integration | 4/7 | select/reset a compatible environment without the complete Preferences dialog; default and per-modifier identity persist, with no silent fallback |
| AST preview of decorated Python functions | no existing implementation | ◐ Phase 2.6 (D51): `PythonIntrospector` runs `ovito_schema.py` isolated and `PythonSchemaPreview` is the report — functions, decorators, parameter schema with annotations as source text, diagnostics; the AST mode is the only mode a file selection may use (a fixture proves that nothing executes), an explicit import mode maps the traceback of the user's frames, and a decorator or argument computed at run time is reported as unsupported instead of guessed ([AUTOMATION_DATA_BRIDGE.md](AUTOMATION_DATA_BRIDGE.md)); reading a schema through the package's own decorator is Phase 4 | 2.6 adaptation (done), Phase 4 | CTest `tst_python_schema_preview`: syntax errors carry line and column, a dynamic decorator is `unsupported` with a reason, a top-level import of the read file does not run in AST mode and does run in import mode, and a raising script maps filename/function/line |
| Python Function Modifier | existing C++ `Modifier::evaluateModifier` contract | ▶ Phase 4 | 4 | decorated function mutates writable upstream data, returns `None`, evaluates asynchronously, reports traceback and survives save/reopen |
| Python scalar parameters and persistence | existing property/undo/session infrastructure | ◐ Phase 2.6 (D51) schema contract: the preview reports each parameter's kind, annotation, default and requiredness, keeping annotations as text and a computed default as "no static value"; the parameter *model* (`PythonParameterSet`), its persistence, validation and undo are Phase 4 | 2.6 adaptation (done), Phase 4 | CTest `tst_python_schema_preview` (kinds, defaults, dynamic defaults) plus the Phase 4 acceptance case: typed scalar values, validation, invalidation, undo and schema reload preserve compatible values |
| Python source/generator and analysis functions | none | ▶ Phase 8; separate contracts from modifiers | 8 | source and analysis workflows have explicit node/result semantics; no return-value inference |
| Richer Python parameter types and optional environment setup helper | none | ▶ Phase 8, beyond the Phase 4 scalar MVP | 8 | richer schemas validate and round-trip; any optional setup shows the environment and changes and requires consent, without silent installation |
| Headless Python batch/CI execution | `--nogui` and existing core pipeline | ◐ Phase 2.6 (D49): the topology is decided and measured — a persistent worker on the selected interpreter with raw length-framed arrays ([AUTOMATION_TOPOLOGY_SPIKE.md](AUTOMATION_TOPOLOGY_SPIKE.md)) — and `ovito-automation-spike` demonstrates start, evaluate, cancel, crash-recover and frame-change behaviour; the production worker and the no-GUI pipeline path are Phase 7 (the first modifier is Phase 4) | 7 | selected environment evaluates a pipeline without QML/QWindow and produces deterministic results/errors |
| Read-only live-session query API | no existing protocol | ▶ Phase 3 | 3 | a local client discovers a session and reads dataset, pipeline, properties, viewport and task state as structured JSON |
| Local JSONL/IPC endpoint and CLI JSON mode | existing command-line startup only | ◐ Phase 2.6 (D50): the spike's endpoint is a per-user local socket speaking JSON Lines, discovered through a session descriptor in Core (`AutomationSessionDescriptor`, tested without a socket), with a capability handshake per connection, a bounded snapshot composed of contract answers and push/pull events; the production endpoint, the CLI and its JSON mode are Phase 3 (O20), and the PNG behind a capture is Phase 5 (O19) | 3 | explicitly discover/connect/query/subscribe locally, report deterministic protocol errors, and reject unauthorized mutations; public PNG capture is Phase 5 |
| Writable pipeline automation | core pipeline operations and transactions | ▶ Phase 4 | 4 | CLI adds a modifier, sets a validated parameter, evaluates, waits/cancels a task and verifies the resulting revision |
| Viewport, selection and frame automation | `Viewport`, `SelectionMode`, animation commands | ▶ Phase 5 | 5 | client changes frame/selection/view state and requests a displayed-view PNG without blocking the GUI |
| Session save, export and explicit scene render automation | Phase 7 file/render workflows | ▶ Phase 7 | 7 | authorized client saves/exports/renders, returns artifact metadata, and reports file-write failures without false success |
| AI read/plan/confirm/execute workflow | none in this checkout; distinct from upstream Pro features | ▶ Phase 2.6 permission/revision/activity contracts; ▶ Phase 8 UI | 8 | read-only by default, destructive/Python/file-write actions require explicit authorization and stale plans are rejected |
| MCP, external tools and remote automation adapters | none | ▶ Phase 8, outside the core gateway | 8 | adapter uses the same catalog, capability checks, task model and revision preconditions; remote access has a separate security gate |

### 3.16 Keyboard, accessibility, HiDPI

| Capability | Classic reference | Qt Quick | Phase | Acceptance case |
|---|---|---|---|---|
| Keyboard shortcuts for the presented commands | `QAction` shortcuts | ✅ the shell's central `Shortcut` objects are the only owner of each sequence and the menu items deliberately show none, so a command cannot fire twice | 2.5 (done) | the command check triggers the commands; the menu walk asserts the items carry no shortcut |
| Keyboard focus order | Qt widget focus chain | ✅ the Import Data button is the first focus stop and the chain continues into the viewport items (`QuickViewportItem` sets `activeFocusOnTab`), which is where the key events of the viewport input modes arrive; accessible *names* exist on the shell's controls but are not asserted | 2.5 (done) | `--qml-parity-check` walks the focus chain (`nextItemInFocusChain()`): the first stop is the import control, the chain has no dead end and it reaches a viewport |
| Screen-reader names and roles | `QWidget` accessible names | ▶ Phase 8 (recorded as an enhancement, not parity) | 8 | — |
| HiDPI scaling | `devicePixelRatio` handling | ✅ measured (picking uses device pixels, like the classic pick radius) | 1 (done) | `QT_SCALE_FACTOR=1.5` and `=2` runs |
| Mixed-DPI multi-monitor | Qt handles it | ✖ not verifiable here (no second display) | — | §6 |

### 3.17 Cross-cutting behaviour

| Capability | Classic reference | Qt Quick | Phase | Acceptance case |
|---|---|---|---|---|
| Undo/redo of every edit, with a readable step name | `UndoStack`, `UndoableTransaction`, `performTransaction` | ✅ for the shell's edits (layout, rename, import cleanup); other areas as they land | 1-2 (done) | `--qml-layout-check`, `--qml-command-check`, `--qml-session-check` assert the step names and that undo restores state |
| Selection shared between pipeline, viewport and inspector | `SelectionSet` | ▶ Phase 4 (rest of A5) | 4 | select in one place, all three agree |
| File drag & drop | `MainWindow::dropEvent` | ✅ | 2 (done) | see 3.3 |
| Pipeline item drag & drop | `PipelineListModel::performDragAndDropOperation` | ▶ Phase 4 | 4 | see 3.4 |
| Import diagnostics (detected format and frame count) | none - the classic frontend reports nothing but the pipeline title | ✅ a persistent `notice` on the workbench controller names the format the file was read as and, once the file source is evaluated, the number of source frames | 2.5 (done) | `--qml-parity-check` imports an XYZ file whose comment mentions atoms (defect F6): the notice names "LAMMPS Data" and 1 source frame while the scene stays empty |
| One progress row per running task | one aggregate bar (`TaskDisplayWidget` reads `TaskProgressModel::activeText()/activeValue()`) | ✅ one row per `TaskProgressModel` row in the status line; per-task cancellation is **not** supported | 2.5 (done) | `--qml-parity-check` holds two progress records and asserts two rows appear |
| Long-running operation can be cancelled, half-done work is removed | `ProgressDialog` plus `OperationCanceled` handling | ✅ shell-level; per-task cancellation is **not** supported because `TaskProgress` carries no task handle | 2 (done) | the import check's cancellation case |
| Thread-safety of presentation-layer callbacks | — | ✅ `GuiTaskScope` opens a bound task for QML entry points (fix O8) | 2 (done) | assert-enabled spike run |
| Deployed-tree resource loading | CMake install layout | ✅ verified on Linux (installed prefix: the QML frontend renders the scene, the classic frontend loads its plugins and creates its windows), macOS bundle and Windows build tree | 2.5 (done) | `cmake --install` into a prefix and run both frontends from there ([UI_TEST_ENV.md](UI_TEST_ENV.md) §2.3) |

## 4. Deliberately not ported

| Item | Reason |
|---|---|
| Python scripting console, "Run Script File", "Generate Script" | declared ids but **no implementation in this tree** to port. Independently implemented Python pipeline/batch support is scheduled in §3.15; a new console is a Phase 8 enhancement, not an existing runtime |
| Integrated SSH client and remote import | optional feature (`OVITO_BUILD_SSH_CLIENT`, off); its widgets are not instantiated anywhere in this checkout |
| `NewPipeline.PythonSource` and `.LammpsScriptSource` placeholders | ids without a registered command |
| Modal property editing (`ModalPropertiesEditorDialog`) | the QML shell edits in the inspector panel; the modal dialog exists to work around narrow widget layouts |
| Screen-reader semantics, high-contrast themes | an accessibility **enhancement** (Phase 8), not part of the classic behaviour to reproduce |
| Mixed-DPI multi-monitor verification | environment gap, not a design decision (§6) |
| Video encoding settings page | depends on OVITO's ffmpeg support, which this build does not have |

## 5. Fixtures and the acceptance harness

Every acceptance case above resolves to one of these fixtures and one of these harness steps.

**Fixtures** (generated by the spike or shipped with it, see [UI_TEST_ENV.md](UI_TEST_ENV.md) for the recipes):

| Fixture | Contents | Used by |
|---|---|---|
| empty data set | construction grid and orientation tripod only | layout, command and context-menu checks |
| `lattice_512.xyz` | 512 atoms, simple cubic, comment without the word `atoms` (defect F6 would otherwise hand it to the LAMMPS importer) | picking, import, layout, performance |
| `lattice_32768.xyz`, `lattice_262144.xyz` | same lattice, larger | performance and picking at scale |
| `lattice_traj.xyz` | a three-file trajectory | multi-frame import, playback probe |
| `poscar_512000.vasp` | a large, eagerly parsed structure | import cancellation |
| `session.ovito` | a saved session used by the session check | session workflow, recent files |
| session with two file sources | a `.ovito` file holding two pipelines, which only OVITO Pro can write | the multi-pipeline question (§3.11): manual, see §6 |
| mixed-DPI/`QT_SCALE_FACTOR` runs | the same scene under `1.5` and `2` | HiDPI |

**Harness steps** (`OvitoQmlSpike`, all four CI jobs run a subset):

| Step | Covers |
|---|---|
| `--qml-layout-check` | panes, handles, splitter drag with one undo step, undo/redo, maximize |
| `--qml-command-check` | command table size, the 23 acting commands, enablement rules, viewport modes, playback |
| `--qml-import-check` | multi-file import, unsupported file, cancellation, playback on a trajectory |
| `--qml-session-check` | save, reopen, discard, recent files, missing file dialog |
| `--qml-pick X,Y` | picking: grid probe, negative control, synthetic click through the selection mode |
| `--qml-frame-stats MS` | frame time (measured per [UI_TEST_ENV.md](UI_TEST_ENV.md) §9.4) |
| `--qml-lifecycle-cycles N`, `--qml-hide-show`, `--qml-resize WxH` | resource teardown, hide/show, interactive resize |
| Classic checks | `ctest --preset native`, `ovito --nogui`, a classic Xvfb run with an ffmpeg screenshot ([UI_TEST_ENV.md](UI_TEST_ENV.md) §3.2) |

A row whose acceptance says "manual" names what to do; delivered checks otherwise use a spike option or a CTest run.
The new §3.15 rows name scheduled acceptance cases, not checks that already exist, with one exception: the **◐** rows of §3.15
are covered today by the CTest case `tst_automation_contracts` (see [UI_PHASE0_AUDIT.md](UI_PHASE0_AUDIT.md) §7), and the
rest of each of those rows moves with the phase that owns the feature. Phase 2.6 introduces contract/probe
harnesses (the contract test suite exists; the probes are open); Phase 3 adds local read-only protocol fixtures; Phase 4 adds compatible/incompatible Python environments,
decorated files with syntax/import/runtime failures, schema reload and a downstream-native-modifier trajectory; Phase 5
adds viewport capture fixtures; Phase 7 adds headless batch/output cases; Phase 8 adds AI/MCP permission and stale-plan
cases. Link the actual harness and results before marking any of these rows ✅. Checks that the
assert-enabled build has to cover are marked in [UI_TEST_ENV.md](UI_TEST_ENV.md) §4.

## 6. Verification gaps this matrix does not close

| Gap | Why it is open | What would close it |
|---|---|---|
| Installed-tree resource loading on Linux | **closed in Phase 2.5**: the installed prefix starts both product frontends and the QML one renders the scene; the spike has no install rule | — |
| Windows packaging prerequisites | a Windows redistributable build needs Boost, a zlib-enabled HDF5 and Perl (F16) | already provided by the Windows CI job and the kitty host recipe |
| Windows/D3D12 picking and layout | **closed**: verified on hardware and through WARP (Phase 1 report, table 3.5) | — |
| Vulkan on a real driver under Xvfb | Xvfb has no DRI3, so presentation fails (lavapipe software ICD is the workaround) | a machine with a real driver and a display, or a Wayland compositor |
| macOS screenshots | no screen-recording permission for the SSH session | run the screenshot from a local macOS session |
| Mixed-DPI multi-monitor | no such setup available | two displays with different scale factors |
| Frame-time claims for a release build with assertions | assertions are compiled out under `NDEBUG`; the assert build is a separate tree | already covered: the assert build runs the same steps (Phase 1, F5/§4 of UI_TEST_ENV.md) |
| Video export | OVITO's ffmpeg support is not built in this tree | a build with ffmpeg available |
| The multi-pipeline question end to end | its fixture is a session file with two file sources, which this frontend cannot write (OVITO Basic imports one pipeline at a time) and which no build in this tree produces | load such a session (written by OVITO Pro) and answer the question in both ways; the chooser itself is already checked by `--qml-parity-check` |
