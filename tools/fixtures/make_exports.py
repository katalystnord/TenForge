#!/usr/bin/env python3
"""The hardening curve TenForge's FreeCAD and CalculiX exports must give for
the committed Ramberg-Osgood fixture, from its closed form.

Run once; the output is committed as tests/fixtures/exports/
ro-hardening.json. Standard library only. The test compares TenForge's
export with these numbers, so this file must not share TenForge's code.

The fixture (make_synthetic.py, ro-continuous): engineering strain
    e(s) = s / E + 0.002 (s / Rp)^n,  E = 200000 MPa, Rp = 300 MPa, n = 10,
up to Rm = 500 MPa. Its zero point is the recorded zero, since the curve
starts at (0, 0) with slope E.

True stress and true strain, as for any uniaxial test before necking:
    sigma_t = s (1 + e),  eps_t = ln(1 + e),
and the true plastic strain is what is left of the true strain once the
elastic part sigma_t / E is taken off:
    eps_p = eps_t - sigma_t / E.
Initial yield is the proof strength Rp0.2 (the fixture has no yield
point), given at plastic strain 0: below it an elastic-plastic model is
elastic. Then the curve's own true plastic strains follow, from the Rp0.2
point (where, by Rp0.2's definition, the permanent strain is 0.2 %) to Rm,
at N - 1 points evenly spaced in eps_p. (Taking eps_p >= 0 alone as the
start would put initial yield at 135 MPa on this curve: eps_p as defined
is slightly negative until the plastic part outgrows the true-strain
correction.) Necking is excluded.
"""

import json
import math
import os
import sys

E, RP, N_EXP, RM = 200000.0, 300.0, 10.0, 500.0
POINTS = 20

OUT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
    os.path.dirname(__file__), "..", "..", "tests", "fixtures", "exports")


def eng_strain(s):
    return s / E + 0.002 * (s / RP) ** N_EXP


def plastic(s):
    e = eng_strain(s)
    return math.log1p(e) - s * (1 + e) / E


def solve(target):
    """The engineering stress whose true plastic strain is target."""
    lo, hi = 0.0, RM
    for _ in range(200):
        mid = (lo + hi) / 2
        if plastic(mid) < target:
            lo = mid
        else:
            hi = mid
    return (lo + hi) / 2


def main():
    s_y = RP                       # Rp0.2 of this curve, exactly
    e_y = eng_strain(s_y)
    p_y = plastic(s_y)
    p_end = plastic(RM)
    points = [{"true_stress_MPa": s_y * (1 + e_y), "plastic_strain": 0.0}]
    for k in range(POINTS - 1):
        p = p_y + (p_end - p_y) * k / (POINTS - 2)
        s = s_y if k == 0 else solve(p)
        points.append({"true_stress_MPa": s * (1 + eng_strain(s)), "plastic_strain": p})
    os.makedirs(OUT, exist_ok=True)
    with open(os.path.join(OUT, "ro-hardening.json"), "w", newline="\n") as f:
        json.dump({"fixture": "synthetic/ro-continuous.csv",
                   "note": "True stress against true plastic strain, closed form: "
                           "Rp0.2 at plastic strain 0, then the curve's own plastic "
                           "strains from Rp0.2 to Rm (500 MPa engineering).",
                   "E_MPa": E, "points": points}, f, indent=2)
        f.write("\n")
    print("written", os.path.join(OUT, "ro-hardening.json"))


if __name__ == "__main__":
    main()
