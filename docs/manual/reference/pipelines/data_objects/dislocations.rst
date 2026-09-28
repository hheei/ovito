.. _scene_objects.dislocations:

Dislocation lines
-----------------

This :ref:`data object <scene_objects>` type holds a set of dislocation lines 
that have been extracted from an atomistic input crystal using the 
:ref:`particles.modifiers.dislocation_analysis` modifier,
or which have been imported from a discrete dislocation dynamics (DDD) simulation.

Each dislocation is represented by a continuous three-dimensional curve and is associated with a Burgers vector
and the type of crystal lattice it is embedded in. The table of dislocation lines is accessible
in the :ref:`data inspector <data_inspector.dislocations>` panel of OVITO. Furthermore,
it is possible to export the dislocation lines from OVITO to a VTK output file, for example.

Note that the visual appearance of dislocation lines in rendered images is controlled through the associated 
:ref:`Dislocations visual element <visual_elements.dislocations>`.

.. seealso::
  
  :py:class:`ovito.data.DislocationNetwork` (Python API)