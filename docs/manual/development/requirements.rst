.. _development.requirements:

Build requirements
==================

The prerequisites for building OVITO from source are:

.. table::
   :widths: auto
   :width: 100%

   ============================= ============================= ===================================================
   Tool/Package                  Requirement                   Notes
   ============================= ============================= ===================================================
   C++ compiler                  required                      Support for C++23 language standard (GCC >= 13, Clang >= 17, MSVC 2022)
   `CMake <http://cmake.org/>`_  required                      Modern CMake workflow (CMake 3.25 or newer required, presets supported)
   `Git <http://git-scm.com/>`_  required                      The distributed version control system needed to retrieve the source code
   `Ninja <https://ninja-build.org/>`_ recommended             Fast build tool recommended for development
   ============================= ============================= ===================================================

The library dependencies are:

.. table::
   :widths: auto
   :width: 100%

   ============================================================ ================= ===================================================
   Library                                                      Requirement       Notes
   ============================================================ ================= ===================================================
   `Qt <http://www.qt.io/developers/>`_                         required          Qt cross-platform framework (version 6.10 or higher, with ShaderTools and GuiPrivate)
   `zlib <http://www.zlib.net/>`_                               optional          Required for reading and writing compressed files.
   `Boost <http://www.boost.org/>`_                             required          OVITO uses header-only utility classes from Boost.
   `libssh <http://www.libssh.org/>`_                           optional          Used by OVITO's built-in SSH client for remote data access.
   `libnetcdf <http://www.unidata.ucar.edu/software/netcdf/>`_  optional          Required by the Amber/NetCDF file reader/writer plugin
   `libhdf5 <https://www.hdfgroup.org/solutions/hdf5/>`_       optional          Required for HDF5-based particle/mesh format imports
   ============================================================ ================= ===================================================

The sources of the following third-party libraries are included in the OVITO source distribution
and get compiled automatically as part of the build process:

.. table::
   :widths: auto
   :width: 100%

   ============================================================ ====================================================================
   Library                                                      Notes
   ============================================================ ====================================================================
   `muparser <http://beltoforion.de/article.php?a=muparser>`_   A math expression parser library.
   `Qwt <http://sourceforge.net/projects/qwt/>`_                For plotting and data visualization.
   `Voro++ <https://doi.org/10.1063/1.3215722>`_                Voronoi cell construction routine required by the :ref:`Voronoi analysis <particles.modifiers.voronoi_analysis>` modifier.
   `KISS FFT <https://github.com/mborgerding/kissfft>`_         Required by the :ref:`Spatial correlation function <particles.modifiers.correlation_function>` plugin.
   ============================================================ ====================================================================

