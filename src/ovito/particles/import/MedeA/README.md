# MedeA SCI/SLI file readers — developer notes

This directory contains the importers for the native file formats of the
[MedeA®](https://www.materialsdesign.com/) materials-modeling suite by Materials Design, Inc.:

- `MedeA_SCI_Importer` — single structures (`.sci`), a section-based Tcl-list text format.
- `MedeA_SLI_Importer` — structure lists / trajectories (`.sli`); reuses `loadFileBody()` of the SCI reader.

Most of the format is straightforward (see the user manual,
`doc/manual/reference/file_formats/input/medea_sci.rst`). This note documents the one genuinely
tricky part: **reconstructing the bond topology of symmetry-reduced periodic structures.** It is
written so that a future maintainer does not have to rediscover everything from scratch.

## The symmetry-expansion problem

A periodic SCI file can store a structure in two ways:

1. **Fully expanded (P1):** an explicit `Atom` table (Cartesian) — or an `AsymmetricAtom` table whose
   space group is P1. Every atom and every bond is listed explicitly. Nothing special is needed.

2. **Symmetry-reduced:** only an `AsymmetricAtom` table (fractional coordinates) plus a `Cell` table
   naming a space group with more than one symmetry operation. OVITO must expand this to P1 itself.

For case 2 the reader expands the asymmetric atoms with gemmi (`SpaceGroup::operations()`), producing
OVITO's atom list. The catch is the **`Bond` table**: its `Atom1`/`Atom2` columns reference atoms by
**MedeA's own expanded-atom index**, i.e. the order in which *MedeA* would have enumerated the
symmetry-equivalent atoms — which is **not** gemmi's order. To build OVITO's bond topology we must map
each MedeA index to the corresponding OVITO (gemmi) index. That mapping is `medeaToOurIdx`.

Building `medeaToOurIdx` requires reproducing MedeA's enumeration order, then matching the generated
positions against OVITO's expanded atoms by position lookup.

## How MedeA enumerates atoms (what we learned)

MedeA's order comes from its internal **BYU / Miller–Love** space-group machinery (confirmed by the
MedeA developers). The relevant facts, distilled from sample files, P1 counterparts, and developer trace
logs (see `tests/files/data/MedeA/9002806.txt` and `5-Aminopyrimidine_COD_2207775.txt`):

- Atoms are grouped contiguously **by asymmetric atom**, in file order. The size of each group is the
  site multiplicity. OVITO's own expansion uses the same grouping, so only the **within-group order**
  can differ.
- Within a group, MedeA generates the orbit as: take a **canonical Wyckoff-site representative**, then
  apply the space-group operations in MedeA's internal order, deduplicating, then apply the lattice
  centering translations as an **outer loop** (centering translations come *after* the pure point
  operations; see the "Centering point" lines in the trace).
- MedeA's operation order is *not* gemmi's, and is *not* the International Tables order either. But when
  the operations are expressed in the space group's **reference setting** (via gemmi's `basisop`
  change-of-basis operator), MedeA's order matches a fixed canonical scheme:
  - **proper rotations first**, then improper operations;
  - within each, ordered by the reference-frame rotation matrix (descending, with the identity-like
    matrix first; improper operations ranked like their `-R` proper equivalent so that, e.g., the
    inversion sorts like the identity and a mirror like its 2-fold);
  - lattice centering translations form the outer loop (origin first).

This is exactly what `reconstructMedeaOperationOrder()` implements, and it reproduces MedeA's order
**exactly for all primitive Bravais lattices** (lattice symbol `P`). This includes non-standard settings
such as Pbnm (a setting of Pnma, #62): the reference-setting trick handles the axis permutation.

## The remaining limitation (centred lattices)

For **centred** lattices (`A`/`B`/`C`/`I`/`F`/`R`) the reconstruction is **not reliable** and bonds may
be attached to the wrong (but symmetry-equivalent) atoms. Atomic positions, the cell, and the bond
*count* are still correct; only the topology can be wrong.

Root cause, observed directly in the trace logs:

- MedeA does not expand from the asymmetric coordinate stored in the file. It first maps that coordinate
  to a **canonical Wyckoff-site representative** of its internal tables, which can differ from the file
  coordinate by a symmetry operation or a centering translation. Examples:
  - `5-Aminopyrimidine` (C2/c): file site `(0.101, 0.539, 0.245)` → MedeA representative
    `(-0.399, 0.039, 0.245)` (differs by the C-centering vector).
  - `catena-…-cadmium` (I2/a): file site `(0.25, 0.25, 0.25)` → MedeA's first orbit atom
    `(0.25, 0.25, 0.75)` (differs by a 2-fold operation).
- Because OVITO expands from the *file* coordinate, it generates the same set of positions but in a
  within-group order that is shifted relative to MedeA's. The `Bond` indices then refer to differently
  ordered atoms, so `medeaToOurIdx` is wrong for the affected groups.

Reproducing the representative requires the proprietary BYU tables, so it cannot be done from the file
alone. The MedeA developers themselves describe the ordering as non-trivial and tied to that internal
database (`...BYUSymmetry.tcl.in`, `get_wyckoff_all.f`, `wyc_pg_cosets.f`, `ispace_elements`).

Separately, the rhombohedral test case `Trisethyl…cobaltIII` (R-3, hexagonal setting #148) shows a plain
**expansion count discrepancy**: gemmi produces 226 atoms where MedeA's P1 file has 222. This is an
expansion/deduplication mismatch independent of the bond-remapping issue and was not investigated in depth.

## Approaches that were tried and rejected

To avoid depending on MedeA's order, several **order-independent, purely geometric** bond
reconstructions were prototyped. None was robust enough to ship:

- **Geometric expansion of the `AsymmetricBond` table** (apply symmetry ops to the asymmetric bond and
  collect images): under-generates, because one atom pair is frequently bonded across several periodic
  images while a single op application yields only one.
- **Asymmetric-bond length targeting** (each symmetry copy of an asymmetric bond has the same length):
  the length is the distance to a *symmetry image* of the partner, not the asymmetric-atom distance; and
  a single group pair can carry several distinct bond lengths, so a single target is wrong.
- **Contact-count matching** (match the number of `Bond` rows per asymmetric bond to the number of
  geometric contacts in each distance shell): defeated by distance degeneracy (different contacts at the
  same length).
- **Backtracking atom assignment** with per-asymmetric-bond equal-length consistency: correct in
  principle but blows up combinatorially on large organic cells.
- **Group-level "representative-shift" search** (choose each orbit's enumeration start by bond-geometry
  consistency, propagated across the bond graph): a heuristic that mispropagates and scored *worse* than
  the reference-setting reconstruction even on primitive cells.

The conclusion was that the reference-setting canonical reconstruction (correct for all primitive
lattices, and a strict improvement over the previous hard-coded `[0,3,1,2]` permutation that only
handled some of them) is the best option achievable without MedeA's internal tables.

## Where this is tested

`tests/scripts/import_file.py`, class `MedeATests`:

- `test_expansion_matches_p1_primitive` — for each primitive reduced/P1 pair, asserts identical atoms
  **and** bond topology.
- `test_expansion_matches_p1_centered_atoms` — for centred (C2/c, I2/a) pairs, asserts identical atomic
  structure only (bond topology is the known limitation above).

The sample `*.sci` / `*_P1.sci` pairs in `tests/files/data/MedeA/` were generated by exporting the same
structure from MedeA once with reduced symmetry and once lowered to P1; both must import to the same
structure. `9002806.txt` and `5-Aminopyrimidine_COD_2207775.txt` are MedeA symmetry-expansion trace logs
provided by the developers and kept for reference.
