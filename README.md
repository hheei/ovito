# OVITO (Modernized Fork) - Open Visualization Tool

> **Status: 🚧 Under Active Development / Modernization in Progress**

[![C++23](https://img.shields.io/badge/C%2B%2B-23-blue.svg)](https://en.cppreference.com/w/cpp/23)
[![Qt](https://img.shields.io/badge/Qt-6.10%2B-green.svg)](https://www.qt.io/)
[![License](https://img.shields.io/badge/License-GPL--3.0-blue.svg)](LICENSE.txt)

This repository is a modernized, community-driven development branch of **OVITO** (Open Visualization Tool) — the scientific data visualization and analysis software for atomistic, molecular, and particle-based simulations.

---

## 📌 Project & Upstream Baseline

- **GitHub Repository**: [https://github.com/hheei/ovito](https://github.com/hheei/ovito)
- **Upstream Repository**: [https://gitlab.com/stuko/ovito.git](https://gitlab.com/stuko/ovito.git)
- **Baseline Version**: **OVITO 3.16.1** (tracking commit `81d76297a`)
- **Official Website & Docs**: [https://www.ovito.org/](https://www.ovito.org/) | [Documentation](https://www.ovito.org/docs/current/)

---

## 🚀 Modernization Roadmap & Key Objectives

This fork focuses on modernizing OVITO's architecture, toolchains, and developer experience while preserving 100% of the proven C++ computational engine:

1. **Modern Toolchain & C++ Standard**:
   - Upgraded to modern **C++23** language features.
   - Built on top of **Qt 6.10+** (leveraging native RHI rendering improvements).
   - Standardized on modern CMake Presets (`CMakePresets.json`) and Ninja for blazing-fast incremental builds.

2. **Native Local Development Workflow**:
   - Zero-system-pollution local Qt management via `aqt` into project-local `.qt/`.
   - Native host compilation without requiring heavy Docker containers.
   - Millisecond-level test execution (`ctest --preset native`).

3. **CI/CD & Multi-Platform Support**:
   - Migrated from GitLab CI to GitHub Actions (`.github/workflows/`).
   - Cross-platform targets: Linux (x86_64 & ARM64), macOS (Apple Silicon ARM64), and Windows (AMD64).
   - Automated tagged release packaging and distribution.

4. **Next-Generation UI (In Progress)**:
   - Gradual transition from legacy QtWidgets towards **Qt Quick / QML** frontend.
   - High-performance viewport integration using `QQuickRhiItem` directly driven by the C++ RHI renderer (`SceneRenderer`).

5. **AI-Friendly Codebase Knowledge**:
   - Deep structural code graph powered by **Graphify** (`graphify-out/`).
   - Git post-commit hooks for continuous architectural knowledge maintenance.
   - Comprehensive guidelines defined in [`AGENTS.md`](AGENTS.md).

---

## 🛠️ Quick Start (Native Build)

### 1. Prerequisites
- **Compiler**: GCC >= 13 or Clang >= 17 supporting C++23.
- **CMake**: >= 3.25 & **Ninja** build system.
- **Qt 6.10+**: With `qtshadertools` and private GUI headers.
- **System Libraries**: Boost, HDF5, NetCDF, Vulkan / OpenGL headers.

### 2. Configure & Build

```bash
# Clone the repository with submodules
git clone --recurse-submodules https://github.com/hheei/ovito.git
cd ovito

# If using local Qt in .qt/ (configured via CMakePresets):
cmake --preset native

# Compile (capped to 8 parallel jobs to avoid memory exhaustion)
cmake --build --preset native -j 8

# Run unit tests (runs headless with offscreen platform)
ctest --preset native

# Launch application
LD_LIBRARY_PATH="$(pwd)/.qt/6.10.2/gcc_64/lib" ./build-native/bin/ovito --version
```

For more detailed workflows, see [`AGENTS.md`](AGENTS.md).

---

## 📄 License

OVITO is released as free software under the terms of the **[GNU General Public License (GPL) version 3](LICENSE.GPL.txt)**:

- See [`LICENSE.txt`](LICENSE.txt) and [`LICENSE.GPL.txt`](LICENSE.GPL.txt) for the full license terms.
- Third-party components bundled under `src/3rdparty/` remain under their respective upstream open-source licenses.
- Documentation under `docs/` is licensed under the GNU Free Documentation License (GFDL 1.2+).

---

## 🤝 Development & Guidelines

This project is in active development. Please refer to [`AGENTS.md`](AGENTS.md) for architectural guidelines, build instructions, and code standards.
