.. _appendix.license.ffmpeg.instructions:

Build instructions for ffmpeg
-----------------------------

The OVITO package includes binary versions of the ffmpeg libraries licensed under the GNU Lesser General Public License (LGPLv2.1).
In accordance with the requirements of this license, this page provides instructions on how to rebuild compatible versions of these libraries from source code.

Windows
"""""""

OVITO for Windows includes binaries that have been built from the unmodified sources of ffmpeg 9.0.1.
The following commands have been used to generate them::

  # Compiler: Microsoft Visual C++ 2022 (command line tools) + MSYS2 environment
  ./configure \
    --toolchain=msvc \
    --target-os=win64 \
    --arch=x86_64 \
    --disable-programs \
    --disable-static \
    --enable-shared \
    --prefix=../../ffmpeg \
    --disable-zlib \
    --disable-bzlib \
    --disable-doc \
    --disable-network \
    --disable-debug \
    --disable-decoders \
    --disable-indevs \
    --disable-sdl2 \
    --disable-libxcb \
    --disable-libxcb-shm \
    --disable-libxcb-xfixes \
    --disable-libxcb-shape \
    --disable-iconv
  make install

Linux
"""""

OVITO for Linux includes shared libraries that have been built from the unmodified sources of ffmpeg 9.0.1.
The following commands have been used to generate them::

  ./configure \
      --enable-pic \
      --enable-shared \
      --disable-static \
      --disable-doc \
      --disable-network \
      --disable-programs \
      --disable-debug \
      --disable-decoders \
      --disable-indevs \
      --disable-libxcb \
      --disable-libxcb-shm \
      --disable-libxcb-xfixes \
      --disable-libxcb-shape \
      --disable-bzlib \
      --disable-zlib \
      --disable-xlib \
      --disable-iconv \
      --disable-sdl2 \
      --disable-libdrm \
      --disable-vaapi \
      --disable-vdpau \
      --disable-vulkan \
      --disable-v4l2-m2m
  make install

The hardware acceleration back-ends are turned off because OVITO only uses ffmpeg's software
encoders. Disabling them also removes the resulting libraries' dependency on *libdrm*.

macOS
"""""

OVITO for macOS includes shared libraries that have been built from the unmodified sources of ffmpeg 9.0.1.
The following commands have been used to generate them::

  git clone https://git.ffmpeg.org/ffmpeg.git ffmpeg_source
  cd ffmpeg_source
  git checkout n9.0.1
  ./configure \
    --disable-network \
    --disable-programs \
    --disable-debug \
    --disable-doc \
    --disable-static \
    --disable-decoders \
    --disable-indevs \
    --disable-sdl2 \
    --disable-libxcb \
    --disable-libxcb-shm \
    --disable-libxcb-xfixes \
    --disable-libxcb-shape \
    --disable-iconv \
    --disable-bzlib \
    --disable-zlib \
    --disable-xlib \
    --enable-shared
  make install
