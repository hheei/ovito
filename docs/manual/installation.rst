.. _installation:

============
Installation
============

.. _installation.requirements:

System requirements
===================

Operating System Compatibility:

Windows:
  Windows 10 (21H2 or later), Windows 11 (21H2 or later) on x86_64 processor architecture.
Linux:
  Any distribution with glibc >= 2.28 running on x86_64 processors, for example RHEL/Rocky/AlmaLinux 8+,
  Ubuntu 20.04+, Debian 10+, SLES 15 SP1+, Fedora 29+ or ALT Linux 10+.
  The program package brings along all libraries it needs, including a software Vulkan driver for machines
  without a GPU, and therefore runs without installing any additional system packages. Only the graphical
  user interface additionally requires the X client libraries of the target system, see
  :ref:`installation.troubleshooting.linux`.
  Additionally, aarch64 (ARM64) processors are supported on distributions with glibc >= 2.39 (e.g. Ubuntu 24.04+, Fedora 40+, Debian 13+).
macOS:
  macOS 13.0+, Apple Silicon (arm64 architecture).

.. _installation.instructions:

Installation instructions
=========================

Download a binary program package for *OVITO Basic* or *OVITO Pro* from `www.ovito.org <https://www.ovito.org/#download>`__.

*Linux*:
    Extract the downloaded `.tar.xz` archive using the tar utility (e.g. :command:`tar xJfv ovito-{{OVITO_VERSION_STRING}}-x86_64.tar.xz` for x86_64 or :command:`tar xJfv ovito-{{OVITO_VERSION_STRING}}-aarch64.tar.xz` for ARM64).
    This creates a subdirectory containing the program files. Change to that directory and run the application: :command:`./bin/ovito`.

*Windows*:
    Run the installer program :file:`ovito-{{OVITO_VERSION_STRING}}-win64.exe` to install OVITO in a directory of your choice.
    Follow the on-screen instructions to install OVITO. Note: Windows may prompt you to confirm before running an installer downloaded from a website.

*macOS*:
    Double-click the downloaded :file:`.dmg` disk image file to open it, agree to the license terms, and drag the :program:`Ovito` bundle into your :file:`Applications` folder.
    You can then launch OVITO by double-clicking the application bundle.

**Anaconda installation:** You can also install the software via the Anaconda package manager from
the `conda-forge channel <https://anaconda.org/conda-forge/ovito>`__ (*OVITO Basic*) or from :ref:`our own conda channel <pydoc:installation.anaconda>` (*OVITO Pro*).

**Unattended installation:** The Windows installers support `unattended installation via command-line parameters <https://nsis.sourceforge.io/Docs/Chapter3.html#installerusage>`__
:command:`/S` and :command:`/D` from an administrator command prompt. Furthermore, Windows versions of OVITO are available as .zip archives
`here <https://www.ovito.org/download_history/>`__ and can simply be extracted to a directory of your choice
(`Microsoft Visual C++ Redistributable <https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist>`__ may need to be installed separately).

.. _installation.python:

Installing the OVITO Python module
==================================

To use OVITO Pro's functionality in standalone Python scripts, install the *OVITO Python module* in your Python environment.
It can be used for free and doesn't require a license key. Refer to :ref:`this section <pydoc:installing_python_module>` for detailed instructions.

.. _installation.remote:

Running OVITO remotely
======================

Simulation data is often stored on a high-performance computing (HPC) cluster, while the desktop
computer you sit at is somewhere else. There are three ways to bridge that gap. They are listed here
in the order in which we recommend them:

1. **Local installation with remote file access**

   Install OVITO on your own computer and open the files stored on the remote machine through the
   :ref:`built-in SSH file transfer function <usage.import.remote>`. All rendering happens on your local
   graphics hardware; only the data files travel over the network. Nothing needs to be installed on the
   cluster, and this option works equally well on Windows, macOS, and Linux.

2. **Batch processing on the cluster**

   Run the analysis and rendering as a non-interactive job on the cluster itself, using the
   :ref:`OVITO Python module <pydoc:installing_python_module>` or the :command:`ovitos` :ref:`script interpreter <pydoc:ovitos_interpreter>`,
   and transfer the resulting images and data files to your local computer afterwards. OVITO's rendering
   engine works headless on Linux: it requires no desktop session, display server, or X11/Wayland
   connection and therefore runs in an SSH terminal, in a batch job on a compute node, or inside a
   container. It uses the GPU of the node if one is available (see the ``OVITO_GPU_ADAPTER`` environment
   variable), and otherwise falls back to the software Vulkan driver shipped with the Linux program
   package (see :ref:`installation.troubleshooting.vulkan`).

3. **Remote desktop session on the cluster**

   If you need the interactive graphical user interface on the remote machine, for instance because the
   data sets are too large to transfer, run OVITO inside a remote desktop session. Any solution that
   provides a complete desktop session on the remote machine and streams it to your computer works:
   `TurboVNC <https://www.turbovnc.org/>`__ or `TigerVNC <https://tigervnc.org/>`__, NoMachine, X2Go,
   xrdp, or the browser-based *Interactive Desktop* of an `Open OnDemand <https://openondemand.org/>`__
   portal. Ask your HPC administrator which of these is offered on your cluster.
   Inside such a session, OVITO finds the display server connection it needs (X11 or Wayland).

.. note::

  **SSH with X11 forwarding** (:command:`ssh -X`) is not recommended for running the OVITO desktop
  application. Every frame OVITO renders on the remote machine has to be transmitted as an uncompressed
  bitmap to your local X server, which makes the interactive viewports very sluggish over anything but a
  fast local network, and depending on the graphics driver on the remote machine, viewport rendering may
  not work at all.

.. _installation.troubleshooting:

Troubleshooting
===============

If you experience installation issues, visit the `OVITO user forum <https://matsci.org/c/ovito/>`__ for support.
*OVITO Pro* users can also contact `customer support <https://www.ovito.org/contact/>`__ directly. The OVITO team will be happy to help you.

Common installation issues and solutions:

  - :ref:`Linux troubleshooting <installation.troubleshooting.linux>`
  - :ref:`Windows troubleshooting <installation.troubleshooting.windows>`
  - :ref:`macOS troubleshooting <installation.troubleshooting.macos>`

.. _installation.troubleshooting.linux:

Linux
-----

Missing shared object files or broken links
  .. error::

    Starting the desktop application :command:`ovito` or the script interpreter :command:`ovitos` may fail with the following error::

      ./ovito: error while loading shared libraries: libQt6DBus.so.6:
              cannot open shared object file: No such file or directory

    This error is typically caused by broken symbolic links in the :file:`lib/ovito/` sub-directory of the OVITO installation after
    extracting the installation package for Linux on a Windows computer.

  .. admonition:: Solution

    Reinstall OVITO by extracting the `.tar.xz` archive on the target machine.
    Do *not* transfer the directory tree between different computers after it has been extracted,
    because this can easily break symbolic links between files.

Missing XCB system libraries
  .. error::

    You may see the following error when running :command:`ovito` on a Linux machine::

      qt.qpa.plugin: Could not load the Qt platform plugin "xcb" in "" even though it was found.
      This application failed to start because no Qt platform plugin could be initialized.
      Reinstalling the application may fix this problem.
      Available platform plugins are: minimal, offscreen, ovitoheadless, vnc, wayland, xcb.

    OVITO's graphical user interface talks to the X server through the X client libraries of
    the system. OVITO brings along the small helper libraries of the *xcb-util* family, but the
    core X libraries have to come from the machine itself, because they have to match its X
    server and its locale data. Minimal installations and container images do not preinstall them.

    This concerns the desktop application only. The :command:`ovitos` script interpreter, the
    OVITO Python module and :command:`ovito --nogui` need none of these libraries and run on a
    system where nothing at all has been installed.

  .. admonition:: Solution

    First find out which library is actually missing. The message above does not say, but Qt
    will name it:

    .. code-block:: shell

      QT_DEBUG_PLUGINS=1 ./bin/ovito 2>&1 | grep -i "cannot load"

    Then install the libraries using your system's package manager:

    .. code-block:: shell

      # On Ubuntu/Debian systems:
      sudo apt install libice6 libsm6 libx11-6 libx11-xcb1 libxcb1 libxcb-randr0 \
               libxcb-render0 libxcb-shape0 libxcb-shm0 libxcb-sync1 libxcb-xfixes0 libxcb-xkb1

      # On RHEL/Rocky/AlmaLinux/Fedora systems:
      sudo dnf install libICE libSM libX11 libX11-xcb libxcb

    Debian users should also pay attention to `this thread in the OVITO support forum <https://www.ovito.org/forum/topic/installation-problem/#postid-2272>`__.

Installing the X libraries without root privileges
  .. error::

    On a shared machine such as an HPC cluster node you may have neither administrator rights
    nor an internet connection, and the command above fails::

      sudo: a password is required

  .. admonition:: Solution

    The libraries do not have to be installed system-wide. Download the packages on a machine
    that does have internet access, transfer them to the target machine, and unpack them into
    your home directory.

    .. code-block:: shell

      # On RHEL/Rocky/AlmaLinux/Fedora systems, using the .rpm packages:
      mkdir -p ~/extralibs && cd ~/extralibs
      rpm2cpio ~/downloads/libX11-*.rpm | cpio -idmv

      # On Ubuntu/Debian systems, using the .deb packages:
      mkdir -p ~/extralibs && cd ~/extralibs
      dpkg-deb -x ~/downloads/libx11-6_*.deb .

    Then point the dynamic linker at that directory when starting OVITO:

    .. code-block:: shell

      LD_LIBRARY_PATH=~/extralibs/usr/lib64 ./bin/ovito                    # RHEL and derivatives
      LD_LIBRARY_PATH=~/extralibs/usr/lib/x86_64-linux-gnu ./bin/ovito     # Debian and Ubuntu

    Alternatively, install OVITO Pro from :ref:`our conda channel <pydoc:installation.anaconda>`
    into a `Miniconda <https://docs.anaconda.com/miniconda/>`__ environment in your home
    directory, which pulls in the dependencies without administrator rights. This requires an
    internet connection on the target machine.


LD_LIBRARY_PATH overrides OVITO's bundled Qt libraries
  .. error::

    Launching :command:`ovito` may fail with an error such as::

      ./bin/ovito: /usr/lib/x86_64-linux-gnu/libQt6Core.so.6: version `Qt_6.10' not found (required by ovito)

    or a similar message pointing to an incompatible copy of a shared library needed by the program.

  .. admonition:: Solution

    OVITO ships its own Qt libraries in :file:`lib/ovito/`.
    If the environment variable ``LD_LIBRARY_PATH`` lists a system directory
    like :file:`/usr/lib/`, the dynamic linker loads the wrong *libQt6Core.so.6*
    before OVITO's own version, causing the mismatch above.

    On HPC clusters this is most often caused by the environment module system, whose
    modules put their own Qt, Mesa or compiler directories on ``LD_LIBRARY_PATH``. Running
    :command:`module purge` before starting OVITO resolves it.

    Either start OVITO with a clean library path

    .. code-block:: shell

      env -u LD_LIBRARY_PATH ./bin/ovito          # temporarily ignore LD_LIBRARY_PATH

    or remove the offending directories from ``LD_LIBRARY_PATH`` in your shell configuration.

.. _installation.troubleshooting.vulkan:

Missing Vulkan support on headless servers, containers, or virtual machines
  .. error::

    On a Linux machine without a GPU, or one where the Vulkan runtime is not installed, :command:`ovito` or
    :command:`ovitos` may print messages such as::

      Failed to load vulkan: Cannot load library vulkan: vulkan: cannot open shared object file: No such file or directory
      initInstance: No Vulkan library available
      Failed to create platform Vulkan instance

    followed by an error reporting::

      Could not initialize the Vulkan graphics backend. This typically means no compatible GPU or
      graphics driver was found.

    OVITO's rendering engine relies on the Vulkan API even when only software (CPU-only) rendering is
    needed, for example when running batch jobs on a headless HPC cluster or inside a Docker container
    without GPU passthrough.

  .. admonition:: Solution

    The Linux program package includes Mesa's *lavapipe* software Vulkan driver, which emulates a GPU on
    the CPU and needs no graphics hardware. OVITO activates it automatically whenever the machine provides
    no Vulkan driver of its own, so the errors above should not occur with the :file:`.tar.xz` package.
    Set the following environment variable to override that automatic choice:

    .. code-block:: shell

      export OVITO_VULKAN_FALLBACK=1   # always use the bundled software driver
      export OVITO_VULKAN_FALLBACK=0   # never use the bundled software driver

    Software rendering is considerably slower than a hardware-accelerated GPU driver. If the machine does
    have a GPU, install the vendor's native Vulkan driver - the proprietary NVIDIA driver, or
    ``mesa-vulkan-drivers`` for AMD/Intel GPUs, which most distributions already include. OVITO then uses
    that driver in preference to the bundled software one:

    .. code-block:: shell

      # On Ubuntu/Debian systems:
      sudo apt install libvulkan1 mesa-vulkan-drivers

      # On RHEL/Rocky/AlmaLinux/Fedora systems:
      sudo dnf install vulkan-loader mesa-vulkan-drivers

    The software driver is part of the program package only. The *OVITO Python module* installed with
    ``pip`` and the :ref:`Anaconda package <pydoc:installation.anaconda>` do not include it and still
    require a Vulkan driver to be available for rendering. Without administrator privileges, for example
    on a shared HPC cluster, install one into your conda environment. Note that the Vulkan loader does not
    search the conda environment's :file:`share/vulkan/icd.d/` directory on its own, so the location of the
    driver manifest must be made known to it using the ``VK_DRIVER_FILES`` environment variable:

    .. code-block:: shell

      conda install -c conda-forge mesa-lavapipe
      export VK_DRIVER_FILES=$CONDA_PREFIX/share/vulkan/icd.d

.. _installation.troubleshooting.windows:

Windows
-------

Windows 7 not supported
  .. error::

    If you try to run OVITO 3.7 or later on a Windows 7 computer, it will fail with the error "*The procedure entry point CreateDXGIFactory2 could not be
    located in the dynamic link library dxgi.dll*".

  .. admonition:: Solution

    Modern versions of OVITO are based on the Qt6 cross-platform framework, which `requires Windows 10 or later to run <https://doc.qt.io/qt-6/supported-platforms.html>`__.
    Windows 7 has reached its end of life and is no longer supported. Please upgrade your Windows operating system.

.. _installation.troubleshooting.macos:

macOS
-----

OVITO Pro license activation fails
  .. error::

    License activation fails due to an issue with the local license information store (file path :file:`$HOME/.config/Ovito/LicenseStore.ini`).
    Please check if file access permissions are correctly set. OVITO Pro requires read/write access to this filesystem path.

  .. admonition:: Solution

    OVITO Pro requires read/write access to :file:`$HOME/.config/Ovito/` for storing the license activation.

    - If this directory does not exist, OVITO Pro will attempt to create it. If permission issues arise, check ownership of :file:`$HOME/.config/`.
    - This directory may be owned by the system administrator (`root`), preventing modifications by applications running under your personal user account.
      In such cases, a system administrator must grant write access.

    In many cases, the issue stems from the parent directory, :file:`$HOME/.config/`, being owned by the wrong macOS user account.
    This directory serves as the `canonical storage location <https://specifications.freedesktop.org/basedir-spec/latest/>`__ for
    application configuration data on Linux/Unix systems. Unlike Linux, macOS does not create :file:`$HOME/.config/` by default;
    instead, individual applications such as OVITO create it when first run. As a result, its ownership and permissions may vary
    depending on which user account initiated its creation.

    If :file:`$HOME/.config/` was originally created by the system administrator (`root`), your personal user account may lack
    write access, preventing OVITO Pro from modifying it. This causes license activation to fail. To fix this issue:

    - Ask your system administrator to create :file:`$HOME/.config/Ovito/` and grant write access to your user account.
    - Alternatively, follow `this guide <https://apple.stackexchange.com/a/320686>`__ to correct ownership of :file:`$HOME/.config/` yourself.

    If changing ownership is not possible, you can set the environment variable ``XDG_CONFIG_HOME`` (`info <https://specifications.freedesktop.org/basedir-spec/latest/#variables>`__)
    to point to a different writable directory:

    .. code-block:: shell

      export XDG_CONFIG_HOME=/path/to/writable/directory

    This redirects OVITO Pro to store its licensing information in a user-specified location.
