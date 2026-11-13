# The FreeCAD and CalculiX exit check (ROADMAP Phase 4), as one CTest:
# tf-analyse writes the exports, FreeCAD (flatpak org.freecad.FreeCAD)
# loads the card and solves its own nonlinear example with the yield
# points, and the result must say ok.
#
#   cmake -DTF_ANALYSE=... -DFIXTURE=... -DSCRIPT=... -DWORK=... -P run_check.cmake
#
# FREECAD_USER_HOME points FreeCAD's settings and user library at a
# throwaway folder, so the user's own FreeCAD is never touched.
# OPENBLAS_CORETYPE=Haswell: the flatpak's OpenBLAS picks a kernel that
# a Ryzen 9 5900X (AVX2, no AVX-512) cannot run, and FEM dies with an
# illegal instruction (seen 2026-11-13 in libopenblas.so.0.3); Haswell's
# kernel needs only AVX2.

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/home")
execute_process(
    COMMAND "${TF_ANALYSE}" --profile iso-6892-1-2019 --area "78.54 mm2" --gauge-length "50 mm"
            --freecad "${WORK}/ro" "${FIXTURE}"
    RESULT_VARIABLE rc OUTPUT_QUIET)
if(NOT rc EQUAL 0)
    message(FATAL_ERROR "tf-analyse could not write the exports")
endif()
execute_process(
    COMMAND flatpak run --command=FreeCADCmd
            "--filesystem=${WORK}" "--filesystem=${SCRIPT}"
            --env=OPENBLAS_CORETYPE=Haswell
            "--env=FREECAD_USER_HOME=${WORK}/home" "--env=FREECAD_USER_DATA=${WORK}/home"
            "--env=FREECAD_USER_TEMP=${WORK}/home/tmp"
            "--env=TF_CARD=${WORK}/ro.FCMat" "--env=TF_YIELD=${WORK}/ro-yieldpoints.txt"
            "--env=TF_OUT=${WORK}/result.json"
            org.freecad.FreeCAD "${SCRIPT}"
    RESULT_VARIABLE rc OUTPUT_QUIET ERROR_QUIET)
if(NOT EXISTS "${WORK}/result.json")
    message(FATAL_ERROR "FreeCAD wrote no result (exit ${rc})")
endif()
file(READ "${WORK}/result.json" result)
message(STATUS "${result}")
string(JSON ok GET "${result}" ok)
if(NOT ok)
    message(FATAL_ERROR "FreeCAD did not accept TenForge's exports")
endif()
