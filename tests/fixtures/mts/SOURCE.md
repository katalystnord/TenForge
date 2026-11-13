# MTS exports

Five raw exports written by MTS software: two from MTS 793 (MultiPurpose
TestWare, `.dat`) and three from MTS TestSuite. Each file is unchanged:
its SHA-256 here equals the upstream file's at the commit or version
named (checked 2026-11-13; for the Zenodo files, the archive's MD5 equals
Zenodo's and the member below was taken from it). Renamed only, so the
folder says which layout each one is.

| File here | Software (from its layout) | Test | Source | Licence |
|---|---|---|---|---|
| `mts793-chunked.dat` | MTS 793, English: the data in two blocks (1024 rows, then 221) under the same header | tension, aluminium | evnchn, [ISDN2603-Tensile-Test](https://github.com/evnchn-HKUST-academics/ISDN2603-Tensile-Test), `Aluminium.dat` at b7b0fdd | MIT, notice below |
| `mts793-german.dat` | MTS 793, German: Latin-1, decimal comma, an operator block ("Bediener Information"), the data, then a second acquisition of other columns and two rows | force-controlled compression, concrete cylinder (a modulus test) | Bundesanstalt für Materialforschung und -prüfung (BAM), [LebeDigital](https://github.com/BAMresearch/LebeDigital), `tests/raw_data_processing/youngs_modulus_data/test_data/specimen.dat` at adaa7e5 | MIT, notice below |
| `testsuite-quoted.csv` | MTS TestSuite: byte order mark, `"Key: value"` header lines, every field quoted | tension, 3D-printed gyroid lattice | Peloquin, J. (2023). *Tensile performance data of 3D printed photopolymer gyroid lattices created using a range of base materials and lattice porosities.* Mendeley Data, v1. https://doi.org/10.17632/wj766p2w9t.1, `Raw Mechanical Data/BMA_01TS55_01_20211122.csv` | CC BY 4.0 |
| `testsuite-decimal-comma.txt` | MTS TestSuite: tab, decimal comma, exponents | tension, bone cement | Ghandour, S., Christie, I., Öhman-Mägi, C., Persson, C. (2023). *Quasi-Static and Fatigue Testing Dataset for soft bone cements according to ASTM F2118.* Zenodo. https://doi.org/10.5281/zenodo.8380702, `TensileData.zip`, member `TensileData/VSLA/VSLA_Sample10.txt` | CC BY 4.0 |
| `testsuite-french.txt` | MTS TestSuite in French: crosshead and two laser-extensometer channels | tension | Cathelineau, P. (2025). *DOPAMICS | Tensile tests-Mechanical data from DIC and laser extensometry.* Zenodo. https://doi.org/10.5281/zenodo.15343816, `2025-04-08_Mechanical_data_tensile_test_laser_ALES.zip`, member `Exécutions d'essai 1 29-05-2024 12 02 59/Exécutions d'essai 1 29-05-2024 12 02 59_DAQ-Contrainte; … - (A intervalle de temps fixe).txt` | CC BY 4.0 |

SHA-256:

```
9117c68d0ae621018bcac656caf20fc4432f45b6a1de14de62c56192dfe7e9a4  mts793-chunked.dat
03878e237d32d62e39a68b8acbe11e81ad9e5c4e670c9f2d0dbcf072282cc4f9  mts793-german.dat
c5a9cf5779851a3b9c9c41fab199b702fad1ca9d637ebdd690514fbb3111d3ad  testsuite-quoted.csv
f33ecd3b31d8e77fe5b93c1de5e25e7961e82820563bbde291bf4ac780911f81  testsuite-decimal-comma.txt
d78048775a44e1ffb24a39efe8ee122f678be3882abac0348be425d5c14d06f8  testsuite-french.txt
```

What each is relied on for (every number read with awk, not TenForge):

- `mts793-chunked.dat`: 1245 rows in all; the first block ends at
  102.01074 s and the second begins at 102.11035 s. TenForge must read
  both, in order.
- `mts793-german.dat`: 572 rows of 8 columns; "Ch 1 Kraft Sollwert" is
  the controller's set point and must never be taken as the force. The
  second acquisition (5 columns, 2 rows, unlabelled) is kept as the
  file's own values. Its force values (0.80352277 and -147.20456 kN) lie
  just outside the logged curve's range (0.776989 to -147.161 kN), as a
  peak and valley caught at a higher rate would; the file does not say
  so, so TenForge does not call them that and uses neither.
- `testsuite-quoted.csv`: 681 rows; the four header lines.
- `testsuite-decimal-comma.txt`: 1856 rows; `6,97374318292532E-05`.
- `testsuite-french.txt`: 120 rows; three columns could be the
  extension (the crosshead's deformation, the extensometer's elongation
  and its strain), so the window must ask.

## MIT notices

For `mts793-german.dat`, from BAMresearch/LebeDigital:

```
MIT License

Copyright (c) 2020 Bundesanstalt für Materialforschung und -prüfung

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

For `mts793-chunked.dat`, from evnchn-HKUST-academics/ISDN2603-Tensile-Test:

```
MIT License

Copyright (c) 2023 evnchn

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```
