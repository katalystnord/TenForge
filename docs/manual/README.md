# TenForge manual

For TenForge 0.1. This manual grows with each release; what is not here
yet is listed under [What 0.1 does not do](#what-01-does-not-do).

1. [Installing](#installing)
2. [Analysing a tensile test](#analysing-a-tensile-test)
3. [Reading the results](#reading-the-results)
4. [The report](#the-report)
5. [Test records](#test-records)
6. [Without a window: tf-analyse](#without-a-window-tf-analyse)
7. [What 0.1 does not do](#what-01-does-not-do)

## Installing

**The AppImage** (Linux, x86_64): download `TenForge-0.1.0-x86_64.AppImage`
from the project's Releases page, make it executable and run it:

```bash
chmod +x TenForge-0.1.0-x86_64.AppImage
./TenForge-0.1.0-x86_64.AppImage
```

It carries Qt, VTK and everything else it needs, except the C library
and the graphics driver, which come from your system. It is built on
Ubuntu 26.04, so it needs a distribution of about that age or newer; on
an older one it refuses to start with a message naming `GLIBC`.

**From source**: see [Building](../../README.md#building) in the README.

## Analysing a tensile test

TenForge reads the file your testing machine exports: a CSV or text
file of columns, whatever its dialect (comma, semicolon or tab; decimal
point or comma; a block of `key;value` lines above the header; units in a
row of their own or in the column names). Nothing in the file is changed;
TenForge only reads it.

The bar across the top of the window always says the **one next step**,
and the field it is about is outlined. Follow it from top to bottom.

![The window on start: the bar says to open a machine export](images/start.png)

### 1. Data

Press **Open data...** (or **File > Open data**, Ctrl+O) and choose the
file. TenForge finds the force and extension columns by their names
(force, load; extensometer, extension, elongation, strain) and their
units from the file, and says under the fields how it decided each. If it
cannot tell, the bar asks you to choose: pick the column or unit from the
list.

The extension may be a length (mm, m, in) or a strain (%, mm/mm). A
strain needs no gauge length.

![A file opened: the columns and units are found, the bar asks for the specimen's width and thickness](images/opened.png)

### 2. Specimen

Choose the shape and enter the measured dimensions in millimetres:
rectangular (width and thickness), round (diameter), tube (outer and
inner diameter), or the cross-section S0 directly if that is all you
have. S0 is shown as soon as it can be computed. Then enter the
extensometer gauge length Le.

If the file states S0 (`So`) or Le in its header block, TenForge fills
them in and says so; check them against your specimen.

The specimen id and material are optional; they appear in the report and
the record.

### 3. Procedure

Choose the standard to analyse with:

- **ISO 6892-1:2019**, metallic materials: E, Rp0.2, ReH, ReL, Fm, Rm,
  Agt, Ag, At, A.
- **ISO 527-1:2019**, plastics: Et, the stress and strain at the maximum
  and at break.

**A profile file...** loads a procedure you wrote yourself in TenForge's
profile format ([docs/specs/profile-v1.md](../specs/profile-v1.md)).
Results from it are labelled as coming from a user profile.

As soon as nothing is missing, TenForge analyses the test. Change
anything on the left and the results follow at once.

![Analysed: the curve with the elastic line and each result, the conformity statement and the results](images/analysed.png)

## Reading the results

**The orange box comes first.** It says whether the results are *to the
standard* or only *aligned with* it, and why. In 0.1 every result is
aligned, never to the standard, because:

- the profiles' parameters (fit windows, offsets, rounding) were entered
  from public sources and are marked unconfirmed until checked against
  the purchased text of the standard;
- no calibration certificate for the force or extension measurement can
  be recorded yet (that arrives with calibration, release 0.6).

The results follow the standard's method; the label says what has not
been verified, so nobody mistakes them for certified values.

**The table** gives each result rounded as the procedure states, with its
unit. A result that could not be found says why, for example *"no yield
point: no sustained fall in stress before the maximum"*, or that the
extensometer stopped before fracture. TenForge never fills a gap with a
guess.

**The chart** shows stress against strain, the elastic line the modulus
was fitted to, and each strength where it was found on the curve.

**Flags** below the table are things to look at even when everything was
determined, such as a peak that was not treated as a yield point and why.

## The report

**File > Export report** (Ctrl+E) writes a PDF, or an HTML page if you
name the file `.html`. The report holds the conformity statement and its
reasons, the specimen, the chart, every result or the reason it is
missing, any flags, and the provenance: the data file's name and SHA-256,
the procedure's version and SHA-256, and the TenForge version and build.
Anyone holding the same file can check that it is the one analysed.

## Test records

**File > Save record** saves the test as a new folder (a *record*):

```
S355-07/
  manifest.tfrec      every file below, with its SHA-256
  test.json           specimen, procedure, where each channel came from
  channels/           force and extension, the file's value beside the SI value
  source/             your file, byte for byte
  results/            results.json and results.csv
```

The format is open and described in
[docs/specs/test-record-v1.md](../specs/test-record-v1.md); any tool can
read it without TenForge.

**File > Open record** opens a record by its `manifest.tfrec`. TenForge
analyses it again and tells you, above the fields:

- if a file in the record has changed or gone missing since it was saved;
- if a stored result differs from what a fresh analysis gives (for
  instance after the procedure was updated);
- if the folder holds files the manifest does not list, for example a
  script or a DIC analysis you put under `analyses/`. TenForge never
  adopts them silently: **Add them to the record's manifest** records
  them with their hashes, and changes nothing else.

## Without a window: tf-analyse

The same analysis from the command line, for scripts:

```bash
tf-analyse --list-profiles
tf-analyse --profile iso-6892-1-2019 --area "120.6 mm2" --gauge-length "50 mm" export.csv
tf-analyse --profile iso-527-1-2019 --area "40 mm2" --json results.json export.csv
```

`tf-analyse --help` lists every option.

## What 0.1 does not do

- **Tension only.** Compression and bending arrive in 0.2.
- **Files only.** Running a machine arrives in 0.4; until then TenForge
  analyses what another program recorded.
- **One test at a time.** Statistics over a series (mean, standard
  deviation, confidence interval, Weibull) are computed by the engine but
  not yet offered in the window.
- **Aligned, never to the standard**, as explained above.
- **No uncertainty** on the results yet; it arrives with calibration.
