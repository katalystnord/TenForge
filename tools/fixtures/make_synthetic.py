#!/usr/bin/env python3
"""Write TenForge's synthetic tension fixtures and their exact answers.

Run once; the output is committed under tests/fixtures/synthetic/. The
answers come from the closed-form curves below, never from TenForge's own
code, so a test that compares the two is a real check (the project's
fixture rule, CLAUDE.md).

Standard library only, so anyone can rerun it and diff the result.

Every curve is written in the units a machine export uses (s, mm, kN) with
a cross-section and gauge length that are not 1, so a unit or a
force/stress mix-up cannot pass by accident.

Curves:

ro-continuous      Ramberg-Osgood, eps = s/E + 0.002 (s/Rp)^n, so the
                   0.2 % proof stress is exactly Rp. Rises to Rm, then a
                   linear neck, then fracture.
discontinuous      Elastic to ReH, a drop, a Lueders plateau with a planted
                   lowest point ReL, work hardening to Rm, neck, fracture.
transient-lowest   As discontinuous, but the first minimum after ReH (the
                   initial transient) is the lowest point of the plateau,
                   so the two ReL rules give different answers.
polymer            Exactly linear to 0.4 % strain (so a chord or a
                   regression between 0.05 % and 0.25 % gives E exactly),
                   then a smooth rise to a planted maximum, a fall, and a
                   break.
"""

import json
import math
import os
import sys

OUT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
    os.path.dirname(__file__), "..", "..", "tests", "fixtures", "synthetic")


def write(name, rows, area_mm2, gauge_mm, answers, note):
    """rows: (strain [1], stress [MPa]). Written as time, extension, force."""
    os.makedirs(OUT, exist_ok=True)
    path = os.path.join(OUT, name + ".csv")
    with open(path, "w", newline="\n") as f:
        f.write("time_s,extension_mm,force_kN\n")
        for i, (eps, sig) in enumerate(rows):
            t = 0.05 * i
            f.write("%.4f,%.9f,%.9f\n" % (t, eps * gauge_mm, sig * area_mm2 / 1000.0))
    meta = {
        "file": name + ".csv",
        "columns": {"time": "time_s:s", "extension": "extension_mm:mm",
                    "force": "force_kN:kN"},
        "area_mm2": area_mm2,
        "gauge_length_mm": gauge_mm,
        "note": note,
        "answers": answers,
    }
    with open(os.path.join(OUT, name + ".json"), "w", newline="\n") as f:
        json.dump(meta, f, indent=2)
        f.write("\n")


def ro_continuous():
    E, Rp, n, Rm = 200000.0, 300.0, 10.0, 500.0   # MPa
    def eps(s):
        return s / E + 0.002 * (s / Rp) ** n
    rows = [(eps(s), s) for s in [0.5 * k for k in range(0, int(Rm / 0.5) + 1)]]
    eps_m = eps(Rm)
    # Neck: stress falls linearly to 0.8 Rm over a further 0.08 strain.
    sig_f = 0.8 * Rm
    for k in range(1, 401):
        rows.append((eps_m + 0.0002 * k, Rm - (Rm - sig_f) * k / 400.0))
    eps_f = rows[-1][0]
    rows.append((eps_f + 0.0003, 0.01 * Rm))  # fracture: force collapses
    rows.append((eps_f + 0.0004, 0.002 * Rm))
    answers = {
        "E_Pa": E * 1e6,
        "Rp0.2_Pa": Rp * 1e6,
        "Rm_Pa": Rm * 1e6,
        "Fm_N": Rm * 78.54,
        "Agt_1": eps_m,
        "Ag_1": eps_m - Rm / E,
        "At_1": eps_f,
        "A_1": eps_f - sig_f / E,
    }
    write("ro-continuous", rows, 78.54, 50.0, answers,
          "Ramberg-Osgood E=200 GPa, Rp0.2=300 MPa, n=10; Rm=500 MPa; "
          "fracture after the last point of the neck.")


def plateau(planted_min, transient_min):
    """Elastic to ReH, drop to the transient minimum, a wavy plateau whose
    lowest point is planted_min, then hardening to Rm, neck, fracture."""
    E, ReH, Rm = 210000.0, 480.0, 560.0
    rows = []
    for k in range(0, 481):
        s = ReH * k / 480.0
        rows.append((s / E, s))
    e_h = ReH / E
    # The drop after ReH: ten points down to the transient minimum.
    for k in range(1, 11):
        rows.append((e_h + 0.00012 * k, ReH - (ReH - transient_min) * k / 10.0))
    e = rows[-1][0]
    # Plateau from e to 0.022: waves around 440 MPa, one planted dip.
    plateau_end = 0.022
    steps = 400
    de = (plateau_end - e) / steps
    for k in range(1, steps + 1):
        x = e + de * k
        s = 440.0 + 4.0 * math.sin(k / 9.0)
        if k == 250:
            s = planted_min
        rows.append((x, s))
    # Every plateau point other than the planted ones must stay above both.
    e_p = rows[-1][0]
    s_p = rows[-1][1]
    # Hardening: s = s_p + (Rm - s_p) * (1 - (1 - u)^2), u from 0 to 1, over
    # strain e_p .. 0.15, so the maximum Rm is reached exactly at u = 1.
    e_m = 0.15
    for k in range(1, 801):
        u = k / 800.0
        rows.append((e_p + (e_m - e_p) * u, s_p + (Rm - s_p) * (1 - (1 - u) ** 2)))
    sig_f = 0.75 * Rm
    for k in range(1, 301):
        rows.append((e_m + 0.0003 * k, Rm - (Rm - sig_f) * k / 300.0))
    e_f = rows[-1][0]
    rows.append((e_f + 0.0003, 0.01 * Rm))
    return rows, E, ReH, Rm, e_m, e_f, sig_f


def discontinuous():
    rows, E, ReH, Rm, e_m, e_f, sig_f = plateau(planted_min=432.0, transient_min=437.0)
    answers = {"E_Pa": E * 1e6, "ReH_Pa": ReH * 1e6, "ReL_Pa": 432.0e6,
               "Rm_Pa": Rm * 1e6, "Agt_1": e_m, "Ag_1": e_m - Rm / E,
               "At_1": e_f, "A_1": e_f - sig_f / E}
    write("discontinuous", rows, 120.64, 50.0, answers,
          "ReH=480 MPa; initial transient minimum 437 MPa; planted plateau "
          "minimum ReL=432 MPa; Rm=560 MPa.")


def transient_lowest():
    rows, E, ReH, Rm, e_m, e_f, sig_f = plateau(planted_min=433.0, transient_min=428.0)
    answers = {"ReH_Pa": ReH * 1e6,
               "ReL_including_transient_Pa": 428.0e6,
               "ReL_ignoring_transient_Pa": 433.0e6,
               "Rm_Pa": Rm * 1e6}
    write("transient-lowest", rows, 120.64, 50.0, answers,
          "ReH=480 MPa; initial transient minimum 428 MPa is the lowest "
          "point; lowest point after it is 433 MPa.")


def polymer():
    E, sm, em, eb = 2500.0, 60.0, 0.05, 0.20   # MPa, MPa, 1, 1
    e_lin = 0.004
    s_lin = E * e_lin   # 10 MPa
    rows = []
    for k in range(0, 401):
        e = e_lin * k / 400.0
        rows.append((e, E * e))
    # From e_lin to em: s = s_lin + (sm - s_lin) * sin(pi/2 * v)^... use a
    # curve with slope E at the join and zero slope at em:
    # s = sm - (sm - s_lin) * (1 - v)^p, v = (e - e_lin)/(em - e_lin),
    # slope at v=0 is (sm - s_lin) p / (em - e_lin); choose p for slope E.
    p = E * (em - e_lin) / (sm - s_lin)
    for k in range(1, 1001):
        v = k / 1000.0
        rows.append((e_lin + (em - e_lin) * v, sm - (sm - s_lin) * (1 - v) ** p))
    # Fall after the maximum to 0.8 sm at break.
    for k in range(1, 601):
        rows.append((em + (eb - em) * k / 600.0, sm - 0.2 * sm * k / 600.0))
    rows.append((eb + 0.0004, 0.005 * sm))
    answers = {"Et_Pa": E * 1e6, "sigma_m_Pa": sm * 1e6, "epsilon_m_1": em,
               "epsilon_b_1": eb, "sigma_b_Pa": 0.8 * sm * 1e6}
    write("polymer", rows, 40.0, 75.0, answers,
          "Linear E=2.5 GPa to 0.4 % strain; maximum 60 MPa at 5 %; break "
          "at 20 % strain and 48 MPa.")


if __name__ == "__main__":
    ro_continuous()
    discontinuous()
    transient_lowest()
    polymer()
    print("written to", os.path.abspath(OUT))
