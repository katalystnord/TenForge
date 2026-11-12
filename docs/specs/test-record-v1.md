# TenForge test record, version 1

**Status: APPROVED by David, 2026-11-12.** It is a contract:
changes are additive within version 1; anything else is version 2
(engineering principle 9).

A test record is everything TenForge knows about one test: what was
measured, how, on what, and with which calibration. It is the thing every
other tool receives from TenForge, so it must be complete and
self-describing without TenForge (tenets 6 and 10).

## 1. Principles

1. **One test, one folder.** A record is a plain directory; zipping it is
   transport, not format.
2. **Raw first.** Every channel keeps raw counts and its own timestamps at
   its own rate. Nothing is resampled onto a shared clock inside the
   record. Derived values are allowed only when labelled derived, with the
   method that produced them.
3. **Nothing unmeasured is written as a number.** A missing value is an
   empty CSV cell or JSON `null`, never zero (SurView's rule).
4. **SI units, stated.** Values are stored in SI base or coherent units
   (s, m, N, Pa, K). Column names carry the unit as a suffix; JSON
   quantities carry a `unit` field. Display units are the GUI's business.
5. **Provenance travels with the record.** Every file is listed in the
   manifest with its SHA-256.
6. **Readable by anything.** CSV with one header row and no comment lines
   (comment conventions differ between readers); JSON; VTK XML.

## 2. Layout

```
my-test/
  manifest.tfrec            the index: format, version, files, hashes, provenance
  test.json                 specimen, fixture, profile, machine, calibration
  channels/
    force.csv               one file per channel, each at its own rate
    position.csv
    extensometer-1.csv
  source/                   imported inputs kept verbatim (optional)
    machine-export.csv
  frames/                   camera frames, zero-padded (optional)
    frame_000001.tif
  frames.csv                frame index (present when frames/ is)
  analyses/                 other tools' analyses of this test (optional)
    surview-2026-11-12/
      analysis.json
      series.csv
      frame_000001.vtu
    fe-model-calculix/
      analysis.json
      model.inp
      results.frd
  fields/                   ParaView view of imported DIC fields (optional)
    surview-2026-11-12/fields.pvd
    surview-2026-11-12/frame_000001.vtu
  results/
    results.json            see results-v1.md
    results.csv
```

The working notes say `channels.csv`. This spec refines that to one file
per channel, because channels arrive at different rates (0.5 Hz to kHz)
and a single table would force a shared tick or a sea of empty cells:
either is a conversion the record must not make (tenet 10).

## 3. `manifest.tfrec`

JSON. Its own extension so a ParaView reader plugin and the file manager
can recognise a record by one file.

```json
{
  "format": "tenforge-record",
  "version": "1.0",
  "created": "2026-11-11T14:03:22Z",
  "start": { "host_utc": "2026-11-11T14:03:25.118Z" },
  "application": { "name": "TenForge", "version": "0.1.0", "build": "<git sha>" },
  "files": [
    { "path": "test.json", "role": "metadata", "sha256": "..." },
    { "path": "channels/force.csv", "role": "channel", "channel": "force", "sha256": "..." },
    { "path": "source/machine-export.csv", "role": "source", "sha256": "..." }
  ]
}
```

- `start.host_utc` is the instant all `t_*_s` columns count from.
- A reader refuses a major version it does not know and says so; it reads
  any minor version of a known major, ignoring fields it does not know.
- Roles: `metadata`, `channel`, `source`, `frame-index`, `frame`, `field`,
  `field-collection`, `results`, `analysis`, and `other` for a file the
  user added outside these folders (section 7, rule 2).

## 4. Channels: `channels/<id>.csv`

One row per sample, in arrival order. Columns:

| Column | Meaning |
|---|---|
| `t_device_s` | Device clock, seconds from the device's own value at record start. Empty when the device gives no timestamps. |
| `t_host_s` | Host arrival time, seconds from `start.host_utc`. Always present. |
| `raw` | Raw reading as the device delivered it (counts, steps, or the value of an imported file). |
| `<quantity>_<unit>` | The calibrated value, e.g. `force_N`, `position_m`, `strain_1`. Empty when no calibration applies. |

**Imported channels** (1.0 as written by release 0.1): a file imported
from a machine's export has no host arrival times and usually no device
clock TenForge can vouch for, so both time columns are empty, never
invented; `start.host_utc` is `null`, and `timestamps` in test.json is
`none`. The file itself is kept verbatim under `source/`, time column
included. A channel whose source column is a strain (e.g. in %) is
written as `channels/strain.csv` with a `strain_1` column.

Each channel is described in `test.json` (section 5.4): quantity, unit,
kind (measured or commanded), timestamp source, calibration used, and for
imported data the source file and column it came from.

## 5. `test.json`

```json
{
  "mode": "tension",
  "specimen": {
    "id": "S355-07",
    "material": "S355 sheet",
    "geometry": { "shape": "rectangular", "width": {"value": 0.0201, "unit": "m"},
                  "thickness": {"value": 0.00302, "unit": "m"} },
    "gauge_length": { "value": 0.080, "unit": "m" },
    "notes": ""
  },
  "fixture": { "kind": "grips", "gripping_distance": { "value": 0.115, "unit": "m" } },
  "profile": { "id": "iso-6892-1-2019-a", "version": "1.0.0", "sha256": "..." },
  "machine": { "name": "", "driver": "klipper", "driver_version": "" },
  "channels": { },
  "calibration": [ ],
  "conditions": { "temperature": { "value": 296.15, "unit": "K" } },
  "operator": ""
}
```

### 5.1 `mode`
`tension`, `compression` or `flexure`. Later versions add more.

### 5.2 `specimen.geometry`
`shape` decides the fields: `rectangular` (width, thickness), `round`
(diameter), `tube` (outer and inner diameter), `beam` (width, thickness,
for flexure), `given-area` (`area`, in m2, when only S0 is known, e.g.
from a machine export's preamble). Measured dimensions are quantities; repeated measurements
may be given as an array, and the profile states how they are combined.

### 5.3 `fixture`
`grips` (gripping distance), `platens` (initial platen distance),
`three-point` (support span, loading-edge radius, support radius),
`four-point` (support span, loading span, radii). These are inputs to the
stress and strain models, recorded as data; TenForge designs no fixtures.

### 5.4 `channels`
Keyed by channel id:

```json
"force": {
  "quantity": "force", "unit": "N", "kind": "measured",
  "timestamps": "device",
  "calibration": "cal-force-2026-11",
  "source": { "device": "klipper:load_cell" }
}
```

`kind` is `measured` or `commanded`; analysis refuses a commanded
position where the profile requires a measured one. For imported data,
`source` names the file under `source/` and the column.

### 5.5 `calibration`
Certificates in force, as data: id, quantity, class, range, valid lower
limit, issued, issuer, method (e.g. ISO 7500-1 verification), and the
verification points. Results check against these (results-v1.md).

## 6. Frames: `frames/` and `frames.csv`

Frames are written as they come from the camera, zero-padded to six
digits, one-based (`frame_000001.tif`), which is the convention ParaView
and SurView already use for series.

`frames.csv` columns:

| Column | Meaning |
|---|---|
| `frame` | One-based frame number. |
| `file` | File name under `frames/`. |
| `sha256` | Of the frame file, so a join by another tool can be verified. |
| `t_device_s`, `t_host_s` | As for channels. |
| `force_at_capture_N` | **Derived**: force at the frame's timestamp, by linear interpolation between the two nearest force samples. Present for convenience (e.g. for plotting against force in another tool); the method is stated in `test.json` and the raw force channel remains the record. |

## 7. Other tools' analyses: `analyses/<id>/`

Everything another tool made from or for this test lives beside the data:
R or Python scripts and their output, SurView or OpenCorr DIC, FE models
and their results, a microscopy or fractography note on the specimen, a
fit from MatCal. One folder per analysis, its files kept verbatim, plus
one small `analysis.json` that any tool can write:

```json
{
  "format": "tenforge-analysis",
  "version": "1.0",
  "tool": { "name": "SurView", "version": "0.9.2" },
  "created": "2026-11-12T10:15:00Z",
  "author": "",
  "kind": "dic",
  "inputs": [
    { "path": "frames/frame_000001.tif", "sha256": "..." },
    { "path": "channels/force.csv", "sha256": "..." }
  ],
  "files": [ "series.csv", "frame_000001.vtu" ],
  "results": [
    { "name": "Poisson's ratio", "value": 0.29, "unit": "1" }
  ],
  "notes": ""
}
```

- `kind` is free text with suggested values: `dic`, `script`, `fe-model`,
  `fit`, `imaging`, `other`.
- `inputs` names the record files the analysis used, by path and SHA-256,
  so any reader can see whether the analysis still matches the data.
- `results` is optional: named values a reader can show without
  understanding the tool.

Rules:

1. **TenForge interprets an analysis only where it has an importer** (for
   SurView series CSV and OpenCorr tables, Phase 6). Otherwise it keeps
   the files, lists them, and shows any `results` beside its own,
   labelled with the tool that made them.
2. **Other tools may write here without TenForge.** The manifest belongs
   to TenForge: when it opens a record and finds files the manifest does
   not list, it says so and offers to add them with their hashes. It
   never adopts them silently and never changes them.
3. **Adding an analysis never changes the record's data or TenForge's
   results** (tenet 10).
4. **`source/` holds inputs only** (e.g. a machine's export); a tool's
   output goes under `analyses/`.

### 7.1 Imported DIC fields: `fields/<id>/`

Where an analysis holds per-frame fields (e.g. SurView's `.vtu`, or
OpenCorr's per-image tables converted), TenForge places a ParaView view
of them under `fields/<id>/` with a `.pvd` collection, `<id>` matching
the folder under `analyses/`. Each `.vtu` written by TenForge carries in
its FieldData: `TimeValue` (seconds from record start; ParaView's XML
readers use it as time) and `force_at_capture_N`. The join to frames is
by frame file name or SHA-256 where the tool carries them, never by row
position.

## 8. What is not in a record

Exports (FreeCAD material cards, CalculiX hardening data, plain CSV,
JSON-LD) are produced on demand from a record into a place the user
chooses. They are views of the record, not part of it.

## 9. Decisions (David, 2026-11-12)

1. **Frames are kept as the camera delivers them** (TIFF, PNG), never
   normalised (tenet 10).
2. **CSV in 1.0**, one file per channel; a binary channel format (e.g.
   HDF5) may come later as an additive option.
3. **`force_at_capture_N` stays** in `frames.csv`, labelled derived; the
   raw force channel remains the record.
4. **Other tools' analyses are co-located** in the record under
   `analyses/` (section 7).
