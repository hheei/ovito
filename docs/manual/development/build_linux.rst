.. _development.build_linux:

Building OVITO on Linux
=======================

Modern OVITO requires **C++23** compiler support (GCC 13+ or Clang 17+) and **Qt 6.10+** (with private development headers and ShaderTools).

Installing dependencies
-----------------------

First, install the required base build tools and system development libraries:

Ubuntu 24.04 / Debian 13+:
  .. code-block:: shell

          sudo apt-get install build-essential git cmake ninja-build \
                libboost-dev libnetcdf-dev libhdf5-dev \
                libgl1-mesa-dev libvulkan-dev libxkbcommon-dev

Setting up Qt 6.10+
-------------------

Because standard Linux distribution repositories may ship older Qt releases (e.g. Qt 6.4), we recommend installing Qt 6.10+ locally into the project-specific ``.qt/`` directory using ``aqtinstall`` (zero system pollution):

.. code-block:: shell

        # Install aqtinstall:
        uv tool install aqtinstall  # or: pip install aqtinstall

        # Install Qt 6.10.2 into .qt directory:
        aqt install-qt linux desktop 6.10.2 gcc_64 -m qtshadertools -O .qt

Compiling OVITO using CMake Presets
-----------------------------------

OVITO provides modern CMake presets defined in :file:`CMakePresets.json`:

.. code-block:: shell

        # Configure CMake using the native preset (automatically points to .qt/)
        cmake --preset native

        # Build with Ninja (parallel jobs capped to 8 to avoid memory exhaustion)
        cmake --build --preset native -j 8

        # Run unit tests
        ctest --preset native

        # Run application
        LD_LIBRARY_PATH="$(pwd)/.qt/6.10.2/gcc_64/lib" ./build-native/bin/ovito --version

If this step is successful, the :program:`ovito` executable can be found in the directory :file:`build-native/bin/`.
