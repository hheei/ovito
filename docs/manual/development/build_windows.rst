.. _development.build_windows:

Building OVITO on Windows
===============================

The required steps are quite involved, particularly those for building/installing the prerequisites,
and the available instructions may be insufficient. Please get in touch with the developers if you want to
build OVITO on the Windows platform.

You are going to need at least the following prerequisites:

* Microsoft Visual Studio 2022 or newer (command line tools for C++ development)
* Qt 6.x (install the `msvc2019 64-bit` component)
* CMake (v3.25 or newer)
* git
* `zlib <http://www.zlib.net>`__
* `Boost <https://www.boost.org>`__ (header-only component)
* HDF5 (optional, requires zlib)
* NetCDF (optional, requires HDF5 and zlib)
* ffmpeg (optional, version 4.2.1, requires MSYS2)

Downloading the source code
---------------------------

To download OVITO's source code into a new subdirectory named :file:`ovito/`, install `Git <https://git-scm.com>`_ and run::

  git clone --recursive https://github.com/hheei/ovito.git ovito