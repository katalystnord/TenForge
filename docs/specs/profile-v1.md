# TenForge profile, version 1

**Status: DRAFT, awaiting David's approval (Phase 0).**

A profile is one standard procedure written as data: what to measure, how
to run the test, and how to compute and report each result. The analysis
engine has no standard hard-coded in it; it executes profiles. This keeps
every number traceable to where it came from (tenet 9) and lets a person
who has read a standard check it without reading C++.

## 1. Principles

1. **Every parameter says where it came from.** Value, unit, the document
   and clause, and whether it was confirmed from the standard's own text.
2. **Unconfirmed is allowed, and visible.** A profile with unconfirmed
   parameters runs, but every result it produces is labelled aligned, not
   to the standard (results-v1.md), and the GUI says which parameters are
   unconfirmed.
3. **ASTM parameters are entered by a person.** ASTM prohibits AI use of
   its standards; an ASTM parameter is `confirmed` only with an
   `entered_by` naming the person who read the purchased text.
4. **We never ship the standard's text.** A profile holds parameters and
   clause references, not quotations.
5. **Profiles are versioned**, and results record the exact profile
   version and SHA-256 that produced them.

## 2. File

One JSON file per profile, `profiles/<id>.json`, for example
`profiles/iso-527-1-2019.json`.

```json
{
  "format": "tenforge-profile",
  "version": "1.0",
  "id": "iso-527-1-2019",
  "profile_version": "1.0.0",
  "title": "Plastics, tensile properties, general principles",
  "standard": { "body": "ISO", "number": "527-1", "edition": "2019" },
  "mode": "tension",
  "inputs": { },
  "conditioning": [ ],
  "results": [ ],
  "sequence": { },
  "validity": [ ],
  "statistics": { },
  "report": { }
}
```

## 3. Parameters

Every number in a profile is a parameter object:

```json
{
  "value": 0.0005,
  "unit": "1",
  "source": { "document": "ISO 527-1:2019", "clause": "3.9" },
  "confirmed": false,
  "confirmed_from": "",
  "entered_by": "",
  "note": "From the public preview's definition of the modulus window; clause number to verify against the bought text."
}
```

`confirmed` is true only when `confirmed_from` names the read text (e.g.
`purchased copy` or `public preview`). Clause numbers in examples here are
illustrative until checked.

## 4. Sections

### 4.1 `inputs`
What the user or the record must supply: specimen geometry fields per
shape, gauge length, fixture parameters (span, loading span, radii),
strain source required (`extensometer`, `nominal`, `crosshead-corrected`,
`any`), and which channels must be measured rather than commanded.

### 4.2 `conditioning`
Ordered steps, each naming an engine primitive and its parameters:
`zero-force`, `preload`, `toe-compensation`, `zero-point` (`tangent` or
`stress-threshold`), `compliance-correction` (table from calibration),
`strain-model` (per mode: `axial`, `nominal`, `flexure` with the support
and loading geometry).

### 4.3 `results`
Each result:

```json
{
  "id": "Et",
  "symbol": "E_t",
  "name": "Tensile modulus",
  "unit": "Pa",
  "method": {
    "primitive": "linear-fit",
    "window": { "axis": "strain",
                "from": { "value": 0.0005, "unit": "1", "...": "parameter fields" },
                "to":   { "value": 0.0025, "unit": "1", "...": "parameter fields" } },
    "fit": "regression"
  },
  "requires": { "strain_source": "extensometer" },
  "report": { "unit": "MPa", "rounding": { "...": "parameter fields" } }
}
```

Primitives, from the engine: `linear-fit`, `offset-intersection`,
`total-strain-intersection`, `maximum`, `first-local-maximum`,
`zero-slope`, `discontinuous-yield`, `break`, `value-at`,
`max-within-window`, `energy`, `max-slope`. A result may refer to another
result (e.g. Rp0.2 uses the slope from the modulus fit).

### 4.4 `sequence`
For live tests (Phase 5); ignored when analysing a file. Phases in order,
each with a control mode (`crosshead-speed`, `strain-rate`,
`stress-rate`), a rate parameter with tolerance, and a switch condition
to the next phase. Plus `approach` (speed, contact force), `preflex`
(cycles, target), and `stop` rules (`break`, `strain-limit`,
`deflection-limit`, `results-determined`, `travel-limit`).

### 4.5 `validity`
Checks that decide whether results are reportable: sampling rate per fit
window, geometry rules (e.g. span to thickness), buckling pre-check,
percent bending, failure-mode gates, minimum specimen count.

### 4.6 `statistics`
Per series: `mean`, `sd`, `median`, `iso-2602-interval`, `weibull`; which
apply to which results.

### 4.7 `report`
Fields the standard requires in a report, by reference to inputs, results
and record metadata.

## 5. Validation of profiles themselves

- A JSON Schema (`docs/specs/profile-v1.schema.json`, written with the
  first profile in Phase 1) checks structure.
- The engine refuses a profile that names a primitive or input it does not
  know, and says which.
- Every shipped profile has fixtures with independently known answers
  (ROADMAP, Phase 8 criteria).

## 6. Open questions for approval

1. JSON, or YAML for easier hand editing? Proposed: JSON (no ambiguity,
   one parser shared with records and results).
2. User-defined profiles: allowed from 0.1, clearly marked as not shipped
   with TenForge? Proposed: yes, always labelled aligned unless every
   parameter is confirmed.
