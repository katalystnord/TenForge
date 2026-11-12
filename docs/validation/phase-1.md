# Phase 1 validation

How TenForge's tension analysis compares with published reference
evaluations, as run by the test suite on 2026-11-12 (TenForge 0.0.1,
profile `iso-6892-1-2019` 0.1.0). The suite regenerates these numbers on
every run: `tenstand_within_en10002_annex_a5` and
`bam_s355_matches_reported_results`.

## How the comparison is made

Each file is analysed exactly as `tf-analyse --profile iso-6892-1-2019`
would: columns found by name, S0 and Le from the file (TENSTAND) or BAM's
measured S0, nothing tuned per file.

**Pass or fail** uses the "proof of confidence" limits of EN 10002-1:2001
Annex A.5, Table A.1, as reproduced in NPL Report DEPC MPE 015, section 9:
Rp0.2, ReL and Rm within 0.5 % or 2 MPa, ReH within 1 % or 4 MPa, A within
2 % (absolute). A difference passes within the relative OR the absolute
limit; that reading of the table is ours and is not yet confirmed from the
standard's text. Where NPL's working group agreed a range, the difference
is measured from the nearest end of it.

For what A.5 does not cover, TenForge sets its own tighter bands so that a
change which moves them is caught: Rp0.1 as Rp0.2, Fm within 0.5 %, At, Ag
and Agt within 0.5 percentage points, and a yield point reported exactly
where the agreed table has one. These are labelled "TenForge" below; they
are ours, not the standard's. E has no limit anywhere and is only
reported.

## NPL TENSTAND

The 15 Premium Quality ASCII datafiles (Nimonic 75, 13 % Mn steel, S355,
316L, packaging steel, DX56, ZStE, three aluminium alloys and three
synthetic curves with 0, 0.5 and 1 % noise), against NPL's agreed values
(Report DEPC MPE 015, Table 9). **137 comparisons, 0 failed.**

| File | Result | TenForge | Agreed | Outside by | Limit | Check | Verdict |
|---|---|---|---|---|---|---|---|
| 01.txt | A | 41.23 | 41.2 | 0.03 | 2.00 | A.5 | pass |
| 01.txt | Ag | 30.82 | 30.8 | 0.02 | 0.50 | TenForge | pass |
| 01.txt | Agt | 31.18 | 31.2 | -0.02 | 0.50 | TenForge | pass |
| 01.txt | At | 41.50 | 41.5 | -0.00 | 0.50 | TenForge | pass |
| 01.txt | E | 213.20 | 200.8 to 216.5 | 0.00 | - | info | in range |
| 01.txt | Fm | 59972.73 | 59973 | -0.27 | 299.87 | TenForge | pass |
| 01.txt | Rm | 764.36 | 764.4 | -0.04 | 3.82 | A.5 | pass |
| 01.txt | Rp0.1 | 303.72 | 303.4 to 304.5 | 0.00 | 2.00 | TenForge | pass |
| 01.txt | Rp0.2 | 309.73 | 309.6 to 310.1 | 0.00 | 2.00 | A.5 | pass |
| 06.txt | A | 41.43 | 41.4 | 0.03 | 2.00 | A.5 | pass |
| 06.txt | Ag | 31.41 | 31.4 | 0.01 | 0.50 | TenForge | pass |
| 06.txt | Agt | 31.80 | 31.8 | 0.00 | 0.50 | TenForge | pass |
| 06.txt | At | 41.73 | 41.7 | 0.03 | 0.50 | TenForge | pass |
| 06.txt | E | 192.60 | 182.7 to 195.8 | 0.00 | - | info | in range |
| 06.txt | Fm | 59779.75 | 59780 | -0.25 | 298.90 | TenForge | pass |
| 06.txt | Rm | 761.14 | 761.1 | 0.04 | 3.81 | A.5 | pass |
| 06.txt | Rp0.1 | 301.25 | 300.5 to 301.8 | 0.00 | 2.00 | TenForge | pass |
| 06.txt | Rp0.2 | 308.34 | 308 to 308.6 | 0.00 | 2.00 | A.5 | pass |
| 10.txt | A | 51.43 | 51.4 | 0.03 | 2.00 | A.5 | pass |
| 10.txt | Ag | 49.85 | 49.8 | 0.05 | 0.50 | TenForge | pass |
| 10.txt | Agt | 50.36 | 50.4 | -0.04 | 0.50 | TenForge | pass |
| 10.txt | At | 51.91 | 51.9 | 0.01 | 0.50 | TenForge | pass |
| 10.txt | E | 183.20 | 180.6 to 184 | 0.00 | - | info | in range |
| 10.txt | Fm | 72666.50 | 72667 | -0.50 | 363.33 | TenForge | pass |
| 10.txt | Rm | 936.99 | 937 | -0.01 | 4.69 | A.5 | pass |
| 10.txt | Rp0.1 | 334.67 | 334.5 to 334.9 | 0.00 | 2.00 | TenForge | pass |
| 10.txt | Rp0.2 | 337.17 | 337.1 to 337.2 | 0.00 | 2.00 | A.5 | pass |
| 13.txt | A | 29.40 | 29.4 | -0.00 | 2.00 | A.5 | pass |
| 13.txt | Ag | 14.49 | 14.5 | -0.01 | 0.50 | TenForge | pass |
| 13.txt | Agt | 14.74 | 14.7 | 0.04 | 0.50 | TenForge | pass |
| 13.txt | At | 29.54 | 29.5 | 0.04 | 0.50 | TenForge | pass |
| 13.txt | E | 227.23 | 221 to 228.8 | 0.00 | - | info | in range |
| 13.txt | Fm | 44502.68 | 44503 | -0.32 | 222.52 | TenForge | pass |
| 13.txt | ReH | 479.36 | 479.4 | -0.04 | 4.79 | A.5 | pass |
| 13.txt | ReL | 431.79 | 431.8 | -0.01 | 2.16 | A.5 | pass |
| 13.txt | Rm | 567.19 | 567.2 | -0.01 | 2.84 | A.5 | pass |
| 17.txt | A | 51.26 | 51.1 | 0.16 | 2.00 | A.5 | pass |
| 17.txt | Ag | 38.29 | 38.3 | -0.01 | 0.50 | TenForge | pass |
| 17.txt | Agt | 38.59 | 38.6 | -0.01 | 0.50 | TenForge | pass |
| 17.txt | At | 51.47 | 51.3 | 0.17 | 0.50 | TenForge | pass |
| 17.txt | E | 192.21 | 189.8 to 202.3 | 0.00 | - | info | in range |
| 17.txt | Fm | 45279.88 | 45278 | 1.88 | 226.39 | TenForge | pass |
| 17.txt | Rm | 575.72 | 575.7 | 0.02 | 2.88 | A.5 | pass |
| 17.txt | Rp0.1 | 244.45 | 244.7 to 245.2 | -0.25 | 2.00 | TenForge | pass |
| 17.txt | Rp0.2 | 260.85 | 261 to 261.2 | -0.15 | 2.00 | A.5 | pass |
| 22.txt | A | 0.88 | 0.9 | -0.02 | 2.00 | A.5 | pass |
| 22.txt | Ag | 0.60 | 0.6 | 0.00 | 0.50 | TenForge | pass |
| 22.txt | Agt | 0.91 | 0.9 | 0.01 | 0.50 | TenForge | pass |
| 22.txt | At | 1.17 | 1.2 | -0.03 | 0.50 | TenForge | pass |
| 22.txt | E | 197.96 | 198.7 to 207.3 | -0.74 | - | info | outside the agreed range (no limit) |
| 22.txt | Fm | 2368.95 | 2369 | -0.05 | 11.85 | TenForge | pass |
| 22.txt | Rm | 596.71 | 596.7 | 0.01 | 2.98 | A.5 | pass |
| 22.txt | Rp0.1 | 526.48 | 525.6 to 530.6 | 0.00 | 2.65 | TenForge | pass |
| 22.txt | Rp0.2 | 563.13 | 562.5 to 564.6 | 0.00 | 2.82 | A.5 | pass |
| 30.txt | A | 40.06 | 39.9 to 40.1 | 0.00 | 2.00 | A.5 | pass |
| 30.txt | Ag | 22.45 | 22.5 | -0.05 | 0.50 | TenForge | pass |
| 30.txt | Agt | 22.60 | 22.6 | 0.00 | 0.50 | TenForge | pass |
| 30.txt | At | 40.12 | 40.1 | 0.02 | 0.50 | TenForge | pass |
| 30.txt | E | 200.92 | 195 to 207.4 | 0.00 | - | info | in range |
| 30.txt | Fm | 4271.69 | 4272 | -0.31 | 21.36 | TenForge | pass |
| 30.txt | Rm | 301.46 | 301.5 | -0.04 | 2.00 | A.5 | pass |
| 30.txt | Rp0.1 | 157.37 | 157.2 to 157.6 | 0.00 | 2.00 | TenForge | pass |
| 30.txt | Rp0.2 | 162.82 | 162.7 to 162.9 | 0.00 | 2.00 | A.5 | pass |
| 38.txt | ReH | 404.11 | 404.1 to 404.11 | 0.00 | 4.04 | labs | pass (agreed row blank; compared with the labs that reported one) |
| 38.txt | ReL | 396.66 | 398.03 to 398.3 | -1.37 | 2.00 | labs | pass (agreed row blank; compared with the labs that reported one) |
| 38.txt | A | 4.73 | 4.7 | 0.03 | 2.00 | A.5 | pass |
| 38.txt | Ag | 4.32 | 4.3 | 0.02 | 0.50 | TenForge | pass |
| 38.txt | Agt | 4.95 | 4.9 | 0.05 | 0.50 | TenForge | pass |
| 38.txt | At | 5.35 | 5.4 | -0.05 | 0.50 | TenForge | pass |
| 38.txt | E | 69.12 | 68.1 to 69.3 | 0.00 | - | info | in range |
| 38.txt | Fm | 2006.53 | 2007 | -0.47 | 10.04 | TenForge | pass |
| 38.txt | Rm | 434.31 | 434.3 | 0.01 | 2.17 | A.5 | pass |
| 38.txt | Rp0.1 | 385.45 | 385.2 to 386.8 | 0.00 | 2.00 | TenForge | pass |
| 38.txt | Rp0.2 | 396.51 | 396.4 to 397.1 | 0.00 | 2.00 | A.5 | pass |
| 42.txt | A | 44.53 | 44.5 | 0.03 | 2.00 | A.5 | pass |
| 42.txt | Ag | 28.56 | 28.6 | -0.04 | 0.50 | TenForge | pass |
| 42.txt | Agt | 28.68 | 28.7 | -0.02 | 0.50 | TenForge | pass |
| 42.txt | At | 44.55 | 44.6 | -0.05 | 0.50 | TenForge | pass |
| 42.txt | E | 68.79 | 68.7 to 72 | 0.00 | - | info | in range |
| 42.txt | Fm | 1209.98 | 1210 | -0.02 | 6.05 | TenForge | pass |
| 42.txt | Rm | 83.56 | 83.6 | -0.04 | 2.00 | A.5 | pass |
| 42.txt | Rp0.1 | 26.55 | 26.48 to 26.55 | 0.00 | 2.00 | TenForge | pass |
| 42.txt | Rp0.2 | 30.05 | 30.01 to 30.05 | 0.00 | 2.00 | A.5 | pass |
| 46.txt | A | 22.65 | 22.6 to 22.7 | 0.00 | 2.00 | A.5 | pass |
| 46.txt | Ag | 20.49 | 20.5 | -0.01 | 0.50 | TenForge | pass |
| 46.txt | Agt | 20.90 | 20.9 | 0.00 | 0.50 | TenForge | pass |
| 46.txt | At | 22.97 | 23.2 | -0.23 | 0.50 | TenForge | pass |
| 46.txt | E | 69.31 | 68.7 to 70 | 0.00 | - | info | in range |
| 46.txt | Fm | 8420.27 | 8420 | 0.27 | 42.10 | TenForge | pass |
| 46.txt | Rm | 284.56 | 284.6 | -0.04 | 2.00 | A.5 | pass |
| 46.txt | Rp0.1 | 133.49 | 133.4 to 133.9 | 0.00 | 2.00 | TenForge | pass |
| 46.txt | Rp0.2 | 134.72 | 134.5 to 134.8 | 0.00 | 2.00 | A.5 | pass |
| 50.txt | A | 44.18 | 43.4 to 43.9 | 0.28 | 2.00 | A.5 | pass |
| 50.txt | Ag | 23.96 | 23.9 | 0.06 | 0.50 | TenForge | pass |
| 50.txt | Agt | 24.13 | 24.1 | 0.03 | 0.50 | TenForge | pass |
| 50.txt | At | 44.18 | 44.2 | -0.02 | 0.50 | TenForge | pass |
| 50.txt | E | 169.73 | 162.2 to 165.3 | 4.43 | - | info | outside the agreed range (no limit) |
| 50.txt | Fm | 2664.99 | 2665 | -0.01 | 13.33 | TenForge | pass |
| 50.txt | Rm | 303.88 | 303.9 | -0.02 | 2.00 | A.5 | pass |
| 50.txt | Rp0.1 | 158.56 | 158.6 to 158.7 | -0.04 | 2.00 | TenForge | pass |
| 50.txt | Rp0.2 | 163.98 | 163.9 to 164 | 0.00 | 2.00 | A.5 | pass |
| 53.txt | A | 40.82 | 40.3 to 40.8 | 0.02 | 2.00 | A.5 | pass |
| 53.txt | Ag | 18.93 | 18.9 | 0.03 | 0.50 | TenForge | pass |
| 53.txt | Agt | 19.09 | 19.1 | -0.01 | 0.50 | TenForge | pass |
| 53.txt | At | 40.83 | 40.8 | 0.03 | 0.50 | TenForge | pass |
| 53.txt | E | 202.92 | 198.7 to 208.9 | 0.00 | - | info | in range |
| 53.txt | Fm | 3781.64 | 3782 | -0.36 | 18.91 | TenForge | pass |
| 53.txt | ReH | 270.06 | 270.1 | -0.04 | 4.00 | A.5 | pass |
| 53.txt | ReL | 228.66 | 228.7 | -0.04 | 2.00 | A.5 | pass |
| 53.txt | Rm | 318.86 | 318.9 | -0.04 | 2.00 | A.5 | pass |
| 57.txt | A | 49.97 | 50 | -0.03 | 2.00 | A.5 | pass |
| 57.txt | Ag | 39.64 | 39.6 | 0.04 | 0.50 | TenForge | pass |
| 57.txt | Agt | 40.00 | 40 | -0.00 | 0.50 | TenForge | pass |
| 57.txt | At | 50.24 | 50.2 | 0.04 | 0.50 | TenForge | pass |
| 57.txt | E | 207.50 | 207.5 to 208 | 0.00 | - | info | in range |
| 57.txt | Fm | 58000.00 | 58000 | 0.00 | 290.00 | TenForge | pass |
| 57.txt | Rm | 738.48 | 738.5 | -0.02 | 3.69 | A.5 | pass |
| 57.txt | Rp0.1 | 432.42 | 432.4 | 0.02 | 2.16 | TenForge | pass |
| 57.txt | Rp0.2 | 434.27 | 434.3 | -0.03 | 2.17 | A.5 | pass |
| 61.txt | A | 49.96 | 50 | -0.04 | 2.00 | A.5 | pass |
| 61.txt | Ag | 39.21 | 39.2 | 0.01 | 0.50 | TenForge | pass |
| 61.txt | Agt | 39.57 | 39.6 | -0.03 | 0.50 | TenForge | pass |
| 61.txt | At | 50.24 | 50.2 | 0.04 | 0.50 | TenForge | pass |
| 61.txt | E | 206.56 | 201.6 to 211.5 | 0.00 | - | info | in range |
| 61.txt | Fm | 58754.51 | 58754 | 0.51 | 293.77 | TenForge | pass |
| 61.txt | Rm | 748.08 | 748.1 | -0.02 | 3.74 | A.5 | pass |
| 61.txt | Rp0.1 | 433.65 | 431.8 to 434.1 | 0.00 | 2.17 | TenForge | pass |
| 61.txt | Rp0.2 | 443.12 | 438.1 to 441.6 | 1.52 | 2.21 | A.5 | pass |
| 63.txt | A | 49.96 | 50 | -0.04 | 2.00 | A.5 | pass |
| 63.txt | Ag | 37.29 | 37.3 | -0.01 | 0.50 | TenForge | pass |
| 63.txt | Agt | 37.66 | 37.7 | -0.04 | 0.50 | TenForge | pass |
| 63.txt | At | 50.24 | 50.2 | 0.04 | 0.50 | TenForge | pass |
| 63.txt | E | 205.73 | 203 to 211.6 | 0.00 | - | info | in range |
| 63.txt | Fm | 59632.39 | 59632 | 0.39 | 298.16 | TenForge | pass |
| 63.txt | Rm | 759.26 | 759.3 | -0.04 | 3.80 | A.5 | pass |
| 63.txt | Rp0.1 | 430.41 | 429.6 to 432.7 | 0.00 | 2.16 | TenForge | pass |
| 63.txt | Rp0.2 | 447.82 | 446.5 to 448.2 | 0.00 | 2.24 | A.5 | pass |

File 38 (hard AA5182, stepped yielding): NPL's agreed row leaves ReH and
ReL blank, but 3 of the 14 laboratories reported a yield point (ReH 404.1,
ReL 398.0 to 398.3 MPa, Table 6). TenForge reports one too and is compared
with those laboratories ("labs").

## BAM S355

Ten specimens of S355 sheet tested by BAM to DIN EN ISO 6892-1
(Zenodo 6778336, CC BY 4.0), against BAM's evaluated results. BAM reports
ReH, Fm, Rm and E; the extensometer was removed at about 3.9 % strain, so
Ag, Agt, At and A cannot be read from the files and TenForge refuses them
with that reason. **5 cases, 0 failed.**

| Specimen | Result | TenForge | BAM | Difference | Limit |
|---|---|---|---|---|---|
| Zd2 | ReH | 399.369 MPa | 399.000 MPa | +0.369 | 4.000 |
| Zd3 | ReH | 396.210 MPa | 396.000 MPa | +0.210 | 4.000 |
| Zx1 | ReH | 379.922 MPa | 380.000 MPa | -0.078 | 4.000 |
| Zx2 | ReH | 378.847 MPa | 379.000 MPa | -0.153 | 4.000 |
| Zx3 | ReH | 379.409 MPa | 379.000 MPa | +0.409 | 4.000 |
| Zx4 | ReH | 388.664 MPa | 388.000 MPa | +0.664 | 4.000 |
| Zy1 | ReH | 399.554 MPa | 399.000 MPa | +0.554 | 4.000 |
| Zy2 | ReH | 398.784 MPa | 399.000 MPa | -0.216 | 4.000 |
| Zy3 | ReH | 407.648 MPa | 408.000 MPa | -0.352 | 4.080 |
| Zy4 | ReH | 407.252 MPa | 407.000 MPa | +0.252 | 4.070 |
| Zd2 | Rm | 514.638 MPa | 515.000 MPa | -0.362 | 2.575 |
| Zd2 | Fm | 62.124 kN | 62.168 kN | -0.044 | 0.311 |
| Zd3 | Rm | 511.442 MPa | 511.000 MPa | +0.442 | 2.555 |
| Zd3 | Fm | 61.735 kN | 61.681 kN | +0.053 | 0.308 |
| Zx1 | Rm | 513.891 MPa | 514.000 MPa | -0.109 | 2.570 |
| Zx1 | Fm | 61.994 kN | 62.007 kN | -0.013 | 0.310 |
| Zx2 | Rm | 503.691 MPa | 504.000 MPa | -0.309 | 2.520 |
| Zx2 | Fm | 60.666 kN | 60.704 kN | -0.037 | 0.304 |
| Zx3 | Rm | 503.570 MPa | 504.000 MPa | -0.430 | 2.520 |
| Zx3 | Fm | 60.628 kN | 60.680 kN | -0.052 | 0.303 |
| Zx4 | Rm | 518.711 MPa | 519.000 MPa | -0.289 | 2.595 |
| Zx4 | Fm | 62.253 kN | 62.288 kN | -0.035 | 0.311 |
| Zy1 | Rm | 508.227 MPa | 508.000 MPa | +0.227 | 2.540 |
| Zy1 | Fm | 61.333 kN | 61.305 kN | +0.027 | 0.307 |
| Zy2 | Rm | 506.712 MPa | 507.000 MPa | -0.288 | 2.535 |
| Zy2 | Fm | 61.165 kN | 61.200 kN | -0.035 | 0.306 |
| Zy3 | Rm | 504.963 MPa | 505.000 MPa | -0.037 | 2.525 |
| Zy3 | Fm | 60.930 kN | 60.935 kN | -0.004 | 0.305 |
| Zy4 | Rm | 514.031 MPa | 514.000 MPa | +0.031 | 2.570 |
| Zy4 | Fm | 61.983 kN | 61.980 kN | +0.004 | 0.310 |

E is reported without a limit:

| Specimen | E, TenForge (GPa) | E, BAM (GPa) |
|---|---|---|
| Zd2 | 200.9 | 201 |
| Zd3 | 158.7 | 195 |
| Zx1 | 194.3 | 194 |
| Zx2 | 198.9 | 199 |
| Zx3 | 200.1 | 199 |
| Zx4 | 195.8 | 196 |
| Zy1 | 209.9 | 210 |
| Zy2 | 213.9 | 214 |
| Zy3 | 205.3 | 206 |
| Zy4 | 211.0 | 211 |

## Known gaps

- **E outside the agreed range on TENSTAND 22 and 50**, and far from BAM's
  on Zd3. The modulus window (10 to 40 % of Rp0.2, by regression) is
  TenForge's choice. No single window put E inside every agreed range;
  NPL's ranges came from visual fits. Zd3's early curve has a kink that
  looks like extensometer seating.
- **The yield-point rule is measured, not read.** A sustained fall of at
  least 0.5 % lasting at least 0.75 % strain, within 1 % plastic strain.
  It separates the real yield points of both datasets from the noise and
  serrations in TENSTAND 46, 61 and 63. It needs more data with yield
  points before it can be trusted widely.
- **Rp0.2 is also reported for materials with a yield point.** Whether to
  report it there is the profile's question, not settled yet.
- **No profile parameter is confirmed** except the Rp0.2 offset: the ISO
  texts have not been read yet. Every result is therefore "aligned".
- **Ae (yield point extension) is not computed.**
