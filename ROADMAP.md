# TenForge roadmap

Approved by David on 2026-11-11. Copied from the planning document of
record (`katalystnord/tenforge-plan.md`, outside this repository); update
both together.

## Architecture

Headless core, thin GUI (engineering principle 5).

| Target | Depends on | Holds |
|---|---|---|
| `tf_analysis` | Eigen | Standards engine, pure functions over arrays, no Qt. |
| `tf_core` | QtCore, VTK data model, `tf_analysis` | Test record, profiles, calibration, sequencer, safety, driver API, importers and exporters. |
| `tf_drivers/*` | `tf_core` | Simulator, Klipper, open protocol, MMTK, Open-Pull, GRBL, LabJack, Phidgets. |
| `tf_gui` | Qt Widgets, VTK rendering | The application. |
| `tf-analyse` | `tf_core` | Headless CLI: analyse a CSV or record with a profile, print or write results. For scripting and for tests. |

### 3.1 Analysis engine (`tf_analysis`)

Primitives, shared across modes; every standard is a declarative profile
on top.

1. Conditioning: zero and preload; toe compensation; zero point by tangent
   extrapolation or stress threshold; compliance correction from a table
   C(F); engineering and true conversion.
2. Stress and strain models per mode: tension and compression (force over
   area for prism, cylinder, tube; strain from extensometer, nominal, or
   compliance-corrected crosshead); one bending kernel with
   stress = 3Fa/(bh^2), strain = 12hs/(3L^2 - 4a^2) covering three-point
   (a = L/2) and four-point (a = L/3, L/4); D790's large-deflection term
   flagged unconfirmed.
3. Linear fits: windows in strain, stress, %Fmax or "most linear steepest
   part"; chord or regression; always slope, intercept, R2, Sm, Sm(rel), N;
   max-slope variant.
4. Offset and intersection solver (Rp, Rr, Rt), interpolated, never
   nearest sample.
5. Event detectors: maximum, first local maximum with hysteresis, zero
   slope yield, discontinuous yield (ReH, ReL, Ae), configurable break
   rules, maximum within a window.
6. Values at a strain, stress, deflection, or cycle; energy to Fm or break.
7. Validity checks: sampling rate per window, buckling pre-check, geometry
   validators, percent bending, failure-mode gates.
8. Statistics: mean, sd, median, ISO 2602 interval, Weibull; rounding.

### 3.2 Profiles

A profile is a versioned data file (JSON) naming: mode, inputs (specimen
geometry, fixture parameters), results, windows, detectors, sequence
(approach, rates, speed changes, preflex cycles, stop rules), rounding,
report fields. **Every parameter carries its source and a confirmed or
unconfirmed flag.** ASTM parameters are entered by a person who has read
the purchased standard (ASTM prohibits AI use of its standards).

### 3.3 Test record (the contract, specified before code)

One test is one folder with a manifest file (its own extension, so a
ParaView reader can trigger on it):

- `channels.csv`: raw counts, device and host timestamps, full rate;
- `test.json`: specimen, fixture, profile and version, calibration
  certificates in force, machine, driver, provenance (app version,
  SHA-256 of every file);
- `results.json` (+ CSV), with the profile version that produced them;
- `frames/` zero-padded, plus `frames.csv` (frame, file, SHA-256, device
  timestamp, force at capture);
- `fields/` imported DIC fields as `.vtu` per frame plus `.pvd`, each with
  `TimeValue` and force in FieldData;
- exports on demand: CSV, FreeCAD `.FCMat`, CalculiX `*PLASTIC` and
  `YieldPoints`, PMD tensile test ontology (JSON-LD), MatCal layout.

Unmeasured is empty or NaN, never zero (SurView's rule). Units are SI,
stated in every file.

### 3.4 Machine layer

Driver API: required `streamForce`, `streamPosition`, `moveAtRate`, `stop`
(hold, disable), `tare`, `status`; optional `moveTo`, `home`, `setRate`,
device calibration, heartbeat. Samples carry timestamp, source and
latency; channels push at their own rates; position flagged measured or
commanded; raw counts exposed; signed direction set at commissioning.
App-side soft limits, compressive overload guard, platen-gap floor and a
watchdog are always on.

Sequencer runs the profile's sequence. Host-side control in three modes:
crosshead speed, strain rate, load or stress rate; achieved rate logged
per phase against tolerance.

Klipper driver via Moonraker (WebSocket JSON-RPC; `load_cell/dump_force`
for force). Its 0.25 to 1 s command buffer is fine for constant speed;
load and strain-rate control through it is scoped in Phase 5.

### 3.5 Interoperability (all optional, all files)

| With | Contract |
|---|---|
| SurView | TF writes frames + `frames.csv`; SurView measures; TF imports its series CSV, joined by frame identity. |
| OpenCorr directly | TF imports per-image `saveTable2D` CSVs, reads a two-point extensometer; TF's frames are usable from OpenCorr scripts. |
| ParaView, PyVista | The record opens as is; a one-file Python reader plugin adds the channels as a time-aware table. |
| FreeCAD | `.FCMat` YAML card into the user library (path resolved at runtime); `YieldPoints` and `*PLASTIC` hardening data; DIC fields with FreeCAD array names. |
| PlotTracer | Import digitised literature curves as reference overlays. |
| Later | FreeCAD Python workbench (DIC vs FEA difference), MatCal adapter, trame viewer, VTKHDF, tomviz CT attachments. |

Each kit's tests run with the others absent; interop is tested against
committed sample files produced once by the other tool.

### 3.6 GUI

Parallel Universe David. Smallest unit: a CSV from any machine, open it,
map columns, choose a standard, see results. No project, no rig.
Screens: Analyse (curve, results with to-standard or aligned labels,
flags), Specimen and fixture, Live test (chart, camera, controls, stop
always visible), Calibration wizard, Record browser. Charts via
`vtkChartXY`. Reports as PDF and HTML through Qt GUI's own `QPdfWriter`
and `QTextDocument` (LGPL).

## Phases

Sizes are relative (S, M, L), not dates. Each phase ends with its exit
criteria met and verified by driving the real thing.

### Phase 0: Foundations (S)

- Repository `katalystnord/TenForge` (created when David says), CMake
  targets, CLAUDE.md with settled tenets, process tooling, CI.
- Trademark check for "TenForge" (EUIPO, Swedish PRV) before anything
  goes public. Collision checks done 2026-11-11: one inactive unrelated
  GitHub repo, PyPI free, tenforge.org and .io unregistered, tenforge.com
  registered but empty.
- Written specs, reviewed by David before code: test record v1, profile
  schema v1, results schema v1.
- Exit: CI green on the skeleton; specs approved.

### Phase 1: Tension analysis core, headless (M)

- `tf_analysis` primitives 1, 3 to 6, 8; tension models.
- Profiles: ISO 6892-1 (Rp0.2, ReH, ReL, Rm, Ag, Agt, A, At, E), ISO 527-1/2
  (Et, sigma_m, epsilon_b).
- `tf-analyse` CLI.
- Fixtures: synthetic with exact answers (committed), BAM S355 (CC BY,
  committed), TENSTAND fetched at test time.
- Exit: all TENSTAND files within EN 10002-1 A.5 tolerances; BAM results
  match the reported values; negative checks recorded.

### Phase 2: First release, analysis app (M)

- CSV import with column and unit mapping; specimen input; Analyse screen;
  results with flags and to-standard or aligned labels; reports; record
  save and open; AppImage; manual started.
- Exit: walkthrough test opens a CSV and produces a report using only
  on-screen guidance; release 0.1.

### Phase 3: Compression and bending analysis (M)

- Conditioning extensions (zero point, compliance tables), bending kernel,
  validity checks.
- Profiles: ISO 604, ISO 844, ISO 178, ISO 14125.
- Fixtures: synthetic beams and foams; TU Braunschweig ISO 14125 set
  (Zenodo 17611621) against testXpert's per-specimen Ef, sigma_fM,
  epsilon_fM; D1621 foam set (Mendeley 2sp8fyvhfm).
- Exit: Braunschweig results agree with testXpert within stated
  tolerances; release 0.2.

### Phase 4: Ecosystem exports (S to M)

- Record as `.pvd`/`.vtu`/CSV; FreeCAD `.FCMat`, `YieldPoints`,
  `*PLASTIC`; ParaView Python reader; PyVista recipe; PlotTracer overlay
  import; PMD JSON-LD.
- Exit: opened by hand in ParaView 6.x and PyVista; card loaded in FreeCAD
  1.1.x; a FreeCAD CalculiX example run with exported `YieldPoints`;
  release 0.3.

### Phase 5: Machine layer (L)

- Driver API, simulator driver (all three modes), sequencer, control
  modes, safety layer, Live test screen.
- Klipper driver via Moonraker; scope load and strain-rate control
  through Klipper's buffer.
- Exit: full tests in all three modes against the simulator, including
  overload, watchdog and stop cases; then on a Klipper bench rig (needs
  David's hardware); release 0.4.

### Phase 6: Camera and DIC through files (M)

- Frame capture with device timestamps and `frames.csv` (camera API to
  choose in this phase).
- Importers: SurView series CSV, OpenCorr `saveTable2D` sequences.
- Exit: sample files produced once by SurView and OpenCorr, committed,
  imported and joined correctly with neither installed in CI;
  release 0.5.

### Phase 7: Calibration and trust (M)

- ISO 7500-1 force verification wizard, ISO 9513 Annex H crosshead
  calibration, compliance measurement, uncertainty budget, sampling-rate
  checks, certificate handling in the record and reports.
- Exit: synthetic calibration runs give known classes and limits; results
  flagged correctly when outside verified ranges; release 0.6.

### Phase 8: Breadth, to 1.0 (L)

- Drivers: open protocol, MMTK, Open-Pull, GRBL/grblHAL, LabJack,
  Phidgets.
- Profiles: ISO 37, ISO 1924-2, ISO 3386, ISO 14126, ISO 7438; ASTM
  profiles as their parameters are entered by a person.
- 1.0 criteria: every shipped profile validated against a fixture; manual
  complete; every confirmed parameter traceable to a read standard.

### Phase 9: Later

FreeCAD workbench, MatCal adapter, trame viewer, VTKHDF export, live DIC
strain control (file or process level), fracture toughness and small
punch profiles (standards researched first), tomviz attachments; then the
hardware track after the web and patent search.

## Needs from outside the code, by phase

| When | Need |
|---|---|
| Phase 0 | Say go; settle the tenets; approve the three specs; trademark check result. |
| Phase 1 | Buy ISO 6892-1:2019 and ISO 527-1/-2 (parameters stay flagged until read); ask NPL about committing TENSTAND (optional). |
| Phase 3 | Buy ISO 604, 844, 178, 14125. |
| Phase 5 | A Klipper bench rig with a load cell. |
| Phase 8 | Decide who reads ASTM texts. |
| Open | SurView side of the file contract: frame identity in its series CSV; whether it reads `frames.csv`. |
