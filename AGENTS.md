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

## graphify

This project has a knowledge graph at graphify-out/ with god nodes, community structure, and cross-file relationships.

When the user types `/graphify`, use the installed graphify skill or instructions before doing anything else.

Rules:
- For codebase questions, first run `graphify query "<question>"` when graphify-out/graph.json exists. Use `graphify path "<A>" "<B>"` for relationships and `graphify explain "<concept>"` for focused concepts. These return a scoped subgraph, usually much smaller than GRAPH_REPORT.md or raw grep output.
- Dirty graphify-out/ files are expected after hooks or incremental updates; dirty graph files are not a reason to skip graphify. Only skip graphify if the task is about stale or incorrect graph output, or the user explicitly says not to use it.
- If graphify-out/wiki/index.md exists, use it for broad navigation instead of raw source browsing.
- Read graphify-out/GRAPH_REPORT.md only for broad architecture review or when query/path/explain do not surface enough context.
- After modifying code, run `graphify update .` to keep the graph current (AST-only, no API cost).
