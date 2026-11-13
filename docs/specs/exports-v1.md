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
samples (those with a finite strain and stress). Point coordinates are
(strain in %, stress in MPa, 0), so the curve looks right when opened;
the point arrays are SI: `strain` (1), `stress_Pa`, `force_N`. Each
determined result is a field-data array named `<id>_<unit>` (e.g.
`Rm_Pa`, `Agt_1`). An XML comment says where it came from.

## 4. Checked against the programs themselves

- The `.vtp` is read back with VTK's XML reader (the one ParaView and
  PyVista use) in the test suite.
- Where FreeCAD's flatpak is installed, the suite loads the card into
  FreeCAD's material manager and solves FreeCAD's own nonlinear example
  (plate with hole) with CalculiX using the yield points
  (`freecad_loads_the_card_and_calculix_runs_the_yield_points`).
