#!/usr/bin/env python3
"""Read a TenForge curve (.vtp, from File > Export curve for ParaView or
tf-analyse --vtp) with PyVista: the arrays, the results stored with it,
and a plot.

    python pyvista_curve.py curve.vtp [plot.png]

Needs only PyVista (pip install pyvista). The point arrays are SI:
strain (1), stress_Pa, force_N; the results are field data named
<id>_<unit> (Rm_Pa, Agt_1, ...). The points themselves are (strain %,
stress MPa, 0), so the curve also looks right in a 3D view.
"""

import sys

import pyvista as pv


def main():
    curve = pv.read(sys.argv[1])
    print(f"{curve.n_points} samples, arrays: {', '.join(curve.point_data.keys())}")
    for name in curve.field_data.keys():
        print(f"  {name} = {curve.field_data[name][0]:g}")

    strain = curve.point_data["strain"] * 100.0      # %
    stress = curve.point_data["stress_Pa"] * 1e-6    # MPa
    if len(sys.argv) > 2:
        chart = pv.Chart2D()
        chart.line(strain, stress, label="TenForge curve")
        chart.x_label = "Strain (%)"
        chart.y_label = "Stress (MPa)"
        chart.show(off_screen=True, screenshot=sys.argv[2])
        print("plot written to", sys.argv[2])


if __name__ == "__main__":
    main()
