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
| macOS test host `buddy` (Mac mini, Apple M4) | Qt Quick and classic frontend with the **Cocoa/Metal** backend | Attached monitor (Acer EK241Y, `devicePixelRatio = 1`), console session belongs to user `kings` — so GUI tests must run as `kings`, not as `buddy` (see section 5.2). Qt lives in `/Users/buddy/Qt/6.10.2/macos` and Homebrew's `boost`/`hdf5`/`netcdf` are installed. |
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
  ./build-native/bin/ovito-qml-spike --qml-capture /tmp/shot.png --qml-capture-delay 4000 /tmp/lattice_512.xyz
```

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

## 3. Classic frontend in a headless environment

* `RenderThread::pickGraphicsApi()` hardcodes the graphics API per platform, and on Linux it is always **Vulkan**. There is
  no environment override and no OpenGL fallback.
* Under Xvfb a Vulkan swapchain cannot be created (see §2.1), so the classic viewport renders **no frames at all** — the
  window appears, but the viewport stays black/empty and no `endFrame()` ever happens. Do not conclude from a surviving
  process that the classic viewport works.
* Forcing the software ICD (`VK_ICD_FILENAMES=lvp_icd.json`) fails earlier with
  `Failed to create Vulkan instance: -9` (`VK_ERROR_INCOMPATIBLE_DRIVER`), because OVITO still creates its
  `QVulkanInstance` with `apiVersion = 0` (finding F3/O2). RADV tolerates this with a validation warning; lavapipe
  rejects it. This makes O2 more than a cosmetic issue.
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

---

## 5. macOS test host

### 5.1 Toolchain setup (already done once, recorded for reproducibility)

```bash
# Qt 6.10.2 with private headers (QtQuick private API is required by the QQuickRhiItem renderer)
python3 -m aqt install-qt mac desktop 6.10.2 clang_64 --outputdir "$HOME/Qt"
python3 -m aqt install-qt mac desktop 6.10.2 clang_64 -m qtshadertools --outputdir "$HOME/Qt"   # qt_add_shaders

brew install boost hdf5 netcdf        # required by geogram (Delaunay plugin) and by the netcdf integration

# Configure: the fork has no macOS preset yet, so pass the Qt prefix explicitly
cmake -S . -B build-mac -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_PREFIX_PATH="$HOME/Qt/6.10.2/macos;/opt/homebrew" \
  -DOVITO_BUILD_APP=ON -DOVITO_BUILD_QML_FRONTEND=ON -DOVITO_BUILD_CPP_TESTS=OFF -DOVITO_USE_CCACHE=OFF
ninja -C build-mac -j 10
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
* `QScreen::grabWindow()` returns a 0×0 image on macOS unless the process has screen-recording permission. Use
  `QQuickWindow::grabWindow()` instead (it re-renders through QRhi and does not need that permission), which is what the
  spike's `--qml-capture` does.
* Use the framework-based Qt: `CMAKE_PREFIX_PATH=$HOME/Qt/6.10.2/macos` works, private headers live inside the frameworks
  (e.g. `QtQuick.framework/Headers/6.10.2/QtQuick/private/qquickrhiitem_p.h`).
* **The prototype executable needs `DYLD_LIBRARY_PATH`** pointing at the bundle's plugin directory. Without it the loader
  aborts with `Library not loaded: @rpath/GuiQml.so`:

```bash
cd ~/ovito
export QT_QPA_PLATFORM=cocoa
export DYLD_LIBRARY_PATH="$PWD/build-mac/Ovito.app/Contents/PlugIns:$HOME/Qt/6.10.2/macos/lib"
./build-mac/Ovito.app/Contents/MacOS/ovito-qml-spike \
  --qml-capture /tmp/mac.png --qml-capture-delay 4000 --qml-pick 300,300 /tmp/lattice_512.xyz
```

  This is a real packaging gap of the prototype target on macOS (recorded as O9): on Linux the same binary finds its
  plugins through its rpath, on macOS it does not.
* Retina displays report `devicePixelRatio = 2`, so `--qml-capture` writes images at twice the logical window size and all
  four viewports render at 4× the pixel count of the same window on a non-Retina machine. Always state the pixel size when
  comparing frame rates across machines.
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

So a D3D12 smoke test needs an x86_64 Windows runner; section 6 describes the CI job that provides one. Qt Quick's D3D12 backend supports the **WARP** software adapter
(`QSG_RHI_PREFER_SOFTWARE_RENDERER=1`), and OVITO's render thread creates its D3D12 device with default
`QRhiD3D12InitParams`, which falls back to the DXGI default adapter (WARP on a GPU-less machine) — i.e. a CPU-only CI runner
can exercise the D3D12 code paths, the same way lavapipe covers Vulkan on Linux. Prerequisites for such a job: a Qt 6.10
Windows installation with private headers, and `dxc` on `PATH` (OVITO precompiles HLSL SM 6.0 to DXIL).

Also note that when a Windows machine *is* available, a classic-frontend run needs the interactive import dialog out of the
way (section 3.1) — that trap applies to every platform.

---

## 6. Continuous integration as a test host (all four target platforms)

The GitHub workflow `.github/workflows/ci.yml` builds and smoke tests the Qt Quick frontend on the four target platforms,
which is the only way to cover **Windows/D3D12** and a second macOS/Metal host without owning that hardware. Each job
configures with `-DOVITO_BUILD_QML_FRONTEND=ON`, builds `OvitoQmlSpike` and runs it against a generated 512-atom lattice
with `--qml-pick`, `--qml-lifecycle-cycles`, `--qml-hide-show` and `--qml-frame-stats`; the spike exits non-zero when a
check fails, so the step is an assertion instead of a log to grep. The screenshots are uploaded as build artifacts.

| Job | Qt Quick backend | OVITO picking backend | Display |
|-----|------------------|------------------------|---------|
| Linux x86_64 | OpenGL (llvmpipe) | Vulkan (lavapipe via `VK_DRIVER_FILES`) | `xvfb-run` + `QT_QPA_PLATFORM=xcb` |
| Linux ARM64 | OpenGL (llvmpipe) | Vulkan (lavapipe) | same |
| macOS ARM64 | Metal | Metal | the runner's own session |
| Windows AMD64 | **D3D12 on WARP** (`QSG_RHI_BACKEND=d3d12`, `QSG_RHI_PREFER_SOFTWARE_RENDERER=1`) | D3D12 on WARP (D3D11 fallback) | the runner's own session |

Prerequisites that the runner images do **not** provide and that the workflow installs, each of which fails the job loudly
if missing:

* **Boost headers** on Windows (`vcpkg install boost-headers:x64-windows`, passed as `-DBOOST_ROOT=...`): geogram requires
  them, and the failure surfaces during configure as `Could NOT find Boost (missing: Boost_INCLUDE_DIR)`.
* **`dxc` on Windows**: `qsb` precompiles the HLSL shaders to DXIL, and OVITO turns a missing `dxc` into a `FATAL_ERROR`
  at configure time. The Windows SDK ships `dxc.exe` under `Windows Kits\10\bin\<version>\x64`; the workflow adds that
  directory to `GITHUB_PATH` and falls back to the `Microsoft.Direct3D.DXC` NuGet package.
* **lavapipe on Linux**: OVITO's offscreen picking `RenderThread` uses Vulkan on Linux and needs a usable ICD on a GPU-less
  runner (`mesa-vulkan-drivers`, `VK_DRIVER_FILES=/usr/share/vulkan/icd.d/lvp_icd.json`). This works because an offscreen
  instance needs no surface — the classic frontend, which presents to X11, cannot use lavapipe this way (section 3).

Note that a *compiler version* difference between a CI runner and the development machine can fail the build for reasons
unrelated to the frontend: the macOS job was failing on `static_assert(false, ...)` in `DataBuffer.h` because Apple clang 15
predates the C++23 resolution of CWG 2518. When a build works locally but fails in CI, compare the compiler versions first.

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
* On macOS the opposite was observed: `QSG_NO_VSYNC=1` made the QML measurements **twice as slow** (16.95 → 35.71 ms per
  frame, reproduced twice). Do not use it there without re-checking; report both numbers.
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
* **Two probes at the same position must agree.** The picking buffer is rendered asynchronously, so a probe taken right
  after a change answers from the previous buffer; if the probes before and after the picking pass disagree (19/25 versus
  13/25 was the symptom), that is a defect in the buffer bookkeeping, not a measurement artefact - it is how the missing
  buffer-size check of `QuickViewportWindow::pick()` was found.

## 10. Quick checklist

1. Build the frontend: `cmake --preset native -DOVITO_BUILD_QML_FRONTEND=ON && cmake --build --preset native -j 16`.
2. Assertions: build `build-asserts` (§4) and run the same checks there; release builds hide lifecycle defects.
3. Rendering: `QT_QPA_PLATFORM=xcb` + `xvfb-run` (+ `QSG_RHI_BACKEND=vulkan` with lavapipe for the Vulkan path).
4. Picking/selection: `--qml-pick X,Y` (grid probe, negative control, synthetic click through `SelectionMode`).
5. Lifecycle: `--qml-lifecycle-cycles N`, `--qml-hide-show`, `--qml-resize WxH`.
6. Frame rate: `--qml-frame-stats MS` (state vsync and the render loop; only compare equal configurations).
7. Non-Linux validation: run the same commands on the macOS host (§5) — and **close the windows afterwards** (§5.3).
8. Windows/D3D12 and further macOS coverage: push the work to a `feature/**` branch and read the CI smoke-test
   artifacts and logs (§6).
