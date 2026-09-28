.. _particles.modifiers.remove_property:

Remove property
---------------

.. versionadded:: 3.16.0

The modifier lets you remove one or more properties associated with particles or bonds.
It can operate on the following kinds of property containers:

- :ref:`Particles <scene_objects.particles>`
- :ref:`Bonds <scene_objects.bonds>`
- :ref:`Surface mesh vertices/faces/regions <scene_objects.surface_mesh>`
- :ref:`Voxel grids <scene_objects.voxel_grid>`
- :ref:`Data tables <scene_objects.data_table>`
- :ref:`Lines <scene_objects.lines>`
- :ref:`Vectors <scene_objects.vectors>`

This modifier is only needed in rare situations, such as when unnecessary properties need to be discarded or are causing unwanted side effects because they :ref:`influence the representation of elements in OVITO <usage.particle_properties.special>`.
One example is the `Color` property, which stores an individual RGB value for each particle. Removing this property restores the default display behavior, in which each particle is colored according to its type.

.. seealso::

  :py:class:`ovito.modifiers.RemovePropertyModifier` (Python API)