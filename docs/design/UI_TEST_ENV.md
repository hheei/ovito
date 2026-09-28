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
echo '512 atoms, simple cubic lattice a=3.6'  > /tmp/c.xyz   # -> "c_*.xyz [LAMMPS Data]", no particles
sed -i '2s/.*/simple cubic lattice, a=3.6/'   /tmp/c.xyz     # -> "c_*.xyz [XYZ]", 512 particles
```

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
  ./build-native/bin/ovito-qml-spike --qml-capture /tmp/shot.png --qml-capture-delay 4000 /tmp/lattice_512.xyz
```

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

To *see* what such a run rendered, grab the X11 display with ffmpeg instead of adding a screenshot API:

```bash
DISPLAY=:99 ffmpeg -y -f x11grab -video_size 1280x800 -i :99 -frames:v 1 /tmp/workbench.png
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
* The spike's `QQuickWindow::grabWindow()` (section 9) remains the reliable way to capture the *viewport* pixels; the
  ffmpeg grab captures the whole X11 screen including window decorations.

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
* `QScreen::grabWindow()` returns a 0×0 image on macOS unless the process has screen-recording permission. Use
  `QQuickWindow::grabWindow()` instead (it re-renders through QRhi and does not need that permission), which is what the
  spike's `--qml-capture` does.
* Use the framework-based Qt: `CMAKE_PREFIX_PATH=$HOME/Qt/6.10.2/macos` works, private headers live inside the frameworks
  (e.g. `QtQuick.framework/Headers/6.10.2/QtQuick/private/qquickrhiitem_p.h`).
* **Qt itself must be on the loader path in the build tree**; the OVITO plugins no longer need to be (the `@rpath`
  packaging gap that required `DYLD_LIBRARY_PATH=<bundle>/Contents/PlugIns` was fixed in the spike target, see §9):

```bash
cd ~/ovito
QT_QPA_PLATFORM=cocoa DYLD_LIBRARY_PATH=/Users/buddy/Qt/6.10.2/macos/lib \
  ./build-buddy/Ovito.app/Contents/MacOS/ovito-qml-spike \
  --qml-capture /tmp/buddy.png --qml-capture-delay 4000 --qml-pick 300,300 /tmp/lattice_512.xyz
```

* Retina displays report `devicePixelRatio = 2`, so `--qml-capture` writes images at twice the logical window size and all
  four viewports render at 4× the pixel count of the same window on a non-Retina machine. Always state the pixel size and
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

**Where the jobs currently stand** (as of the Phase 1 close-out): Linux x86_64, Linux ARM64 and macOS ARM64 run the full
smoke test and pass. The Windows job **builds the whole tree** (1149/1149 targets, `ovito.exe` and `ovito-qml-spike.exe`
included) but has not yet reached its D3D12 smoke test: the run stopped at the preceding CTest step, where five tests
aborted with `0xc0000135`, which turned out to be the `PATH` trap described below rather than a test failure. That trap is
fixed, but the job has not been re-run since - a D3D12 runtime result is still outstanding, and CI usage is deliberately
kept low, so a Windows verification should be a single deliberate run rather than a debug loop.

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
