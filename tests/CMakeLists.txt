# One executable per subject, each registered with CTest. Test names are for
# the CASE, not the function: a named red test is evidence, a design note is
# not.
#
# NEGATIVE CHECKS, 2026-11-11 (each property broken on purpose, test seen red,
# then restored):
#   - a Qt include added to analysis/Version.h: red, but caught FIRST by
#     tf_analysis's own compile (its sources include the header and it has no
#     Qt path), so this test is the second guard for that header, and the
#     first only for a future header no tf_analysis source includes;
#   - Qt6::Core added to tf_analysis's link line: configure refuses
#     (src/CMakeLists.txt guard);
#   - empty engine version, wrong CLI version: each turned its own test red;
#   - window title without the subtitle, empty state without "release 0.1",
#     chart widget hidden: each turned main_window_walkthrough red.
#
# NEGATIVE CHECKS, 2026-11-12, Phase 1:
#   - engine and importer tests were written first and seen red against
#     stubs (12 of 14 primitive cases, all tension and import cases);
#   - profiles_and_results_documents was written after its code, so each
#     property was broken instead: primitive check removed, rounding
#     truncating, every parameter treated as confirmed, a missing result
#     written as 0, a user profile left unlabelled, the profile's modulus
#     window ignored. Each turned at least one case red.
#   - the CLI refusals are each checked twice (message, and WILL_FAIL for
#     the exit code), since a crash also exits non-zero.
#   NOT exercised: the chart's render-size assertion was not separately
#   broken (no clean way to deny the widget a GL context in the test); it
#   rests on VTK reporting zero size without one.

# --- The engine stands alone ------------------------------------------------
# Every public header of tf_analysis, included into one translation unit and
# compiled with ONLY tf_analysis's own include path and link line: Eigen, no
# Qt, no VTK. A header that reaches for Qt fails to compile here, which the
# configure-time guard in src/CMakeLists.txt cannot see.
file(GLOB TF_ANALYSIS_HEADERS CONFIGURE_DEPENDS
     ${CMAKE_SOURCE_DIR}/src/analysis/*.h)
set(tf_headers_tu "${CMAKE_CURRENT_BINARY_DIR}/analysis_headers_tu.cpp")
set(tf_headers_body "// Generated: every tf_analysis header, nothing else.\n")
foreach(h IN LISTS TF_ANALYSIS_HEADERS)
    string(APPEND tf_headers_body "#include \"${h}\"\n")
endforeach()
string(APPEND tf_headers_body
    "#include <cstdio>\n"
    "int main() { const char *v = tf::analysis::version();\n"
    "  if (!v || !*v) { std::puts(\"empty engine version\"); return 1; }\n"
    "  std::printf(\"tf_analysis %s, built with no Qt\\n\", v); return 0; }\n")
file(GENERATE OUTPUT ${tf_headers_tu} CONTENT "${tf_headers_body}")
add_executable(test_analysis_stands_alone ${tf_headers_tu})
target_link_libraries(test_analysis_stands_alone PRIVATE tf_analysis)
add_test(NAME analysis_headers_compile_without_qt
         COMMAND test_analysis_stands_alone)

# --- The engine's primitives and the tension model ---------------------------
# Built with tf_analysis's own dependencies only (tests/check.h, no Qt), so
# the engine's tests prove it stands alone too.
add_executable(test_primitives test_primitives.cpp)
target_link_libraries(test_primitives PRIVATE tf_analysis)
add_test(NAME engine_primitives COMMAND test_primitives)

add_executable(test_tension_synthetic test_tension_synthetic.cpp)
target_link_libraries(test_tension_synthetic PRIVATE tf_analysis)
target_compile_definitions(test_tension_synthetic PRIVATE
    TF_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
add_test(NAME tension_on_synthetic_curves COMMAND test_tension_synthetic)

# --- Reading machine exports (tf_core) ---------------------------------------
add_executable(test_import test_import.cpp)
target_link_libraries(test_import PRIVATE tf_core)
target_compile_definitions(test_import PRIVATE
    TF_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
add_test(NAME import_machine_exports COMMAND test_import)

# --- Profiles in, results documents out (tf_core) ----------------------------
add_executable(test_profiles_results test_profiles_results.cpp)
target_link_libraries(test_profiles_results PRIVATE tf_core)
target_compile_definitions(test_profiles_results PRIVATE
    TF_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
add_test(NAME profiles_and_results_documents COMMAND test_profiles_results)

# --- The command line says what it is and what it cannot do yet -------------
add_test(NAME cli_reports_its_version
         COMMAND tf-analyse --version)
set_tests_properties(cli_reports_its_version PROPERTIES
    PASS_REGULAR_EXPRESSION "^tf-analyse ${PROJECT_VERSION}")

# The analysis from the command line, on a committed synthetic curve with a
# known answer (Rp0.2 = 300 MPa exactly, Rm = 500 MPa).
set(TF_SYN ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/synthetic)
add_test(NAME cli_analyses_a_file_with_a_shipped_profile
         COMMAND tf-analyse --profile iso-6892-1-2019 --area "78.54 mm2"
                 --gauge-length "50 mm" ${TF_SYN}/ro-continuous.csv)
set_tests_properties(cli_analyses_a_file_with_a_shipped_profile PROPERTIES
    PASS_REGULAR_EXPRESSION "Rp0\\.2 +300 MPa .*Rm +500 MPa")

# Conformity is said on screen, not only in the file (tenet 9).
add_test(NAME cli_says_the_results_are_aligned_not_to_standard
         COMMAND tf-analyse --profile iso-6892-1-2019 --area "78.54 mm2"
                 --gauge-length "50 mm" ${TF_SYN}/ro-continuous.csv)
set_tests_properties(cli_says_the_results_are_aligned_not_to_standard PROPERTIES
    PASS_REGULAR_EXPRESSION "Conformity: ALIGNED.*force calibration")

add_test(NAME cli_writes_results_json_to_standard_output
         COMMAND tf-analyse --profile iso-527-1-2019 --area "40 mm2"
                 --gauge-length "75 mm" --json - ${TF_SYN}/polymer.csv)
set_tests_properties(cli_writes_results_json_to_standard_output PROPERTIES
    PASS_REGULAR_EXPRESSION "\"format\": \"tenforge-results\".*\"id\": \"Et\"")

# PASS_REGULAR_EXPRESSION ignores the exit code, so each refusal is checked
# twice: once for what it says, once (WILL_FAIL) that it exits non-zero.
add_test(NAME cli_without_a_profile_says_which_exist
         COMMAND tf-analyse ${TF_SYN}/polymer.csv)
set_tests_properties(cli_without_a_profile_says_which_exist PROPERTIES
    PASS_REGULAR_EXPRESSION "--profile.*iso-527-1-2019")
add_test(NAME cli_without_a_profile_exits_non_zero
         COMMAND tf-analyse ${TF_SYN}/polymer.csv)
set_tests_properties(cli_without_a_profile_exits_non_zero PROPERTIES WILL_FAIL TRUE)

add_test(NAME cli_without_an_area_names_what_is_missing
         COMMAND tf-analyse --profile iso-527-1-2019 ${TF_SYN}/polymer.csv)
set_tests_properties(cli_without_an_area_names_what_is_missing PROPERTIES
    PASS_REGULAR_EXPRESSION "cross-section area S0 is not in the file")
add_test(NAME cli_without_an_area_exits_non_zero
         COMMAND tf-analyse --profile iso-527-1-2019 ${TF_SYN}/polymer.csv)
set_tests_properties(cli_without_an_area_exits_non_zero PROPERTIES WILL_FAIL TRUE)

# --- Walkthrough: the real window -------------------------------------------
add_executable(test_main_window_walkthrough test_main_window_walkthrough.cpp)
target_link_libraries(test_main_window_walkthrough PRIVATE tf_gui Qt6::Test)
vtk_module_autoinit(TARGETS test_main_window_walkthrough MODULES ${VTK_LIBRARIES})

# HEADLESS BY CONSTRUCTION, not by how the suite is invoked (SurView's
# lesson): the test shows a real window, so it carries its own private X
# server. It cannot run offscreen: QT_QPA_PLATFORM=offscreen crashes VTK's
# OpenGL widget. -a picks a free display; 24-bit because xvfb-run's default
# depth is too shallow for the GL context.
option(TF_TESTS_HEADLESS
       "Run the walkthrough tests on a private X server instead of your desktop" ON)
find_program(TF_XVFB_RUN xvfb-run)
if(TF_TESTS_HEADLESS AND TF_XVFB_RUN)
    add_test(NAME main_window_walkthrough
             COMMAND ${TF_XVFB_RUN} -a -s "-screen 0 1600x1000x24"
                     $<TARGET_FILE:test_main_window_walkthrough>)
else()
    if(TF_TESTS_HEADLESS)
        message(WARNING "xvfb-run not found: the walkthrough tests will open "
                        "windows on the current display. Install xvfb.")
    endif()
    add_test(NAME main_window_walkthrough COMMAND test_main_window_walkthrough)
endif()
# Software GL, so the result does not depend on this machine's graphics card
# (and never touches it); WAYLAND_DISPLAY cleared so Qt uses the X server
# above rather than reaching past it to a compositor.
set_tests_properties(main_window_walkthrough PROPERTIES
    ENVIRONMENT "LIBGL_ALWAYS_SOFTWARE=1;QT_QPA_PLATFORM=xcb;WAYLAND_DISPLAY=")
