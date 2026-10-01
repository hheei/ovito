# AGENTS.md — AI & Agent Development Guidelines for OVITO

Welcome, AI coding agent! This document contains the architectural overview, rules, workflow standards, and guidelines for developing and modernizing this OVITO fork.

---

## 1. Project Mission & Modernization Roadmap

- **Core Goal**: Maintain this fork on GitHub with an updated, AI-friendly codebase.
- **Frontend Modernization**: Build a new, responsive **Qt Quick / QML frontend** while **preserving existing QtWidgets code** (`src/ovito/gui/`) side-by-side during the transition.
- **Backend Reuse (Zero Wheel Reinvention)**: Maximize reuse of OVITO's rock-solid C++ backend:
  - Particle and simulation cell pipelines (`src/ovito/particles`, `src/ovito/stdobj`).
  - Asynchronous task/coroutine system (`TaskScope`, `FutureWatcher`, `C++20 coroutines`).
  - Cross-platform hardware rendering via Qt Rendering Hardware Interface (`QRhi`).
- **Target Platform Matrix**:
  - macOS: **Apple Silicon ARM64 only** (Metal backend)
  - Windows: **AMD64 / x86_64** (Direct3D 12 backend)
  - Linux: **x86_64 & ARM64** (Vulkan backend)

---

## 2. Standard Build & Development Commands

Always use CMake presets.

### Quick Workflow: Native Host Development (via local `.qt/`)
Qt 6.10.2 is installed locally under `.qt/` (git-ignored).
The default `native` preset leverages **Clang 18**, **mold** fast linker, and **ccache** for near-instant builds:
```bash
# 1. Configure CMake using native preset (Clang 18 + mold + ccache)
cmake --preset native

# Or if preferring GCC:
# cmake --preset native-gcc

# 2. Build (fast link with mold, cached with ccache)
cmake --build --preset native -j 16

# 3. Build a specific test or target
cmake --build --preset native --target tst_core_math
cmake --build --preset native --target ovito

# 4. Run C++ tests (headless with QT_QPA_PLATFORM=offscreen)
ctest --preset native

# 5. Run the compiled application
LD_LIBRARY_PATH="$(pwd)/.qt/6.10.2/gcc_64/lib" ./build-native/bin/ovito --version
LD_LIBRARY_PATH="$(pwd)/.qt/6.10.2/gcc_64/lib" QT_QPA_PLATFORM=offscreen ./build-native/bin/ovito --nogui
```

### Build speed: tell the compiler cache about the precompiled headers

OVITO compiles nearly every translation unit with a precompiled header (`-include-pch .../cmake_pch.hxx.pch`), and ccache
refuses to cache such a call - 81% of the compile calls in this tree were uncacheable, which silently made the `ccache`
launcher of the presets nearly useless. Export the documented sloppiness settings **before** configuring (the `native` and
`native-gcc` presets pass no environment, and a preset cannot set one):

```bash
export CCACHE_SLOPPINESS=pch_defines,time_macros
export CCACHE_DEPEND=1
```

With them a cached translation unit recompiles in 0.15s instead of about 6s. The CI jobs use these presets: `ci-linux` (the single Linux x86_64 job: whole product, clang, `-O2`), `ci-macos`
(frontend scope, `-O2`) and `ci-windows` (whole product, MSVC, no optimization override because an MSVC Release build is
already `/O2`), all in `Release` and without debug info; `ci` and `ci-frontend` are their hidden bases. Only the *test*
workflow uses them - `release.yml` builds with the `-O3` and the full plugin set that ship, so use its `release` flags
when a change is performance-sensitive. `OVITO_COMPILER_LAUNCHER` selects the compiler cache (`ccache`); that variable is
what decides, because the top-level CMakeLists only enables ccache by itself when neither `CMAKE_C_COMPILER_LAUNCHER` nor
`CMAKE_CXX_COMPILER_LAUNCHER` is already defined (an empty value therefore means "no compiler cache", which is what a
build-time measurement needs). For iterating on the Qt Quick frontend and the spike there is also a local
`native-frontend` preset (clang, ccache, mold, `.qt`, 512 instead of 951 translation units, no product executable).
Measurements and rationale: docs/design/UI_TEST_ENV.md section 6.1.

---

## 3. Architecture & Code Boundaries

```
src/
├── ovito/
│   ├── core/           <-- Core object model, pipeline, undo/redo, QRhi SceneRenderer
│   ├── particles/      <-- File I/O (LAMMPS, XYZ, PDB), neighbor finders, analysis
│   ├── stdobj/         <-- Simulation cell, geometric data objects, properties
│   ├── gui/            <-- Legacy QtWidgets frontend (KEEP AS-IS, DO NOT BREAK)
│   └── ...             <-- Specific analysis plugins (Mesh, Grid, etc.)
└── 3rdparty/           <-- Submodules (zstd, hdf5, netcdf-c) and bundled headers
```

### The Qt Quick / QML frontend (architecture frozen at the end of Phase 2.5)

The new frontend lives in `src/ovito/gui/qml/` and is built only with `-DOVITO_BUILD_QML_FRONTEND=ON`; run it with
`ovito --gui=qml <data file>`. Its design is frozen: read `docs/design/UI_DESIGN.md` (status: frozen) and the decisions
D1–D37 in `docs/design/UI_PHASE0_AUDIT.md` before changing its layering, and treat a change to the rendering bridge or
to the shared `gui/base` layer as a new decision in that audit rather than as an edit in passing.

* **Layering**: `gui/qml` depends on `gui/base` and `core` only - never on `gui/desktop` and never on QtWidgets. Shared
  behaviour (commands, session workflow, libraries, settings, icons, offscreen rendering) belongs in `gui/base` or
  `core/rendering`; the QML layer presents it.
* **Every check** of the frontend is an `OvitoQmlSpike` option (`src/ovito/gui/qml/spike/Main.cpp`), and the CI runs them
  on all four platforms. A new option must be registered *and* added to the option table of the harness, otherwise the run
  is treated as interactive and times out.
* **`docs/design/UI_PARITY_MATRIX.md`** is the inventory of what the classic frontend and the QML frontend do; keep it and
  `docs/design/UI_PLAN.md` current when a capability moves.
* **The frontend is verified, not assumed**: a shell change needs the spike suite, and a rendering change needs a look at
  the screenshot artifact of the Linux CI job (the checks assert model state, which a shell rendering an empty pane passes).

### Critical Rules for AI Agents:
1. **DO NOT reinvent particle analysis or geometry algorithms**:
   - Simulation cell math: Use `SimulationCell` / `SimulationCellDataT`.
   - Neighbor finding: Use `CutoffNeighborFinder` / `NearestNeighborFinder`.
   - Pipeline flow: Inherit from `Modifier` or `ModifierDelegate`.
2. **DO NOT delete legacy QtWidgets frontend yet**:
   - The current GUI is functional and serves as the reference implementation.
   - Any new QML components should be added in a modular fashion (e.g. `src/ovito/qml/` or separate QML plugin).
3. **Rendering & Viewport in QML**:
   - OVITO's rendering runs on a dedicated `RenderThread` using `QRhi`.
   - When integrating into QML, use **`QQuickRhiItem`** (Qt 6.7+) to render hardware-accelerated viewports directly into the Qt Quick scene graph.
4. **Threading & Concurrency**:
   - Never spawn raw `std::thread` or `pthread`.
   - Use OVITO's async framework: `TaskScope`, `asyncLaunch()`, or C++20 coroutines.
5. **Headless Execution**:
   - Qt requires `QT_QPA_PLATFORM=offscreen` in environments without an active X11/Wayland display server (CI, Docker, headless servers).
   - But `offscreen` cannot provide a QRhi for the Qt Quick frontend: use `QT_QPA_PLATFORM=xcb` under `xvfb-run` instead.
     The full recipe (backend selection, datasets, measurement flags, known traps) is in
     **[docs/design/UI_TEST_ENV.md](docs/design/UI_TEST_ENV.md)** — read it before running or interpreting any
     frontend test, and extend it whenever a new testing pitfall is found.
6. **Clean up after GUI test runs**:
   - A frontend window keeps rendering until the process is terminated, which can saturate the machine (and on remote hosts
     even block SSH). Always kill the process, then verify that none is left (`pkill -9 -f Ovito.app; pgrep -fl Ovito.app`),
     and always launch long-running GUI runs under `timeout`.

---

## 4. Coding Standards

- **Language Standard**: Modern C++23 (`set(CMAKE_CXX_STANDARD 23)`).
- **Qt Version**: Qt 6.10+ (private headers require `qt6-base-private-dev`).
- **Memory Safety**: Use smart pointers (`std::shared_ptr`, `OORef<T>`); OVITO has an internal intrusive ref-counting object system (`RefTarget`).
- **Formatting**: Adhere to `.clang-format` in the repository.

---

## 5. Testing & Verification Requirements

Before reporting any task complete:
1. Ensure the code compiles cleanly without new compiler warnings.
2. Run CTest: `ctest --preset native` and ensure all tests pass (100% pass rate).
3. Verify headless startup: `LD_LIBRARY_PATH="$(pwd)/.qt/6.10.2/gcc_64/lib" QT_QPA_PLATFORM=offscreen ./build-native/bin/ovito --nogui`.

### Platform verification is done on three machines, not on CI

A change that can break on one platform is verified on that platform's machine. This is fixed policy, not a fallback:

| Platform | Where | How |
| --- | --- | --- |
| Linux x86_64 | this workstation | `cmake --preset native`, `ctest --preset native`; QML checks through `xvfb-run` with `QT_QPA_PLATFORM=xcb` |
| Windows AMD64 | `ssh kitty` | `C:\ovito\build-verify` (Visual Studio 2022 x64 developer environment, VS-bundled CMake/Ninja, `-DOVITO_REDISTRIBUTABLE_PACKAGE=ON -DOVITO_IS_DEVELOPMENT_BUILD=OFF`), tests asked for their own `-o <file>,txt` log |
| macOS ARM64 | `ssh kings@buddy` | `~/ovito/build` with `/opt/homebrew/bin/cmake --build build --parallel 8` and the warm `/Users/kings/.ccache`, tests under `QT_QPA_PLATFORM=offscreen` |

GitHub Actions is the fourth and slowest place, not the place to iterate: its three jobs confirm that the machines and the
runners agree, so a platform-specific fix found on a machine is pushed once, after the machines pass. Both hosts take the
code as a `git bundle` over ssh (kitty cannot reach `github.com`). Recipes, traps and the per-platform rules that the
suites must respect are in **[docs/design/UI_TEST_ENV.md](docs/design/UI_TEST_ENV.md) sections 5 and 6.2** - read them
before running anything there, and keep them correct whenever a new trap is found.

## graphify

This project has a knowledge graph at graphify-out/ with god nodes, community structure, and cross-file relationships.

When the user types `/graphify`, use the installed graphify skill or instructions before doing anything else.

Rules:
- For codebase questions, first run `graphify query "<question>"` when graphify-out/graph.json exists. Use `graphify path "<A>" "<B>"` for relationships and `graphify explain "<concept>"` for focused concepts. These return a scoped subgraph, usually much smaller than GRAPH_REPORT.md or raw grep output.
- Dirty graphify-out/ files are expected after hooks or incremental updates; dirty graph files are not a reason to skip graphify. Only skip graphify if the task is about stale or incorrect graph output, or the user explicitly says not to use it.
- If graphify-out/wiki/index.md exists, use it for broad navigation instead of raw source browsing.
- Read graphify-out/GRAPH_REPORT.md only for broad architecture review or when query/path/explain do not surface enough context.
- After modifying code, run `graphify update .` to keep the graph current (AST-only, no API cost).
