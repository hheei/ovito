.. _viewports.configure_graphics_dialog:

Viewport graphics configuration
===============================

.. image:: /images/graphics_config_dialog/graphics_config_dialog.*
  :width: 35%
  :align: right

.. versionadded:: 3.11.1

.. rubric:: How to open the configuration dialog:

Select the :menuselection:`Configure Graphics...` command from the :ref:`viewport context menu <usage.viewports.menu>`.

.. image:: /images/graphics_config_dialog/viewport_menu_graphics_config.*
  :width: 25%

Changes you make in this dialog will immediately take effect in *all* viewport windows of the application.

GPU adapter
-----------

On systems with more than one GPU, this option lets you choose which graphics device OVITO uses for rendering the
interactive viewports and for producing output images. The drop-down list shows all physical adapters detected by
the graphics driver, along with their type (Integrated / Discrete / Software).

Selecting `(Default)` leaves the choice to the underlying graphics API:

- **Windows (Direct3D 12):** DXGI enumerates adapters in performance order, so the default is normally the
  highest-performance discrete GPU.
- **Linux (Vulkan):** the Vulkan driver defines the enumeration order. In practice, hardware devices appear before
  software renderers (e.g. `lavapipe`), but the relative ordering of multiple hardware GPUs (e.g. integrated vs. discrete)
  is driver-dependent and not guaranteed. When in doubt, select an adapter explicitly.

.. note::

  On macOS, adapter enumeration is not available because Apple's Metal API always selects the GPU automatically;
  the adapter drop-down is therefore disabled on that platform.

Real-time rendering method
--------------------------

This option selects the rendering backend to be used for the interactive 3D viewports.
OVITO currently provides two options: the :ref:`rendering.standard_renderer`
and the :ref:`rendering.visrtx_renderer`, which requires NVIDIA hardware with ray-tracing capability.

.. note::

  The :ref:`VisRTX renderer <rendering.visrtx_renderer>` is included in :ref:`OVITO Pro <credits.ovito_pro>` for Windows and Linux.
  OVITO Basic only provides a demo version.

Settings management
-------------------

OVITO manages two sets of render settings: one for the interactive viewports
and one for :ref:`final-frame rendering <usage.rendering>` of high-quality output images and movies.
The settings in this dialog apply to the interactive viewports only (real-time rendering). You can
use the :guilabel:`Copy settings...` function to copy the current settings to the final-frame render settings
and vice versa.

Real-time render settings are shared between all viewport windows and get stored in the application's :ref:`configuration
file <application_settings>`. :ref:`Final-frame render settings <core.render_settings>` get stored in
:ref:`session state files <usage.saving_loading_scene>`.

For further information on the parameters of the available real-time viewport renderers,
please see the respective documentation pages:

- :ref:`rendering.standard_renderer`
- :ref:`rendering.visrtx_renderer`
