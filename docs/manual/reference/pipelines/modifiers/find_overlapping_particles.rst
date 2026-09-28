.. _particles.modifiers.find_overlapping_particles:

Find overlapping particles
--------------------------

.. image:: /images/modifiers/find_overlapping_particles_panel.png
  :width: 35%
  :align: right

.. versionadded:: 3.15.0

Selects particles that are within a given distance of each other. This selection can be used to subsequently remove particles
that are too close to each other or which have duplicate coordinates.

Parameters
""""""""""

Overlap distance
  The inter-particle distance (threshold) below which particles are considered overlapping and will be selected by this modifier.

Use only selected particles
  Controls whether the modifier should operate only on currently selected particles. Requires an input particle selection, which will be replaced by the output selection of overlapping particles.
  If this option is enabled, unselected input particles will be completely ignored by the modifier.

  Use this option when you want to identify overlapping particles within a specific subset of particles only.

Keep one particle unselected on overlap
  When two or more particles are within the threshold distance of each other, this option controls whether the modifier
  selects all of the particles in the overlap group or excludes one of them from the selection.

  If this option is turned on (the default), one randomly picked particle from each overlap group will *not* be selected.
  Use this mode if the goal is to delete duplicate particles and keep exactly one.

  If this option is turned off, all particles in the overlap group will be selected.
  Use this mode when the goal is to identify particles that are close to each other.

Move overlapping particles to mean position
  When two or more particles are within the threshold distance of each other, enabling this option
  causes the modifier to move all particles in the overlap group to their mean position (centroid).
  Use this option to merge near-duplicate particles into a single representative position.

To subsequently remove the selected particles from the system, you can apply a :ref:`particles.modifiers.delete_selected` modifier.

.. seealso::

  :py:class:`ovito.modifiers.FindOverlappingParticlesModifier` (Python API)