# Shimadzu exports

Two raw exports written by Shimadzu's Trapezium software. Each file is
unchanged (checked 2026-11-13): the first's SHA-256 equals the upstream
file's at the commit named; the second was taken from the Zenodo archive
by range requests, and its CRC-32 (62f94fc0) equals the one in the
archive's own directory. Renamed only, so the folder says which layout
each one is.

| File here | Layout | Test | Source | Licence |
|---|---|---|---|---|
| `trapezium-japanese.csv` | Trapezium in Japanese: Shift-JIS (half-width katakana in the names), settings and dimensions above one specimen's block, every line ending with a comma | compression, sand | trtku, [sand-report](https://github.com/trtku/sand-report), `data/trush/20220613_test1.csv` at 71a07bd | MIT, notice below |
| `trapezium-side-by-side.csv` | Trapezium: three specimens side by side, each under its title (` 1- 1`, ` 1- 2`, ` 1- 3`), six columns and two empty ones apart; "Force" in kN and again in N | tension, fused filament fabricated dogbones | Irving, J., Eluri, H., Zhang, X., Gnanasambandam, R. (2026). *Bayesian Sequential Experiments for Tensile Testing of Multi-material Fused Filament Fabricated Dogbones.* Zenodo. https://doi.org/10.5281/zenodo.21939238, `Raw Data.zip`, member `5_Sequential Design Batch 4/Design03/Tensile_Test_3reps/TensileTestB4-3.csv`. The record says the tests were run on a Shimadzu universal testing machine. | CC BY 4.0 |

SHA-256:

```
f5bfd739260e661d4fe7d52f3ecf06d6b79c48a93f4a845701224e8780efc89d  trapezium-japanese.csv
51ee9ed00095dcaea73a007b5c7077ad9ff654e4b5551a64f0754fc1c3e241c8  trapezium-side-by-side.csv
```

What each is relied on for (every number read with iconv and awk, not
TenForge):

- `trapezium-japanese.csv`: names 時間, 試験力, ｽﾄﾛｰｸ (time, test force,
  stroke), units sec, N, mm; 2012 rows; the last stroke written `.335`.
- `trapezium-side-by-side.csv`: the three specimens' own rows end at
  1724, 4579 and 1690; below a specimen's end its cells are empty. The
  last rows: specimen 1 at 86.15 s, specimen 2 a stress of 0.273284314
  N/mm2, specimen 3 a strain of 0.039390486 %.

## MIT notice

For `trapezium-japanese.csv`, from trtku/sand-report:

```
MIT License

Copyright (c) 2022 trtku

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
