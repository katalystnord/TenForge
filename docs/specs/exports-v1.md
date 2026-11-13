# TenForge exports, version 1

**Status: DRAFT, for David's review (2026-11-13).** Written with Phase 4.

Exports carry an analysis to other tools. They are **derived**: each says
inside itself where it came from (data file and SHA-256, procedure,
TenForge version) and whether its results are to the standard or only
aligned with it. The test record remains the record.

## 1. The hardening curve (FreeCAD FEM, CalculiX)

True stress against true plastic strain, for an elastic-plastic material
model. **Tension only**: a compression curve is not a tensile flow curve,
and a bending test's stress is not uniform through the beam; both are
refused with that reason.

- Strains are counted from the analysis's zero point. With engineering
  stress s and strain e:
  true stress = s (1 + e), true plastic strain = ln(1 + e) - true stress / E,
  E the analysis's modulus.
- **Initial yield** is ReL when the curve has a yield point, otherwise the
  proof strength with the smallest offset (Rp0.2 in the shipped profiles).
  It is the first point, at plastic strain 0: an elastic-plastic model is
  elastic below it.
- Then the curve's own true plastic strains, from the initial-yield point
  to the maximum force, evenly spaced (20 points in all by default). At
  Rp0.2 the plastic strain is 0.2 % by that strength's definition, so the
  first segment is flat: the model's first 0.2 % of plastic flow happens
  at constant stress. (Counting only where true plastic strain becomes
  positive would start yielding far below Rp0.2: on the committed
  Ramberg-Osgood fixture, at 135 MPa instead of 300.)
- **Necking is excluded**: after the maximum force the stress is no longer
  uniaxial.
- **Stress never falls**: a point below the one before it (a yield drop,
  noise) is raised to it, and the file says how many were.

Written as:

- `<name>-yieldpoints.txt`: one `stress, plastic strain` per line, MPa,
  for FreeCAD FEM's nonlinear material (`YieldPoints`), which writes them
  unchanged into CalculiX's `*PLASTIC`.
- `<name>-plastic.inp`: a CalculiX `*PLASTIC` block (isotropic hardening,
  or `HARDENING=KINEMATIC`), MPa, for a model in mm, N, s, with `**`
  comment lines saying where it came from.

## 2. The FreeCAD material card (`<name>.FCMat`)

FreeCAD 1.x's YAML card, model `LinearElastic`
(`7b561d1d-fb9b-44f6-9da9-56a4f74d7536`), holding **only what was
measured**, as reported (value and unit as in results.json):

| Card field | Tension | Compression |
|---|---|---|
| `YoungsModulus` | E | (not written: a compressive modulus) |
| `YieldStrength` | the initial-yield result (ReL, else Rp0.2), named in a comment | |
| `UltimateTensileStrength` | Rm | |
| `CompressiveStrength` | | the compressive strength (sigma_M, sigma_m) |

Not written: **Poisson's ratio and density** (a tensile test does not
measure them; the card says to enter them in FreeCAD before an FEM
analysis), and **ultimate strain** (FreeCAD 1.1 declares it with a
pressure unit). Bending results are refused: the card's values are
uniaxial. `General` holds a new UUID, a name (the material, else the
specimen id), `Author: TenForge <version>` and the conformity in
`Description`; comment lines say where the values came from.

## 3. The analysed curve (`.vtp`, ParaView and PyVista)

VTK XML PolyData, ASCII: one polyline through the analysed curve's
samples (those with a finite strain and stress). Point arrays: the SI data
`strain` (1), `stress_Pa`, `force_N`, and the same for plotting,
`strain_percent` and `stress_MPa`. **In ParaView, plot it in a Line Chart
View** with X array `strain_percent` and Y `stress_MPa`: the 3D view
scales both axes alike, so the curve (tens of % against hundreds of MPa)
shows as a sliver. The point coordinates are (strain in %, stress in MPa,
0). Each determined result is a field-data array named `<id>_<unit>`
(e.g. `Rm_Pa`, `Agt_1`). An XML comment says where it came from and how to
plot it. (Corrected 2026-11-13: the first draft said the coordinates
alone made the curve look right when opened; opened in ParaView 6.1, the
3D view squeezed it.)

## 4. Checked against the programs themselves

- The `.vtp` is read back with VTK's XML reader (the one ParaView and
  PyVista use) in the test suite; ParaView 6.1.0's pvpython reads it with
  every array and result, and plots it as a line chart.
- Where FreeCAD's flatpak is installed, the suite loads the card into
  FreeCAD's material manager and solves FreeCAD's own nonlinear example
  (plate with hole) with CalculiX using the yield points
  (`freecad_loads_the_card_and_calculix_runs_the_yield_points`).

- ParaView reads whole records with TenForge's reader plugin
  (`tools/paraview/TenForgeRecordReader.py`, plain Python): it opens
  `manifest.tfrec`, checks `results/curve.vtp` against the manifest's
  SHA-256 and refuses it if changed, and outputs the curve with the
  specimen, mode, procedure and conformity as field data. Where ParaView's
  flatpak is installed, the suite opens a record with it and has a changed
  copy refused (`paraview_opens_a_record_and_refuses_a_changed_one`).

## 5. Reference curves in (PlotTracer)

Not an export but its mirror: a curve from elsewhere (a figure in a paper,
digitised with PlotTracer) drawn beside the test's own, dashed, named
"<series> (reference)". Read from PlotTracer's JSON export
(`{"series": [{"name", "points": [{<axis label>: value}], "relation"?}]}`):

- PlotTracer records axis labels but no units, so the user says which
  column is strain and which is stress, and their units (strain: %, 1;
  stress: MPa, N/mm2, GPa, kPa, Pa). TenForge proposes them from the labels.
- A point missing either value is left out: a gap is not a zero.
- An error-bar series (with `relation`) is not offered as a curve.
- Never used by the analysis. Named in results.json (`reference_curves`)
  and in the report's provenance, with the file and its SHA-256.
- A record keeps each file byte for byte (test-record-v1, section 10).

The importer's fixture was written by PlotTracer's own exporter
(`tools/fixtures/make_plottracer_export.test.ts`).

## 6. Platform MaterialDigital (JSON-LD)

The test as linked data for PMD: **PMD Core Ontology 3.1.2** and the
**Tensile Test Ontology (TTO) 3.0.0** (both CC BY 4.0), with PMDco's
measurement pattern: the tensile testing process (`pmd:PMD_0000974`) has
the test piece (`pmd:PMD_0020193`, evaluant role) as input and one
measurement datum (`obo:IAO_0000109`) per value as output; each datum is a
measurement of a quality or characteristic of the test piece and carries a
scalar value specification (`obo:OBI_0001931`) with the number
(`obo:OBI_0001937`) and a QUDT unit (`obo:IAO_0000039`). The process
realizes a plan concretizing the test method (`tto:TTO_0000054`), labelled
with the standard.

| TenForge | Class | Unit |
|---|---|---|
| S0 | `tto:TTO_0000026` | `unit:MilliM2` |
| gauge length | `pmd:PMD_0060016` | `unit:MilliM` |
| E | `pmd:PMD_0000618` | `unit:GigaPA` |
| Rp0.2 | `tto:TTO_0000047` | `unit:MegaPA` |
| ReH, ReL | `pmd:PMD_0080173`, `pmd:PMD_0080172` | `unit:MegaPA` |
| Rm | `tto:TTO_0000053` | `unit:MegaPA` |
| Fm | `tto:TTO_0000023` | `unit:N` |
| A, Agt, Ag | `pmd:PMD_0060004`, `tto:TTO_0000040`, `tto:TTO_0000037` | `unit:PERCENT` |

Where TTO's unreleased main branch deprecates a TTO 3.0.0 term in favour
of a PMDco 3.1.2 one (ReH, ReL, A, the gauge length), the PMDco term is
used, so the file stays valid when TTO next releases. Results with no
class in either (At, proof strengths other than Rp0.2) are named in the
process's `rdfs:comment`, never forced onto a wrong class; so are the
provenance and the conformity. Tension only: PMD has process classes for
compression and bending but no ontology of their results.

Two choices are TenForge's and not taken from a PMD example: the
strengths and the modulus are characteristics of the test piece
(`obo:RO_0000053`), S0 and the gauge length its qualities
(`obo:RO_0000086`); PMD's own S355 example puts strengths on the material
as qualities, and also uses IRIs that are not in PMDco 3.1.2 or that do
not resolve (`unit:MegaPa`), so it was not copied. Not yet checked
against PMDco's SHACL shapes. The suite checks every class and property
written against the released ontology files
(`pmd_jsonld_uses_only_terms_of_the_released_ontologies`).
