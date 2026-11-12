# BAM S355 tensile tests

The ten `.csv` files here are the raw tensile test exports (`01_primary_data/`)
from:

> Schilling, M., Glaubitz, S., Matzak, K., Rehmer, B., Skrotzki, B. (2022).
> *Full dataset of several mechanical tests on an S355 steel sheet as
> reference data for digital representations* (1.0.0). Zenodo.
> https://doi.org/10.5281/zenodo.6778336

Licence: [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/)
(read on the Zenodo record, 2026-11-12). The files are unchanged, copied
from `tensile_test.zip` (SHA-256
b7dae010538a8145ca2ceeca218cbea0b6492493089b8f0bb7e4ff25f582e580).

`reported.json` holds BAM's evaluated results for each specimen (S0, FeH,
ReH, Fm, Rm, E, and A measured after fracture), transcribed from the same
archive's `tensile_test_results.xlsx`. They are facts used to check
TenForge's analysis (`tests/validate_bam_s355.cpp`).
