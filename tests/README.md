# C++ Unit Tests (OVITO Core)

This directory contains C++ unit tests for the open-source OVITO core library. Tests use the Qt Test framework (QTest) and are registered with CTest.

## Directory layout

```
ovito/tests/
├── CMakeLists.txt         — top-level, guarded by OVITO_BUILD_CPP_TESTS
├── README.md              — this file
└── cpp/
    ├── CMakeLists.txt     — adds per-area subdirectories
    ├── core/
    │   ├── math/
    │   │   ├── CMakeLists.txt
    │   │   └── tst_core_math.cpp   — Vector3, Matrix3, Matrix4, Box3, Quaternion, AffineTransformation
    │   └── utilities/
    │       ├── CMakeLists.txt
    │       └── tst_containers.cpp  — BoundedPriorityQueue, DisjointSet, Graph, MemoryPool
    └── stdobj/
        └── simcell/
            ├── CMakeLists.txt
            └── tst_simulation_cell.cpp  — SimulationCellDataT PBC math
```

## Running the tests

```bash
# Using native CMake preset:
ctest --preset native

# Or directly in any build directory:
ctest --test-dir <build-dir> --output-on-failure -L cpp

# Run a specific executable:
ctest --preset native -R tst_core_math
```

## Build options

| CMake option | Default | Meaning |
|---|---|---|
| `OVITO_BUILD_CPP_TESTS` | `ON` (when `Qt6Test` found and `BUILD_TESTING` is on) | Enable/disable all C++ unit tests |

## Conventions

- Test executable names follow the pattern `tst_<area>` (e.g. `tst_core_math`).
- Source files are named `tst_<area>.cpp`.
- Each test executable links against the minimum set of OVITO libraries needed (e.g. `tst_core_math` links `Core`; `tst_simulation_cell` links `StdObj`).
- Tests are registered with CTest label `cpp`.
- Tests should run in <30 s total on a debug build and are included in the `quick` label.

## What belongs here

Good C++ test targets are **library-like** pieces — pure data structures, algorithms, math utilities — that can be instantiated without a running OVITO application context or Qt event loop:

- Linear algebra: `Vector3`, `Matrix3`, `Matrix4`, `Box3`, `Quaternion`, `AffineTransformation` (headers in `ovito/src/ovito/core/utilities/linalg/`)
- Containers: `BoundedPriorityQueue`, `DisjointSet`, `Graph`, `MemoryPool` (in `ovito/src/ovito/core/utilities/`)
- Simulation cell math: `SimulationCellDataT` (in `ovito/src/ovito/stdobj/simcell/`)
- Future: `DataBuffer`, `PropertyContainer`, `CutoffNeighborFinder`

Poor targets (not suitable here): pipeline/scene-graph code, Qt GUI widgets, QRhi rendering, DataSet/DataObject subclasses that require the OVITO object system to be fully initialized.

## Adding a new test executable

1. Create a subdirectory under `ovito/tests/cpp/<area>/`.
2. Add `tst_<area>.cpp` with a `QTEST_MAIN(...)` entry point and one or more `QObject`-based test classes.
3. Add `CMakeLists.txt` calling `ovito_add_cpp_test(NAME tst_<area> SOURCES tst_<area>.cpp LINK_LIBRARIES OvitoCore)`.
4. Add `add_subdirectory(<area>)` to `ovito/tests/cpp/CMakeLists.txt`.
5. Commit the submodule changes first, then the outer repo (see root `CLAUDE.md` for the git workflow).

## Repository split

These tests live in the `ovito/` submodule (the open-source core repository). Tests covering proprietary plugins live in `proprietary/tests/` in the outer repo. See the root-level `tests/README.md` for the full picture.
