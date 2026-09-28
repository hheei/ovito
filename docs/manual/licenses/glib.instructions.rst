.. _appendix.license.glib.instructions:

Build instructions for GLib
---------------------------

The OVITO Python module for Linux includes a binary version of the GLib library licensed under the GNU Lesser General Public License (LGPLv2.1 or later).
In accordance with the requirements of this license, this page provides instructions on how to rebuild a compatible version of the library from source code.

The library is not part of the *OVITO Pro* or *OVITO Basic* program packages. Those ship a Qt framework that OVITO builds
itself with GLib support switched off. The ``ovito`` package installed from `PyPI <https://pypi.org/project/ovito/>`__ takes
its Qt libraries from the *PySide6* package instead, and those are built with GLib support enabled, which is why the two
files :file:`libglib-2.0.so.0` and :file:`libgthread-2.0.so.0` are shipped alongside it in the :file:`ovito/plugins/syslibs/`
directory of the installed package.

Linux
"""""

The OVITO Python module includes shared libraries that have been built from the unmodified sources of GLib 2.88.3.
The following commands were used to build them::

  tar xJf glib-2.88.3.tar.xz
  cd glib-2.88.3
  meson setup build \
      --prefix=/usr/local \
      --libdir=lib \
      --buildtype=release \
      --wrap-mode=nofallback \
      --force-fallback-for=pcre2 \
      -Dlibmount=disabled \
      -Dselinux=disabled \
      -Dsysprof=disabled \
      -Dman-pages=disabled \
      -Ddocumentation=false \
      -Dtests=false \
      -Dglib_debug=disabled \
      -Dintrospection=disabled
  meson compile -C build
  meson install -C build

``--force-fallback-for=pcre2`` makes the build compile PCRE2 10.46 as a subproject and link it into
:file:`libglib-2.0.so.0` statically, because the build container provides an older PCRE2 than GLib requires.
PCRE2 is distributed under the BSD-3-Clause license; its sources are available at https://github.com/PCRE2Project/pcre2.
