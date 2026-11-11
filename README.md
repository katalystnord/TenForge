# TenForge

Open universal testing software: tension, compression and bending, for
any universal testing machine.

**Status: Phase 0, foundations.** Nothing usable yet. The first release
(0.1) will analyse a CSV exported from any testing machine against named
standards; see [ROADMAP.md](ROADMAP.md).

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

## Built with

[Qt 6](https://www.qt.io/) (LGPL-3.0), [VTK](https://vtk.org/)
(BSD-3-Clause), [Eigen](https://eigen.tuxfamily.org/) (MPL-2.0).

## Licence

LGPL-2.1-or-later. See [LICENSE](LICENSE).
