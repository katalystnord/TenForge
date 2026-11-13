# TenForge results, version 1

**Status: APPROVED by David, 2026-11-12.** A contract from here: changes
are additive within version 1; anything else is version 2.

What TenForge computed from a record with a profile, how, and how far it
can be trusted. Written into the record's `results/` folder; one
`results.json` per analysis of one test.

## 1. Principles

1. **Every number says how it was made**: the profile version, the
   primitive, the window, the fit statistics, the parameters and whether
   each was confirmed.
2. **A result that could not be determined is `null` with a reason**,
   never a fallback value.
3. **Conformity is stated, not implied** (tenet 9). Each analysis is
   either `to-standard` or `aligned`, and an aligned analysis lists every
   reason.
4. **Recomputable.** Given the record and the profile, the same TenForge
   version produces the same results; the file records what is needed to
   check that.

## 2. `results.json`

```json
{
  "format": "tenforge-results",
  "version": "1.0",
  "computed": "2026-11-11T15:20:00Z",
  "application": { "name": "TenForge", "version": "0.1.0", "build": "<git sha>" },
  "record": { "manifest_sha256": "..." },
  "profile": { "id": "iso-527-1-2019", "profile_version": "1.0.0", "sha256": "..." },
  "conformity": {
    "status": "aligned",
    "reasons": [
      { "code": "unconfirmed-parameter", "detail": "Et window limits not yet confirmed from the purchased standard" },
      { "code": "no-force-calibration", "detail": "No ISO 7500-1 certificate in the record" }
    ]
  },
  "results": [ ],
  "flags": [ ]
}
```

`record` (refined while implementing, 2026-11-12): inside a record the
manifest lists `results.json` with its hash, so the results cannot also
carry the manifest's hash. There `manifest_sha256` is `null` and
`record.inputs` lists the record files the analysis read, each with its
path and SHA-256 (`test.json` and the channel files). `source.path` is
then relative to the record (`source/<file>`). For results written
outside a record (tf-analyse), `record` is `null`.

### 2.1 Conformity

`to-standard` only when all of these hold; otherwise `aligned`, with one
reason per failure:

| Code | Condition for `to-standard` |
|---|---|
| `unconfirmed-parameter` | Every parameter the analysis used is confirmed. |
| `no-force-calibration`, `force-class` | A force certificate is in force, of the class the profile requires, and every force used lies above its valid lower limit. |
| `no-strain-calibration`, `strain-class` | The same for the extensometer or strain source. |
| `strain-source` | The strain source is one the profile accepts for each result. |
| `sampling-rate` | Sampling rate and points per fit window meet the profile's checks. |
| `rate-tolerance` | For live tests: achieved rates stayed within tolerance. |
| `validity` | Every validity check of the profile passed. |

### 2.2 Each result

```json
{
  "id": "Et",
  "symbol": "E_t",
  "status": "determined",
  "value": 2.41e9,
  "unit": "Pa",
  "reported": { "value": "2410", "unit": "MPa" },
  "uncertainty": { "u": 3.1e7, "k": 2, "unit": "Pa", "budget": [ ] },
  "how": {
    "primitive": "linear-fit",
    "fit": "regression",
    "window": { "axis": "strain", "from": 0.0005, "to": 0.0025 },
    "samples": { "channel": "extensometer-1", "first": 412, "last": 1318, "n": 907 },
    "statistics": { "slope": 2.41e9, "intercept": -1.2e5, "r2": 0.99991, "sm": 1.1e7, "sm_rel": 0.0046 },
    "parameters": [ { "path": "results.Et.method.window.from", "confirmed": false } ]
  },
  "reason": null
}
```

- `status`: `determined`, `not-determined` (with `reason`, e.g. no break
  within the strain limit), `not-applicable` (the profile's result does
  not apply to this test).
- `value` is SI and unrounded; `reported` is the profile's rounding and
  display unit, as text so the rounding survives.
- `uncertainty` is present once Phase 7 delivers the budget; until then it
  is absent, not zero.
- `samples` points into the record by channel and row, so any reader can
  see exactly which data a result used.

### 2.3 `flags`
Things a reader should see even when conformity holds, e.g. a value close
to a calibration limit, a toe compensation applied, a speed change
detected in an imported file.

## 3. `results.csv`

One header row, then one row per result, for spreadsheets: `id, symbol,
status, value, unit, reported_value, reported_unit, u, k, conformity,
reason`. `conformity` repeats the analysis status (`to-standard` or
`aligned`) on every row, so a row copied elsewhere keeps it; the reasons
stay in `results.json`. Empty cells, never zero, for anything not
determined.

## 4. Series

Statistics over several tests (mean, sd, ISO 2602 interval, Weibull) go
in a separate series file referencing each test's `results.json` by
SHA-256. Specified with Phase 2, when the GUI first handles series.

## 5. Decisions (David, 2026-11-12)

1. **Nothing is `to-standard` before Phase 7.** Until calibration
   certificates can be recorded, every analysis is `aligned`.
2. **Reported values are stored as text**, so what was reported is
   exactly what is stored; the unrounded SI value sits beside it.

## 6. Additions in release 0.2 (Phase 3; additive, for David's review)

- `profile` gains `mode` and `fixture` (as the profile names them).
- `validity`: the profile's checks as evaluated on this specimen and
  fixture, each `{"id", "status", "value", "detail"}` with `status`
  `met`, `not-met` or `not-checked` (a quantity it needs was not given).
- Conformity reasons gain `validity-not-met` and `validity-not-checked`;
  either keeps the results from being to the standard.
- `no-strain-calibration` names what measured the displacement: the
  extensometer (ISO 9513) in tension, the deflection or deformation
  measurement otherwise.
- Compression and bending are analysed with the test's own direction
  positive. When the file records them negative, a note in
  `source.notes` says so; the record keeps the file's signs.
- **`analysis_choices`** (when the user moved the modulus window): as in
  test-record-v1, section 10. The conformity reasons then include
  `user-modulus-window`, naming the window.

## 7. Addition after release 0.3 (machine exports; additive, for David's review)

- **`source_results`**: results the machine's own software wrote into the
  data file (Instron Bluehill's "Results Table"; the rows of an MTS 793
  file's acquisition with other columns than the curve's, its `table` the
  section's title line), when it has any. One
  object per value, `{"table", "row", "name", "unit", "value"}`, every
  field a string exactly as written ("285.30", "-----"), the unit with its
  brackets removed. They are the file's, not TenForge's: never converted,
  never checked, never used by the analysis, and never among `results`.
  The report and the window show them in their own section, saying so.
- The area the file's software used, from such a table (`Area`), is taken
  as the cross-section when the file states no S0, with a note in
  `source.notes`; when the file's header gives a width and thickness that
  do not match it within 1 %, a note says that too.
