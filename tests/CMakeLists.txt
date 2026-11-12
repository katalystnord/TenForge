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
#   - empty engine version, wrong CLI version, changed refusal text, refusal
#     exiting 0: each turned its own test red;
#   - window title without the subtitle, empty state without "release 0.1",
#     chart widget hidden: each turned main_window_walkthrough red.
#   NOT caught by `cli_refusing_a_file_exits_non_zero` alone: a crash also
#   exits non-zero and so also "passes" WILL_FAIL. Its sibling
#   `cli_refuses_a_file_and_says_when_it_will_not` covers that, since a crash
#   prints no refusal.
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

# --- The command line says what it is and what it cannot do yet -------------
add_test(NAME cli_reports_its_version
         COMMAND tf-analyse --version)
set_tests_properties(cli_reports_its_version PROPERTIES
    PASS_REGULAR_EXPRESSION "^tf-analyse ${PROJECT_VERSION}")

# PASS_REGULAR_EXPRESSION ignores the exit code, so the refusal's non-zero
# exit is checked separately below rather than assumed.
add_test(NAME cli_refuses_a_file_and_says_when_it_will_not
         COMMAND tf-analyse some-test.csv)
set_tests_properties(cli_refuses_a_file_and_says_when_it_will_not PROPERTIES
    PASS_REGULAR_EXPRESSION "arrives in release 0\\.1\\. Nothing was read")
add_test(NAME cli_refusing_a_file_exits_non_zero
         COMMAND tf-analyse some-test.csv)
set_tests_properties(cli_refusing_a_file_exits_non_zero PROPERTIES
    WILL_FAIL TRUE)

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
