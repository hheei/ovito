.. _modifiers.calculate_local_entropy:

Calculate local entropy |ovito-pro|
-----------------------------------

.. figure:: /images/modifiers/calculate_local_entropy_example.*
  :figwidth: 30%
  :align: right

  Distribution of per-atom local entropy values in a nanocrystalline 
  palladium microstructure

This is a :ref:`Python-based modifier function <particles.modifiers.python_script>` computing the local pair entropy fingerprint
of each particle as described in:

  | P.M. Piaggi and M. Parrinello, 
  | Entropy based fingerprint for local crystalline order,
  | J. Chem. Phys. 147, 114112 (2017)
  | https://doi.org/10.1063/1.4998408

The modifier function stores the computed pair entropy values, which are always negative, in the output particle property ``Entropy``. 
Lower entropy values correspond to more ordered structural environments. 
The modifier function does not take into account the types of the input particles; it assumes the model is a single-component system.

The calculation algorithm follows closely the C++ implementation of the 
`compute entropy/atom <https://docs.lammps.org/compute_entropy_atom.html>`__ command of the LAMMPS MD code.  
The meaning of the input parameters of the modifier is the same as for the LAMMPS command.

Parameters
""""""""""

.. image:: /images/modifiers/calculate_local_entropy_panel.*
  :width: 30%
  :align: right

Cutoff
  Cutoff distance for the :math:`g(r)` calculation.

Sigma
  Width of Gaussians used in the :math:`g(r)` smoothing.

Use local density
  Use the local density around each atom to normalize the :math:`g(r)`.

Compute average
  Average the pair entropy values over neighbors.

Average cutoff
  Cutoff distance for the averaging over neighbors.

.. seealso::

  :ref:`Source code of this modifier function <example_calculate_local_entropy>`