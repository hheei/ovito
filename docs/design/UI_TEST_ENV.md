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
| macOS test host (`mac`, Apple Silicon, macOS 27) | Qt Quick and classic frontend with the **Cocoa/Metal** backend | Real logged-in GUI session; SSH-launched GUI processes can open windows (§5). |
| Windows / D3D12 | — | No such machine is available, so the D3D12 target cannot be verified yet. |

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

* The user has a real console session, so a plain SSH-launched process **can** create and show an `NSWindow` (verified with
  a 10-line Cocoa program before investing in the Qt install). `QT_QPA_PLATFORM=cocoa` is the default; no
  `launchctl asuser` trick is needed.
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

Add `timeout <seconds>` in front of the command as a second line of defence, and start long runs with an explicit PID
file so the process can be killed deterministically.

---

## 6. Temporary benchmark instrumentation (never commit)

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

## 7. Measuring frame rates of the Qt Quick frontend

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

## 8. Quick checklist

1. Build the frontend: `cmake --preset native -DOVITO_BUILD_QML_FRONTEND=ON && cmake --build --preset native -j 16`.
2. Assertions: build `build-asserts` (§4) and run the same checks there; release builds hide lifecycle defects.
3. Rendering: `QT_QPA_PLATFORM=xcb` + `xvfb-run` (+ `QSG_RHI_BACKEND=vulkan` with lavapipe for the Vulkan path).
4. Picking/selection: `--qml-pick X,Y` (grid probe, negative control, synthetic click through `SelectionMode`).
5. Lifecycle: `--qml-lifecycle-cycles N`, `--qml-hide-show`, `--qml-resize WxH`.
6. Frame rate: `--qml-frame-stats MS` (state vsync and the render loop; only compare equal configurations).
7. Non-Linux validation: run the same commands on the macOS host (§5) — and **close the windows afterwards** (§5.3).
