#!/usr/bin/env python3
"""Write TenForge's synthetic compression and flexure fixtures and their
exact answers.

Run once; the output is committed under tests/fixtures/modes/. As with
make_synthetic.py, the answers come from the closed-form curves below,
never from TenForge's own code, so a test comparing the two is a real
check (the project's fixture rule, CLAUDE.md). Standard library only.

Each curve is defined as stress against strain, then written as a machine
would record it: force and a displacement (deflection, or the platens'
travel) in machine units, with dimensions that are not round, so a unit,
geometry or formula slip cannot pass by accident.

Curves:

beam-3pt      Three-point bending. Exactly linear to 0.6 % strain, a smooth
              rise to a planted maximum, a straight fall, then a break.
              A sample sits exactly at 3.5 % strain, the conventional
              strain, so the stress there has an exact answer.
beam-4pt      Four-point bending, loading span a third of the span,
              deflection measured at the loading points (the crosshead).
              Linear, a rise to the maximum, a slight fall, and NO break,
              so every result at break must be refused.
foam          Rigid foam in compression. A quadratic toe that joins the
              straight part tangentially, so the steepest straight line
              meets zero stress at a planted strain; a maximum before 10 %
              relative deformation, a fall, a straight stretch through
              10 %, and densification. No break.
compression   A plastic in compression, written with NEGATIVE force and
              displacement as many machines record compression. Linear, a
              rise to a yield maximum (zero slope), strain softening, then
              hardening to a maximum above the yield, then a break.
"""

import json
import os
import sys

OUT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
    os.path.dirname(__file__), "..", "..", "tests", "fixtures", "modes")


def write(name, header, rows, meta):
    """rows: tuples already in the file's units, written with 9 decimals."""
    os.makedirs(OUT, exist_ok=True)
    with open(os.path.join(OUT, name + ".csv"), "w", newline="\n") as f:
        f.write(header + "\n")
        for i, r in enumerate(rows):
            f.write("%.4f," % (0.05 * i) + ",".join("%.9f" % v for v in r) + "\n")
    meta = dict(meta, file=name + ".csv")
    with open(os.path.join(OUT, name + ".json"), "w", newline="\n") as f:
        json.dump(meta, f, indent=2, sort_keys=True)
        f.write("\n")


def rise(e0, s0, e1, s1, E, steps):
    """From (e0, s0), where the slope is E, to a maximum (e1, s1) with zero
    slope: s = s1 - (s1 - s0) (1 - v)^p, v = (e - e0) / (e1 - e0), with p
    chosen so the slope at v = 0 is E. The slope falls monotonically."""
    p = E * (e1 - e0) / (s1 - s0)
    assert 1 < p < 4, "the rise must start at slope E, flatten, and stay resolvable"
    return [(e0 + (e1 - e0) * k / steps, s1 - (s1 - s0) * (1 - k / steps) ** p)
            for k in range(1, steps + 1)]


def line(e0, s0, e1, s1, steps):
    return [(e0 + (e1 - e0) * k / steps, s0 + (s1 - s0) * k / steps)
            for k in range(1, steps + 1)]


def beam_3pt():
    E, el = 2800.0, 0.006                 # MPa, end of the straight part
    sm, em = 92.0, 0.05                   # the maximum
    sb, eb = 85.0, 0.07                   # the break
    b, h, L = 10.2, 4.1, 64.0             # mm
    curve = [(el * k / 600, E * el * k / 600) for k in range(0, 601)]
    # 4400 steps of 1e-5 strain: 0.035 is sample 2900 of the rise.
    curve += rise(el, E * el, em, sm, E, 4400)
    e_c = 0.035
    s_c = [s for (e, s) in curve if abs(e - e_c) < 1e-12][0]
    # ISO 178's conventional deflection is 1.5 h: strain 6 (1.5 h) h / L^2
    # = 9 h^2 / L^2, which is 3.5 % only when L = 16 h. Here L/h = 15.6, so
    # the stress there is taken from the closed form of the rise (exact on
    # the curve; linear interpolation between samples 1e-5 apart differs by
    # under 1e-7 of it).
    e_15h = 9 * h * h / (L * L)
    p_rise = E * (em - el) / (sm - E * el)
    s_15h = sm - (sm - E * el) * (1 - (e_15h - el) / (em - el)) ** p_rise
    curve += line(em, sm, eb, sb, 400)
    curve.append((eb + 0.0004, 0.01 * sm))
    # stress = 3 F L / (2 b h^2); strain = 6 s h / L^2
    rows = [(e * L * L / (6 * h), s * 2 * b * h * h / (3 * L)) for (e, s) in curve]
    write("beam-3pt", "time_s,deflection_mm,force_N", rows, {
        "columns": {"deflection": "deflection_mm:mm", "force": "force_N:N"},
        "fixture": "three-point",
        "width_mm": b, "thickness_mm": h, "span_mm": L,
        "note": "Three-point bend: linear Ef = 2.8 GPa to 0.6 %; maximum 92 MPa "
                "at 5 %; break at 7 % and 85 MPa; a sample exactly at 3.5 %.",
        "answers": {"Ef_Pa": E * 1e6, "sigma_fM_Pa": sm * 1e6, "epsilon_fM_1": em,
                    "sigma_fB_Pa": sb * 1e6, "epsilon_fB_1": eb,
                    "sigma_fC_Pa": s_c * 1e6, "epsilon_fC_1": e_c,
                    "sigma_at_1.5h_Pa": s_15h * 1e6, "epsilon_at_1.5h_1": e_15h,
                    "Fm_N": sm * 2 * b * h * h / (3 * L)},
    })


def beam_4pt():
    E, el = 25000.0, 0.012
    sm, em = 420.0, 0.018
    b, h, L, la = 15.3, 2.7, 81.0, 27.0   # loading span la = L / 3
    a = (L - la) / 2                      # support to the nearest loading point
    curve = [(el * k / 1200, E * el * k / 1200) for k in range(0, 1201)]
    curve += rise(el, E * el, em, sm, E, 600)
    curve += line(em, sm, 0.02, 410.0, 200)
    # stress = 3 F a / (b h^2). Deflection at the loading points of a beam
    # under two equal loads F/2 at a from each support:
    #   s_load = F a^2 (3L - 4a) / (12 E I),  I = b h^3 / 12,
    # and the surface strain between the loads is sigma / E, so
    #   strain = 3 h s_load / (a (3L - 4a)).
    rows = [(e * a * (3 * L - 4 * a) / (3 * h), s * b * h * h / (3 * a)) for (e, s) in curve]
    write("beam-4pt", "time_s,deflection_mm,force_N", rows, {
        "columns": {"deflection": "deflection_mm:mm", "force": "force_N:N"},
        "fixture": "four-point", "deflection_at": "loading-points",
        "width_mm": b, "thickness_mm": h, "span_mm": L, "loading_span_mm": la,
        "note": "Four-point bend, deflection at the loading points: linear "
                "Ef = 25 GPa to 1.2 %; maximum 420 MPa at 1.8 %; a slight fall; "
                "no break.",
        "answers": {"Ef_Pa": E * 1e6, "sigma_fM_Pa": sm * 1e6, "epsilon_fM_1": em,
                    "Fm_N": sm * b * h * h / (3 * a)},
    })


def foam():
    E, d0 = 11.5, 0.004                   # MPa; strain where the line meets zero
    w, t, h0 = 50.3, 49.6, 49.8           # mm
    area = w * t
    # Recorded strain = true strain + d0. Toe: s = E e^2 / (4 d0) for e in
    # [0, 2 d0], which meets the line s = E (e - d0) at e = 2 d0 with the
    # same slope E; its slope is below E everywhere before.
    curve = [(2 * d0 * k / 80, E * (2 * d0 * k / 80) ** 2 / (4 * d0)) for k in range(0, 81)]
    # The rise to the maximum must be steep enough near its top that the
    # samples before the maximum still differ from it at the file's 9
    # decimals; the first version (straight to 2 %, maximum at 5 %, exponent
    # 5.75) was not, and several samples printed equal to the maximum.
    el = 0.015                            # true strain at the end of the straight part
    curve += [(d0 + e, E * e) for e in [d0 + (el - d0) * k / 160 for k in range(1, 161)]]
    sm, em = 0.29, 0.04                   # maximum, true strain
    curve += [(d0 + e, s) for (e, s) in rise(el, E * el, em, sm, E, 300)]
    curve += [(d0 + e, s) for (e, s) in line(em, sm, 0.07, 0.275, 200)]
    curve += [(d0 + e, s) for (e, s) in line(0.07, 0.275, 0.20, 0.31, 1300)]
    curve += [(d0 + e, s) for (e, s) in line(0.20, 0.31, 0.60, 0.60, 400)]
    s10 = 0.275 + (0.31 - 0.275) * (0.10 - 0.07) / 0.13
    rows = [(e * h0, s * area) for (e, s) in curve]
    write("foam", "time_s,deformation_mm,force_N", rows, {
        "columns": {"deformation": "deformation_mm:mm", "force": "force_N:N"},
        "fixture": "platens",
        "width_mm": w, "thickness_mm": t, "height_mm": h0,
        "note": "Rigid foam: quadratic toe; straight part E = 11.5 MPa meeting "
                "zero stress at 0.4 % recorded strain; maximum 0.29 MPa at 4 % "
                "from that zero; 10 % falls on a straight stretch; no break.",
        "answers": {"E_Pa": E * 1e6, "zero_point_1": d0, "sigma_m_Pa": sm * 1e6,
                    "epsilon_m_1": em, "sigma_10_Pa": s10 * 1e6},
    })


def compression():
    E, el = 3200.0, 0.01
    sy, ey = 110.0, 0.05
    sM, eM = 150.0, 0.40
    w, t, L0 = 10.1, 4.05, 25.4           # mm; L0 the length strain is taken over
    area = w * t
    curve = [(el * k / 500, E * el * k / 500) for k in range(0, 501)]
    curve += rise(el, E * el, ey, sy, E, 800)
    curve += line(ey, sy, 0.08, 98.0, 300)
    curve += line(0.08, 98.0, eM, sM, 3200)
    curve.append((eM + 0.0004, 0.01 * sM))
    # Compression written as negative force and displacement, in kN.
    rows = [(-e * L0, -s * area / 1000.0) for (e, s) in curve]
    write("compression", "time_s,compression_mm,force_kN", rows, {
        "columns": {"compression": "compression_mm:mm", "force": "force_kN:kN"},
        "fixture": "platens",
        "width_mm": w, "thickness_mm": t, "length_mm": L0,
        "note": "Plastic in compression, recorded negative: linear Ec = 3.2 GPa "
                "to 1 %; yield maximum 110 MPa at 5 %; softening to 98 MPa at "
                "8 %; hardening to 150 MPa at 40 %; break.",
        "answers": {"Ec_Pa": E * 1e6, "sigma_y_Pa": sy * 1e6, "epsilon_y_1": ey,
                    "sigma_M_Pa": sM * 1e6, "epsilon_M_1": eM,
                    "sigma_B_Pa": sM * 1e6, "epsilon_B_1": eM},
    })


if __name__ == "__main__":
    beam_3pt()
    beam_4pt()
    foam()
    compression()
    print("written to", os.path.abspath(OUT))
