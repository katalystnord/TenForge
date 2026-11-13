# The ParaView exit check (ROADMAP Phase 4) as one CTest: tf-analyse writes
# a record, a copy has its curve changed, and ParaView (flatpak
# org.paraview.ParaView) must open the record with TenForge's reader plugin
# and refuse the copy.
#
#   cmake -DTF_ANALYSE=... -DFIXTURE=... -DSCRIPT=... -DPLUGIN=... -DWORK=... -P run_check.cmake
#
# Headless: on a private X server (xvfb-run), Wayland withheld from the
# sandbox, so no window reaches the desktop.

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
execute_process(
    COMMAND "${TF_ANALYSE}" --profile iso-6892-1-2019 --area "120.64 mm2" --gauge-length "50 mm"
            --record "${WORK}/rec" "${FIXTURE}"
    RESULT_VARIABLE rc OUTPUT_QUIET)
if(NOT rc EQUAL 0)
    message(FATAL_ERROR "tf-analyse could not write the record")
endif()
file(COPY "${WORK}/rec/" DESTINATION "${WORK}/bad")
file(READ "${WORK}/bad/results/curve.vtp" vtp)
string(REPLACE "stress_Pa" "stress_Px" vtp "${vtp}")
file(WRITE "${WORK}/bad/results/curve.vtp" "${vtp}")

execute_process(
    COMMAND env -u WAYLAND_DISPLAY xvfb-run -a -s "-screen 0 1280x1024x24"
            flatpak run --nosocket=wayland --socket=x11 --env=LIBGL_ALWAYS_SOFTWARE=1
            "--filesystem=${WORK}" "--filesystem=${SCRIPT}" "--filesystem=${PLUGIN}"
            --command=pvpython org.paraview.ParaView
            "${SCRIPT}" "${PLUGIN}" "${WORK}/rec" "${WORK}/bad" "${WORK}/result.json"
    RESULT_VARIABLE rc OUTPUT_QUIET ERROR_QUIET)
if(NOT EXISTS "${WORK}/result.json")
    message(FATAL_ERROR "ParaView wrote no result (exit ${rc})")
endif()
file(READ "${WORK}/result.json" result)
message(STATUS "${result}")
string(JSON ok GET "${result}" ok)
if(NOT ok)
    message(FATAL_ERROR "ParaView did not read the record as it should")
endif()
