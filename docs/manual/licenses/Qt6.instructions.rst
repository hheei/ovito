.. _appendix.license.qt6.instructions:

Build instructions for Qt6
--------------------------

*OVITO Basic* and *OVITO Pro* program packages include a binary copy of the Qt framework licensed under the GNU Lesser General Public License (LGPLv3).
In accordance with the requirements of this license, this section gives instructions on how to obtain or rebuild compatible versions of these binaries from source.

Windows
"""""""

OVITO for Windows includes an unmodified copy of the Qt shared libraries (version 6.11.2, MSVC 2022 64-bit), as distributed by the Qt Company.

Linux
"""""

OVITO for Linux includes a copy of Qt shared libraries, which have been built from the unmodified Qt sources (version 6.11.2) as follows::

  ./configure -opensource -confirm-license -shared -nomake examples -qt-libpng -qt-libjpeg -qt-harfbuzz -qt-pcre -no-use-gold-linker \
  -xcb -xkbcommon -no-cups -pch -no-eglfs -no-linuxfb -fontconfig -libinput -icu \
  -no-opengl -no-glib \
  -skip qtactiveqt -skip qtconnectivity -skip qt3d -skip qtcanvas3d -skip qtdatavis3d -skip qtcharts -skip qtlocation -skip qtsensors -skip qtdeclarative -skip qtdoc \
  -skip qtgraphicaleffects -skip qtmultimedia -skip qtquickcontrols -skip qtquickcontrols2 -skip qtpurchasing -skip qtremoteobjects -skip qtsensors \
  -skip qtserialport -skip qttranslations -skip qtwebchannel -skip qtgamepad -skip qtscript -skip qtserialbus -skip qtvirtualkeyboard \
  -skip qtwebengine -skip qtwebsockets -skip qtwebview -skip qtwebglplugin -skip qtxmlpatterns \
  -skip qt5compat -skip qtlottie -skip qtmqtt -skip qtopcua -skip qtquicktimeline -skip qtquick3d -skip qtquick3dphysics -skip qtscxml \
  -skip qtspeech -skip qtcoap -skip qthttpserver -skip qtpositioning -skip qtquickeffectmaker -skip qtgrpc -skip qtgraphs -skip qtlanguageserver -skip qtcanvaspainter \
  -prefix /usr/local/lib/qt6 \
  -- -DFEATURE_dbus_linked=OFF
  cmake --build . --parallel
  cmake --install .

The ``-no-opengl`` and ``-no-glib`` switches and the ``-DFEATURE_dbus_linked=OFF`` argument reduce the
set of system libraries the resulting Qt build depends on. OVITO renders through the Vulkan backend of
Qt's rendering hardware interface and never uses the OpenGL one, and it does not integrate with the
GLib event loop. D-Bus is loaded on demand at runtime instead of being linked.

macOS
"""""

OVITO for macOS includes an unmodified copy of the Qt framework (version 6.11.2), as distributed by the Qt Company.