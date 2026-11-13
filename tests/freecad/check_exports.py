# Runs inside FreeCAD (FreeCADCmd): TenForge's FreeCAD exports, used as a
# FreeCAD user would use them (ROADMAP Phase 4 exit criterion).
#
#   TF_CARD=<name>.FCMat  TF_YIELD=<name>-yieldpoints.txt  TF_OUT=<result.json>
#   FREECAD_USER_HOME=<an empty folder>  FreeCADCmd check_exports.py
#
# FREECAD_USER_HOME keeps FreeCAD's settings and user material library in a
# throwaway folder: a test never touches the user's own FreeCAD.
#
# 1. The material card loads in FreeCAD's material manager, with the values
#    TenForge wrote.
# 2. FreeCAD's own nonlinear example (femexamples.material_nl_platewithhole)
#    is set up, given TenForge's modulus and yield points, written for
#    CalculiX and solved by ccx through FEM's own tools. The solve must end,
#    the input FreeCAD wrote must carry TenForge's *PLASTIC lines, and the
#    plate must yield (some von Mises stress above initial yield).
#
# Poisson's ratio is not measured by a tensile test; the example's 0.30 is
# kept and the result says so.

import json
import os
import traceback

import FreeCAD

out = {"ok": False}
try:
    import Materials

    import re
    import shutil

    card = os.environ["TF_CARD"]
    yields = [l.strip() for l in open(os.environ["TF_YIELD"]) if l.strip()]
    # Installed as a user installs a card: copied into the user library.
    home = os.environ["FREECAD_USER_HOME"]
    if not os.path.realpath(FreeCAD.getUserAppDataDir()).startswith(os.path.realpath(home)):
        raise RuntimeError("refusing to run outside the throwaway FREECAD_USER_HOME")
    lib = os.path.join(home, "Material", "TenForge")
    os.makedirs(lib, exist_ok=True)
    shutil.copy(card, lib)
    uuid = re.search(r'UUID: "([0-9a-f-]{36})"', open(card).read()).group(1)
    manager = Materials.MaterialManager()
    manager.refresh()
    mat = manager.getMaterial(uuid)
    props = mat.PhysicalProperties
    out["card"] = {"name": mat.Name,
                   "YoungsModulus": str(props.get("YoungsModulus")),
                   "YieldStrength": str(props.get("YieldStrength")),
                   "UltimateTensileStrength": str(props.get("UltimateTensileStrength"))}

    from femexamples import material_nl_platewithhole as example
    from femtools import ccxtools

    doc = example.setup()
    lin = doc.getObject("Material_lin")
    m = lin.Material
    m["YoungsModulus"] = props["YoungsModulus"]
    lin.Material = m
    nonlin = doc.getObject("Material_nonlin")
    nonlin.YieldPoints = yields
    doc.recompute()

    analysis = doc.getObject("Analysis")
    solver = doc.getObject("CalculiXCcxTools")
    work = os.path.join(os.path.dirname(os.environ["TF_OUT"]), "ccx")
    os.makedirs(work, exist_ok=True)
    solver.WorkingDir = work
    fea = ccxtools.FemToolsCcx(analysis, solver)
    fea.update_objects()
    fea.setup_working_dir(work)
    fea.setup_ccx()
    message = fea.check_prerequisites()
    out["prerequisites"] = message
    fea.write_inp_file()
    inp = open(fea.inp_file_name).read()
    out["plastic_lines_in_inp"] = sum(1 for y in yields if y in inp)
    out["yield_points"] = len(yields)
    fea.ccx_run()
    fea.load_results()
    result = None
    for o in doc.Objects:
        if o.isDerivedFrom("Fem::FemResultObject"):
            result = o
    if result is None:
        raise RuntimeError("CalculiX gave no result")
    vm = max(result.vonMises)
    out["max_von_mises_MPa"] = vm
    out["initial_yield_MPa"] = float(yields[0].split(",")[0])
    out["ok"] = (out["plastic_lines_in_inp"] == len(yields)
                 and vm > out["initial_yield_MPa"] * 0.99)
except Exception:
    out["error"] = traceback.format_exc()

with open(os.environ["TF_OUT"], "w") as f:
    json.dump(out, f, indent=2)
print(json.dumps(out, indent=2))
