.. _file_formats.input.vasp_hdf5:

VASP HDF5 file reader
---------------------

.. versionadded:: 3.16.0

This file reader imports the HDF5-based output files written by recent versions of the
`VASP <https://www.vasp.at/>`__ simulation package. The HDF5 format is a more compact and structured
alternative to the legacy text-based files (see the :ref:`file_formats.input.poscar`).
Both ``vaspout.h5`` (ionic trajectories) and ``vaspwave.h5`` (charge densities and wavefunctions) are recognized.

OVITO reads the following data from VASP HDF5 files:

- The atomic structure: ion positions, chemical element types, and the number of ions of each type.
- The :ref:`simulation cell <scene_objects.simulation_cell>` (lattice vectors).
- The full ion-dynamics trajectory of a molecular-dynamics or relaxation run. Each ionic step is
  displayed as a separate frame in the :ref:`animation timeline <usage.animation>`.
- Per-atom velocities (``Velocity`` particle property) and forces (``Force`` particle property), when present.
- The per-frame energies (e.g. free energy, total energy), which are exposed as :ref:`global attributes <usage.global_attributes>` with a
  ``VASP.`` name prefix.
- The electronic charge density field (from ``vaspwave.h5``), which is imported as a
  :ref:`voxel grid <scene_objects.voxel_grid>` with identifier ``charge-density``.
  The field values are divided by the simulation cell volume, so the ``Charge Density`` property is in
  units of electrons/Å³. For spin-polarized calculations (two spin channels), OVITO computes the
  total charge density (spin-up + spin-down) as ``Charge Density`` and the magnetization density
  (spin-up - spin-down) as a second property ``Magnetization Density``.
  When the file is first opened in the OVITO desktop application, a :ref:`particles.modifiers.create_isosurface` modifier
  is automatically inserted into the pipeline to visualize the charge density.

Trajectory files
""""""""""""""""

If the file contains an ion-dynamics trajectory (stored under the ``intermediate/ion_dynamics`` group),
OVITO loads all ionic steps as a sequence of animation frames. The lattice vectors, positions, velocities,
forces, and energies are read on a per-frame basis, so variable-cell runs (e.g. ``ISIF=3`` relaxations or
NpT molecular dynamics) are handled correctly.

Options
"""""""

Center simulation cell on coordinate origin
  If enabled, the simulation cell and atomic coordinates are translated to center the box at the coordinate origin.
  Otherwise, one corner of the simulation cell remains fixed at the coordinate origin.

Generate bonds
  Activates the generation of ad-hoc bonds connecting the atoms loaded from the file.
  Ad-hoc bond generation is based on the van der Waals radii of the chemical elements.
  Alternatively, you can apply the :py:class:`~ovito.modifiers.CreateBondsModifier` to the
  system after import, which provides more control over the generation of pair-wise bonds.

.. _file_formats.input.vasp_hdf5.python:

Python parameters
"""""""""""""""""

The file reader accepts the following optional keyword parameters in a call to the :py:func:`~ovito.io.import_file` or :py:meth:`~ovito.pipeline.FileSource.load` Python functions.

.. py:function:: import_file(location, centering = False, generate_bonds = False)
  :noindex:

  :param centering: If ``True``, the simulation cell and atomic coordinates are translated to center the box at the coordinate origin.
                    If ``False``, one corner of the simulation cell remains fixed at the coordinate origin.
  :type centering: bool

  :param generate_bonds: Activates the generation of ad-hoc bonds connecting the atoms loaded from the file.
  :type generate_bonds: bool
