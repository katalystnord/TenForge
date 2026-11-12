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
