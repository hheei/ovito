.. _file_formats.input.mol2:

Tripos MOL2 file reader
-----------------------

.. versionadded:: 3.15.5

For loading molecular structures stored in the `Tripos MOL2 <https://github.com/UnixJunkie/mol2-file-format-spec/blob/master/mol2.pdf>`__ format.
This section-based text format is widely used in computational drug discovery, molecular docking, and
molecular modeling tools such as DOCK, AutoDock, OpenEye OEChem, and the SYBYL software family.
OVITO can directly load gzipped MOL2 files (".gz" suffix).

The file reader processes the following ``@<TRIPOS>`` sections:

``@<TRIPOS>MOLECULE``
  The molecule name, molecule type, and charge type are read and stored as
  :ref:`global attributes <usage.global_attributes>` named ``MOL2.Molecule``,
  ``MOL2.MoleculeType``, and ``MOL2.ChargeType``.

``@<TRIPOS>ATOM``
  Reads all atoms and the following :ref:`per-atom data <scene_objects.particles>`:

  - **Position** (x, y, z)
  - **Particle Type** — the chemical element symbol derived from the SYBYL atom type
    (the prefix before the ``.`` separator, e.g. ``C`` from ``C.ar``, ``N`` from ``N.am``).
    OVITO assigns the standard CPK color for the element.
  - **Hybridization State** — the full SYBYL atom type string (e.g. ``C.3``, ``C.ar``, ``N.am``),
    stored as a :ref:`typed particle property <scene_objects.particle_types>`.
  - **Particle Identifier** — the atom serial number from the file.
  - **Atom Name** — the per-residue atom name (e.g. ``CA``, ``N``, ``CD``).
  - **Molecule Identifier** — the substructure (residue/chain) integer ID, if present.
  - **Molecule Type** — the substructure name (e.g. ``PRO1``, ``LIG``), if present.
  - **Charge** — the partial charge value, if the charge type is not ``NO_CHARGES``.

``@<TRIPOS>BOND``
  Reads all bonds and creates the following :ref:`per-bond properties <scene_objects.bonds>`:

  - **Bond Type** — named types *Single*, *Double*, *Triple*, *Aromatic*, *Amide*, *Dummy*,
    *Unknown*, and *Not Connected*, corresponding to the MOL2 bond-type codes
    ``1``, ``2``, ``3``, ``ar``/``4``, ``am``, ``du``, ``un``, and ``nc``.
  - **Bond Order** — a numeric bond order value derived from the bond type:
    single→1.0, double→2.0, triple→3.0, aromatic/amide→1.5, others→1.0.
    This property is used by the :ref:`Bonds visual element <visual_elements.bonds.fractional>`
    to render double/triple bonds as parallel cylinders and aromatic bonds as
    dashed cylinders.

``@<TRIPOS>CRYSIN``
  If present, reads the crystallographic unit cell parameters (a, b, c, α, β, γ) and converts
  them to a triclinic :ref:`simulation cell <scene_objects.simulation_cell>` with periodic
  boundary conditions enabled in all three directions. Atomic coordinates are wrapped into the unit cell
  if this record is present.  If this record is absent, an axis-aligned bounding box may be
  generated instead (see :guilabel:`Generate bounding box if needed` option below).

A MOL2 file may contain multiple molecule records (multiple ``@<TRIPOS>MOLECULE`` sections);
OVITO will load them as individual animation frames.

All other ``@<TRIPOS>`` sections (e.g. ``@<TRIPOS>SUBSTRUCTURE``, ``@<TRIPOS>UNITY_ATOM_ATTR``)
are silently skipped.

Options
"""""""

Generate bounding box if needed
  If this option is enabled and the MOL2 file does not contain a ``@<TRIPOS>CRYSIN`` record,
  OVITO will generate an axis-aligned bounding box enclosing all atoms. This bounding box has
  open boundary conditions and serves as an approximate simulation cell.

Center simulation cell on coordinate origin
  If enabled, OVITO shifts the simulation cell and all atom coordinates so that the geometric
  center of the cell coincides with the coordinate origin.

Sort particles by ID
  If enabled, OVITO reorders the imported atoms by their numeric identifiers (atom serial numbers
  from the ``@<TRIPOS>ATOM`` section) after loading. Otherwise, the atom order in the file is preserved.

.. _file_formats.input.mol2.python:

Python parameters
"""""""""""""""""

The file reader accepts the following optional keyword parameters in a call to the :py:func:`~ovito.io.import_file` or :py:meth:`~ovito.pipeline.FileSource.load` Python functions.

.. py:function:: import_file(location, sort_particles = False, bounding_box = False, centering = False)
  :noindex:

  :param sort_particles: Makes the file reader reorder the loaded atoms before passing them to the pipeline.
                         Sorting is based on the atom serial numbers loaded from the ``@<TRIPOS>ATOM`` section.
  :type sort_particles: bool
  :param bounding_box: Generate an ad-hoc simulation cell as a bounding box around the imported atoms
                       when the file contains no ``@<TRIPOS>CRYSIN`` record.
  :type bounding_box: bool
  :param centering: Translate atom coordinates and simulation cell to center them at the coordinate origin.
  :type centering: bool
