# TenForge

Open universal testing software: tension, compression and bending, for
any universal testing machine.

**Status: Phase 1, the tension analysis core.** `tf-analyse` analyses a
testing machine's export from the command line with the ISO 6892-1
(metals) or ISO 527-1 (plastics) profile, validated against NPL's
TENSTAND dataset and BAM's S355 tests (see [Validation](#validation)). The
first release with a window (0.1) follows in Phase 2; see
[ROADMAP.md](ROADMAP.md).

## What it is for

- **Any machine.** One driver layer for DIY rigs (Klipper first),
  open-protocol devices and retrofitted frames; or no machine at all, by
  analysing a file another machine wrote.
- **Named standards, honestly reported.** Results computed by the
  procedures of ISO 6892-1, ISO 527, ISO 604, ISO 844, ISO 178, ISO 14125
  and more, each labelled as either to the standard or only aligned with
  it, with calibration class and uncertainty.
- **A pristine record.** Raw counts and device timestamps kept, nothing
  resampled behind your back, provenance with every file.
- **At home in the VTK and FreeCAD ecosystems.** Tests open in ParaView
  and PyVista; material cards and hardening curves export to FreeCAD and
  CalculiX.

## Works alongside, never requires

TenForge interoperates with [SurView](https://github.com/katalystnord/SurView)
(digital image correlation) and [OpenCorr](https://github.com/vincentjzy/OpenCorr)
through open, documented files, and with any other tool that reads or
writes them. None of them is needed to use TenForge, and TenForge is not
needed to use them.

## Building

Linux, Ubuntu 26.04 or later:

```bash
sudo apt install cmake ninja-build g++ qt6-base-dev qt6-base-dev-tools \
    libvtk9-dev libvtk9-qt-dev libgl1-mesa-dev libeigen3-dev xvfb
cmake -S . -B build-ninja -G Ninja
cmake --build build-ninja
tools/run-tests.sh
```

## Analysing a file

```bash
tf-analyse --list-profiles
tf-analyse --profile iso-6892-1-2019 --area "120.6 mm2" --gauge-length "50 mm" export.csv
tf-analyse --profile iso-527-1-2019 --area "40 mm2" --json results.json export.csv
```

Columns are found by name (force, load; extensometer, extension, strain)
unless given with `--force` and `--extension`; units come from the file's
units row or column names, or `--force-unit` and `--extension-unit`.
Comma, semicolon and tab files with a decimal point or comma are read as
they are. Every result says how it was made, or why it could not be.

**Every result is currently "aligned", not "to the standard".** The
profiles' parameters are entered from public sources and stay marked
unconfirmed until checked against the purchased standards, and calibration
certificates can be recorded only from Phase 7. The results say so, with
each reason.

## Validation

- **NPL TENSTAND** (15 files with agreed values, NPL Report DEPC MPE 015):
  Rp0.2, ReH, ReL, Rm and A for all 15 files within the proof-of-confidence
  limits of EN 10002-1 Annex A.5; Rp0.1, Fm, At, Ag and Agt within
  TenForge's own tighter bands. E falls inside the agreed range for 13 of
  15 files. The data is fetched when the tests run.
- **BAM S355** (10 specimens, raw exports and BAM's evaluated results):
  ReH within 0.7 MPa, Rm within 0.5 MPa and Fm within 0.1 % of BAM's
  values; E within 1 GPa for 9 of 10.

Details: [docs/validation/phase-1.md](docs/validation/phase-1.md).

## Built with

[Qt 6](https://www.qt.io/) (LGPL-3.0), [VTK](https://vtk.org/)
(BSD-3-Clause), [Eigen](https://eigen.tuxfamily.org/) (MPL-2.0).

## Data credits

- BAM S355 tensile tests: Schilling, M., Glaubitz, S., Matzak, K.,
  Rehmer, B., Skrotzki, B. (2022), [doi:10.5281/zenodo.6778336](https://doi.org/10.5281/zenodo.6778336),
  CC BY 4.0. Included unchanged in `tests/fixtures/bam-s355/`.
- NPL TENSTAND Premium Quality ASCII datafiles and agreed values: National
  Physical Laboratory, TENSTAND project (EU G6RD-CT-2000-00412), NPL Report
  DEPC MPE 015 (2005). The datafiles are fetched from npl.co.uk when the
  tests run, not included.

See [NOTICE](NOTICE).

## Licence

LGPL-2.1-or-later. See [LICENSE](LICENSE).
