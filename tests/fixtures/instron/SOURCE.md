# Instron exports

Six raw exports written by Instron's own software, one per dialect
TenForge reads. Each file is unchanged: its SHA-256 here equals the
upstream file's at the commit or version named (checked 2026-11-13).
Renamed only, so the folder says which dialect each one is.

| File here | Software (from its layout) | Test | Source | Licence |
|---|---|---|---|---|
| `bluehill2-flexure.csv` | Bluehill 2/3, imperial units, results in the header | three-point bend | UML Applied Solid Mechanics Lab, [TensileDataAnalysis](https://github.com/UML-AppliedSolidMechanicsLab/TensileDataAnalysis), `DataAnalyzer/TestFiles/Specimen_RawData_1.csv` at 28f44dd | LGPL-2.1 (the repository's LICENSE.txt), as TenForge |
| `bluehill2-dimensions.csv` | Bluehill 2/3, a `Dimension : key,"value",unit` header | cyclic compression | Aryan Senthil, [Contact-Detection](https://github.com/AryanSenthil/Contact-Detection), `Specimen_RawData_1.csv` at 670fd16 | MIT, notice below |
| `bluehill2-italian.csv` | Bluehill 2/3 in Italian: semicolons with a decimal point | tension | Signorini, C., Nobili, A., Sola, A., Messori, M. (2019). *Diluted epoxy-coatings for TRM composites: tensile tests raw data.* Mendeley Data, v3. https://doi.org/10.17632/kbg7dz3yhx.3, `LGEW25_1.csv` | CC BY 4.0 |
| `bluehill-universal-results.csv` | Bluehill Universal: Bluehill's results table above the curve | tension | Javier Linero, [gradient-optimized-joints](https://github.com/javierlinero/gradient-optimized-joints), `src/instron_data/firstsample_1.csv` at 57b2d2a | MIT, notice below |
| `bluehill-universal-composite.csv` | Bluehill Universal: `key,(unit),"value"` header, results table, curve | tension, carbon fibre composite | henryflushman, [AERO](https://github.com/henryflushman/AERO), `seniorFall/stressAnalysis/data/tensile_data/team13_tues_pm_6_1.csv` at cc4abd9 | MIT, notice below |
| `merlin-compression.raw` | Merlin "Save ASCII Raw Data": `"Key:",value,"unit"` header, units inside the column names | cyclic compression | Christopher Hong (2020). *04-08-2019 Instron.* Figshare. https://doi.org/10.6084/m9.figshare.13345313.v1, `Size6_1.raw` | CC BY 4.0 |

SHA-256:

```
4767a00ee3ec4e7c60f8a00cfa0c269f82af680d05142ac89de3173480bc3c99  bluehill2-flexure.csv
6417c11c27e75f264f034fc05f8e35710e52e2401a37c27ac4e81c245fbca4ee  bluehill2-dimensions.csv
491a412818c701fa35c4731598d9103c1c5e7075c9a11f099487c8799369bde3  bluehill2-italian.csv
fad54d610ce8cdc3627f819f95cc1e7b8f6d56c432ba2ca9b664dad395308373  bluehill-universal-results.csv
12abf14233a026a95e606f83457271dacd3dbc999ab02819dbd0144362e27319  bluehill-universal-composite.csv
992ed53adcce7abff1e66f022ee2a7b1219ae1afdbc0aba6d360ea98b96fdb2b  merlin-compression.raw
```

What each is relied on for:

- `bluehill2-flexure.csv`: Bluehill's "Maximum Load" (131.18347 lbf) and
  the extension there (1.50294 in), from its header, are checked against
  what TenForge reads in lbf and in.
- `bluehill-universal-results.csv`: Bluehill's results table (maximum force
  285.30 N, its strain and displacement there, its modulus 143.72 MPa) is
  read as the file's own results, never as TenForge's.
- `bluehill-universal-composite.csv`: the header's width and thickness
  (0.39 in, 0.04 in) are the method's defaults and disagree with the
  results table's measured area (30.42 mm2); TenForge must take the area
  and say so.

  This set was considered as a validation of tensile strength and
  rejected (2026-11-13). In 5 of its 6 specimens, Bluehill's "Tensile
  stress at Tensile strength" is not the maximum of the stress the same
  file records: 0.2 to 2.3 MPa lower, and in one specimen at a later time
  than the maximum. No moving average of the curve reproduces all six.
  Bluehill's rule for that result is a method setting the export does not
  record, so the comparison cannot say whether TenForge is right. (Its
  maximum force does agree with the curve, in the two files that report
  one, and is checked.)
- `merlin-compression.raw`: read only for its layout. Merlin's results
  file for the same test (`Size6_1.rlt`) gives "Load Peak 1" = -6.93 N,
  which is not the curve's minimum (-9.16 N): a peak of one cycle, by
  Merlin's own rule, so it is not used as a check.

## MIT notices

For the files from javierlinero/gradient-optimized-joints:

```
MIT License

Copyright (c) 2024 Javier Linero

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

For the files from henryflushman/AERO:

```
MIT License

Copyright (c) 2026 henryflushman

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

For the files from AryanSenthil/Contact-Detection:

```
MIT License

Copyright (c) 2025 AryanSenthil

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

