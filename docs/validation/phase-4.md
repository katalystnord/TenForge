# Phase 4 validation

What TenForge's exports were checked against, on 2026-11-13 (TenForge
0.3.0). Exports are judged by whether the receiving program reads them
and does the right thing with them, so each check runs that program.
The rules for each file: [exports-v1](../specs/exports-v1.md).

| Export | Checked with | How | Test |
|---|---|---|---|
| FreeCAD card (`.FCMat`) | FreeCAD 1.1.4 (flatpak) | The card is put in a temporary user library, found by its UUID through FreeCAD's material manager, and its values read back. | `freecad_loads_the_card_and_calculix_runs_the_yield_points` |
| Yield points | FreeCAD 1.1.4 with CalculiX | FreeCAD's own nonlinear example (plate with a hole) given the exported yield points and solved with CalculiX: peak von Mises 318 MPa against a 301 MPa initial yield, so the plastic branch was used. | same |
| Curve (`.vtp`) | VTK's XML reader; ParaView 6.1.0 pvpython | Every array and every result read back; plotted in a line chart. | `exports_for_freecad_calculix_and_paraview`; ParaView by a hand-run pvpython script |
| Record reader plugin | ParaView 6.1.0 (flatpak) | Opens a record's `manifest.tfrec`, gives the curve and field data; a copy with a changed `curve.vtp` is refused. | `paraview_opens_a_record_and_refuses_a_changed_one` |
| Curve (`.vtp`) | PyVista 0.49.2 on VTK 9.7.2 | `tools/recipes/pyvista_curve.py` reads the curve and its results. | by hand, not in the suite |
| PMD JSON-LD | PMDco 3.1.2, TTO 3.0.0 (fetched, SHA-256 pinned) | Every class and property written is declared in the released ontology files; QUDT unit IRIs resolve. Not yet checked against PMDco's SHACL shapes. | `pmd_jsonld_uses_only_terms_of_the_released_ontologies` |
| PlotTracer import | PlotTracer's own exporter | The fixture was written by PlotTracer's export code (`tools/fixtures/make_plottracer_export.test.ts`). | `reference_curves_from_plottracer` |

The FreeCAD and ParaView tests run only where those flatpaks are
installed; CI has neither, and each kit's tests run with the others
absent. The exports, PMD, ParaView and PlotTracer tests were each seen
red with the property they guard broken; the FreeCAD test was not (the
log is at the top of `tests/CMakeLists.txt`).

Not done: the record as `.pvd`/`.vtu` (moved to Phase 6, when there are
DIC fields to hold); PMD's SHACL shapes.
