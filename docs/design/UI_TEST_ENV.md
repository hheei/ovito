# Test & Verification Environment Notes (Qt Quick Frontend)

> **Companion documents**: [UI_PLAN.md](UI_PLAN.md), [UI_DESIGN.md](UI_DESIGN.md),
> [UI_PHASE0_AUDIT.md](UI_PHASE0_AUDIT.md), [UI_PHASE1_SPIKE.md](UI_PHASE1_SPIKE.md)
>
> **Purpose**: record how the Qt Quick frontend is built, run and measured in this project's environments, and which
> traps cost time in practice. Read this before believing a "it does not work" observation: several of the failures
> recorded here are environment limitations, not defects of the code.

---

## 1. Environments at a glance

| Environment | Frontend runs | Notes |
|-------------|---------------|-------|
| Linux dev box (`chlo` workstation) | Qt Quick via `xcb` under `xvfb-run` | Headless: no display server, no Wayland compositor. GPU: AMD Ryzen 9 9950X (RADV) + Mesa llvmpipe/lavapipe software drivers. |
| macOS test host `buddy` (Mac mini, Apple M4) | Qt Quick and classic frontend with the **Cocoa/Metal** backend | Attached monitor (Acer EK241Y, `devicePixelRatio = 1`), console session belongs to user `kings` — so GUI tests must run as `kings`, not as `buddy` (see section 5.2). The checkout used for testing is `/Users/kings/ovito` with the build directory `build-buddy`; Qt lives in `/Users/buddy/Qt/6.10.2/macos` (readable for `kings`) and Homebrew's `boost`/`hdf5`/`netcdf` are installed. |
| macOS host `mac` (Apple Silicon) | same | Retina (`devicePixelRatio = 2`). Not used any more: `buddy` is the designated test machine. |
| Windows / D3D12 | — | No x86_64 Windows machine is available. A Parallels Windows 11 VM exists on `buddy` but **cannot** be used for this (section 5.4). |

### Dataset generation used in all measurements

```bash
python3 - <<'EOF'
def lattice(n, a=3.6, path=None):
    lines = [str(n**3), f"Lattice {n}x{n}x{n}"]
    lines += [f"Ar {i*a:.3f} {j*a:.3f} {k*a:.3f}" for i in range(n) for j in range(n) for k in range(n)]
    open(path, "w").write("\n".join(lines) + "\n")
lattice(8,   path="/tmp/lattice_512.xyz")       # 512 atoms
lattice(16,  path="/tmp/lattice_4096.xyz")      # 4096 atoms
lattice(32,  path="/tmp/lattice_32768.xyz")     # 32768 atoms
lattice(64,  path="/tmp/lattice_262144.xyz")    # 262144 atoms
EOF
```

**The comment line of an XYZ file decides which importer gets it.** OVITO's importer autodetection hands a file to the
LAMMPS **data** importer if the second line looks like a LAMMPS header — for example a comment containing the word
"atoms" — and the resulting pipeline is an *empty* scene. The file extension does not win over that heuristic:

```bash
# only the comment line differs; the atom lines are identical
cp /tmp/lattice.xyz /tmp/c.xyz
sed -i '2s/.*/512 atoms, simple cubic lattice a=3.6/' /tmp/c.xyz   # -> "c_*.xyz [LAMMPS Data]", no particles
sed -i '2s/.*/simple cubic lattice, a=3.6/'           /tmp/c.xyz   # -> "c_*.xyz [XYZ]", 512 particles
```

An XYZ file needs the atom-count line *and* the atom lines for the misdetection to be visible: a file that holds only the
comment line is not a data file at all and reports an import error instead (`ovito-qml-spike --qml-parity-check` writes
its fixture as `8` / `8 atoms` / `Ar 0 0 0` and asserts that the notice names the format it was read as).

The visible symptom is a picking check that finds nothing (there is nothing to pick), not an import error. The spike
prints the imported pipelines and their detected format as `DATASET "… [XYZ]"` lines for exactly this reason — check
them first when a picking check fails. The recipe above therefore uses `Lattice 8x8x8`, which imports as XYZ.

---

## 2. Linux: rendering a Qt Quick window without a display server

* `StandaloneApplication` sets `QT_QPA_PLATFORM=ovitoheadless` by default on Linux, but that QPA plugin **does not exist in
  this tree** (open item O1). Always set `QT_QPA_PLATFORM` explicitly.
* The `offscreen` QPA plugin is useless for this frontend: it provides neither a Vulkan instance nor a GL context, so Qt
  Quick cannot create a QRhi and `QQuickRhiItem` logs *"No QRhi found for window …, QQuickRhiItem will not be
  functional"*. The viewport stays empty even though the process runs.
* Working recipe (what all Phase 1 Linux measurements used):

```bash
LD_LIBRARY_PATH="$PWD/.qt/6.10.2/gcc_64/lib" QT_QPA_PLATFORM=xcb timeout 300 \
  xvfb-run -a --server-args="-screen 0 1280x800x24" \
  ./build-native/bin/ovito-qml-spike --qml-startup-delay 4000 --qml-hold-ms 10000 /tmp/lattice_512.xyz
```

A screenshot is taken from outside the process while it holds the window open (§2.2): the spike has no capture option of
its own any more, because `QQuickWindow::grabWindow()` is unreliable with `QQuickRhiItem` viewports.

### 1.1 Picking checks: the probe must not alias with the scene

Two ways a picking check reports "nothing was picked" while nothing is wrong with picking:

* **The probe position can lie outside the scene.** The camera fits a scene to the *aspect ratio* of the viewport, so
  the same pixel position holds different content on viewports of different shapes: the CI smoke test probed
  `(300,300)` and hit 19 of 25 positions on a 496x380 viewport, but nothing at all on the macOS runner's 496x307
  viewport, while the synthetic click at the item centre still selected the lattice. Probe a grid over the whole
  viewport, or the item centre, not one fixed position.
* **The probe grid can alias with a periodic scene.** A fitted 8x8 lattice in a 496 pixel wide viewport has a particle
  spacing of about 55 pixels. A 9x9 grid over that viewport has almost exactly the same spacing and landed in the gaps
  between the particles: 0 of 81 positions hit while a 4 pixel grid around a particle hit 19 of 25. `pick()` searches
  within four device pixels of the probe position, so the grid spacing must stay below that (the smoke test uses 5
  pixels, which turned the same viewport into 2468 of 7524 hits).

Both failures look identical in a log that only prints a hit count, so the smoke test prints the item size, the probe
centre and a full-viewport scan whenever a picking check fails.

### 2.1 Qt Quick graphics backends under Xvfb

| Backend | Works? | Recipe / reason |
|---------|--------|-----------------|
| OpenGL (default) | **Yes** | Qt Quick picks OpenGL; Mesa llvmpipe provides GL 4.5 through GLX. |
| Vulkan (RADV, hardware) | No | `vulkan: No DRI3 support detected - required for presentation`, then `Failed to create RHI (backend 1)`. Xvfb has no DRI3, so a Vulkan swapchain cannot be created. |
| Vulkan (lavapipe, software) | **Yes** | `VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json VK_DRIVER_FILES=… QSG_RHI_BACKEND=vulkan`. lavapipe presents through X11 shared memory and does not need DRI3. |
| `vkkhrdisplay` QPA | No | It enumerates `/dev/input` (permission denied) and reports `Display count: 0` — there is no monitor connected to the GPU. Qt's plugin then continues with a null mode and the process dies with SIGSEGV. |
| Wayland | No | No compositor is installed (`weston`, `sway`, `cage`, `labwc` are all absent). |

Consequence: on this box the Qt Quick **Vulkan** path is only verified with the software ICD (lavapipe). The hardware
Vulkan path of OVITO itself (the render thread, used for offscreen picking) does run on RADV, so Vulkan-driver-level
integration is exercised too.

---

### 2.2 Smoke-checking and screenshotting the real frontends

The commands above exercise the prototype executable. The real entry point is the `ovito` binary, whose frontend is
selected with `--gui` (classic `qt-widgets` is the default, `qml` is the Qt Quick workbench):

```bash
# A headless smoke check of the real application. It has no capture option, so the check is "starts and stays up":
# a crash or a failed startup exits differently, which is enough to catch startup regressions without adding a
# test-only command line option to the product.
Xvfb :99 -screen 0 1280x800x24 &
DISPLAY=:99 LD_LIBRARY_PATH="$PWD/.qt/6.10.2/gcc_64/lib" ./build-native/bin/ovito --gui=qml /tmp/lattice_512.xyz &
APP=$!
sleep 10 && kill -0 $APP && echo "still running"
```

To *see* what such a run rendered, grab the X11 display with ffmpeg instead of adding a screenshot API. This is also
what the Linux x86_64 CI job does: it starts `Xvfb :77 -screen 0 1400x900x24` itself (a fixed display is what makes the
capture possible, `xvfb-run -a` hides the number it picks), runs the spike in the background with `--qml-hold-ms 8000`,
waits for `VERIFICATION_DONE` in its log, captures **after** the checks (a capture taken while the checks run shows the
window of an earlier moment - or a black screen, when it is taken before the window is mapped), and fails the step on the
spike's exit code while the capture itself is allowed to fail.

The job photographs a **second, short run** (layout, command and pick checks plus `--qml-hold-ms`), not the gating run:
the check sequence ends with a cancelled import that deliberately leaves the scene empty, so a capture taken there shows
the shell's empty state - which is honest but useless as the "does the frontend render scene data" image this artifact
exists for. Two QML traps to know when a dialog shows up in such a picture:

* A `Dialog` that the *frontend* answers (a check answers the multi-pipeline question programmatically, for example) never
  goes through its own close path, so it stays open until something closes it. Follow the controller's state from a
  `Connections` handler and call `close()` there - binding `visible` to the controller instead does not work, because a
  popup writes `visible` itself and destroys the binding (the same reason the About dialog is opened from a signal
  handler).

```bash
DISPLAY=:99 ffmpeg -y -f x11grab -video_size 1280x800 -i :99 -frames:v 1 /tmp/workbench.png
```

The full recipe for a verified screenshot of the Qt Quick workbench (used for the evidence files in this directory):

```bash
Xvfb :77 -screen 0 1400x900x24 &                  # a private display, so the grab cannot catch another window
sleep 2
DISPLAY=:77 QT_QPA_PLATFORM=xcb ./build-native/bin/ovito-qml-spike --qml-window-size 1280x800 \
    --qml-startup-delay 1500 --qml-frame-stats 1500 --qml-layout-check --qml-pick 300,300 \
    --qml-hold-ms 8000 /tmp/lattice_512.xyz > /tmp/evidence.log 2>&1 &
until grep -q VERIFICATION_DONE /tmp/evidence.log; do sleep 1; done
sleep 1
ffmpeg -loglevel error -f x11grab -video_size 1400x900 -i :77 -frames:v 1 -y evidence.png
pkill -x Xvfb
```

Traps encountered here, in order of how much time they cost:

* **A startup error in app mode opens a modal dialog.** `GuiApplication::reportError()` displays an application-modal
  message box in `Application::AppMode`, so an automated run of an unavailable `--gui` name blocked forever under
  `QT_QPA_PLATFORM=offscreen` until the dialog was dismissed (with no `xdotool` installed, only `kill` was left). For
  that reason the unavailable-frontend path prints to the terminal and aborts the startup with exit code 1 instead of
  reporting an exception, and any other *automated* run has to expect modal dialogs on fatal errors.
* **`SIGKILL` by pattern can kill the test harness.** `pkill -9 -f build-native/bin/ovito` matches the invoking shell as
  well, because the pattern occurs in its own command line; start GUI processes in the background, keep their PID and
  kill that.
* **An unattended classic-frontend *import* cannot be verified on this box.** The classic frontend blocks first on the
  `NewGraphicsSystemService` first-run dialog and then on `FileImporterEditor`/import-mode dialogs (open item O10), and
  there is no `xdotool`/`xte` installed to dismiss them. What can be verified is that the classic window starts, that
  `--nogui` works, and that the classic import path compiles and runs the shared `WorkbenchUI::importFiles()` code (the
  dialog that appears *is* the `inspectImporterFiles()` hook being reached).
* **`QQuickWindow::grabWindow()` cannot be used to screenshot the Qt Quick workbench.** With the `QQuickRhiItem`
  viewports in the window, the grab repeated its own renderer's synchronize/render steps and came back empty, blank or
  stale (the first scene-graph frame instead of the current one), and in roughly half of the runs it *crashed* inside
  `QCoreApplicationPrivate::notify_helper` — i.e. while Qt dispatched an event to a QML item whose graphics node was
  being re-created by the grab. The in-process capture option of the spike was therefore removed. Screenshots are taken
  from the X server instead: start the window on a *private* Xvfb display, let the spike hold it open with
  `--qml-hold-ms` (it prints `VERIFICATION_DONE` when the checks are done), and grab that display with ffmpeg.

### 2.3 Checking an installed tree

The build tree is not the deployed layout: CMake installs the frontends, the plugins and the shared data into different
relative directories, and the plugin lookup of `PluginManager` follows the executable. Check it instead of assuming it:

```bash
cmake --install build-native --prefix /tmp/ovito-install   # bin/ovito, lib/ovito/plugins, share/ovito
LD_LIBRARY_PATH="$PWD/.qt/6.10.2/gcc_64/lib:/tmp/ovito-install/lib/ovito/plugins" /tmp/ovito-install/bin/ovito --version
QT_QPA_PLATFORM=offscreen /tmp/ovito-install/bin/ovito --gui=bogus   # lists "qml, qt-widgets": the QML plugin was found
QT_QPA_PLATFORM=offscreen /tmp/ovito-install/bin/ovito --nogui
```

Then run the two product frontends from the prefix under Xvfb and screenshot with `ffmpeg -f x11grab` (§2.2); for the
classic one, add the settings file of §3.2. Measured on this machine (Phase 2.5):

* the installed **Qt Quick** frontend renders the scene: `ovito --gui=qml lattice.xyz` shows the four panes with the fitted
  lattice, the per-pane overlays (tripods, view titles), the active-pane border and the status line;
* the installed **classic** frontend starts, loads its 26 plugins, creates its viewport windows and then takes the known
  Xvfb renderer-failure path of §3.2 (`Presenting not supported on this window`,
  `Failed to create render pass descriptor for onscreen render target.`);
* `ovito-qml-spike` has **no install rule** — it is a prototype and lives in the build tree — so an install-tree check
  exercises the two product frontends and not the spike.

## 3. Classic frontend in a headless environment

* `RenderThread::pickGraphicsApi()` hardcodes the graphics API per platform, and on Linux it is always **Vulkan**. There is
  no environment override and no OpenGL fallback.
* Under Xvfb a Vulkan swapchain cannot be created (see §2.1), so the classic viewport renders **no frames at all** — the
  window appears, but the viewport stays black/empty and no `endFrame()` ever happens. Do not conclude from a surviving
  process that the classic viewport works.
* Forcing the software ICD (`VK_ICD_FILENAMES=lvp_icd.json`) fails earlier with
  `Failed to create Vulkan instance: -9` (`VK_ERROR_INCOMPATIBLE_DRIVER`). This did **not** come from the missing
  `apiVersion`, which finding O2 fixed (`GraphicsApi::configureVulkanApiVersion()` asks for
  `QVulkanInstance::supportedApiVersion()` before `create()`): a probe that creates a `QVulkanInstance` with and without
  an api version fails the same way, so the failure is caused by the platform integration of this environment (no
  compositor/DRI3) rather than by OVITO's instance setup.
* Therefore: **frame-rate or pixel comparisons between the classic frontend and the Qt Quick frontend must be done on a
  machine with a real display** (here: the macOS host), and the picking/rendering checks of the Qt Quick frontend should
  be done with the Qt Quick backend that works in the environment at hand.

### 3.1 Interactive import in the classic frontend blocks unattended runs

The classic frontend's command-line file import path (`MainWindowUI::importFiles()`) displays the importer's
`FileImporterEditor` UI for every file, and for an `.xyz` file that is a dialog that **waits for the user to press OK**.
Until then the scene contains no data — a benchmark would happily measure an *empty* scene at vsync-limited frame rate.
Additional modal dialogs appear when the scene is not empty ("reset the existing pipeline?").

Options when automating the classic frontend:

1. Do not automate the classic frontend; use `--nogui` (which imports without dialogs but renders nothing).
2. Apply the temporary instrumentation in §6, which skips that dialog when `OVITO_BENCHMARK_FRAMES` is set, and *verify*
   that the data was loaded (the instrumentation prints the number of scene pipelines; a blocked import prints `0`).

### 3.2 Getting the classic GUI past its first-run dialogs (and exercising the viewport path)

An unattended classic run spends its life behind two dialogs, and since neither `xdotool` nor `xte` is installed there
is nothing to click them away:

* the **first-run "new graphics system" dialog** (`NewGraphicsSystemService::applicationStarting()`, shown while
  `QSettings` lacks `viewport/adapter_setup_done`). Without it the main window is created but the viewport windows are
  never built — which is harmless for a startup smoke test, but it means an "empty grey viewport area" in a screenshot
  proves nothing about the viewport code;
* the **import dialog** of §3.1, and on this machine the Vulkan error dialog (§3) that follows a failed onscreen render
  target.

Write the settings key yourself to skip the first one, and keep the test's settings in their own directory so a test run
cannot rotate the developer's recent-files list or renderer selection:

```bash
mkdir -p /tmp/ovito-cfg/Ovito
printf '[viewport]\nadapter_setup_done=true\n' > /tmp/ovito-cfg/Ovito/Ovito.conf
XDG_CONFIG_HOME=/tmp/ovito-cfg QT_QPA_PLATFORM=xcb xvfb-run -a --server-args='-screen 0 1400x900x24' ./build-native/bin/ovito
```

**The file is a `QSettings` INI file, and `QSettings::value("viewport/adapter_setup_done")` means the
`[viewport]` section with an `adapter_setup_done` key** — a top-level line `viewport/adapter_setup_done=true` (with a slash
or with a backslash) is *not* read as that key. This matters because a wrong file fails silently: the dialog still appears
and the run looks exactly like a healthy one whose viewport area happens to be empty, which is how an earlier version of
this recipe was recorded as working without ever having worked. Verify the suppression instead of assuming it — with the
dialog gone, a run under Xvfb prints

```
Presenting not supported on this window
ERROR: Failed to create render pass descriptor for onscreen render target.
```

while a run still behind the dialog prints nothing at all (the classic frontend logged six lines in the working case and
zero in the blocked one).

With that, the classic frontend creates its four viewport windows, assigns the interactive renderer from
`ViewportRendererRegistry` and - on a box without DRI3 - ends up in the renderer-failure path it has always had
("Failed to create render pass descriptor for onscreen render target."). That is the expected outcome under Xvfb, and it
is the cheapest way to check that a change to the renderer selection did not break the classic window creation.

Related trap: the interactive renderer selection is stored in `QSettings`
(`rendering/selected_graphics_api`, overridden by `OVITO_VIEWPORT_RENDERER`). A test can therefore switch the renderer
without touching the settings:

```bash
# An implementation that this build does not provide: the registry must fall back to the default renderer and warn
OVITO_VIEWPORT_RENDERER=anari ./build-native/bin/ovito-qml-spike --qml-pick 200,200 /tmp/lattice.xyz
# -> 'The selected interactive viewport renderer "anari" is not available in this build; using the default renderer instead.'
```

---

## 4. Assertion-enabled builds on this machine

The bundled Qt (``.qt/6.10.2/gcc_64`) ships **no debug libraries** (`libQt6Cored.so` is absent), so the `debug` preset
cannot link. Use a release-style build with the assertion machinery switched on instead:

```bash
cmake --preset native -B build-asserts \
  -DCMAKE_CXX_FLAGS="-DOVITO_DEBUG -DQT_FORCE_ASSERTS" \
  -DOVITO_BUILD_QML_FRONTEND=ON
cmake --build build-asserts -j 12
```

* `OVITO_DEBUG` enables `OVITO_ASSERT()`; `QT_FORCE_ASSERTS` enables `Q_ASSERT()` even though Qt was built with
  `QT_NO_DEBUG`.
* Verify that assertions are really active instead of assuming it: the condition string literals appear in the object
  files of `build-asserts` and not in `build-native`
  (`strings build-asserts/…/RenderThread.cpp.o | grep -c "this_task::get()"`).
* Running CTest from that build directory needs the plugin and Qt directories on the loader path:

```bash
LD_LIBRARY_PATH="$PWD/.qt/6.10.2/gcc_64/lib:$PWD/build-asserts/lib/ovito/plugins:$PWD/build-asserts/lib/ovito" \
QT_QPA_PLATFORM=offscreen ctest --test-dir build-asserts
```

This configuration found a real defect in the picking path (F4: asynchronous work started from a Qt event handler runs in
a task **without** a `UserInterface`), which the release build hid completely.

`ctest --preset native` runs 12 tests, six of which are the automation suites
(`tst_automation_contracts`, `tst_python_environment_probe`, `tst_python_data_bridge`, `tst_python_schema_preview`,
`tst_session_descriptor`, `tst_automation_cli`, 107 QtTest cases between them). What each of them is allowed to promise, and how the suites relate to
the prose they check, is [AUTOMATION_CONTRACTS.md](AUTOMATION_CONTRACTS.md) §1 and §19.

### 4.1 What a core integration test has to provide

A `ctest` unit test that creates OVITO objects — a `DataSet`, a `SceneNode`, a `Pipeline` — needs three preconditions that
the running application always has and a bare `QTest` process does not. All three are invisible in a release build: the
mistake becomes a null-pointer dereference somewhere inside core instead of a failed assertion.

* **An ambient task.** `OORef<X>::create()` of a `RefTarget` asks the current task whether the creation is interactive
  (`this_task::isInteractive()`), so with no task installed a release build reads through a null `Task*`. Install one per
  test function: `std::make_shared<Task>()` plus a `Task::Scope` held in a `std::unique_ptr` member, and undo it in
  `cleanup()` — `setFinished()` before the shared pointer is dropped, because a task must be finished when it goes away. Do
  not count on the assertion that is meant to catch the missing task: `OVITO_ASSERT(this)` sits inside the member function,
  and the assertion-enabled build stayed quiet about it in practice (the abort of the automation suite came from the next
  two bullets, not from this one).
* **An `Application`.** Core asks `Application::instance()` for the main thread (`this_task::isMainThread()` in
  `SceneNode::invalidateWorldTransformation()`), so creating a `SceneNode` in a test process without an application aborts
  the assertion-enabled build with `ASSERT: "Application::instance() != nullptr"` in `Task.cpp`, and dereferences null in a
  release build. Create it once for the whole suite in `initTestCase()` through `OORef::create()` (a plain stack object
  trips an assertion when it is destroyed) using the minimal subclass the async tests already use —
  `TestApplication : public Application` whose `createQtApplicationImpl()` returns `nullptr`, the only pure virtual;
  `QTest` owns the real `QCoreApplication`, so it is never called. Retire it in `cleanupTestCase()` after
  `_app->taskManager().requestShutdown()`, because the destructor asserts that the task manager was shut down. Unlike the
  task, this creation needs no ambient task: `OvitoObject` is not a `RefTarget`, so that `create()` never asks the current
  task.
* **A user interface on the task.** `SceneNode::insertChildNode()` resolves `this_task::ui()->datasetContainer()` for the
  current animation time, so a task that carries no user interface makes an unattended release run dereference null at
  `UserInterface::datasetContainer()` (address `0x20` of a null object) the moment the test adds a node to a scene. Set it
  with `_task->setUserInterface(app)` after the scope is installed.

The working examples are `tests/cpp/core/utilities/concurrent/tst_concurrent_pool.cpp` (the application fixture) and
`tests/cpp/core/automation/tst_automation_contracts.cpp` (all three together, in the fixture of a Phase 2.6 contract test).
`ctest --preset native` sets `QT_QPA_PLATFORM=offscreen` for the test run; a single test binary started by hand needs the
loader path **and** that variable:

```bash
LD_LIBRARY_PATH="$PWD/.qt/6.10.2/gcc_64/lib:$PWD/build-native/lib/ovito/plugins" \
QT_QPA_PLATFORM=offscreen ./build-native/tests/cpp/core/automation/tst_automation_contracts
```

And run the suite in the assertion-enabled tree as well (§4): it is the only one of the two builds that reports the
missing `Application` at all.

A test that only touches value types and `QProcess` needs none of this. `tst_python_environment_probe.cpp` is the example:
creating no `RefTarget`, it runs against a bare `QTest` process, and the one trap of writing it was different — `QSKIP`
and `QVERIFY` expand to a bare `return`, which a helper that returns a value cannot use (`-Wreturn-type` is an error in
this build). It skips from the test function itself instead (see `SKIP_WITHOUT_PYTHON` in that file).

**A `tests/cpp` target that links a plugin must be gated on the plugin *option*, not on `if(TARGET …)`.** The top-level
`CMakeLists.txt` adds the `tests` subdirectory at line 158 and `src` only at line 360, so at test-registration time no
plugin target exists yet and `if(TARGET Gui)` is false even in a build that will create `Gui`. A target guarded that way
disappears silently from `ctest -N` instead of failing loudly. The reliable guard is the option the plugin's
`ADD_SUBDIRECTORY` reads, e.g. `if(OVITO_BUILD_APP AND OVITO_BUILD_PLUGIN_STDMOD AND …)`, since the `OPTION()`
declarations are at the top of the file and are already set. This is how `tests/cpp/gui/CMakeLists.txt` keeps the classic
comparison test out of the `OVITO_BUILD_APP=OFF` frontend scope while registering the shared model test that only needs
`GuiBase` and `StdMod`.

### 4.2 Running the Python runtime probe and the topology spike

Both are part of the Phase 2.6 Python track and both expect a real interpreter. Three rules keep them honest on a machine
that has one:

* **The probe tests need a `python3` on the path** (`QStandardPaths::findExecutable`), never a specific one: they use the
  interpreter they find and *skip* the cases that need one if there is none, so a machine without Python loses coverage
  rather than reporting failures `ctest` cannot distinguish from real ones. Everything they assert about a package is
  written into a temporary directory as a fixture (`ovito/automation/__init__.py` that answers `handshake()`), with
  `PYTHONPATH` and an explicitly non-isolated interpreter, because `-I` ignores `PYTHONPATH` by design.
* **The topology spike takes its interpreter explicitly.** Without `--python` it uses
  `PythonEnvironmentProbe::findInterpreter()`, i.e. the same discovery the tests use, and prints the verdict of the
  environment probe next to the numbers, so a recorded run always names its environment. The numpy comparison of
  [AUTOMATION_TOPOLOGY_SPIKE.md](AUTOMATION_TOPOLOGY_SPIKE.md) §2 used a throwaway `uv` environment:
  `uv venv .venv-automation --python 3.12 && uv pip install --python .venv-automation/bin/python numpy` (the directory is
  git-ignored like every dot-directory and is *not* a supported configuration — OVITO never creates or modifies it).
* **Both binaries need the same loader path as every other test**, and the spike needs no window system at all:

```bash
LD_LIBRARY_PATH="$PWD/.qt/6.10.2/gcc_64/lib:$PWD/build-native/lib/ovito/plugins" \
QT_QPA_PLATFORM=offscreen ./build-native/tests/cpp/core/automation/tst_python_environment_probe

LD_LIBRARY_PATH="$PWD/.qt/6.10.2/gcc_64/lib:$PWD/build-native/lib/ovito/plugins" \
  ./build-native/bin/ovito-automation-spike --quick                      # smoke pass, seconds
LD_LIBRARY_PATH="$PWD/.qt/6.10.2/gcc_64/lib:$PWD/build-native/lib/ovito/plugins" \
  ./build-native/bin/ovito-automation-spike --python .venv-automation/bin/python --json /tmp/spike.json
```

The spike exits `0` when every check passed, `1` when a check failed (the measurements still print) and `2` when there was
no interpreter to measure, so a CI job can tell a failure from a skip. It starts and kills its own interpreters; if a run
is interrupted, `pgrep -fl ovito_worker.py` finds a leftover one, which is the only process it can leak.

One more rule follows from what the probe *is*: the version envelope of `PythonContract` (CPython 3.10 to 3.13) is checked
before the platform, the package and the features, so a case that tests one of those three needs an interpreter the probe
does not reject first. `SKIP_WITHOUT_SUPPORTED_PYTHON` in that file looks for the machine's `python3` and then for
`python3.13` ... `python3.10`, and *skips* a case when none of them is inside the envelope - failing would blame the code
for the machine. The GitHub runners' default `python3` is newer than the envelope (3.14.7 when this was written), which is
why all three CI jobs install a supported interpreter (`actions/setup-python`, 3.13) before CTest: `ctest` counts a skipped
case as passed, so those rules would otherwise be tested nowhere.

### 4.3 Running the local-protocol spike

The discovery half is a CTest case like any other (`tst_session_descriptor`); the endpoint itself is driven by a program
that starts servers of its own and talks to them:

```bash
LD_LIBRARY_PATH="$PWD/.qt/6.10.2/gcc_64/lib:$PWD/build-native/lib/ovito/plugins" \
  ./build-native/bin/ovito-automation-ipc-spike --json /tmp/ipc-spike.json    # the 24-check self-test

# Discovery by hand, with two live workbenches that a client may have to choose between.
LD_LIBRARY_PATH="$PWD/.qt/6.10.2/gcc_64/lib:$PWD/build-native/lib/ovito/plugins" \
  ./build-native/bin/ovito-automation-ipc-spike --serve --scope /tmp/ipctwo &
LD_LIBRARY_PATH="$PWD/.qt/6.10.2/gcc_64/lib:$PWD/build-native/lib/ovito/plugins" \
  ./build-native/bin/ovito-automation-ipc-spike --list --scope /tmp/ipctwo | python3 -m json.tool
pkill -f 'ovito-automation-ipc-spike --serve'; rm -rf /tmp/ipctwo      # always clean up
```

Three things about it are worth knowing before running it:

* **It writes outside the build tree, and where it writes is controlled.** The sessions of a run live in one discovery
  directory: `--scope <dir>` (and the `OVITO_AUTOMATION_SESSION_DIR` variable the spike exports for its children), or a
  private directory under the system temporary directory when nothing is passed. The self-test's own runs are therefore
  invisible to a user's real sessions - and a `--serve` started without a scope is *not*, which is why a hand-started
  server belongs in a temporary scope.
* **A `--serve` process serves until it is told to stop or killed.** `--allow-quit` lets a client send the `quit`
  message, which is the graceful case (the descriptor is removed); `pkill` is the killed case, and it leaves a *stale*
  descriptor behind on purpose - that is what `--list --prune` is for. Always kill the processes and remove the scope
  directory afterwards, and check with `pgrep -fl 'ovito-automation-ipc-spike --serve'`.
* **A capture needs a graphics device, which this program does not create.** An endpoint started with
  `--synthetic-capture` answers with a generated image (the transport checks); one started without it answers
  `render_unavailable`, which is the honest answer of a process without a QRhi and the one the audit's O1 predicts for any
  headless run of the frontend.

---

### 4.4 Running the Python seam tests (schema preview and data bridge)

The two suites that exercise the schema preview and the data bridge follow the same environment recipe as the probe:

```bash
LD_LIBRARY_PATH="$(pwd)/.qt/6.10.2/gcc_64/lib:$(pwd)/build-native/lib/ovito/plugins" QT_QPA_PLATFORM=offscreen \
    ./build-native/tests/cpp/core/automation/tst_python_schema_preview
LD_LIBRARY_PATH="$(pwd)/.qt/6.10.2/gcc_64/lib:$(pwd)/build-native/lib/ovito/plugins" QT_QPA_PLATFORM=offscreen \
    ./build-native/tests/cpp/core/automation/tst_python_data_bridge
```

Both find their interpreter through `PythonEnvironmentProbe::findInterpreter()` (i.e. `python3` on `PATH`) and skip their
environment cases without one, so a run in an environment whose Python behaviour is under scrutiny should set `PATH`
accordingly. `tst_python_data_bridge` starts real workers and every case stops its own (a graceful stop is asserted to
leave exit code 0), so only an interrupted run leaves an interpreter behind — `pgrep -fl ovito_worker.py` finds it. Its
8 MB case prints what that exchange took; [AUTOMATION_DATA_BRIDGE.md](AUTOMATION_DATA_BRIDGE.md) §5 records the reference
numbers and what the suites cover.

## 5. macOS test host

### 5.1 Toolchain setup (already done once, recorded for reproducibility)

Qt is installed by the `buddy` account with `aqt` (a Python venv is enough; there is no `aqt` in Homebrew) into
`/Users/buddy/Qt`, and `/Users/buddy` is mode 750 with group `staff` — `kings` is in `staff`, so the build running as `kings`
can read it. Two accounts are involved because only `kings` owns the console session (section 5.2):

```bash
# once, as `buddy` (needs a Python venv with aqtinstall)
python3 -m aqt install-qt mac desktop 6.10.2 clang_64 --outputdir /Users/buddy/Qt
python3 -m aqt install-qt mac desktop 6.10.2 clang_64 -m qtshadertools --outputdir /Users/buddy/Qt   # qt_add_shaders
```

```bash
# once, as `kings` (Homebrew's boost/hdf5/netcdf are system-wide)
/opt/homebrew/bin/brew install cmake ninja boost hdf5 netcdf

# clone + submodules, then configure; the fork has no macOS preset, so pass the Qt prefix explicitly
cd ~/ovito && git submodule update --init src/3rdparty/zstd src/3rdparty/hdf5 src/3rdparty/netcdf-c
cmake -S . -B build-buddy -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_PREFIX_PATH="/Users/buddy/Qt/6.10.2/macos;/opt/homebrew" \
  -DOVITO_BUILD_APP=ON -DOVITO_BUILD_QML_FRONTEND=ON -DOVITO_BUILD_CPP_TESTS=OFF -DOVITO_USE_CCACHE=OFF
/opt/homebrew/bin/ninja -C build-buddy -j 8     # full target set, ~30 min on the M4
```

The non-interactive SSH shell has no Homebrew in `PATH`, hence the absolute `/opt/homebrew/bin/ninja`; `OVITO_USE_CCACHE=OFF`
because Homebrew ccache is not installed there.

The binaries end up in the macOS bundle layout, not in `bin/`:

```
build-buddy/Ovito.app/Contents/MacOS/ovito              # classic frontend
build-buddy/Ovito.app/Contents/MacOS/ovito-qml-spike    # Qt Quick frontend
```

### 5.2 Running GUI tests over SSH

**A graphical session for the SSH user is a hard precondition.** macOS refuses to create a window for a user who has no
Aqua session, and the symptom is a Qt abort with:

```
Cannot create window: no screens available
PasteBoard: Error creating pasteboard: com.apple.pasteboard.clipboard [-4960]
Abort trap: 6
```

This is not a Qt or OVITO problem, and it is what happens when the SSH user is not the console user. Check before blaming the
code:

```bash
who                                     # who owns the console session?
stat -f "%Su" /dev/console              # console user
system_profiler SPDisplaysDataType      # an attached display shows up as a "Displays:" section
```

On `buddy` the console session belongs to `kings` (attached monitor, so a real screen exists), while `buddy` has only a
remote session — hence the earlier abort. Additionally, `open -a` and other Launch Services calls fail for a non-GUI
session as well:

```
The application /Applications/… cannot be opened for an unexpected reason,
error=Error Domain=RBSRequestErrorDomain Code=5 "Launch failed."
    Underlying Error: Domain does not support specified action
```

This is why the Parallels Desktop application cannot be started from an SSH session (section 5.4).

Quick check that a Qt GUI window really works for the intended user (replace `kings@buddy`):

```bash
ssh kings@buddy 'QT_QPA_PLATFORM=cocoa DYLD_FRAMEWORK_PATH=$HOME/Qt/6.10.2/macos/lib \
  <cmake-built test that shows a QWindow and prints isExposed()>'
```

* `QT_QPA_PLATFORM=cocoa` is the default; no `launchctl asuser` trick is needed when the user owns the console session.
* Harmless noise in such runs: `qt.gui.icc: fromIccProfile: failed size sanity 1`, `PasteBoard: Error creating pasteboard`.
* `QScreen::grabWindow()` returns a 0×0 image on macOS unless the process has screen-recording permission, and
  `QQuickWindow::grabWindow()` is not an alternative for this frontend (it crashes or returns an empty image with
  `QQuickRhiItem` viewports, §2.2). On this host a screenshot of a Qt Quick run is therefore **not possible from an SSH
  session**; the proof that the frontend works is its exit code, its check output and the on-device captures taken from a
  console session.
* Use the framework-based Qt: `CMAKE_PREFIX_PATH=$HOME/Qt/6.10.2/macos` works, private headers live inside the frameworks
  (e.g. `QtQuick.framework/Headers/6.10.2/QtQuick/private/qquickrhiitem_p.h`).
* **Qt itself must be on the loader path in the build tree**; the OVITO plugins no longer need to be (the `@rpath`
  packaging gap that required `DYLD_LIBRARY_PATH=<bundle>/Contents/PlugIns` was fixed in the spike target, see §9):

```bash
cd ~/ovito
QT_QPA_PLATFORM=cocoa DYLD_LIBRARY_PATH=/Users/buddy/Qt/6.10.2/macos/lib \
  ./build-buddy/Ovito.app/Contents/MacOS/ovito-qml-spike \
  --qml-startup-delay 4000 --qml-pick 300,300 --qml-hold-ms 5000 /tmp/lattice_512.xyz
```

* Retina displays report `devicePixelRatio = 2`, so the same logical window covers four times the pixel count of a
  non-Retina machine and all four viewports render at that size. Always state the pixel size and
  the device pixel ratio when comparing frame rates across machines (`buddy` reports `devicePixelRatio = 1`, the retired
  `mac` host reported 2).
* `PasteBoard: Error creating pasteboard` messages from a SSH-launched GUI process are harmless.
* **Log files can contain binary bytes** (Qt/pasteboard/UTF-8 output), so `grep` prints `Binary file … matches`. Use
  `grep -a` in scripts that parse these logs.

### 5.3 Always close the windows after a test run

A Qt Quick window keeps rendering as long as it is open. A leftover instance saturates the machine and made further SSH
logins time out (and it silently inflated a later measurement: an apparently "aborted" classic run had actually kept
running for 160 s). Always terminate and verify:

```bash
pkill -9 -f Ovito.app            # or: kill <pid> recorded when the run was started
sleep 2
pgrep -fl Ovito.app || echo CLEAN
```

Add a timeout in front of the command as a second line of defence (note that macOS has no `/usr/bin/timeout`; either build
one from the command itself — `cmd & pid=$!; sleep N; kill -9 $pid` — or install `coreutils` for `gtimeout`), and start long
runs with an explicit PID file so the process can be killed deterministically.

### 5.4 Windows/D3D12 cannot be validated with the Parallels VM

The `buddy` host has a `Windows 11.pvm` virtual machine, but it is not a usable D3D12 test target:

* Parallels Desktop on Apple Silicon exposes **DirectX 11.1** at most — there is no D3D12 support in the guest.
* The guest is Windows 11 **ARM64**, while OVITO's Windows target is **AMD64/x86_64**.

So a D3D12 smoke test needs an x86_64 Windows machine; a real one is described in section 5.5, and section 6 shows the CI
job that covers the same ground without owning the hardware. Qt Quick's D3D12 backend supports the **WARP** software adapter
(`QSG_RHI_PREFER_SOFTWARE_RENDERER=1`), and OVITO's render thread creates its D3D12 device with default
`QRhiD3D12InitParams`, which falls back to the DXGI default adapter (WARP on a GPU-less machine) — i.e. a CPU-only CI runner
can exercise the D3D12 code paths, the same way lavapipe covers Vulkan on Linux. Prerequisites for such a job: a Qt 6.10
Windows installation with private headers, and `dxc` on `PATH` (OVITO precompiles HLSL SM 6.0 to DXIL).

Also note that when a Windows machine *is* available, a classic-frontend run needs the interactive import dialog out of the
way (section 3.1) — that trap applies to every platform.

### 5.5 Windows x86_64 host: build, run and diagnose

This section is the recipe that produced the Windows/D3D12 results in
[UI_PHASE1_SPIKE.md](UI_PHASE1_SPIKE.md) section 3.5 and the fixes F13–F16. Two of its traps cost a long debugging
session each, so they are written down in the order they were hit.

**Toolchain and configure.** Visual Studio 2022 (x64 native tools), CMake and Ninja (the copy inside the Visual Studio
installation works), Qt 6.10.2 `msvc2022_64` including the `qtshadertools` module, the **official Boost source tarball**
(header-only use, `-DBOOST_ROOT=… -DBoost_NO_BOOST_CMAKE=ON`; the vcpkg `boost-headers` port is missing per-library headers
such as `boost/algorithm/algorithm.hpp`), `dxc.exe` from the Windows SDK on `PATH`, and — because
`-DOVITO_REDISTRIBUTABLE_PACKAGE=ON` builds the bundled HDF5/NetCDF/SQLite — a **zlib-enabled** environment: the bundled
netcdf-c stops with `HDF5 was built without zlib. Rebuild HDF5 with zlib.` and also requires **Perl** on `PATH` (defect F16).
Build zlib first (`-DBUILD_SHARED_LIBS=ON`, install to a prefix), pass both `-DZLIB_ROOT=<prefix>` and that prefix inside
`-DCMAKE_PREFIX_PATH` (module-mode `FindZLIB` does not always honour `ZLIB_ROOT` alone), and delete the external-project
stamps `build-win/_ep` and `build-win/_ep_build` when zlib is added afterwards, because an HDF5 stamp records the source
version only and a stale one silently keeps the zlib-less build.

**Where the binaries land.** Unlike Linux (`bin/` plus `lib/ovito/plugins/`), a Windows build tree puts `ovito.exe`,
`ovito-qml-spike.exe` and all `*.ovito.dll` plugins directly into the build root, and it also writes a `qt.conf` there whose
`Plugins = plugins/` describes the layout of an *installed* package. Qt therefore cannot find its own plugins or QML
modules from the build tree, which shows up as

```
qt.qpa.plugin: Could not find the Qt platform plugin "windows" in ""
qrc:/ovito/gui/qml/WorkbenchWindow.qml:5:1: module "QtQuick.Controls" plugin "qtquickcontrols2plugin" not found
```

Set these before running anything from a build tree (a `.cmd` file is the reliable way, see below):

```bat
set PATH=C:\ovito\build-win;%QT%\bin;%PATH%
set QT_PLUGIN_PATH=%QT%\plugins
set QT_QPA_PLATFORM_PLUGIN_PATH=%QT%\plugins\platforms
set QML_IMPORT_PATH=%QT%\qml
set QML2_IMPORT_PATH=%QT%\qml
```

**GUI tests must run in the console session.** An SSH session lands in Windows session 0, which has no DWM compositor: a
D3D12 device and its command queue can be created there, but `CreateSwapChainForHwnd` fails with
`DXGI_ERROR_NOT_CURRENTLY_AVAILABLE` and every GUI run dies at startup (Qt's `offscreen` plugin cannot supply a D3D12 QRhi
either). Start the test through the task scheduler as the logged-on user, which lands in session 1, and collect the output
from a file; the same trick keeps the build running while the SSH connection comes and goes:

```bat
schtasks /create /tn ovito-verify /tr "cmd /c C:\Users\chlo\jobs\verify.cmd > C:\Users\chlo\jobs\verify.log 2>&1" /sc once /st 00:00 /f
schtasks /run /tn ovito-verify
```

Scheduled tasks start in `C:\Windows\System32`, so every wrapper script has to `mkdir` and `cd /d` its working directory
first (otherwise downloads and logs fail with confusing write errors), and the warning `Task may not run because /ST is
earlier than current time` is harmless. There is no screenshot to be had this way: `QQuickWindow::grabWindow()` returns
nothing usable for a `QQuickRhiItem` viewport (section 2.2), so a product screenshot has to be taken from outside with a
`PrintWindow(PW_RENDERFULLCONTENT)` helper while the application is held open (`--qml-hold-ms`). Such a helper can crash in
`gdiplus.dll` *after* writing the file — check the file size, not the exit code.

**Diagnosing a crash without a debugger.** Windows Error Reporting already records the essentials, including the module and
the fault offset, and `llvm-symbolizer` from the Visual Studio LLVM component resolves that offset against the PDB:

```powershell
Get-WinEvent -FilterHashtable @{LogName='Application'; ProviderName='Application Error'} -MaxEvents 5 |
  ForEach-Object { ($_.Message -split "`n") | Where-Object { $_ -match 'Faulting module name|Fault offset' } }
# fault offset is an RVA: add the image base (0x180000000 for these x64 DLLs) and let the symbolizer read the PDB
& "<VS>\VC\Tools\Llvm\x64\bin\llvm-symbolizer.exe" --obj=C:\ovito\build-win\GuiBase.ovito.dll 0x18004f3a2
# -> Ovito::WorkbenchUI::importFiles(...) C:\ovito\src\ovito\gui\base\app\WorkbenchUI.cpp:103:0
```

Two details of that: `llvm-symbolizer` takes the **virtual address** (image base + RVA) and has no `--pdb=` option — it
finds the PDB through the binary — and the disassembly only lines up when it starts at a real instruction boundary, so use
`llvm-objdump --start-address=<symbol start>` rather than a round offset. The compiler had inlined the real culprit into
the call site, so the crash line named a *statement*: it was narrowed by splitting that statement into temporaries with
`qDebug()` markers between them until the failing step was obvious (defect F13 — an argument evaluation order that only
MSVC chose differently). That technique is worth reaching for early: one rebuild of `GuiBase` costs two minutes.

**One more build-tree trap.** After a source file appears in a target through a new condition (here: zlib was added, so
`GzipIODevice.cpp` became part of `Core`), AUTOMOC may not process its header, and `Core` then fails to link with missing
`moc_*` symbols even though the `.cpp` compiled. Deleting the stale `build-win/src/ovito/core/Core_autogen` directory and
rebuilding fixes it.

**Verifying.** `C:\Users\chlo\jobs\verify.cmd` runs the whole gate in one go: `ovito --version`, `--nogui` and
`--gui=bogus` (which must print the available user interfaces and exit 1), then the spike with the full check set under the
frontend's own backend choice, with `QSG_RHI_BACKEND=d3d12`, with `QSG_RHI_BACKEND=d3d12` plus
`QSG_RHI_PREFER_SOFTWARE_RENDERER=1` and with `QSG_RHI_BACKEND=vulkan`, and finally the product `ovito --gui=qml` with a
data file, which must stay alive and can be screenshotted from outside. Every spike run prints
`VERIFICATION_DONE with 0 failed check(s)` and exits 0; the picking numbers (9 of 25 probe positions, 2315 of 7128 scan
positions, empty background control, identical hit locations across backends) are the regression signal. The classic
frontend is affected by defect F13 as well — it shares `WorkbenchUI::importFiles()` — but it blocks in its modal import
dialog before reaching that call, which is why the crash surfaced through the Qt Quick frontend first; the product crash
that was reproducible is `ovito --gui=qml` with a data file.

---

### 5.6 Verifying a change on the two hosts rather than on CI

A change is verified where it can break, and the two hosts above are faster than the runners by an order of magnitude:

| A change to | Is verified on |
| --- | --- |
| anything (the default) | this Linux workstation: the native preset, `ctest --preset native`, the QML checks under `xvfb-run` |
| Windows-specific code, the suites that only fail on Windows, the D3D12/WARP smoke path | the Windows host `kitty` (`ssh kitty`) |
| macOS-specific code, the Cocoa/Metal smoke path | the macOS host `buddy` (`ssh kings@buddy`, sections 5.1 and 5.2) |

GitHub CI stays the fourth and slowest place: a push starts a three-platform matrix and cancels the run in flight, so it
confirms that the hosts agree with the runners instead of being where a failure is first diagnosed.

**Both hosts take the code from this workstation, never from the network** (kitty has no route to `github.com` at all, and
`git fetch` there fails with `Failed to connect to github.com port 443`). The bundle needs a commit the host already has:

```bash
git bundle create /tmp/ovito.bundle 5decf25..master        # 578 KB for a few dozen commits
scp /tmp/ovito.bundle kitty:C:/ovito/ ; scp /tmp/ovito.bundle kings@buddy:/Users/kings/
ssh kitty powershell -NoProfile -EncodedCommand <script as base64 UTF-16LE>   # quoting-free remote commands
ssh kings@buddy bash -s < script.sh
# then, on either host: git fetch <bundle> master:refs/heads/verify-ci && git checkout verify-ci
```

Each host keeps the checkout it already had and gets a verification branch, so the branch and build configuration the
host uses for its own work stay as they are:

* **buddy**: `~/ovito/build` is already configured with `OVITO_BUILD_CPP_TESTS=ON` and `/Users/kings/.ccache` is warm, so
  `/opt/homebrew/bin/cmake --build build --parallel 8 --target <test>` rebuilds a small change in about a minute.
* **kitty**: `C:\ovito\build-verify` is a *second* build directory (the machine's own `C:\ovito\build` is left alone),
  configured with the Visual Studio copy of CMake and Ninja, Qt `C:/Users/chlo/Qt/6.10.2/msvc2022_64`,
  `BOOST_ROOT=C:/Users/chlo/Tools/boost_1_92_0`, `ZLIB_ROOT=C:/Users/chlo/Tools/zlib` (section 5.5, defect F16),
  `-DOVITO_REDISTRIBUTABLE_PACKAGE=ON` — the option that makes the *bundled* HDF5/NetCDF submodules be built instead of a
  system HDF5 being looked for, which is why a machine without HDF5 can configure at all — and `-DOVITO_BUILD_CPP_TESTS=ON`.

**The Python suites need a real interpreter on kitty.** What `findInterpreter()` finds there by default is a Microsoft
Store *app execution alias* (`...\WindowsApps\python3.exe`), which fails when it is started without a console. The official
*embeddable* distribution is enough, because the worker and the schema preview use the standard library only:

```powershell
Expand-Archive C:\ovito\py313.zip C:\ovito\py313                   # python-3.13.7-embed-amd64.zip, 11 MB
Copy-Item C:\ovito\py313\python.exe C:\ovito\py313\python3.exe     # findInterpreter() looks for 'python3' first
```

Prepended to `PATH`, that gives the suites a CPython 3.13 inside the version envelope of the Python contract (D48).
Both hosts run the suites the same way they are run here - `tst_x.exe -o <file>,txt` for the full test log, and
`OVITO_REQUIRE_TEST_BINARY=1` where a missing product executable must be a failure rather than a skip (section 6.2).

## 6. Continuous integration as a test host (all four target platforms)

The GitHub workflow `.github/workflows/ci.yml` builds and smoke tests the Qt Quick frontend on the four target platforms,
which is the only way to cover **Windows/D3D12** and a second macOS/Metal host without owning that hardware. Each job
configures with `-DOVITO_BUILD_QML_FRONTEND=ON`, builds the whole tree including `OvitoQmlSpike`, and runs it against a
generated 512-atom lattice with the full check list (`--qml-layout-check`, `--qml-command-check`, `--qml-settings-check`,
`--qml-session-check`,
`--qml-library-check`, `--qml-icon-check`, `--qml-offscreen-check`, `--qml-prewarm-check`, `--qml-parity-check`,
`--qml-import-check`, `--qml-pick`, `--qml-frame-stats`, `--qml-lifecycle-cycles`, `--qml-hide-show`); the spike exits
non-zero when a check fails, so the step is an assertion instead of a log to grep. The two Linux jobs also run a second
step with `QT_QPA_PLATFORM=offscreen` and only `--qml-device-check`, which asserts the missing-graphics-device answer. No
screenshots are uploaded: a `QQuickRhiItem` viewport cannot be captured from inside the process (§2.2), and the evidence
images of this directory were taken from a private Xvfb display with ffmpeg.

| Job | Qt Quick backend | OVITO picking backend | Display |
|-----|------------------|------------------------|---------|
| Linux x86_64 | OpenGL (llvmpipe) | Vulkan (lavapipe via `VK_DRIVER_FILES`) | `xvfb-run` + `QT_QPA_PLATFORM=xcb` |
| macOS ARM64 | Metal | Metal | the runner's own session |
| Windows AMD64 | D3D12 (the frontend selects it itself) | D3D12 on WARP | the runner's own session |

The Linux ARM64 job that this table used to list was dropped from the matrix (its two backends - llvmpipe OpenGL and
lavapipe Vulkan - are the same as the x86_64 job's, and it was the most expensive job per risk covered); the ARM64
binaries themselves were verified on that architecture during Phase 1 and are built by the release workflow.

**Where the jobs currently stand** (all green as of run `36623313897`, commit `19a02ea0a`, when the matrix still had four
jobs): the Linux x86_64 job additionally photographs the workbench while the spike holds its window open and uploads it as
the artifact `qt-quick-shell-linux-x86_64`; Linux x86_64,
macOS ARM64 and Windows AMD64 run the whole smoke test - layout, commands, settings, session, library, icons, offscreen
rendering, picking, parity and the import path - and pass, so the Windows job finally reached **and passed** its
Direct3D 12 smoke test (through WARP, since the runners have no GPU). Getting there took four defects, all of them in the
CI wiring or in the checks rather than in the frontend; each is described in place below:

1. The Windows CTest step pointed `PATH` at `build\lib\ovito\plugins` - that is where *Linux* keeps the libraries. On
   Windows every OVITO library (Core, Gui, GuiBase and all plugin DLLs) is placed **directly in the build directory**, and
   so are the executables (`build\ovito.exe`, `build\ovito-qml-spike.exe`), because Windows would not find Core in a
   plugins subdirectory (`OVITO_RELATIVE_BINARY_DIRECTORY`, `OVITO_RELATIVE_PLUGINS_DIRECTORY`, `cmake/Plugins.cmake`).
   Five of six tests aborted with `0xc0000135` while the header-only `tst_containers` passed.
2. `install-qt-action` exports `QT_ROOT_DIR`, but `Qt6_DIR` - the variable it sets for CMake - is empty when a step reads
   it as an environment variable, which made the smoke-test step abort in `Split-Path ... $env:Qt6_DIR` before the spike
   was even started (see the trap paragraph below).
3. On Windows Qt sends its messages to the debugger rather than to a redirected stderr, so the job ran the spike for 37
   seconds and printed **nothing** while the spike exited with 1 - the failing check was invisible. The step sets
   `QT_FORCE_STDERR_LOGGING=1` and captures both streams into files, which is what finally revealed the last defect.
4. The offscreen check *grew* the workbench window by 40x30 to force the picking service to replace its offscreen target;
   the CI runners' small displays make that resize fail, so the pane size never changed and the check reported a picking
   failure although its own log showed 1908 of 4662 probed positions hitting the lattice. The check shrinks the window
   now, and if a platform refuses even that it says in the log that the superseded target was not exercised instead of
   reporting a failure.
5. `ovito.exe` is linked as a Windows-subsystem application (`WIN32_EXECUTABLE` in `src/main/CMakeLists.txt`), so
   `& ovito.exe --nogui` neither waits for it nor sets `$LASTEXITCODE`: the first version of the headless smoke step
   threw `ovito --nogui failed with exit code` with an *empty* code while OVITO had started and finished fine. The step
   runs it through `Start-Process -Wait -PassThru` and checks `$process.ExitCode` now, which on the Windows test host
   returns 0 for exactly that command.

Two failures were in the checks themselves as well: the import check required the task progress model to have reported an
import that lasted 205 ms (the model refreshes at most every 100 ms, which flaked on the slower runners),
and the parity check required a window-imposed keyboard focus chain and an exactly honoured window size (see the two
window-manager notes in §9.2.3).

**No Windows verification is outstanding any more**, and the same run confirms the on-demand picking refresh (the picking
pre-warm of §9.2.7) on all four platforms. CI usage is still deliberately kept low: a verification should be a deliberate
push, not a debug loop.

**`--qml-session-check` and `--qml-parity-check` cover the closing behaviour of the shell.** The session check answers the
question about a modified session three ways (cancel keeps the window open and the changes, discard lets the close
proceed, save writes the file and clears the marker), closes the real window once to prove that its close event goes
through the same question, and opens the session again through its recent files entry and through the import path. Two
traps that cost a run each: a *closed* menu reports all of its items as `visible: false`, so a menu walk must not filter
by visibility (the shell's recent files submenu names its empty-list placeholder instead), and the items a `Repeater`
creates inside a `Menu` are not children of that menu - they are read through `QMetaObject::invokeMethod(repeater,
"itemAt", ...)`. The submenu entry itself is a `MenuItem` whose `subMenu` property points at the menu it opens; that
property is what tells a container apart from an action.

**A trap that cost a CI cycle:** `install-qt-action` exports `QT_ROOT_DIR` (the Qt prefix) and the plugin/QML paths, but
`Qt6_DIR` - which it sets for CMake - is **not** visible as an environment variable inside the steps. The Windows
configure step therefore passed an empty `CMAKE_PREFIX_PATH` (CMake still found Qt through the `qmake` on `PATH`) and the
smoke-test step aborted in `Split-Path ... $env:Qt6_DIR` with *Cannot bind argument to parameter 'Path' because it is
null*, before the spike was even started. Both Windows steps use `QT_ROOT_DIR` now (with an explicit check that the Qt
plugins directory exists), and the `${{ env.Qt6_DIR }}` uses in the Linux and macOS jobs are left as they are: they
expand to an empty string there as well, which is harmless because the action puts the Qt libraries on
`LD_LIBRARY_PATH`/`DYLD_LIBRARY_PATH` itself.

Prerequisites that the runner images do **not** provide and that the workflow installs, each of which fails the job loudly
if missing:

> **Find this list locally instead of one CI round trip per missing package.** Configure the same option set on the
development machine and read its package lines: `cmake -S . -B /tmp/cfgprobe -G Ninja -DOVITO_BUILD_APP=ON
-DOVITO_BUILD_QML_FRONTEND=ON -DCMAKE_PREFIX_PATH=<Qt> > /tmp/cfgprobe.log` then `grep -E "^-- Found |^-- Could NOT find"
/tmp/cfgprobe.log`. What is *required* here is required on every platform (only `XKB`/`OpenGL` are Linux-specific, and
`FFMPEG`/`WrapVulkanHeaders` are optional); what the runner lacks is exactly what its package manager does not install.
Three Windows dependency failures in a row (`Boost`, then `HDF5`, then `SQLite3`, then the zlib runtime library) came
from skipping this step.

* **Boost headers** on Windows: download and unpack the complete official headers
  (`boost_1_92_0.tar.gz`, 235 MB) and configure with `-DBOOST_ROOT=<dir> -DBoost_NO_BOOST_CMAKE=ON`. Neither the runner
  image nor a plain `vcpkg install boost-headers:x64-windows` provides them: that port installs only the headers that do
  not belong to a library-specific port, so the build fails later on `boost/algorithm/algorithm.hpp` (and would fail on
  `boost/version.hpp` as well). Installing the individual ports instead means ~16 of them, one per Boost library that
  OVITO includes. OVITO links only `Boost::boost` (header-only), so the headers are all that is needed.
* **HDF5 + NetCDF-C** on Windows (`vcpkg install "hdf5[hl]:x64-windows" "netcdf-c[core,netcdf-4]:x64-windows"` plus
  `-DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake`): the Particles plugin's `netcdf_integration` module
  needs them, and only `OVITO_REDISTRIBUTABLE_PACKAGE`/`OVITO_BUILD_PYPI` builds take them from the bundled submodules.
  Without them configure fails with `Could NOT find HDF5 (missing: HDF5_LIBRARIES HDF5_INCLUDE_DIRS HDF5_HL_LIBRARIES C HL)`.
  The Linux jobs get the equivalent from `libhdf5-dev`/`libnetcdf-dev`, the macOS job from `brew`, and the Windows runner
  image provides neither. Spell the port as `netcdf-c[core,netcdf-4]`: `netcdf-c[netcdf-4]` alone *adds* the feature to the
  default features, which pull `dap`/`nczarr` and build curl for remote access that OVITO does not use.
* **A matching zlib runtime library on Windows**: OVITO deploys the runtime DLLs of its dependencies into the build tree,
  and it used to assume that zlib's DLL is `bin/zlib.dll` next to `lib/zlib.lib` (the conda layout). A vcpkg tree names the
  import library `z.lib` (upstream zlib sets the shared target's `OUTPUT_NAME` to `z`) and the DLL `z.dll`, so configure
  failed with `Did not find any library files that match the file path .../bin/zlib.dll`. It now derives the DLL name from
  the import library `ZLIB::ZLIB` points at and prints the runtime libraries that are actually present when it finds none.
* **SQLite3** on Windows (`vcpkg install sqlite3:x64-windows`): the particles plugin links it, and configure fails with
  `Could NOT find SQLite3 (missing: SQLite3_INCLUDE_DIR SQLite3_LIBRARY)`. The Linux and macOS images ship it.
* **`dxc` on Windows**: `qsb` precompiles the HLSL shaders to DXIL, and OVITO turns a missing `dxc` into a `FATAL_ERROR`
  at configure time. The Windows SDK ships `dxc.exe` under `Windows Kits\10\bin\<version>\x64`; the workflow adds that
  directory to `GITHUB_PATH` and falls back to the `Microsoft.Direct3D.DXC` NuGet package.
* **lavapipe on Linux**: OVITO's offscreen picking `RenderThread` uses Vulkan on Linux and needs a usable ICD on a GPU-less
  runner (`mesa-vulkan-drivers`, `VK_DRIVER_FILES=/usr/share/vulkan/icd.d/lvp_icd.json`). This works because an offscreen
  instance needs no surface — the classic frontend, which presents to X11, cannot use lavapipe this way (section 3).

Note that a *compiler version* difference between a CI runner and the development machine can fail the build for reasons
unrelated to the frontend: the macOS job was failing on `static_assert(false, ...)` in `DataBuffer.h` because Apple clang 15
predates the C++23 resolution of CWG 2518. When a build works locally but fails in CI, compare the compiler versions first.

Another trap that cost one CI round trip: the Actions **`env` context only contains variables defined by the workflow or
written to `GITHUB_ENV`**, not the runner's machine-level environment variables. `${{ env.VCPKG_INSTALLATION_ROOT }}` or
`${{ env.LOCALAPPDATA }}` therefore expand to the empty string even though the variables exist in the shell; read them as
`$env:VCPKG_INSTALLATION_ROOT` in a `pwsh` step and export derived values through `GITHUB_ENV` (`$env:GITHUB_ENV`).
The vcpkg installation root is also *not* `VCPKG_ROOT` on the runner, which points at Visual Studio's copy.

The same trap hits `PATH`: a step that sets `env: PATH: <dir>;${{ env.PATH }}` does not append to the runner's search path
but replaces it with `<dir>` alone. On Windows that made every CTest run abort with exit code `0xc0000135`
(STATUS_DLL_NOT_FOUND) for the five tests that link `Core`, because neither `Core`'s dependency DLLs nor anything else
could be found; the header-only `tst_containers` still passed, which is the hint that the failure is about loading, not
about the test. Compose the variable in the shell instead
(`$env:PATH = "...;$env:VCPKG_INSTALLATION_ROOT\installed\x64-windows\bin;$env:PATH"`).

### 6.1 Build speed: what the workflow does, and what it measured

The Build step is 92-95% of every job's wall time: in run `36665850459` (all four jobs green) Linux x86_64 took 24m52s of
which 22m56s was building, Linux ARM64 27m52s/25m22s, macOS ARM64 30m57s/28m42s and Windows AMD64 19m57s/16m47s, while
CTest takes about a second, the Qt Quick smoke tests 3-25 seconds and the dependency installs (apt, brew, vcpkg with its
binary cache, Qt with the action's own cache) about a minute each. The matrix has since been reduced from four jobs to
three - the Linux ARM64 job was the most expensive per risk it covered and its scope is compile-tested on the x86_64 job
of the same architecture family - so the numbers to watch now are Linux x86_64, macOS ARM64 and Windows AMD64. Five
measures address that, and each was measured before it was wired into the workflow:

* **A compiler cache - with the precompiled-header trap fixed.** OVITO compiles nearly every translation unit with
  `-include-pch .../cmake_pch.hxx.pch`, and ccache refuses to cache such a call: `ccache --show-stats --verbose` counted
  **81% uncacheable** calls on this tree, 99% of them `Could not use precompiled header`, so the `ccache` launcher of the
  `native` preset was doing almost nothing. `CCACHE_SLOPPINESS=pch_defines,time_macros` plus `CCACHE_DEPEND=1` makes every
  OVITO call cacheable (511 of 511 in one full tree), and a cached translation unit then rebuilds in **0.15s** against
  **5.87s** for the same file compiled directly (`clang++ -O2 -g`, `ParticlesVis.cpp`). Recompiling **every** translation
  unit of the full product at `-j4` once the cache is warm - 858 compile calls and 39 link steps - takes **0.7s**, where
  the four cold cores need 22m56s; even a partly warm cache (15% hits at the time) already brought a full tree down to
  263s. The workflow sets those variables in
  its `env:` block, keeps the cache inside the workspace (`CCACHE_DIR`/`CCACHE_BASEDIR`) so `actions/cache` can save it,
  and hashes `CMakeLists.txt`, `CMakePresets.json`, `cmake/**` and the workflow itself into the cache key (with matching
  `restore-keys`, so a flag change does not start from zero). One cache per operating system and architecture is shared
  by every job of that platform: the compile commands of the libraries the Qt Quick frontend is built on are identical in
  both scopes, and ccache hashes the whole command line, so an entry another job wrote can only ever produce the correct
  object - a scope suffix in the key just blocked those hits. Windows uses ccache as well (installed with chocolatey): it
  is the only one of the two caches that can cache a translation unit which *consumes* an MSVC precompiled header
  (`sccache` reports "MSVC Precompiled header flags not supported"), and a command it cannot handle is simply not cached.
* **clang on the Linux x86_64 job.** Measured on the full tree (905 translation units, Release, compiler cache off):
  clang 18 needed 1459s of compile work and 143s of wall time at `-j16`, gcc 13 needed 2570s and 233s, and the twelve
  precompiled headers dropped from 170s to 90s - clang is much faster on the Qt and Boost template code. The single Linux
  job uses the preset `ci-linux` and builds the whole product. That means the compiler the release workflow ships (gcc
  with -O3) is no longer exercised by every CI run, only by `release.yml` - which is why that workflow can be started by
  hand (`workflow_dispatch`) before a release that touches compiler-sensitive code. The scoped frontend tree the macOS
  job builds costs 715s of compile work over 504 units with clang (170s of wall time at `-j16` with gcc before).
* **`Release`, but with `-O2` in the test jobs.** Nothing debugs a CI binary and `NDEBUG` compiles the assertions out
  either way, while debug info costs real time: a 22-unit sample of the largest translation units compiled in 52.6s with
  `-g` against 39.7s with `-g0` (about 25% per unit, 20-32% for the biggest ones), and a first-party target set of 211
  units 109.3s against 100.3s at `-j32`. The Linux test preset `ci-linux` therefore sets
  `CMAKE_CXX_FLAGS_RELEASE=-O2 -DNDEBUG`: nothing in a test job measures performance, so the `-O3` of a release build
  buys nothing there (`ci-windows` keeps the MSVC defaults, because an MSVC Release build is already `/O2` and replacing
  `CMAKE_CXX_FLAGS_RELEASE` would drop the `/MD` runtime flag that Qt requires, and `ci-macos` keeps the release flags
  for the reason in the scope bullet below). Measured on this box the compile cost of
  a full tree was 1691s at `-O2` against 1459s at `-O3` for clang, i.e. within the run-to-run spread of these numbers, so
  `-O2` is chosen for what it does not pretend to be, not for a measured compile-time win. The assert-enabled tree of
  section 4 stays a local verification, not a job.
* **A smaller scope for the frontend job.** The Qt Quick smoke test never touches the QtWidgets application, Qwt, the
  terminal widget or the analysis plugins, yet every job compiled them. The hidden preset `ci-frontend` (the base of
  `ci-macos` and of the local `native-frontend`) disables `OVITO_BUILD_APP` and the CrystalAnalysis, Correlation, VoroTop,
  Galamost and oxDNA plugins, which is **512 instead of 951 translation units**; `Mesh`, `Grid` and `Delaunay` have to stay
  enabled because `Particles` depends on them and `StdMod` on `Mesh`. Measured with gcc, Release, `-j16`: **170s** for the
  whole scoped tree cold, and the complete smoke check list passes in that configuration (exit 0, no failed check). The
  Linux x86_64 job keeps the full product scope, so the desktop frontend and every plugin stay covered there; the macOS
  job builds the whole product as well, at the release optimization level. Two observations of that platform's runs are
  worth knowing before narrowing it: the frontend scope *did* build and pass CTest on the runners, but the job then hung
  and was cancelled - and the cause was the check list, not the scope. **The macOS runners cannot run the frame-rate and
  hide/show probes**: with them the spike prints its `BOOTSTRAP` line and never reaches its first check (a real macOS host
  with a display runs them fine and produced the numbers in section 9.5). The shared `QML_SMOKE_CHECKS` variable therefore
  leaves those two probes out and the Linux and Windows jobs add them back, and because narrowing the scope as well would
  be two changes at once, the macOS job keeps building the whole product.
* **A path filter.** A change to Markdown cannot break a build, so `paths-ignore` skips `docs/**`, `**/*.md` and
  `graphify-out/**`. All three jobs run for every push to a monitored branch, for pull requests and for a manual
  dispatch; the feature-branch gating of the previous round went away with the fast frontend job it was built around,
  because a feature branch would otherwise have run no job at all. Trap for a repository that requires these checks
  before merging: a path-filtered workflow reports **no status at all**, so a docs-only pull request would wait forever;
  lift the filter then.
* **Every Qt Quick smoke step has a `timeout-minutes`.** The spike is a GUI program whose checks wait on frames and on a
  picking buffer, so a hung run is a failure mode the job has to bound itself: without the step timeout the first macOS
  hang occupied the runner until the workflow's own limit.

The four Qt Quick smoke steps share one check list, which the workflow keeps in the variable `QML_SMOKE_CHECKS` (the
Windows step splits it into an argument array), so a new check is added in one place instead of four.

Two things were measured and deliberately **not** taken:

* **The linker is not a bottleneck.** Relinking `Gui.so` (129 translation units) takes 0.10s with mold against 0.66s with
  bfd, so picking a faster linker in CI would save about a second per job.
* **`OVITO_USE_UNITY_BUILD=ON` does not work in this tree.** The measurement on the scoped frontend tree (clang,
  compiler cache off, 504 units in 84s and 715s of compile time) stopped after 68 units: merging the sources of one
  target makes file-scope symbols collide, first in sources OVITO does not own (`src/3rdparty/ptm/ptm_quat.cpp` redefines
  `generator_cubic`, `generator_diamond_cubic` and `generator_hcp`; an earlier attempt also hit libvterm's
  `utf8_seqlen`/`fill_utf8`) and then in its own meta-object macros, which emit one file-scope `__metadata_<line>`
  variable per `OVITO_CLASSINFO` - switching that to `__COUNTER__` removes the collision but is not enough on its own.
  Unity build would need a per-target `UNITY_BUILD OFF` for every third-party library plus whatever the first-party
  targets surface next, and it masks missing includes (see the precompiled-header note above), so this project does not
  take that lever; the compiler and the scope above are what reduce the work.

The configure presets `ci-linux` (clang, whole product), `ci-macos` (frontend scope), `ci-windows` (MSVC) and
`native-frontend` (the local Qt Quick iteration tree) plus the variable `OVITO_COMPILER_LAUNCHER` (empty or `ccache`) exist
so that a developer can reproduce a CI build exactly; `ci` and `ci-frontend` are the hidden bases they share, and the
`release` preset is what the release workflow's flags amount to:

```bash
# The Linux test job, as CI runs it (the compiler cache is optional but strongly recommended here):
export OVITO_COMPILER_LAUNCHER=ccache
export CCACHE_SLOPPINESS=pch_defines,time_macros CCACHE_DEPEND=1
cmake --preset ci-linux -DCMAKE_PREFIX_PATH=<Qt prefix>
cmake --build --preset ci-linux --parallel "$(nproc)"
```

**The precompiled headers are not optional.** `OVITO_USE_PRECOMPILED_HEADERS=OFF` does not build this tree: without the
header that the per-target precompiled header happens to provide, the translation units that include `TaskProgress.h` and
`Application.h` fail with `invalid use of incomplete type 'class Ovito::UserInterface'`. They are also worth their cost -
the twelve precompiled headers need 170s of the full tree's 2570s, and the units of the PCH-less run that did compile
averaged 3.8s against 2.99s with them - so a compiler cache has to be made to coexist with them (the `CCACHE_SLOPPINESS`
above) instead of switching them off. That is why the Windows job keeps them enabled and accepts that ccache cannot cache
their *generation* (one compile per target; the units that consume them are cached).

**`OVITO_USE_CCACHE` no longer overrides an explicit launcher.** Until this round the top-level `CMakeLists.txt` forced
ccache into every build whenever it found the program, so a preset that asked for `sccache` was silently ignored and no
build could be measured without a compiler cache at all. It applies now only when neither `CMAKE_C_COMPILER_LAUNCHER` nor
`CMAKE_CXX_COMPILER_LAUNCHER` is defined, which is what makes `OVITO_COMPILER_LAUNCHER` authoritative and an empty value
a usable "no cache" for measurements.

---

### 6.2 What only the three-platform run finds

A green Linux build says nothing about whether the automation module builds or its suites pass elsewhere: run `36817421518`
was green on Linux x86_64 while Windows AMD64 could not link `tst_python_data_bridge` at all and macOS ARM64 failed four
cases of `tst_session_descriptor`. Four traps are behind that, and each is a rule rather than an accident.

* **A nested type's out-of-line member is not exported from a DLL on Windows.** `PythonArrayBridge::Result::toString()` was
  declared in the header (inside a nested `struct Result`) and defined in the `.cpp`; because only the *enclosing*
  `OVITO_CORE_EXPORT PythonArrayBridge` carries the export macro, MSVC produced `Core.lib` without that symbol and the
  test failed with `LNK2019: unresolved external symbol`. GCC and clang link it anyway, so the build only breaks on
  Windows. Define such a member inline in the header (or export the nested type itself).
* **A UNIX socket path may be 104 characters on macOS, and a platform's temporary directory can be long enough to exceed
  that on its own.** On macOS `QDir::tempPath()` is `/var/folders/<2>/<30>/T/`, so a per-test scope below it plus a session
  ID crosses the limit and `AutomationSessionDescriptor::writeToDisk()` refuses the descriptor - correctly, because a
  client could not connect to the socket. Every case of `tst_session_descriptor` that *writes* a descriptor failed there
  while the others passed, which is the signature of this trap; the suite now creates its private scope under the shortest
  temp base the platform has (`/tmp` on UNIX) and pins the refusal deliberately in
  `an_endpoint_that_does_not_fit_a_socket_path_is_refused`. The same applies to a *spike check* that serves a session:
  `--qml-automation-check` built its private scope from `QDir::tempPath()`, and with macOS's own temp path plus the check's
  name plus a session ID the endpoint came to about 109 characters, so the workbench refused to serve and the check failed
  there; it now uses the short temp base too (`/tmp/ovito-qml-check-<pid>`, an endpoint of about 60 characters). Reproduce it
  on Linux without a Mac by pointing `TMPDIR` at a 60-character directory:
  `TMPDIR=/tmp/$(printf 'a%.0s' {1..60}) build-native/tests/cpp/core/automation/tst_session_descriptor` for the suite and
  `TMPDIR=/tmp/$(printf 'a%.0s' {1..60}) xvfb-run -a build-native/bin/ovito-qml-spike --qml-automation-check <file.xyz>`
  for the check (both then report the workbench or the descriptor refusing the endpoint).
* **The descriptor's endpoint is a path, not a string that starts with `/`.** On Windows the session directory is an
  absolute `C:/Users/...` path, so a validity rule written for UNIX refuses every session there and no workbench could ever
  serve one. `isUsableEndpoint()` therefore tests `QDir::isAbsolutePath()` and keeps refusing *relative* paths, which is the
  property that matters (a relative endpoint would resolve differently in the two processes).
* **Where the platform cannot answer, the suite asserts the documented rule.** `AutomationSessionDescriptor::isProcessAlive()`
  asks the operating system with `kill(pid, 0)` on UNIX and answers "alive" everywhere else, because deleting the descriptor
  of a live session is worse than trying to connect to a dead one and failing. A case that asserts "a dead PID is stale"
  is therefore a UNIX case; the suite branches on the platform and checks the rule in both directions.
* **A text-mode stdout writes `\r\n` on Windows.** The Python worker framed its JSON headers with
  `sys.stdout.write(... + "\n")`, which a Windows interpreter turns into a carriage return *and* a line feed. JSON tolerates
  the stray `\r`, so a tolerant client keeps working by luck, but the protocol promises one JSON object per line and the
  caller is entitled to trust the line ending. The worker writes its header lines through `sys.stdout.buffer` now
  (`_write_line()`), which is the same stream the raw payload uses and is never translated; every write holds the one
  output lock, so a header cannot be interleaved with an answer from the reader thread.
* **A Windows build puts the executable in the build directory, not in `bin/`.** `OVITO_RELATIVE_BINARY_DIRECTORY` is
  `bin` on Linux, `<name>.app/Contents/MacOS` on macOS and **`.`** on Windows (a Conda build is the exception), so
  `tst_automation_cli`'s search for the running `ovito` failed on the Windows runner although the executable was in the
  build root it walked through - and because the job sets `OVITO_REQUIRE_TEST_BINARY=1`, that was a `QFAIL` rather than
  the `QSKIP` a build without an application deserves. The walk now looks for `ovito`, `bin/ovito` and the bundle paths
  in every directory it visits.
* **A test can fail without its output reaching the log.** `tst_python_data_bridge` and `tst_session_descriptor` failed
  on the Windows runner with exit code 1 - an ordinary QtTest failure, not a crash - while `ctest --output-on-failure`
  and even a verbose rerun printed nothing at all for them, so the log said only `***Failed`. When that happens, ask the
  test for its own log instead of relying on the console: `tst_x.exe -o <file>,txt` writes the whole run (every `FAIL!`
  line and the totals) into the file, and the Windows job's diagnostic step does exactly that for each failed test. The
  missing output is worth knowing about on its own: a silent `***Failed` does not mean a crashed process.
* **A temporary path behind a symbolic link is not the path the process reports.** Asserting that a working directory
  changed has to compare *canonical* paths: `QDir::currentPath()` answers in the form the operating system resolved, so
  a directory created through a symbolic link never matches the string it was created from. On macOS this bites every
  time, because `QDir::tempPath()` is `/var/folders/...` and `/var` is a link to `/private/var` - `--qml-session-check`
  failed there with "a directory handed to the import path did not become the working directory
  (/private/var/folders/...)" although the directory had in fact been changed. Reproduce it on Linux by pointing `TMPDIR`
  at a symbolic link: a two-line Qt probe prints `/tmp/link/...` for the temp-derived path and `/tmp/real/...` for
  `QDir::currentPath()`, unequal as strings and equal as canonical paths.
* **A `pwsh` step's exit code is the exit code of its last statement.** The Windows Build step ended with an
  `if (Get-Command ccache ...) { ccache --show-stats }` report, so the failed `cmake --build` did not fail the step: the job
  reported a *successful* Build and the linker error reappeared 20 seconds later as an unexplained `Test (CTest)` failure
  with nine tests missing their executables. The step now exits with `$LASTEXITCODE` when it is non-zero; macOS and Linux
  use `bash -e`, whose last-command rule is safe.

---

## 7. Temporary benchmark instrumentation (never commit)

The classic frontend and the render thread expose no frame-time counters, and on macOS vsync makes "how many frames per
second" the only externally visible number. For the Phase 1 comparison a temporary patch was applied locally (and
reverted afterwards; the same patch is kept out of the repository):

```diff
--- a/src/ovito/core/rendering/RenderThread.cpp
+++ b/src/ovito/core/rendering/RenderThread.cpp
@@
                 Q_EMIT vpWin->frameCompleted();
+                // TEMPORARY BENCHMARK INSTRUMENTATION - do not commit.
+                if(qEnvironmentVariableIsSet("OVITO_BENCHMARK_FRAMES"))
+                    vpWin->requestRerender(false);          // keep frames coming, like animation playback
@@
     rhi()->endFrame(state.swapChain.get(), endFrameFlags);
+    // TEMPORARY BENCHMARK INSTRUMENTATION - do not commit.
+    if(qEnvironmentVariableIsSet("OVITO_BENCHMARK_FRAMES")) {
+        static int benchmarkFrameCount = 0;
+        static QElapsedTimer benchmarkTimer;
+        if(benchmarkFrameCount == 0)
+            benchmarkTimer.start();
+        if(benchmarkFrameCount == 0) {
+            if(OORef<ViewportWindow> vpWin = state.viewportWindow.lock()) {
+                Scene* scene = vpWin->viewport() ? vpWin->viewport()->scene() : nullptr;
+                qInfo() << "CLASSIC_SURFACE" << state.swapChain->currentPixelSize()
+                        << "pipelines" << (scene ? scene->children().size() : -1);
+            }
+        }
+        benchmarkFrameCount++;
+        if(benchmarkFrameCount % 200 == 0) { /* print frames, elapsed time, fps */ }
+    }
```

```diff
--- a/src/ovito/gui/desktop/mainwin/MainWindowUI.cpp
+++ b/src/ovito/gui/desktop/mainwin/MainWindowUI.cpp
@@
-    for(const auto& item : urlImporters) {
+    // TEMPORARY BENCHMARK INSTRUMENTATION - do not commit: skip the interactive import dialog.
+    for(const auto& item : urlImporters) if(!qEnvironmentVariableIsSet("OVITO_BENCHMARK_FRAMES")) {
```

Run it with `OVITO_BENCHMARK_FRAMES=1`. The `pipelines` field of the `CLASSIC_SURFACE` line is the guard against the
silent-empty-scene trap of §3.1: it must be ≥ 1.

---

## 8. Measuring frame rates of the Qt Quick frontend

* The spike drives its own frame loop: `--qml-frame-stats <ms>` counts `QQuickWindow::frameSwapped` signals while asking
  OVITO for a new frame graph on every presented frame (like animation playback does). The requests are issued through a
  queued connection because `frameSwapped` is emitted on the render thread while `ViewportWindow::requestRerender()` is a
  GUI-thread operation.
* Everything below 59 fps is *vsync-limited* on this hardware, so a comparison must always state whether vsync was on.
  `QSG_NO_VSYNC=1` removes the cap on Linux (68 → 131 fps for a 512-atom scene in the `basic` render loop).
* **A 60 Hz display can hide the whole result.** On the first macOS host (Retina) every data set measured 57.7–59.0 fps
  regardless of the particle count, which says nothing more than "vsync holds". `QSG_NO_VSYNC=1` there made the QML run
  *twice as slow* (16.95 → 35.71 ms, reproduced twice) — that anomaly was never explained, and on the second host the same
  variable has the expected effect (102 → 116 fps). Always check whether the measured numbers are pinned to the display
  refresh, and prefer a host or a data set where they are not.
* A single data point is easy to misread: on the M4 the frame time of a four-viewport scene is flat (8.9–10.9 ms from 512 to
  32768 atoms) and *independent of the window size* (8.87 ms at 1920×940 versus 9.80 ms at 1280×800 at the same data set),
  i.e. the per-frame fixed cost dominates. Vary both the data set and the window size before drawing a conclusion about
  scaling.
* The Qt Quick render loop mode matters more than the graphics backend: for the same scene the `basic` (single-threaded)
  loop was ~2× faster than the default (threaded) loop (7.6 ms vs 15.3 ms per frame with four viewports, 512 atoms).
* `--qml-window-size WxH` sets the window size before anything is measured; use it when comparing against the classic
  frontend, whose rendered surface size is printed by the instrumentation above.

---

## 9. QML viewport (Qt Quick frontend) specifics

* **Bundle rpath on macOS**: an app-bundle executable resolves plugins through `@executable_path/../PlugIns/`, *not* through
  `@executable_path/../${OVITO_RELATIVE_PLUGINS_DIRECTORY}` — that variable is bundle-*root* relative (it already contains
  `Ovito.app/Contents/PlugIns`), so adding another `../` yields `Contents/Contents/PlugIns` and dyld fails with
  `Library not loaded: @rpath/GuiQml.so`. `src/main/CMakeLists.txt` uses the literal form; the spike target now does too.
  Verify without a GUI: `otool -l <binary> | grep -A2 LC_RPATH` and a run that must get past plugin loading.
* Version-skew trap when a test host gets source files copied by hand: the executable and the `GuiQml` plugin must be
  built from the same revision, otherwise option parsing or QML type registration silently mismatches (`Error: Unknown
  option qml-frame-stats` was the observable symptom). Copy the *source file* and rebuild, never a stale binary.
* Assertion/lifecycle checks are only meaningful in an assert-enabled build (§4); a clean run of an `NDEBUG` build proves
  nothing about object lifetimes.
* **Probe positions must be inside the viewport item**: at `QT_SCALE_FACTOR=2` (or with a small window) the first
  viewport item can shrink to a few hundred logical pixels, and a probe at `300,300` then measures `0 of 0 positions`
  and looks like a picking failure. The spike clamps its grid into the item and prints the position it used plus the item
  size, so a probe taken before and after a resize is comparable.
* **Never destroy a viewport item from inside the code that creates it.** `QmlViewportController::createViewportItem()`
  is called from QML while the scene rebuilds its pane delegates, and the pane that asked for the item may be destroyed
  a moment later. Replacing an item with `delete` there crashed the process while the item was handling a mouse event;
  both replacement paths now use `deleteLater()`, and every pane delegate owns its own item. Corollary for tests: after
  a click or a drag that changes the layout, look up the viewport item again instead of reusing a pointer captured before
  the interaction — the spike did that and crashed in the same way.
* **Two probes at the same position must agree.** The picking buffer is rendered asynchronously, so a probe taken right
  after a change answers from the previous buffer; if the probes before and after the picking pass disagree (19/25 versus
  13/25 was the symptom), that is a defect in the buffer bookkeeping, not a measurement artefact - it is how the missing
  buffer-size check of `QuickViewportWindow::pick()` was found.

## 9.1 Testing the import path of the QML shell

The shell imports data through `WorkbenchUI::importFiles()` (the same code the classic frontend uses), so the spike can
verify the whole path without a file dialog: `--qml-import-check` writes its own data files, drives the shell's import
command and checks the states.

Traps and facts that cost time here, in order:

* **An import is not a long operation.** `FileImporter::importFileSet()` sets up a `FileSource` and returns; the data is
  loaded later, when the pipeline is evaluated for the first time - which the viewports trigger. Consequences:
  * The number of frames of a multi-file trajectory is `0` immediately after the import and becomes correct after the
    first frame is rendered, so a check has to **poll** for it (the spike waits up to 20 s). The animation interval
    (0..2 for three files) follows at the same moment.
  * Cancelling an import of a plain XYZ file has nothing to cancel, because the operation is over in about a
    millisecond. The operation can only be cancelled while it is really running, so the spike uses a **large VASP
    POSCAR** file: `POSCARImporter::setupPipeline()` evaluates the pipeline while the file is being imported (only in
    interactive mode), which is the one in-tree format that loads the data inside the import call. The XYZ importer
    does not.
  * A cancel request needs a running Qt event loop. A `QTimer` only fires while the import is inside a nested loop
    (the shell keeps processing events while it waits); during the format detection of a long list of files the main
    thread never returns to the event loop, so a cancel requested then is only noticed when the loop is entered again.
  * **The cancellation has to win a race against the import, and the window is narrow.** The spike cancels as soon as
    the task progress model reports the running import and at the latest 200 ms into it; the 512k-atom POSCAR is still
    being read then on every machine measured (about 0.5 s here), but a *later* cancel arrives after the import has
    finished, and the check then fails for the right reason (nothing was cancelled, the scene keeps the pipeline). A
    larger file is the knob to turn. Whether the model reported the import *at that moment* is deliberately only logged:
    the shell refreshes the model at most every 100 ms and an importer starts reporting when it chooses, which made the
    check fail on the slower CI runners although the import had been cancelled correctly there - the per-task display is
    asserted where it is deterministic (the parity check drives the model itself, §9.2.3).
* **`QQuickWindow::grabWindow()` is unusable with the `QQuickRhiItem` viewports** (§2.2 and §9): the shell is
  screenshotted from the X server.
* **QML components must not share their name with a C++ type registered in the same module.** `WorkbenchWindow.qml`
  imports `Ovito.Qml`, and registering the pane model object as `ViewportPane` while a `ViewportPane.qml` also existed
  failed with `Panes are created by the viewport layout model.` - the module type shadowed the local component. The
  views of the shell's model objects are therefore named `WorkbenchPane.qml`, `WorkbenchSplitter.qml`,
  `WorkbenchStatusBar.qml` and `WorkbenchMessageBox.qml`, and the model types keep the names of their C++ classes
  (`ViewportPane`, `ViewportSplitter`, `ViewportLayout`, `ViewportController`, `WorkbenchController`).
* **Two parity checks are sensitive to the window manager, and the CI runners differ from a desktop.** The keyboard
  focus order can only be read from `QQuickWindow::contentItem()->nextItemInFocusChain()` while the window *has* the
  keyboard focus: the headless macOS runner never gives it to the window, the walk then returns a single stop, and the
  check used to fail there although the shell was fine. It now always verifies the structural precondition (the import
  control and the viewports are tabbable items) and asserts the order itself only when the chain can be walked. Window
  sizes are clamped as well (the same runner turned a requested 1100x700 window into 1100x656), so the window-state check
  asks for a modest 900x600, compares the store against the size the window *actually* became, and asserts the restore
  direction exactly only when the platform honoured that size while writing it.
* **A check that resizes the workbench has to shrink it.** The offscreen check resizes the window while a picking pass
  is in flight, so that the picking service has to replace its offscreen target. Its first version *grew* the window by
  40x30, which a window manager refuses when the window already fills the screen (the CI runners have small virtual
  displays): the size never changed, the guard that waits for the new layout never became true and the Windows job failed
  with *no object was picked after the resize had superseded the picking target* while picking worked perfectly. The
  check shrinks the window now and, should a platform refuse even that, reports that the superseded target was not
  exercised instead of failing - what it then verifies is that picking works, which the log states explicitly.
* **The message dialog blocks the caller.** `showMessageBox()` runs a nested event loop while the dialog is open (like
  the desktop frontend's modal `QMessageBox::exec()`), so an automated run that triggers an error has to answer it -
  the spike polls for `messageBoxVisible` and calls `answerMessageBox(Ok)`, which also verifies the dialog.
* **Qt Quick Controls needs its style set before the QML scene is created.** The shell sets `Basic` in
  `QmlFrontend::createWorkbench()` and colors the controls from `Theme.qml`; the styles of the platform are deliberately
  not used, so the workbench looks the same on every platform.

## 9.2 Testing the shared workbench state

The workbench state that both frontends display (audit decision D27) lives in `gui/base`: `RecentFilesList` holds the
recently opened sessions and imports, and `TaskProgressModel` lists the running tasks. `--qml-import-check` verifies the
progress model with a real operation, because the model is only interesting while something runs:

* it imports a POSCAR of 140^3 atoms, i.e. one that takes longer to read than the 100 ms after which the workbench
  starts displaying progress, and checks from a timer that the model reports the import (`Reading VASP file ...`);
* it compares the model with what the QML scene sees through the `taskProgress` context property (busy, count, text), so
  an unbound model or a broken binding fails the run instead of showing up as an empty status line;
* it waits for the model to become idle again after the canceled import, which catches a progress record that stayed
  behind (a leaked record would keep the status line busy forever).

Traps:

* **A progress record is registered with the `UserInterface` of its task.** A large file imported from the command line
  or by a file source is *scanned and read* later, when the pipeline is evaluated - and the task that evaluates it has no
  user interface, so its `TaskProgress` record never reaches the workbench and neither frontend's status bar shows it.
  The progress of an operation that the frontend starts (an import, an export) does show up. The check therefore drives
  the import through the shell's own command, not by evaluating a pipeline.
* **The workbench refreshes the model at most every 100 ms** (`WorkbenchUI::notifyProgressTasksChanged`), deliberately:
  short operations must not flash a progress bar. Consequences for tests: an operation that is over in less than 100 ms
  must not be expected in the model (the check only requires it when the operation took longer than 150 ms), and a row
  can still be there right after an operation returned - the check waits for it to disappear rather than reading it
  immediately.

### 9.2.1 Testing the session workflow

`--qml-session-check` verifies the session workflow of `WorkbenchUI` (audit decisions D28, D64-D66), which both frontends
use: the scene is saved to a `.ovito` file (`saveSessionFile()` writes it, the workbench remembers the path, the session
counts as unmodified), a change marks the session as modified (`isSessionModified()`), saving again through `saveSession()`
clears that without asking for a file name, loading the file back with `loadSessionFile()` replaces the current scene by
the saved one, the file becomes the most recently opened file, and the file dialog of the frontend decides where a session
without a file name goes:

1. saving, modifying and reloading a session, the modified marker in the window title and the recent-files entry;
2. the file question of the frontend: a *cancelled* dialog leaves the session without a file and writes nothing, an
   *answered* one writes the session where it points and the session adopts that file, and the `FileSaveAs` command asks
   again although the session has a file of its own - and the check asserts that the shell *presented* the dialog it is
   waiting on (`QmlWorkbenchController::fileDialogPresented()`), not only that it reported the request;
3. a save that cannot be written (the parent directory of the target does not exist): no file is written, the session
   keeps its file and its modified state, and the frontend reports the reason in the status line and in an error dialog;
4. `openSession()` through the same dialog, which loads the file it answered with;
5. a directory handed to the shared import path, which becomes the working directory of the process and opens the import
   dialog there (D66);
6. the question about unsaved changes when the workbench is closed (`canCloseWorkbench()`), answered three ways:
   cancelling keeps the window open and the changes, discarding lets it close, saving writes the session and clears the
   marker - through the window's own close event as well as through the command;
7. the recent-files entry and a `.ovito` file handed to the import path, which both restore the saved session.

Traps:

* **The scene has to contain something.** The check runs before the import check (which ends with a canceled import and an
  empty scene) and imports a small lattice of its own if the scene is empty - `loadSessionFile()` of an empty scene would
  otherwise pass every assertion while verifying nothing.
* **A loaded session replaces the data set**, so every pointer into the scene (the scene node, its children) is invalid
  afterwards and has to be looked up again, and so is the `DataSet` itself: writing the session file path of a session the
  check loaded has to go through `datasetContainer().currentSet()`, not through the data set it started with.
* **The file dialog is answered from an event loop turn.** The shared workflow blocks in the dialog, so the answer has to
  arrive from a timer (`answerNextFileDialog()`), exactly like the answer to the question about unsaved changes
  (`answerNextMessageBox()`). The spike application sets `Qt::AA_DontUseNativeDialogs`, so the check drives Qt's own
  dialog implementation; the native dialog of macOS and Windows is what a *user* sees and is not covered by a check.
* **The check writes user settings**: the recently opened files are persisted in `QSettings` under `file/mru`, like the
  recent-files menu of the classic frontend does, and the session file directory is remembered.

### 9.2.2 Testing the shared settings facade

`--qml-settings-check` verifies `gui/base/app/GuiSettings` (audit decision D30), the one place that owns which settings
the workbenches persist and with which default:

* the QML scene reaches the facade (`guiSettings.usingDarkTheme`) and the shell's theme resolves to the *same* value
  (`workbench.darkTheme`) - that is what proves the palette follows the shared rule instead of the shell deciding for
  itself;
* every accessor round-trips: the window rectangle and the maximized flag of a frontend without widgets, the opaque
  `QWidget` blobs of the classic main window, the directory-history flag, the preferred file dialog, the session-file
  directory, the per-dialog-class directory history and the flags the first start sets (GPU adapter confirmation, and the
  multi-file import mode in a professional build).

Traps:

* **The check writes to the settings store**, so it restores every value it changes. One entry stays behind: the directory
  history of a file-dialog class that exists only for this check (`spike-settings-check`). Run the spike with an isolated
  `XDG_CONFIG_HOME` (section 2.2) to keep the store untouched. Because the setters treat an empty value as "nothing
  stored" and remove the key, a check run adds no empty entries to a fresh settings file.
* **Following the system color scheme is compulsory on Linux and macOS**, so the stored flag cannot be flipped there and
  the check only asserts that it is on; its round trip runs on Windows and in the CI jobs.
* **The color scheme decides the palette of the shell.** Where the platform reports no scheme at all
  (`Qt::ColorScheme::Unknown`, which is what Xvfb reports) both frontends are *light*. Screenshots taken before Phase 2.5
  therefore show a dark shell and newer ones a light one; the viewport pixels are unaffected, the surrounding chrome is
  not - compare captures across that change with care.

## 9.3 Testing the shared command layer

The commands of the workbench live in `gui/base` and are the objects both frontends use (`ovito::Command`, audit
decision D26): the classic frontend displays each one through a `QAction` that mirrors it, the QML frontend binds
shortcuts and buttons to the command. `--qml-command-check` verifies the layer from inside a QML process, which is the
part that cannot be checked from the classic frontend:

* the commands exist and their `QAction` view mirrors text, enabled and checked state (this is what the classic menus
  display, so a one-way sync defect breaks the *classic* frontend);
* the QML engine sees the same objects - the check evaluates `workbench.undoCommand.text`,
  `commandManager.commandList.length` and `commandManager.triggerCommand('ViewportMaximize')` through
  `QQmlExpression`, so the context property, the registered type and the invokable all have to work;
* the state rules drive the commands: a transaction enables Undo (whose title carries the operation name), triggering
  it reverts the edit and enables Redo; the playback command starts and stops the animation; the viewport-mode commands
  activate a mode (an exclusive mode such as `SelectionMode` cannot be switched off again, and stays checked);
  maximizing toggles the maximized viewport.

Traps:

* **A command that the shell names but the frontend does not provide fails silently.** `ViewportMaximize` exists in both
  frontends, but the pipeline commands (`PipelineDelete`, ...) are created by the desktop command panel, so the QML
  process has fewer commands than the classic one (43 of the 76 action ids today). `WorkbenchWindow.qml` warns once at
  startup for a command it expects and does not get, instead of leaving a shortcut dead.
* **`Q_INVOKABLE` alone is not a QML property.** `commandManager.commandList.length` returned 0 until `commandList()`
  was also declared as a `Q_PROPERTY`; an invokable is callable but not readable as a property of an object.
* **`enabled` needs a NOTIFY signal.** The QML bindings (`Shortcut.enabled`) only follow the command state if
  `Command::changed()` is emitted for every property, which is why the command has a single `changed()` signal that
  serves as the NOTIFY of all of its properties.
* **The playback command cannot be probed in a static scene.** Checking the "Play Animation" command only starts the
  playback if the animation interval holds more than one frame (`SceneAnimationPlayback::startAnimationPlayback` refuses
  a single-frame scene), so the check imports a small trajectory of its own when the workbench shows a static structure -
  otherwise a healthy command is reported as broken just because the test data has one frame.
* **The check needs a scene object to rename.** Its transaction edit renames the current scene node, so a run without a
  data file - `ovito-qml-spike --qml-command-check` with the empty default scene - fails with "the scene has no object to
  rename". Pass a data file (the CI smoke test does) or run the other checks without this one.

### 9.4 Frame-time measurements of the Qt Quick viewport

The frame time of the QML frontend is only comparable when the display's refresh rate is out of the picture:

* `QSG_NO_VSYNC=1` removes the refresh-rate cap of the Qt Quick render loop. Without it every configuration that can keep up
  reports the same number (60 Hz on a 60 Hz display), which hides both regressions and improvements.
* `QSG_RENDER_LOOP=basic` removes the scene graph's own threading and is the loop in which a change of the rendering path
  shows up most clearly; measure both loops, because a change can affect them differently.
* Measure medians of at least three runs per configuration and per dataset size, and keep the window size and the dataset
  identical. The interesting sizes are a small scene (a few hundred atoms - the frame is dominated by the fixed cost of the
  four viewports) and a large one (tens of thousands of atoms - the frame is dominated by the geometry).
* A headless Linux recipe: `xvfb-run -a --server-args="-screen 0 1400x900x24"` with `QT_QPA_PLATFORM=xcb`,
  `QSG_NO_VSYNC=1` and the spike's `--qml-frame-stats 2000`. Both loops and both dataset sizes are needed to see which part
  of the frame a change moved; the numbers of the per-item versus per-window renderer service are in
  [UI_PLAN.md](UI_PLAN.md) deliverable 7.

**Baseline of the four-viewport QML frontend** (`QSG_NO_VSYNC=1`, 1280x800, four viewports, medians of three runs, the
datasets of section 9.1):

| Scene | threaded loop | `QSG_RENDER_LOOP=basic` |
|---|---|---|
| 512 atoms | 117.0 fps (8.55 ms) | 173.5 fps (5.76 ms) |
| 32768 atoms | 27.0 fps (37.04 ms) | 42.0 fps (23.81 ms) |

Two isolate-experiments are worth repeating before optimizing the viewport, because they say where the frame time actually
goes (both need temporary instrumentation, which is reverted afterwards):

* **Per-item cost** - maximize one pane (so that one of the four items renders and the other three are invisible, with the
  same total pane area) and measure again: 512 atoms goes from 8.55 ms to **6.99 ms**, 32768 atoms from 37.04 ms to
  **11.63 ms**. The large change is *the rasterization of three panes that are no longer drawn* - it is not something a
  single canvas item could save, because such an item would still render four panes. The fixed per-item share is bounded by
  the small-scene pair: (8.55 - 6.99) / 3 = 0.52 ms per item, i.e. ~1.6 ms per frame, and on the `basic` loop the same
  comparison shows no difference at all (5.76 ms versus 5.92 ms). The conclusion recorded in the plan is therefore "keep the
  four items" and "optimize the panes, not the items".
* **Frame-graph generation** - temporary timers around `ViewportWindow::generateFrameGraph()` and the hand-off:
  0.29 ms per frame graph (1.16 ms per frame, 14%) at 512 atoms and 1.05 ms per graph (4.19 ms, 11%) at 32768 atoms, with
  the render pass costing 0.05 ms of CPU time per pane and the hand-off nothing measurable. Generation is a tenth of the
  frame; the rest is the scene graph and the rasterization of four panes.
* **Cross-session noise** is real: the 32768-atom threaded figure moved between 23.5 fps and 27.0 fps across sessions on
  this box, so only differences larger than about 10% (or medians of three with a matching control run) are worth acting on.

### 9.2.3 Testing the shell's parity surfaces

`--qml-parity-check` verifies the surfaces the shell gained for parity with the classic frontend: the menu bar (every
entry either presents a command of the shared command layer or is disabled and names the phase that delivers it), the
viewport context menu (opened with a right-click on the title label of a pane, with all four delivered actions driven and
restored), the About dialog of the shared About command, one status-line row per running task, the remembered window
state, and the report about the last import.

Traps that cost time, in the order they were met:

1. **Clicking a viewport caption**: `ViewportWindow::contextMenuArea()` is a rectangle of the *rendered frame graph*, i.e.
   in device pixels, while a `QQuickItem` receives mouse events in device-independent coordinates - divide by
   `window()->devicePixelRatio()`. The check waits (up to 5 s) for an item whose caption area is non-empty, because the
   area is only known after the viewport has rendered a frame graph, and because the viewport items are rebuilt when the
   data set or the layout changes (so never hold an item pointer across steps, and never wait for one either).
2. **Creating a progress record from a timer**: `TaskProgress`'s constructor and its setters call
   `this_task::throwIfCanceled()`, which dereferences a null task outside a task scope. A check that fabricates a second
   running task for the one-row-per-task case must open a `GuiTaskScope` first (audit open item O8).
3. **Reading the frame count of an import**: the number of source frames is discovered when the file source is evaluated,
   which is *after* `FileSourceImporter::importFileSet()` returned. The continuation of `FileSource::requestFrameList()`
   must be kept alive (hold the returned future in a member - a future nobody awaits cancels the continuation it carries),
   and the `FileSource` has to be taken from `Pipeline::source()`, not by casting the `Pipeline` itself (which silently
   makes the branch unreachable).
4. **Asserting on a status message**: the shared `BaseViewportWindow::leaveEvent()` calls
   `UserInterface::clearStatusBarMessage()`, so anything reported only through `statusMessage` disappears as soon as the
   mouse leaves a viewport. The report about the last import therefore lives in a separate, persistent `notice`,
   and the check asserts on that.
5. **Menu items**: `QQuickMenuItem` toggles its own `checked` state when clicked and thereby destroys a binding, so every
   checkable entry re-syncs from its model. The walk reads QML-declared properties (`command`, `ownerPhase`) through
   `QQmlProperty`, because `QObject::property()` cannot see properties that only exist in QML; it recognizes entries by
   `inherits("QQuickMenuItem")` and skips `QQuickMenuSeparator`.
6. **Expected noise at the end of a run**: after `VERIFICATION_DONE`, `Theme.qml` logs
   `TypeError: Cannot read property 'usingDarkTheme' of null` once per theme instance. The QML context property is being
   torn down while the bindings re-evaluate; this is not a defect, and the QML errors of a run have to be judged before
   that line.

The evidence screenshot of this round was taken with the recipe of section 2.2 (private `Xvfb` plus `ffmpeg -f x11grab`)
from the *product* frontend (`ovito --gui=qml <file>`, which shows the menu bar and the persistent import notice) and is
stored as `docs/design/evidence/phase25_parity_shell.png`.

### 9.2.4 Testing the shared library models and the graphics-device report

`--qml-library-check` constructs the two library models of the classic frontend (`PipelineListModel`,
`AvailableModifiersModel`, `AvailableOverlaysModel`) inside a `GuiTaskScope` and verifies every row: it exposes a command
of the shared layer (`CommandRole`), that command is registered in the `ActionManager` under the id of the row's object
name, and `ActionRole` returns exactly the `QAction` view of that command. It also compares the row flags with the enabled
state of the command. It runs before the import check, because its models register commands that stay registered for the
rest of the run.

`--qml-device-check` compares the frontend's own answer (`QmlMainWindowUI::hasGraphicsDevice()`, filled from
`QQuickWindow::rhi()`) with the scene graph of the window and fails if they disagree; when no graphics device exists, the
workbench notice must name the platform plugin. The acceptance run is the one the CI jobs use for the two Linux runners:

```bash
QT_QPA_PLATFORM=offscreen LD_LIBRARY_PATH="$QT_LIB:$PWD/build/lib/ovito/plugins" \
  ./build/bin/ovito-qml-spike --qml-startup-delay 3000 --qml-device-check   # DEVICE_TEST ... missing/missing, exit 0
```

Traps of this round:

1. **A frontend without the desktop's snippet commands**: `PipelineListModel` resolved the two modifier-snippet commands
   with the asserting `getCommand()` and used the result unconditionally, so constructing it in the QML frontend either
   tripped the assertion (assert build) or dereferenced null (release). Look up shared commands that a frontend may not
   own with `findCommand()` and guard the use (defect F19; the same pattern as defect F17).
2. **A library entry can switch on a mode the frontend lacks**: inserting a viewport layer runs
   `viewport->setRenderPreviewMode(true)`, which the Qt Quick viewport does not implement yet, so a check must not trigger
   a viewport-layer entry expecting an undoable, harmless edit. Verify the row-to-command wiring instead, and cover the
   trigger path once preview mode exists (Phase 5).
3. **A missing `continuation()` hangs the harness, not the check**: a step that returns without calling its continuation
   leaves the step runner waiting until the process timeout (`EXIT=124`) and prints no `VERIFICATION_DONE`, which looks
   like a crash of the feature under test. Check the step's exit path first.

### 9.2.5 Testing the icons of the shared icon set

`--qml-icon-check` verifies the icon path end to end: `IconTheme::image()` resolves the icons the shell and its commands
name (both in the current and in the other icon theme), the QML singleton `Icons` reports the same theme name as the C++
layer and builds URLs of the `ovito-icon` image provider, every QML `Image` of the shell whose source comes from that
provider has reached `Image.Ready`, the four pane buttons show the maximize icon, and maximizing a viewport switches the
button of that pane to the new `viewport_restore.svg` (the layout is put back afterwards).

Traps of this round:

1. **A new verification option that is not in the interactive-mode guard never runs.** The prototype keeps the window open
   when no verification option is given, and it decides that from a hand-written list of options
   (`Main.cpp`, "Interactive mode: without a verification option, keep the window open"). A new option that is missing
   from that list makes the run hang until the timeout without printing anything, which looks like a broken feature rather
   than a broken test. Add the option in both places.
2. **Reading QML items without Qt Quick's private headers.** The check inspects the `Image` items of the shell through the
   meta object (`child->inherits("QQuickImage")` plus `property("source")`/`property("status")`), because the private
   header `QtQuick/private/qquickimage_p.h` needs `Qt6::QuickPrivate`, which this target does not link.
3. **The icon theme is process-global.** `IconTheme::apply()` changes what every icon lookup returns, so a check that
   switches the theme for its own purposes has to put the previous one back (it reads it from
   `GuiSettings::instance().usingDarkTheme()`), or the rest of the run renders with the wrong icons.

### 9.2.6 Testing the shared offscreen rendering service

`--qml-offscreen-check` covers `core/rendering/OffscreenRenderTarget` (audit decision D34), the one owner of offscreen
rendering for both frontends. It first proves the *kind* contract - a `renderPicking()` submitted to a `Kind::Visual`
target and a `renderImage()` submitted to a `Kind::PickingOnly` target are refused (in a build with active assertions they
abort, which is the point of the check, so the case is skipped there) - and then drives the three offscreen paths the Qt
Quick process can reach **at the same time**:

1. the ambient-occlusion sampling, started by triggering the library command whose id ends in `AmbientOcclusionModifier`
   on a pipeline that the check selected through `PipelineListModel`,
2. a picking pass whose result the check deliberately does not await, and
3. a render output (`RenderSettings::render()`), whose read-back image is compared scan line by scan line against a
   baseline until the ambient-occlusion shading appears.

While the sampling, the picking and the render output are in flight the check resizes the workbench window, which
supersedes the picking target; afterwards a fresh pick has to succeed, which is the teardown/deadlock part of the
acceptance. The desktop-only viewport grab (`WidgetViewportWindow::grabViewportImage()`) shares `renderImage()` with the
render-output path and is not exercised separately.

Traps of this round:

1. **A Qt callback has no task context of its own.** `Task::waitFor()` and the coroutine machinery read `this_task`, and
   a timer/event callback runs without one, so waiting for a future or starting a render output from a poll callback
   crashes in ways that have nothing to do with the feature (a null dereference in `this_task::ui()` or in
   `Task::waitFor()`). Every callback that touches OVITO opens its own `GuiTaskScope` - the harness callbacks do this as
   their first statement, and the poll helper calls them outside the scope of the function that started the poll.
2. **Only the main thread may allocate an offscreen target**, because allocation creates the shared render thread and its
   graphics device (`UserInterface::renderThread()` asserts main-thread execution). The ambient-occlusion sampling
   submits its passes from a worker thread, so its modifier calls `OffscreenRenderTarget::prepare()` while it is still on
   the main thread. This is the one defect of the round that the *assert-enabled* build found and the release build hid.
3. **The scene a later check inherits may hold no particles.** The parity check imports a file that OVITO's autodetection
   hands to the LAMMPS importer, which parses it into an empty scene, and ambient-occlusion shading has nothing to recolor
   there. The check therefore imports a lattice of its own and waits until the file source has evaluated it, instead of
   assuming the startup dataset of the run.
4. **A `PipelineListModel` or library model can exist only once per process.** Their constructors register commands with
   fixed ids, and `ActionManager::addCommand()` refuses a duplicate id (in the assert-enabled build it aborts with
   "There is already a command with the same ID"). The spike creates them once in `workbenchModels()` and shares them
   between checks; a frontend is in the same position.
5. **Selecting the node that is already selected is not a selection change.** The model adopts the scene selection through
   `DataSetContainer::selectionChangeComplete`, which is emitted only for a real change, so a check that wants the model
   to adopt a node has to clear the selection first.

### 9.2.7 Testing the picking pre-warm and the frontend selection

`--qml-prewarm-check` verifies that the picking buffer of a viewport catches up **on its own** after the view changed:
it waits for the buffer of the imported scene (which has to arrive without any pick being made), moves the camera of the
viewport and requests a repaint, waits for the buffer to notice the change and to become current again - still without a
pick - and then asserts that the first pick after the camera move names the object the second one names. It finishes by
counting the frames of the settled window for one second: a pre-warm that refreshed the buffer over and over would look
like a continuous renderer here (the check fails above four frames, which covers the tail of the passes of the other
panes). Disabling the pre-warm (`pickingPrewarmTimeout()` as a no-op) makes the check fail with *"the picking buffer did
not catch up with the imported scene within 20 s"*, which is how it was shown to have teeth.

**A release-only corruption hides from every sanitizer** - defect F20 was this case, and it cost the most time of the
whole frontend work. A picking buffer that survived the render thread which had produced it crashed release builds in 4
of 5 runs of the scene-replacing `--qml-session-check`, while valgrind, the assert-enabled build and even an
AddressSanitizer build ran the same scenario without a diagnostic until the timing was right (the sanitizer needed five
attempts). Two lessons: run a scenario that replaces the data set in a **loop in a release build** when a crash is
reported inside an unrelated allocation (`malloc()`, `QThreadPool::start()`), and build with AddressSanitizer
(`build-asan`, GCC, see section 4) rather than trusting valgrind when a lifetime bug is suspected - ASan named the exact
use-after-free and the two stacks involved, valgrind reported nothing at all.


### 9.2.8 Where the checks live, and how to add one

The spike used to be one 3300-line `Main.cpp`. It is now five files, and the rule for adding a check follows from that:

| file | content |
| --- | --- |
| `spike/Main.cpp` | the application class, the **table** of options and steps, and `main()` |
| `spike/SpikeHarness.h` / `.cpp` | the machinery every check uses: polling, picking probes, scene and camera dumps, fixture writers, the cached command models |
| `spike/checks/ShellChecks.cpp` | layout, commands, settings, session, library, icons, parity (incl. the menu walk and the About dialog) |
| `spike/checks/RenderingChecks.cpp` | graphics device, picking, hide/show, resize, offscreen service, picking pre-warm, frame rate, lifecycle |
| `spike/checks/ImportChecks.cpp` | the import path: trajectory, unsupported file, cancelled import, playback |
| `spike/checks/PipelineChecks.cpp` | the pipeline model of Phase 3: roles, stable IDs, selection while editing, the shared operations with their undo, the command list model |
| `spike/checks/AnimationChecks.cpp` | the animation model of Phase 3: the interval and the current time with their undo, the tracks and keys of the selected objects, key selection, key moves, key deletion, the visible range, playback |
| `spike/checks/AutomationChecks.cpp` | the automation foundation of Phase 3: serving a session, the command line of this build as the client, the read-only capability grant, the snapshot/describe answers, and the removal of the session when the server stops |

A new check is one entry in `main.cpp`'s `spikeSteps()` table - the option, the predicate that decides whether the value asks for the step at all, and the check to run. That table is the single source for the parser options, the help text, the run order and the *is this a verification run* decision, and it exists because those used to be three separate lists: a check that was registered in the parser but forgotten in the predicate hung until the caller's timeout killed it, which costs a CI round and looks like a product defect. A helper moves into `SpikeHarness.h` once two check files need it; a helper of one check stays `static` in its file.

Order matters and the table's comments say why: the checks run in the order of the table, the importing checks come last because they load data sets of their own, and the shell checks that only read state come before the ones that change the scene. A new check that leaves the scene in a different state belongs at the end of the list, or it has to restore the scene itself.

**Every check declares the phases it walks through.** `declareCheckPhases({...})` at the top of a check names them in the
order they run, `reportCheckPhase("...")` marks each one as it is reached, and the harness reports a failure when the
check's continuation arrives with a declared phase missing:

```
VERIFY_FAILED "qml-pipeline-check stopped early: it declared 8 phase(s) and never reached command list"
```

This exists because a check that stops early used to be indistinguishable from a check that verified everything: both
called their continuation and both left the failure counter at zero, so a phase that bailed out - a missing
precondition, an awaited state that never arrived, a phase deleted while the rest of the chain was adapted - read as a
pass in CI. The three rules are:

* **One report per phase, not per continuation.** The chains in these checks call their continuation at every
  intermediate step (streaming one large file in chunks, waiting for a polling condition on the way to a phase), and
  those are not phases. A phase may be reported more than once - a phase that iterates over the objects of a scene is
  one phase - and only completeness is checked, not a count.
* **The order of the reports is not part of the contract, only the set.** A check whose phases are reached through
  continuations may report them in an order other than the one its declaration lists (the `offscreen` check reports its
  six phases in the order its callbacks finish, and a check that imports a file first can report `import` last if the
  report was placed at the end of the runner). Declaring the phases in the order they are *usually* walked keeps the
  declaration readable; what the harness enforces is that every declared phase is reached and that nothing else is
  reported.
* **A skipped phase still reports.** A section that is skipped because the platform or the data set provides nothing to
  check walks its phase anyway (the report sits at the end of the section, not inside the branch that would run its
  body); the skip is reported by the check itself, as `qInfo("... (skipped) ...")`. A conditional phase is declared
  conditionally, e.g. `declareCheckPhases({ "device", hasGpu ? "render" : "no render" })`.
* **A report of a phase that was not declared is a failure too**, which is what keeps the declaration and the check from
  drifting apart in the other direction: adding a phase means adding it to the declaration in the same edit.

A step whose check declares nothing is not checked at all - that is the state `--qml-startup-delay` is in, because it
waits rather than verifying something - and it is the only kind of step that may do so.

### 9.2.9 Testing the pipeline model (Phase 3, S1)

`--qml-pipeline-check` verifies what a QML view reads from the shared pipeline model and what it may write back through it
(audit D53-D62). It is a *model* check: no pipeline panel exists yet (Phase 4 d1), so the check drives the same
`QmlPipelineController`, the same `PipelineListModel` and the same `AvailableModifiersModel` a panel will bind to, and
every phase of it is separated by a `scheduleDelayed()` wait because the shared model rebuilds its rows on a 200 ms timer
and the pipeline evaluates in the background.

It verifies, in this order:

1. the role vocabulary a QML delegate can read (`title`, `type`, `ischecked`, `iscollapsed`, `decoration`, `tooltip`,
   `statusinfo`), that the icon role hands out a name or a path and never a `QIcon`, and that a row's ID of the
   automation contract resolves back to that row (no ID for a visual-element or data-source row, audit D62);
2. insertion through the shared modifier library - the first modifier the library offers that applies to the imported
   data - after which the new row, its `modifier:mN` ID and its selection are read from the model, and the insertion's
   undo has to remove the row *and* the validity of the ID;
3. that a write through the ID of the undone insertion is refused instead of reaching the row's next occupant, that the
   redo returns the same ID, and that two modifiers can be exchanged with `moveObjectUp()` (undo restores the order);
4. deletion of the selected modifier, the coherence of the selection afterwards, and that the undo brings back both the
   object and the selection (audit D61, the shared model's repair of the classic behaviour);
5. disablement and its undo;
6. that replacing the data set advances the session revision, ends the validity of every ID, refuses stale writes and
   leaves the panel without a selection;
7. the shared command list model of D57: order, the filter over text and status tip, that an invisible command is not
   listed, that `triggerAt()` refuses a disabled command, and the number of commands the workbench offers (`97` in the
   QML workbench at the time of writing, against the same table `--qml-library-check` walks).

Two traps it records:

* **The models are the UI's, not the check's.** `PipelineListModel` and `AvailableModifiersModel` can exist only once per
  process, because their constructors register commands with fixed ids. `SpikeHarness.cpp` therefore hands out the
  instances the workbench already owns (`ui->pipelineController()->model()`), and a check that constructed its own would
  abort in `ActionManager::addCommand`.
* **The step replaces the data set.** Its last phase loads a fresh, empty `DataSet` on purpose (that is how the ID
  invalidation is verified), so it must stay the last step of `spikeSteps()`. The check imports a lattice file of its own
  first, because the scene the earlier steps leave behind may contain no data at all.


### 9.2.10 Testing the animation model (Phase 3, S2)

`--qml-animation-check` verifies the model side of the timeline (audit D53, D56, D58, D63). It is a *model* check as well:
the timeline widget is Phase 5 d2, so the check drives the `QmlAnimationModel` a timeline will bind to and looks at the
controllers and keys that the model found. Every phase is separated by a wait, because the key rows follow the
notifications of the controllers and the scene applies the current frame asynchronously.

It verifies, in this order:

1. the animation state of a freshly imported, static lattice (one frame, `isSingleFrame()`), that `setInterval()` is one
   undo step which the undo reverts and the redo repeats, that the current frame is clamped to the interval, and that the
   frame<->time conversion is the one of the animation settings (a string they cannot parse has no frame);
2. the tracks: which animated parameters of the *selected* scene node the model found (its transformation controller
   contributes a position, a rotation and a scaling track), that a parameter without keys contributes no row, and that
   three keys created on those controllers - the way a property editor or auto-key mode creates them - appear as three
   rows naming parameter, frame, time string and value;
3. the selection: replacing, adding, selecting all keys of a frame, clearing, and jumping the current frame to a selected
   key;
4. the continuous key move of a drag (D58): a shift, a second shift that *replaces* the first one instead of adding to it,
   the clamping at the interval boundary, that cancelling restores the frames the gesture started from without leaving an
   undo step, and that an accepted drag is exactly one undo step whose undo puts the keys back;
5. the discrete move: one undo step per call, and a shift that cannot move anything (all selected keys already at the
   interval boundary) recording nothing;
6. the deletion of two of three keys, its undo returning the keys at the frames they were deleted at, and the fact that
   the selection of deleted keys is *not* restored by the undo, which is what the classic track bar does as well;
7. the visible range as presentation state (D56): it follows the interval, an explicit range survives an interval change
   and `resetRange()` makes it follow again, an inverted range is refused and an out-of-interval one is clamped;
8. the playback state - which is core's `SceneAnimationPlayback` and not this model, mirrored by the model and driven
   through the shared `AnimationTogglePlayback` command - and the playback settings as undoable edits.

Three traps it records:

* **Keys cannot be created through a timeline.** The classic timeline has no add-key operation (keys come from the
  property editors and auto-key mode), so the check creates them through `KeyframeController::createKey()` on the selected
  node's transformation controllers and verifies the *presentation* of the result.
* **Do not measure an edit by the size of the undo stack.** Undoing keeps the undone operation on the stack for redo, so an
  edit that truncates the redo part leaves the size unchanged while it does add a step; the check counts the *depth*
  (`canUndo() ? index() + 1 : 0`) instead. Measuring the size produced a false failure of the deletion phase.
* **The check animates the scene node it imports**, so it belongs after the checks that need the scene they leave behind
  and before `--qml-pipeline-check`, which replaces the data set at its end.


### 9.2.11 When the spike dies without reporting a failure

A full run of the list can die of a `SIGSEGV` *between* two checks, in which case it prints neither `VERIFY_FAILED` nor
`VERIFICATION_DONE` and the shell reports exit code `139` (or the caller's `timeout` reports `124`). That is not a
check failure but a crash of the application, and the core dump says where: `coredumpctl list ovito-qml-spike`, then
`coredumpctl dump ovito-qml-spike --output /tmp/core.spike` and
`gdb -batch -ex "bt 20" ./build-native/bin/ovito-qml-spike /tmp/core.spike`.

The one such crash the list produced so far (audit finding 20) was a race in the *render* path: the import check
replaced the data set while a frame graph build of the previous one was suspended, and the builder dereferenced the
pipeline of a scene node that `SceneNode::requestObjectDeletion()` had already cleared. It was rare - one run in about
seven - so a crash here does not mean the run before it changed anything; it means an interleaving happened. The reason
to keep reading the exit code of a smoke run rather than only its failed-check count is exactly this case: a dead
process reports no failure.

### 9.2.12 Testing the automation foundation (Phase 3, S4)

`--qml-automation-check` verifies the machine-facing path end to end (audit D67-D70). It is the only check that talks to
a *second process*, and it does so because a client that blocks the event loop of the workbench it asks could not be
answered: the workbench serves the session, and the check starts the `ovito` executable of this build next to the spike
binary as a child process and reads its JSON answer, while polling lets the event loop keep running.

Its phases:

1. nothing is discoverable before the workbench serves (the check points `OVITO_AUTOMATION_SESSION_DIR` at a temporary
   directory of its own process, so the sessions of the developer are neither seen nor disturbed);
2. `startAutomationServer()` publishes exactly one session - this process, an endpoint inside that directory, a socket
   and not a port - and keeps the workbench's own session;
3. `ovito --automation status --json` answers: exit code 0, `ok`, the workbench's *own* revision, a data set, four
   viewports, and a grant of exactly `session.read`, `scene.read`, `selection.read` and `file.read` with nothing refused
   and no write capability among the granted ones;
4. `ovito --automation snapshot --json` answers the viewports, the selection and the events in one call;
5. `ovito --automation describe <viewport-id> --json` answers a viewport with its parameters, each carrying a
   `property:.../...` ID - the only place in the test suite where property IDs are minted with the plugin classes loaded;
6. `stopAutomationServer()` removes the descriptor while the workbench keeps its session.

Traps it records:

* **A blocking client cannot be answered by its own process.** Calling `AutomationLocalClient` in the check would block
  the event loop that has to accept the connection, so the client is the command line, started as a child process with
  `QProcess` and awaited by polling rather than by `waitForFinished()`.
* **The child needs the session scope in its environment**, not just in this process: the check passes
  `OVITO_AUTOMATION_SESSION_DIR` to the child, otherwise the child would look at the user's sessions.
* **The check needs the `ovito` executable next to the spike binary.** It skips (and reports so) when this build has no
  product executable, which is the case for the frontend-only preset.
* **It does not verify the `--automation-serve` option's plumbing**, because it calls `startAutomationServer()` directly;
  the option is read in `WorkbenchUI::initializeWorkbench()`, which both frontends share.

The command line itself has a CTest suite of its own, `tst_automation_cli`, which needs no GUI and covers the failing
paths (an empty discovery scope, a descriptor whose process is gone, unknown verbs and missing arguments, the exit codes
and the one-JSON-object rule). Run it with `ctest --preset native -R tst_automation_cli`, or directly with the
`LD_LIBRARY_PATH` of §4.1. The suite looks for the `ovito` executable
by walking up from its own build directory, and **skips itself with `QSKIP` when the build has no application** - which
is right for a frontend-only preset and a trap for a job that builds the product, because `ctest` counts a skipped test
as passed. The CI jobs that build the application therefore set `OVITO_REQUIRE_TEST_BINARY=1`, which turns the skip into
a `QFAIL`; a local run can do the same, or move `bin/ovito` aside once to see the difference (exit 1 instead of 0).

### 9.2.13 When the macOS smoke step hangs (and what the hang was hiding)

The macOS ARM64 job hung on every run from `d586ece9f` on: the spike printed its `BOOTSTRAP` line and then nothing at all
for the whole ten minutes of the step's timeout, with no `VERIFY_FAILED` and no completed check - a state that looks like
a slow machine and is not one. `git diff --stat e895922e..0f673aa89 -- src/` is empty, so no source change was involved;
the difference between the last green run and the first hanging one is one line of the workflow:

```yaml
DYLD_LIBRARY_PATH: ${{ env.Qt6_DIR }}/lib      # e895922e: Qt6_DIR is not exported on macOS, so the value was the inert '/lib'
DYLD_LIBRARY_PATH: ${{ env.QT_ROOT_DIR }}/lib  # d586ece9f: now the Qt installation's lib directory is an active loader path
```

The spike application reaches Qt through its link-time rpath - the Qt 6.10.2 installation for macOS holds 66 `.framework`
directories and no `.dylib` files, so the variable was never needed - and making the Qt lib directory an active loader path
is what made the frontend stop after its bootstrap line. The macOS smoke step therefore sets no environment at all now,
`unset DYLD_LIBRARY_PATH` explicitly, and starts the spike **in the background**: it polls for 450 s and, when there is no
progress, prints `SPIKE_HUNG`, dumps a stack with macOS's `sample(1)` and fails the step, so a hang produces evidence
instead of only a timeout message (the Windows and Linux steps keep their own platform's settings; they never hung).

That change is what let the smoke step reach its checks for the first time in days, and it immediately reported two
macOS-only defects that the hang had been hiding: `--qml-automation-check` could not serve a session (the endpoint came to
about 109 characters against the 104 a socket path may have - see §6.2) and `--qml-session-check` compared a temporary
path against a resolved one (also §6.2). Both are fixed; the lesson worth keeping is that a step which *times out* is not
a step that passed, and that the checks behind a hang are unverified until the hang is gone.

## 10. Quick checklist

1. Build the frontend: `cmake --preset native -DOVITO_BUILD_QML_FRONTEND=ON && cmake --build --preset native -j 16`.
2. Assertions: build `build-asserts` (§4) and run the same checks there; release builds hide lifecycle defects.
3. Rendering: `QT_QPA_PLATFORM=xcb` + `xvfb-run` (+ `QSG_RHI_BACKEND=vulkan` with lavapipe for the Vulkan path).
4. Picking/selection: `--qml-pick X,Y` (grid probe, negative control, synthetic click through `SelectionMode`).
5. Lifecycle: `--qml-lifecycle-cycles N`, `--qml-hide-show`, `--qml-resize WxH`.
   Import path: `--qml-import-check` (trajectory, unsupported file, cancelled import; see §9.1).
   Pipeline model: `--qml-pipeline-check` (roles, stable IDs, selection and undo, command list; see §9.2.9). It belongs at
   the end of a run: it imports a data set of its own and replaces the current one. Animation model:
   `--qml-animation-check` (interval, tracks, keys, selection, key moves, deletion, range, playback; see §9.2.10); it
   animates a scene node of its own, so it runs before the pipeline check. Automation foundation:
   `--qml-automation-check` (serving a session, the `ovito --automation` client of this build, the read-only grant, the
   teardown; see §9.2.12); it starts the product executable as a child process, so this build must have one.
   Command layer: `--qml-command-check` (the commands the QML workbench sees, their state rules and their handlers;
   see §9.3). Workbench state: the shared task progress model is covered by `--qml-import-check` (§9.2), the session
   workflow by `--qml-session-check` (§9.2.1) and the settings facade by `--qml-settings-check` (§9.2.2). Pass a data
   file when the command and pick checks are part of the run, because they need a scene object.
6. Frame rate: `--qml-frame-stats MS` (state vsync and the render loop; only compare equal configurations).
7. Non-Linux validation: run the same commands on the macOS host (§5) — and **close the windows afterwards** (§5.3).
8. Screenshots: `--qml-hold-ms` + a private `Xvfb` display + `ffmpeg -f x11grab` (§2.2); never `grabWindow()` (§9).
9. Windows/D3D12 and further macOS coverage: push the work to a `feature/**` branch and read the CI smoke-test logs
   (§6). The CI smoke test verifies numerically (frame statistics, picking, layout) and uploads no screenshot.
