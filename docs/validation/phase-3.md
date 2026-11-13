# Phase 3 validation

How TenForge's compression and bending analysis compares with a reference
evaluation, as run by the test suite on 2026-11-13 (TenForge 0.2.0,
profile `iso-14125-1998-four-point` 0.1.0). The suite regenerates these
numbers on every run: `braunschweig_14125_agrees_with_testxpert`.

## How the comparison is made

**Data.** TU Braunschweig's four-point bending tests of hemp fibre
reinforced laminates (Osten, Steinmetz, Charif, Voelkerink, 2025;
[doi:10.5281/zenodo.17611621](https://doi.org/10.5281/zenodo.17611621),
CC BY 4.0): 25 raw exports from ZwickRoell testXpert III, and the four
test protocols testXpert printed for them. ISO 14125:1998 method B,
class III: span 45 mm, loading span 15 mm, 0.1 N preload, 2 mm/min.

**Reference.** What testXpert III printed for each specimen: the flexural
modulus Ef (regression), the stresses f' and f'' at the modulus window's
ends, the number of samples in the regression N, its R2, and the strain
at maximum eps_fM; and the maximum flexural stress sigma_fM to three
decimals from each export's own header. Transcribed into
`tests/validation/braunschweig-14125-agreed.json`.

**Analysis.** Each export goes through `tf::core::Session`, the same path
as the window and `tf-analyse`, with the shipped profile: width,
thickness and span read from the export's own header; the units (N, mm),
the loading span and the deflection position entered as a person would
from the protocol. Nothing is tuned per file.

**Tolerance.** Half a unit in the last digit testXpert printed, for every
quantity (Ef printed 8.54 must lie within 8.535 to 8.545 GPa); N exactly.

**One finding before the first run.** testXpert's "Verformung" in these
exports is the crosshead travel (it equals "Traversenweg" to the last
digit in all 25 files). The crosshead moves the loading points, not the
beam's middle. testXpert's strains reproduce only when the deflection is
taken at the loading points (strain = 3 h s / (a (3 L - 4 a)), 5.4 h s / L^2
at a = L/3), not with the textbook mid-span formula (4.70 h s / L^2), which
gives moduli 14 to 23 % higher than testXpert's on these files. TenForge therefore asks where a four-point
deflection was measured instead of assuming either; the walkthrough and
the session tests follow that step.

## Result

**23 specimens, 161 comparisons, 0 failed; 2 excluded.**

The test was seen to fail when broken: deflection read at mid-span, 138
of 161 comparisons failed; the modulus window's upper end moved from
0.25 % to 0.30 % strain, 67 failed; an exclusion whose reason no longer held (feucht8's
header set to its data's maximum), refused.

| Specimen | Quantity | testXpert | TenForge | Tolerance | |
|---|---|---|---|---|---|
| Hanf80°Cgetrocknet_90min_15 | Ef (GPa) | 6.22 | 6.21532 | 0.005 | pass |
| Hanf80°Cgetrocknet_90min_15 | f' (MPa) | 1.680 | 1.67958 | 0.0005 | pass |
| Hanf80°Cgetrocknet_90min_15 | f'' (MPa) | 13.794 | 13.79387 | 0.0005 | pass |
| Hanf80°Cgetrocknet_90min_15 | N | 284 | 284 | 0.5 | pass |
| Hanf80°Cgetrocknet_90min_15 | R2 | 0.99967 | 0.99967 | 5e-06 | pass |
| Hanf80°Cgetrocknet_90min_15 | sigma_fM (MPa) | 123.678 | 123.67777 | 0.0005 | pass |
| Hanf80°Cgetrocknet_90min_15 | eps_fM (%) | 5.2 | 5.19870 | 0.05 | pass |
| Hanf80°Cgetrocknet_90min_16 | Ef (GPa) | 5.41 | 5.40769 | 0.005 | pass |
| Hanf80°Cgetrocknet_90min_16 | f' (MPa) | 2.474 | 2.47370 | 0.0005 | pass |
| Hanf80°Cgetrocknet_90min_16 | f'' (MPa) | 13.264 | 13.26422 | 0.0005 | pass |
| Hanf80°Cgetrocknet_90min_16 | N | 278 | 278 | 0.5 | pass |
| Hanf80°Cgetrocknet_90min_16 | R2 | 0.99866 | 0.99866 | 5e-06 | pass |
| Hanf80°Cgetrocknet_90min_16 | sigma_fM (MPa) | 114.864 | 114.86405 | 0.0005 | pass |
| Hanf80°Cgetrocknet_90min_16 | eps_fM (%) | 5.4 | 5.40162 | 0.05 | pass |
| Hanf80°Cgetrocknet_90min_17 | Ef (GPa) | 5.80 | 5.79681 | 0.005 | pass |
| Hanf80°Cgetrocknet_90min_17 | f' (MPa) | 1.508 | 1.50751 | 0.0005 | pass |
| Hanf80°Cgetrocknet_90min_17 | f'' (MPa) | 12.791 | 12.79092 | 0.0005 | pass |
| Hanf80°Cgetrocknet_90min_17 | N | 231 | 231 | 0.5 | pass |
| Hanf80°Cgetrocknet_90min_17 | R2 | 0.99892 | 0.99892 | 5e-06 | pass |
| Hanf80°Cgetrocknet_90min_17 | sigma_fM (MPa) | 123.503 | 123.50327 | 0.0005 | pass |
| Hanf80°Cgetrocknet_90min_17 | eps_fM (%) | 6.3 | 6.28539 | 0.05 | pass |
| Hanf80°Cgetrocknet_90min_18 | Ef (GPa) | 4.49 | 4.49301 | 0.005 | pass |
| Hanf80°Cgetrocknet_90min_18 | f' (MPa) | 0.582 | 0.58185 | 0.0005 | pass |
| Hanf80°Cgetrocknet_90min_18 | f'' (MPa) | 9.482 | 9.48187 | 0.0005 | pass |
| Hanf80°Cgetrocknet_90min_18 | N | 239 | 239 | 0.5 | pass |
| Hanf80°Cgetrocknet_90min_18 | R2 | 0.99690 | 0.99690 | 5e-06 | pass |
| Hanf80°Cgetrocknet_90min_18 | sigma_fM (MPa) | 115.274 | 115.27440 | 0.0005 | pass |
| Hanf80°Cgetrocknet_90min_18 | eps_fM (%) | 5.5 | 5.49096 | 0.05 | pass |
| Hanf80°Cgetrocknet_90min_19 | Ef (GPa) | 6.67 | 6.67417 | 0.005 | pass |
| Hanf80°Cgetrocknet_90min_19 | f' (MPa) | 2.122 | 2.12150 | 0.0005 | pass |
| Hanf80°Cgetrocknet_90min_19 | f'' (MPa) | 15.265 | 15.26473 | 0.0005 | pass |
| Hanf80°Cgetrocknet_90min_19 | N | 251 | 251 | 0.5 | pass |
| Hanf80°Cgetrocknet_90min_19 | R2 | 0.99988 | 0.99988 | 5e-06 | pass |
| Hanf80°Cgetrocknet_90min_19 | sigma_fM (MPa) | 123.649 | 123.64934 | 0.0005 | pass |
| Hanf80°Cgetrocknet_90min_19 | eps_fM (%) | 4.3 | 4.32884 | 0.05 | pass |
| Hanf80°Cgetrocknet_90min_20 | Ef (GPa) | 6.15 | 6.15146 | 0.005 | pass |
| Hanf80°Cgetrocknet_90min_20 | f' (MPa) | 1.659 | 1.65870 | 0.0005 | pass |
| Hanf80°Cgetrocknet_90min_20 | f'' (MPa) | 13.566 | 13.56585 | 0.0005 | pass |
| Hanf80°Cgetrocknet_90min_20 | N | 270 | 270 | 0.5 | pass |
| Hanf80°Cgetrocknet_90min_20 | R2 | 0.99943 | 0.99943 | 5e-06 | pass |
| Hanf80°Cgetrocknet_90min_20 | sigma_fM (MPa) | 133.718 | 133.71832 | 0.0005 | pass |
| Hanf80°Cgetrocknet_90min_20 | eps_fM (%) | 5.0 | 5.01917 | 0.05 | pass |
| Hanf80°Cgetrocknet_90min_21 | Ef (GPa) | 5.91 | 5.90892 | 0.005 | pass |
| Hanf80°Cgetrocknet_90min_21 | f' (MPa) | 2.341 | 2.34097 | 0.0005 | pass |
| Hanf80°Cgetrocknet_90min_21 | f'' (MPa) | 13.910 | 13.90996 | 0.0005 | pass |
| Hanf80°Cgetrocknet_90min_21 | N | 267 | 267 | 0.5 | pass |
| Hanf80°Cgetrocknet_90min_21 | R2 | 0.99941 | 0.99941 | 5e-06 | pass |
| Hanf80°Cgetrocknet_90min_21 | sigma_fM (MPa) | 118.848 | 118.84841 | 0.0005 | pass |
| Hanf80°Cgetrocknet_90min_21 | eps_fM (%) | 5.0 | 5.03329 | 0.05 | pass |
| Hanf80°Cfeucht9 | Ef (GPa) | 5.23 | 5.23489 | 0.005 | pass |
| Hanf80°Cfeucht9 | f' (MPa) | 1.997 | 1.99687 | 0.0005 | pass |
| Hanf80°Cfeucht9 | f'' (MPa) | 12.439 | 12.43882 | 0.0005 | pass |
| Hanf80°Cfeucht9 | N | 270 | 270 | 0.5 | pass |
| Hanf80°Cfeucht9 | R2 | 0.99926 | 0.99926 | 5e-06 | pass |
| Hanf80°Cfeucht9 | sigma_fM (MPa) | 144.473 | 144.47311 | 0.0005 | pass |
| Hanf80°Cfeucht9 | eps_fM (%) | 5.9 | 5.85080 | 0.05 | pass |
| Hanf80°Cfeucht10 | Ef (GPa) | 4.11 | 4.10618 | 0.005 | pass |
| Hanf80°Cfeucht10 | f' (MPa) | 1.840 | 1.84043 | 0.0005 | pass |
| Hanf80°Cfeucht10 | f'' (MPa) | 10.014 | 10.01434 | 0.0005 | pass |
| Hanf80°Cfeucht10 | N | 260 | 260 | 0.5 | pass |
| Hanf80°Cfeucht10 | R2 | 0.99948 | 0.99948 | 5e-06 | pass |
| Hanf80°Cfeucht10 | sigma_fM (MPa) | 121.358 | 121.35829 | 0.0005 | pass |
| Hanf80°Cfeucht10 | eps_fM (%) | 6.4 | 6.44139 | 0.05 | pass |
| Hanf80°Cfeucht11 | Ef (GPa) | 3.92 | 3.92436 | 0.005 | pass |
| Hanf80°Cfeucht11 | f' (MPa) | 1.246 | 1.24585 | 0.0005 | pass |
| Hanf80°Cfeucht11 | f'' (MPa) | 8.985 | 8.98476 | 0.0005 | pass |
| Hanf80°Cfeucht11 | N | 241 | 241 | 0.5 | pass |
| Hanf80°Cfeucht11 | R2 | 0.99980 | 0.99980 | 5e-06 | pass |
| Hanf80°Cfeucht11 | sigma_fM (MPa) | 125.262 | 125.26162 | 0.0005 | pass |
| Hanf80°Cfeucht11 | eps_fM (%) | 7.0 | 6.98384 | 0.05 | pass |
| Hanf80°Cfeucht12 | Ef (GPa) | 3.07 | 3.07058 | 0.005 | pass |
| Hanf80°Cfeucht12 | f' (MPa) | 0.991 | 0.99078 | 0.0005 | pass |
| Hanf80°Cfeucht12 | f'' (MPa) | 7.086 | 7.08599 | 0.0005 | pass |
| Hanf80°Cfeucht12 | N | 246 | 246 | 0.5 | pass |
| Hanf80°Cfeucht12 | R2 | 0.99971 | 0.99971 | 5e-06 | pass |
| Hanf80°Cfeucht12 | sigma_fM (MPa) | 131.719 | 131.71942 | 0.0005 | pass |
| Hanf80°Cfeucht12 | eps_fM (%) | 6.6 | 6.55355 | 0.05 | pass |
| Hanf80°Cfeucht13 | Ef (GPa) | 3.51 | 3.51034 | 0.005 | pass |
| Hanf80°Cfeucht13 | f' (MPa) | 1.261 | 1.26078 | 0.0005 | pass |
| Hanf80°Cfeucht13 | f'' (MPa) | 8.181 | 8.18131 | 0.0005 | pass |
| Hanf80°Cfeucht13 | N | 250 | 250 | 0.5 | pass |
| Hanf80°Cfeucht13 | R2 | 0.99968 | 0.99968 | 5e-06 | pass |
| Hanf80°Cfeucht13 | sigma_fM (MPa) | 131.476 | 131.47571 | 0.0005 | pass |
| Hanf80°Cfeucht13 | eps_fM (%) | 6.6 | 6.59263 | 0.05 | pass |
| Hanf80°Cfeucht14 | Ef (GPa) | 3.89 | 3.89152 | 0.005 | pass |
| Hanf80°Cfeucht14 | f' (MPa) | 1.259 | 1.25904 | 0.0005 | pass |
| Hanf80°Cfeucht14 | f'' (MPa) | 8.953 | 8.95325 | 0.0005 | pass |
| Hanf80°Cfeucht14 | N | 236 | 236 | 0.5 | pass |
| Hanf80°Cfeucht14 | R2 | 0.99987 | 0.99987 | 5e-06 | pass |
| Hanf80°Cfeucht14 | sigma_fM (MPa) | 138.383 | 138.38325 | 0.0005 | pass |
| Hanf80°Cfeucht14 | eps_fM (%) | 6.8 | 6.83212 | 0.05 | pass |
| Hanf80°Cfeucht_mit_Sensor2 | Ef (GPa) | 2.43 | 2.42541 | 0.005 | pass |
| Hanf80°Cfeucht_mit_Sensor2 | f' (MPa) | 0.878 | 0.87795 | 0.0005 | pass |
| Hanf80°Cfeucht_mit_Sensor2 | f'' (MPa) | 5.759 | 5.75869 | 0.0005 | pass |
| Hanf80°Cfeucht_mit_Sensor2 | N | 235 | 235 | 0.5 | pass |
| Hanf80°Cfeucht_mit_Sensor2 | R2 | 0.99734 | 0.99734 | 5e-06 | pass |
| Hanf80°Cfeucht_mit_Sensor2 | sigma_fM (MPa) | 132.448 | 132.44799 | 0.0005 | pass |
| Hanf80°Cfeucht_mit_Sensor2 | eps_fM (%) | 6.9 | 6.94976 | 0.05 | pass |
| Hanf80°Cfeucht_mit_Sensor3 | Ef (GPa) | 3.21 | 3.21320 | 0.005 | pass |
| Hanf80°Cfeucht_mit_Sensor3 | f' (MPa) | 1.184 | 1.18380 | 0.0005 | pass |
| Hanf80°Cfeucht_mit_Sensor3 | f'' (MPa) | 7.539 | 7.53874 | 0.0005 | pass |
| Hanf80°Cfeucht_mit_Sensor3 | N | 240 | 240 | 0.5 | pass |
| Hanf80°Cfeucht_mit_Sensor3 | R2 | 0.99992 | 0.99992 | 5e-06 | pass |
| Hanf80°Cfeucht_mit_Sensor3 | sigma_fM (MPa) | 104.453 | 104.45253 | 0.0005 | pass |
| Hanf80°Cfeucht_mit_Sensor3 | eps_fM (%) | 6.3 | 6.27042 | 0.05 | pass |
| Hanf80°Cfeucht_mit_Sensor4 | Ef (GPa) | 3.56 | 3.56010 | 0.005 | pass |
| Hanf80°Cfeucht_mit_Sensor4 | f' (MPa) | 1.144 | 1.14351 | 0.0005 | pass |
| Hanf80°Cfeucht_mit_Sensor4 | f'' (MPa) | 8.155 | 8.15544 | 0.0005 | pass |
| Hanf80°Cfeucht_mit_Sensor4 | N | 240 | 240 | 0.5 | pass |
| Hanf80°Cfeucht_mit_Sensor4 | R2 | 0.99991 | 0.99991 | 5e-06 | pass |
| Hanf80°Cfeucht_mit_Sensor4 | sigma_fM (MPa) | 112.203 | 112.20263 | 0.0005 | pass |
| Hanf80°Cfeucht_mit_Sensor4 | eps_fM (%) | 6.4 | 6.37602 | 0.05 | pass |
| Hanf80°Cfeucht_mit_Sensor5 | Ef (GPa) | 3.70 | 3.70435 | 0.005 | pass |
| Hanf80°Cfeucht_mit_Sensor5 | f' (MPa) | 1.315 | 1.31540 | 0.0005 | pass |
| Hanf80°Cfeucht_mit_Sensor5 | f'' (MPa) | 8.521 | 8.52067 | 0.0005 | pass |
| Hanf80°Cfeucht_mit_Sensor5 | N | 231 | 231 | 0.5 | pass |
| Hanf80°Cfeucht_mit_Sensor5 | R2 | 0.99960 | 0.99960 | 5e-06 | pass |
| Hanf80°Cfeucht_mit_Sensor5 | sigma_fM (MPa) | 116.773 | 116.77324 | 0.0005 | pass |
| Hanf80°Cfeucht_mit_Sensor5 | eps_fM (%) | 6.8 | 6.80838 | 0.05 | pass |
| Hanf80°Cfeucht_mit_Sensor6 | Ef (GPa) | 3.06 | 3.05972 | 0.005 | pass |
| Hanf80°Cfeucht_mit_Sensor6 | f' (MPa) | 0.896 | 0.89554 | 0.0005 | pass |
| Hanf80°Cfeucht_mit_Sensor6 | f'' (MPa) | 6.916 | 6.91643 | 0.0005 | pass |
| Hanf80°Cfeucht_mit_Sensor6 | N | 235 | 235 | 0.5 | pass |
| Hanf80°Cfeucht_mit_Sensor6 | R2 | 0.99844 | 0.99844 | 5e-06 | pass |
| Hanf80°Cfeucht_mit_Sensor6 | sigma_fM (MPa) | 130.812 | 130.81164 | 0.0005 | pass |
| Hanf80°Cfeucht_mit_Sensor6 | eps_fM (%) | 6.8 | 6.84537 | 0.05 | pass |
| Hanf80°Cfeucht_mit_Sensor7 | Ef (GPa) | 2.41 | 2.41233 | 0.005 | pass |
| Hanf80°Cfeucht_mit_Sensor7 | f' (MPa) | 0.245 | 0.24528 | 0.0005 | pass |
| Hanf80°Cfeucht_mit_Sensor7 | f'' (MPa) | 4.878 | 4.87809 | 0.0005 | pass |
| Hanf80°Cfeucht_mit_Sensor7 | N | 229 | 229 | 0.5 | pass |
| Hanf80°Cfeucht_mit_Sensor7 | R2 | 0.99393 | 0.99393 | 5e-06 | pass |
| Hanf80°Cfeucht_mit_Sensor7 | sigma_fM (MPa) | 129.054 | 129.05450 | 0.0005 | pass |
| Hanf80°Cfeucht_mit_Sensor7 | eps_fM (%) | 7.5 | 7.51726 | 0.05 | pass |
| Hanf80°Ctrocken_Ref_32 | Ef (GPa) | 8.54 | 8.53880 | 0.005 | pass |
| Hanf80°Ctrocken_Ref_32 | f' (MPa) | 1.679 | 1.67921 | 0.0005 | pass |
| Hanf80°Ctrocken_Ref_32 | f'' (MPa) | 18.846 | 18.84570 | 0.0005 | pass |
| Hanf80°Ctrocken_Ref_32 | N | 262 | 262 | 0.5 | pass |
| Hanf80°Ctrocken_Ref_32 | R2 | 0.99103 | 0.99103 | 5e-06 | pass |
| Hanf80°Ctrocken_Ref_32 | sigma_fM (MPa) | 174.162 | 174.16196 | 0.0005 | pass |
| Hanf80°Ctrocken_Ref_32 | eps_fM (%) | 2.3 | 2.28743 | 0.05 | pass |
| Hanf80°Ctrocken_Ref_29 | Ef (GPa) | 7.18 | 7.17704 | 0.005 | pass |
| Hanf80°Ctrocken_Ref_29 | f' (MPa) | 1.428 | 1.42806 | 0.0005 | pass |
| Hanf80°Ctrocken_Ref_29 | f'' (MPa) | 16.066 | 16.06607 | 0.0005 | pass |
| Hanf80°Ctrocken_Ref_29 | N | 288 | 288 | 0.5 | pass |
| Hanf80°Ctrocken_Ref_29 | R2 | 0.98679 | 0.98679 | 5e-06 | pass |
| Hanf80°Ctrocken_Ref_29 | sigma_fM (MPa) | 170.553 | 170.55291 | 0.0005 | pass |
| Hanf80°Ctrocken_Ref_29 | eps_fM (%) | 2.2 | 2.22806 | 0.05 | pass |
| Hanf80°Ctrocken_Ref_30 | Ef (GPa) | 12.2 | 12.15799 | 0.05 | pass |
| Hanf80°Ctrocken_Ref_30 | f' (MPa) | 4.800 | 4.79971 | 0.0005 | pass |
| Hanf80°Ctrocken_Ref_30 | f'' (MPa) | 29.031 | 29.03051 | 0.0005 | pass |
| Hanf80°Ctrocken_Ref_30 | N | 286 | 286 | 0.5 | pass |
| Hanf80°Ctrocken_Ref_30 | R2 | 0.99957 | 0.99957 | 5e-06 | pass |
| Hanf80°Ctrocken_Ref_30 | sigma_fM (MPa) | 192.537 | 192.53669 | 0.0005 | pass |
| Hanf80°Ctrocken_Ref_30 | eps_fM (%) | 2.4 | 2.42939 | 0.05 | pass |
| Hanf80°Ctrocken_Ref_31 | Ef (GPa) | 12.2 | 12.18199 | 0.05 | pass |
| Hanf80°Ctrocken_Ref_31 | f' (MPa) | 4.470 | 4.46958 | 0.0005 | pass |
| Hanf80°Ctrocken_Ref_31 | f'' (MPa) | 28.608 | 28.60820 | 0.0005 | pass |
| Hanf80°Ctrocken_Ref_31 | N | 264 | 264 | 0.5 | pass |
| Hanf80°Ctrocken_Ref_31 | R2 | 0.99986 | 0.99986 | 5e-06 | pass |
| Hanf80°Ctrocken_Ref_31 | sigma_fM (MPa) | 183.763 | 183.76335 | 0.0005 | pass |
| Hanf80°Ctrocken_Ref_31 | eps_fM (%) | 2.2 | 2.23623 | 0.05 | pass |

## Excluded, and why

Each excluded file's own header states a maximum flexural stress its force
data never reach, at the header's own width, thickness and span. The
curve exported is not the one testXpert evaluated, so it cannot check
TenForge against testXpert. The test re-checks this on every run: if an
excluded file's data did reach its header's maximum, the run fails.


- Hanf80°Cfeucht8: header 120.259 MPa, data 101.117 MPa. The file's own header gives a maximum flexural stress of 120.259 MPa; its force data peak at 242.2 N, which at its stated width, thickness and span is 101.1 MPa. The curve in the file is not the one testXpert evaluated, so it cannot check TenForge against testXpert.
- Hanf80°Cfeucht_mit_Sensor1: header 116.381 MPa, data 114.983 MPa. The file's own header gives a maximum flexural stress of 116.381 MPa; its force data peak at 1.2 % less. As for feucht8, the curve in the file is not the one testXpert evaluated.

## Synthetic curves

`tools/fixtures/make_modes.py` writes four curves with closed-form
answers, in machine units with lopsided dimensions (committed under
`tests/fixtures/modes/`): a three-point beam (break, and a sample at
exactly 3.5 % strain), a four-point beam without a break, a rigid foam
with a toe and a maximum before 10 %, and a plastic in compression
recorded with negative signs. The engine, the session and the profiles
reproduce every answer to within 1e-6 of its value
(`compression_and_bending_models`, `analysis_session_guides_and_analyses`).

ISO 178's stress at conventional deflection is defined at a deflection of
1.5 h, which is 3.5 % strain only when L = 16 h. TenForge computes the
strain for each specimen; on the three-point fixture (L/h = 15.6) that is
3.69 %, and the stress matches the closed form there.

## Real foam, no reference

Universidade de Sao Paulo's aged PU foam compressed to ASTM D1621
(Pires da Silva, [doi:10.17632/2sp8fyvhfm.3](https://doi.org/10.17632/2sp8fyvhfm.3),
CC BY 4.0) publishes raw curves and dimensions but no evaluated results,
so it checks only that the importer reads its export (French column
names, a units row, Latin-1) and that ISO 844 runs on real, noisy data
with a toe. On `1BAT-DIR1-1.TRA` (taking the first row of
`1BAT-DIM.txt` as 50.4 x 50.8 mm, h0 50.0 mm; the archive does not say
which column is the height): E 13590 kPa, sigma_m 454.0 kPa at 5.54 %,
sigma_10 420.4 kPa, the zero point after the recorded zero. These are not
compared with anything.

## Known gaps

- **Compression has no reference evaluation.** No open dataset found so
  far publishes raw compression curves together with a reference
  software's results. ISO 604 and ISO 844 rest on the synthetic curves
  and the real-foam run above.
- **Three-point bending has no reference evaluation** either; the
  bending model is the same kernel (a = L/2), checked by hand and on the
  synthetic beam.
- **ISO 14125 Annex B** (the large-deflection correction for method A) is
  not implemented, and its stress term is not yet confirmed from the text.
- **ISO 844's fallback zero** (the deformation at 250 Pa, for a curve
  with no straight part) is not implemented; such a curve is refused for
  want of a modulus.
- **ISO 844 has a 2026 edition.** The profile follows the 2021 edition's
  public preview; the 2026 text has not been seen.
- **Every parameter is unconfirmed** until read in the purchased
  standards (ISO 604, 844, 178, 14125), so every result is labelled
  aligned, not to the standard.
- **Specimen classes** of ISO 14125 (span-to-thickness by class) are not
  checked: the class is not an input yet. ISO 14125 method B checks that
  the loading span is a third of the span; ISO 178 that L/h is 15 to 17.
