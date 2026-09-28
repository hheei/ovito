.. _viewport_layers.color_legend:

Color legend layer
------------------

.. image:: /images/viewport_layers/color_legend_overlay_panel.*
   :width: 35%
   :align: right

This :ref:`viewport layer <viewport_layers>` renders the color scale for a :ref:`Color coding <particles.modifiers.color_coding>` modifier
or displays the discrete colors associated with a :ref:`typed property <scene_objects.particle_types>`.
The following figure shows two typical examples of color legends: A color map for a continuous particle property
and a discrete color legend for the ``Particle Type`` property:

.. image:: /images/viewport_layers/color_legend_example.*
   :width: 60%

Color legend source
"""""""""""""""""""

The legend obtains the displayed :guilabel:`Color mapping` from the selected :guilabel:`Pipeline`.
In the current program version, three types of color mapping sources are shown in the drop-down list:

Color coding modifier
  If the selected data pipeline contains a :ref:`Color coding <particles.modifiers.color_coding>` modifier, the legend can display its
  gradient and min/max value interval.

Typed properties
  Alternatively, the legend can display a list of discrete element types associated with a :ref:`typed property <scene_objects.particle_types>` in the pipeline's output.
  Typical examples are the particle properties ``Particle Type``, ``Structure Type`` or ``Residue Type``.
  The legend will display the name and color of each type defined for the selected source property.

  Types that would look exactly the same in the legend, i.e., which agree in name, color, and shape, are listed only once.
  Molecular datasets, for example, typically define one force field type per chemical environment of an atom, and all of them
  stand for the same chemical element. The legend then shows a single entry per element instead of repeating it several times.

  .. note::

    The color legend layer itself is not responsible for coloring the particles or bonds.
    It may be necessary to insert a :ref:`Color by type <particles.modifiers.color_by_type>` modifier
    into the pipeline to actually give particles and bonds their corresponding colors.

Pseudo-color mappings
  Some :ref:`visual elements <visual_elements>` provide an option to color
  discrete elements or surfaces based on some local property. If such a color mapping function has been activated
  for a visual element in the scene, you can select it here as source for the color legend.

Options
"""""""

The color legend layer provides parameters that let you control its size, positioning, and appearance in the rendered picture.
By default, the legend will be labeled with the name of the source property and, if based on a :ref:`Color coding <particles.modifiers.color_coding>` modifier, the
numeric range set for that modifier. You can **override the text** of the labels by entering it into the input fields :guilabel:`Caption` and :guilabel:`Label 1`/:guilabel:`Label 2`.

.. tip::

  You can include HTML markup elements in the labels to format the text, e.g., to produce special notations such as superscripts or subscripts.
  See :ref:`viewport_layers.text_label.text_formatting` for further information.

If the legend displays a min/max range, the **numeric format** of the limits is controlled by a format specification string.
You have the choice between decimal notation (``%f``), exponential notation (``%e``), and an
automatic mode (``%g``), which picks the best representation depending on the value's magnitude.
The format string also gives you explicit control over the displayed numeric precision, i.e., the number of digits that
appear after the decimal point. Use ``%.2f``, for example, to always show two digits after the decimal point.
The format string must follow the rules of the standard `printf() C function <https://en.cppreference.com/w/cpp/io/c/fprintf>`__.

.. tip::

  You can also include physical units or other literal text in the number format string, e.g., ``%g eV``.
  The literal parts of the format string will then be copied to the numeric labels.

**Tick marks** can be added to the color legend by checking the :guilabel:`Tick marks` box. By default, OVITO places the ticks automatically,
but you can take manual control of the tick spacing by entering a value into the :guilabel:`Spacing` field.
The first tick gets placed at the next integer multiple of the tick spacing which is greater than the lower limit of the current value range.
The total number of ticks is currently limited to 100 by OVITO.

.. _viewport_layers.color_legend.type_shapes:

Showing actual particle shapes in the legend
""""""""""""""""""""""""""""""""""""""""""""

.. versionadded:: 3.16.1

For legends based on a :ref:`typed property <scene_objects.particle_types>`, the option :guilabel:`Show type shapes` replaces the generic
colored boxes with the actual three-dimensional shapes of the element types. For particle types, which typically represent chemical elements,
this renders the atomic spheres just as they appear in the 3d visualization -- including their shapes, colors, and relative sizes.
This makes it much easier to associate the legend entries with the elements shown in the picture.

The symbols are rendered offscreen using the same rendering engine that produces the main image, so that their appearance is
consistent with the rest of the visualization.

The option is turned on by default. Element types that have no three-dimensional representation, for example structure types,
are not affected by it and keep their flat colored box. Whenever the types really are rendered as 3d shapes, the legend cells
become square, i.e., the :guilabel:`Aspect ratio` parameter no longer applies -- the overall :guilabel:`Legend size` alone then
controls how large the symbols appear. The :guilabel:`Border` option is ignored in this mode as well, because the symbols are
drawn on a transparent background and a frame around them would turn that background into an opaque block. Both parameters
remain available for types without a shape.

.. seealso::

  :py:class:`ovito.vis.ColorLegendOverlay` (Python API)
