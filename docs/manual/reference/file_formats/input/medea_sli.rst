.. _file_formats.input.medea_sli:

MedeA SLI file reader
---------------------

.. versionadded:: 3.15.5

For loading structure lists and molecular dynamics trajectories stored in the native ``.sli`` file format
(Structures LIst) of the `MedeA® <https://www.materialsdesign.com/>`__ software suite by Materials Design, Inc.

Loading modes
"""""""""""""

Depending on the file contents, OVITO selects one of two loading modes:

**Structure-list mode**
  Each structure stored in the SLI file becomes one OVITO animation frame.
  The frame label is taken from the structure name, or from its molecular formula if no name is assigned.
  The atomic structure of each frame is read from the SCI data embedded in the file — see the
  :ref:`MedeA SCI file reader <file_formats.input.medea_sci>` for a description of the per-particle
  data imported in this step.

**Trajectory mode**
  When the SLI file contains trajectory data (a sequence of simulation frames), OVITO activates
  trajectory mode.  Each simulation frame becomes one OVITO animation frame, labelled with its simulation
  time as available.

  Loading proceeds in two steps per frame:

  1. The base topology (atom types, bonds, charges, etc.) is read from the first structure in the file,
     exactly as in structure-list mode.
  2. The particle coordinates and the simulation cell are then updated with the current-frame values,
     overriding the static structure data.  Periodic boundary conditions are kept as defined by the
     static structure.

Global attributes
"""""""""""""""""

In trajectory mode, OVITO imports the following scalar thermodynamic quantities as time-dependent
:ref:`global attributes <usage.global_attributes>` when they are present in the file.

.. list-table::
   :header-rows: 1
   :widths: 20 25 55

   * - OVITO attribute
     - SLI property name(s)
     - Description
   * - ``SLI.Energy``
     - ``E`` / ``Energy``
     - Total energy per frame (unit as stored, typically kJ/mol).
   * - ``SLI.Temperature``
     - ``T`` / ``Temperature``
     - Instantaneous temperature.
   * - ``SLI.Volume``
     - ``V`` / ``Volume``
     - Simulation cell volume.
   * - ``SLI.Pressure``
     - ``P`` / ``Pressure``
     - Instantaneous pressure.
   * - ``SLI.PotentialEnergy``
     - ``Epot`` / ``PotentialEnergy``
     - Potential energy contribution.
   * - ``SLI.KineticEnergy``
     - ``Ekin`` / ``KineticEnergy``
     - Kinetic energy contribution.

Options
"""""""

Generate bounding box if needed
  If this option is enabled and a frame contains no periodic cell information,
  OVITO will generate an axis-aligned bounding box enclosing all atoms. This bounding box has
  open boundary conditions and serves as an approximate simulation cell.

.. _file_formats.input.medea_sli.python:

Python parameters
"""""""""""""""""

The file reader accepts the following optional keyword parameters in a call to the :py:func:`~ovito.io.import_file` or :py:meth:`~ovito.pipeline.FileSource.load` Python functions.

.. py:function:: import_file(location, bounding_box = False)
  :noindex:

  :param bounding_box: Generate an ad-hoc simulation cell as a bounding box around the imported atoms
                       when the file contains no periodic cell information.
  :type bounding_box: bool
