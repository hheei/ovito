.. _file_formats.input.medea_sci:

MedeA SCI file reader
---------------------

.. versionadded:: 3.15.5

For loading molecular and crystalline structures stored in the native ``.sci`` file format of the
`MedeA® <https://www.materialsdesign.com/>`__ software suite by Materials Design, Inc.
This section-based text format supports non-periodic molecular systems, periodic crystalline structures,
and mesoscale coarse-grained models.

The file header must start with ``#MD System 2.0`` or ``#SciCo System 1.0`` (legacy).
An optional ``@Title`` directive sets the ``SCI.Title`` :ref:`global attribute <usage.global_attributes>`.

The file reader processes the following table sections:

``Atom``
  Reads all atoms in a non-periodic or periodically-expanded system (Cartesian coordinates) and imports:

  - **Position** (x, y, z)
  - **Particle Type** — the chemical element derived from the ``AtomicNumber`` column.
    OVITO assigns standard CPK colors for each element.
  - **Particle Identifier** — from the ``Point`` column (1-based atom index).
  - **Atom Name** — per-atom name string.
  - **Force-Field Type** — SYBYL/CHARMM force-field atom type (``FFAtomType`` column), if present.
  - **Charge** — partial charge from the ``FFCharge`` column, if present.
  - **Mass** — atomic mass from the ``Mass`` column, if present.

``AsymmetricAtom``
  Present in periodic systems that have not been expanded to the full unit cell.
  Contains fractional coordinates and the same optional columns as ``Atom`` above, plus:

  - **Spin** — magnetic spin value from the ``Spin`` column, if present.

  When an ``AsymmetricAtom`` table is found without a corresponding expanded ``Atom`` table,
  OVITO uses the `gemmi crystallography library <https://github.com/project-gemmi/gemmi>`__ to generate the full P1 unit cell
  by applying all symmetry operations of the space group.

  .. note::

     **Bond topology of symmetry-reduced files with centred lattices.**
     When a structure is stored with reduced symmetry (only an ``AsymmetricAtom`` table) and the
     space group has a *centred* Bravais lattice (lattice symbol ``A``, ``B``, ``C``, ``I``, ``F`` or ``R``,
     e.g. the common monoclinic setting :math:`C2/c`), OVITO may connect the wrong pairs of atoms when
     rebuilding the bond network after the symmetry expansion. The atomic positions, the simulation cell,
     and the number of bonds are correct, but individual bonds can be attached to symmetry-equivalent
     atoms other than the intended ones.

     The reason is that the ``Bond`` table references atoms by MedeA's internal enumeration order, which
     OVITO must reproduce in order to remap the bonds onto its own atom ordering. For *primitive* lattices
     this order can be reconstructed reliably, but for centred lattices MedeA derives it from proprietary
     internal tables (and from a canonical site representative that need not match the coordinate stored in
     the file), so it cannot be reproduced from the file alone.

     If you need correct bond topology for such a structure, export it from MedeA in its fully expanded
     (``P1``) form, in which all atoms and bonds are listed explicitly and no symmetry expansion is required.

``Cell``
  Reads the unit cell parameters and sets up a periodic :ref:`simulation cell <scene_objects.simulation_cell>`:

  - Lattice parameters (a, b, c, α, β, γ) are converted to a triclinic cell matrix.
  - Periodic boundary conditions are enabled in all three directions.
  - The space group name and number are stored as the :ref:`global attributes <usage.global_attributes>` ``SCI.SpaceGroup`` and
    ``SCI.SpaceGroupNumber``.

``Bond``
  Reads all bonds and creates:

  - **Bond Topology** — the two connected atom indices.
  - **Bond Type** — named types *Single*, *Aromatic*, *Double*, and *Triple*,
    corresponding to the SCI bond-order integers 0, 1, 2, and 3.
  - **Bond Order** — a numeric value: single→1.0, aromatic→1.5, double→2.0, triple→3.0.
    This property is used by the :ref:`Bonds visual element <visual_elements.bonds.fractional>`
    to render double/triple bonds as parallel cylinders and aromatic bonds as dashed cylinders.
  - **Periodic Image** — integer cell-offset vector for bonds across periodic boundaries
    (from the ``CellOffset2`` column), present in periodic systems.

``AsymmetricBond``
  Present alongside ``Bond`` in periodic systems. Stores the bond order (``Order`` column)
  which is looked up when processing expanded ``Bond`` rows.

``Bead``
  Present in mesoscale coarse-grained systems. Each row defines one bead type and provides:

  - **Name** — bead type label (e.g. ``C1``, ``Qa``), used as the particle type name.
  - **Mass** — bead mass in g/mol, stored as the type mass.
  - **Radius** — bead radius in Å, stored as the type display radius.
  - **Color** — RGB color in [0, 1] range, stored as the type display color.
  - **Charge** — bead charge.

  When a ``Bead`` table is present and the ``Atom`` table (non-periodic systems) or the
  ``AsymmetricAtom`` table (periodic systems) contains a ``Bead`` reference column,
  the system is treated as a mesoscale model: particle types are taken from bead type names
  instead of chemical element symbols.

``Properties``
  Reads molecule-level scalar quantities (non-periodic systems only):

  - ``Charge`` → stored as the ``SCI.MolecularCharge`` :ref:`global attribute <usage.global_attributes>`.
  - ``SpinMultiplicity`` → stored as the ``SCI.SpinMultiplicity`` :ref:`global attribute <usage.global_attributes>`.

All other table sections are silently ignored.

Options
"""""""

Generate bounding box if needed
  If this option is enabled and the SCI file contains no ``Cell`` table,
  OVITO will generate an axis-aligned bounding box enclosing all atoms. This bounding box has
  open boundary conditions and serves as an approximate simulation cell.

Center simulation cell on coordinate origin
  If enabled, OVITO shifts the simulation cell and all atom coordinates so that the geometric
  center of the cell coincides with the coordinate origin.

Sort particles by ID
  If enabled, OVITO reorders the imported atoms by their numeric identifiers (from the ``Point`` column)
  after loading. This is useful when the atom storage order in the file does not match the identifier sequence,
  for example after symmetry expansion of an ``AsymmetricAtom`` table.

.. _file_formats.input.medea_sci.python:

Python parameters
"""""""""""""""""

The file reader accepts the following optional keyword parameters in a call to the :py:func:`~ovito.io.import_file` or :py:meth:`~ovito.pipeline.FileSource.load` Python functions.

.. py:function:: import_file(location, sort_particles = False, bounding_box = False, centering = False)
  :noindex:

  :param sort_particles: Makes the file reader reorder the loaded atoms before passing them to the pipeline.
                         Sorting is based on the atom identifiers from the ``Point`` column of the SCI file.
  :type sort_particles: bool
  :param bounding_box: Generate an ad-hoc simulation cell as a bounding box around the imported atoms
                       when the file contains no ``Cell`` table.
  :type bounding_box: bool
  :param centering: Translate atom coordinates and simulation cell to center them at the coordinate origin.
  :type centering: bool
