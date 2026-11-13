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
#   - TENSTAND: the profile's yield rule loosened (max_plastic 0.005 under
#     the first rule) turned 6 comparisons red (false yield points on 38, 46,
#     63); the modulus window widened to 0.10..0.80 turned 5 red (Rp on 17,
#     22, 42).
#   - BAM S355 was red before it was green: under the first yield rule
#     (max_plastic 0.001, tuned on TENSTAND alone) no specimen's ReH was
#     found. The rule was re-measured on both datasets (Tension.h).
#   - the CLI refusals are each checked twice (message, and WILL_FAIL for
#     the exit code), since a crash also exits non-zero.
#   - Phase 2: specimen_geometry_and_json, test_record_written_and_opened
#     and analysis_session_guides_and_analyses were written first and seen
#     red against stubs (8 of 9, abort, 12 of 12). Then one real defect
#     caught: a notice capitalised its leading path ("Results/...").
#   - report_carries_results_conformity_and_provenance: written first, red
#     against a stub (5 of 6; the no-dashes case passes on an empty page).
#   - main_window_walkthrough, Phase 2: first run red on its own (no
#     procedure named "ISO 6892-1" was offered: the list showed only
#     titles, a real UX defect, fixed). Then broken on purpose, each red:
#     chart left out of the HTML report; gauge length edits not read;
#     conformity not stated; the unlisted-files button never shown.
#   - gui_reports_its_version_without_a_display: the build id dropped from
#     the output turned it red.
#   NOT exercised: the chart's render-size assertion was not separately
#   broken (no clean way to deny the widget a GL context in the test); it
#   rests on VTK reporting zero size without one.
#
# NEGATIVE CHECKS, 2026-11-13, Phase 3:
#   - compression_and_bending_models: red against stubs (abort on the
#     first missing factor). One fixture defect then found: the foam's
#     maximum was too flat to resolve at the file's 9 decimals (fixed in
#     make_modes.py, which says why).
#   - import_machine_exports, Phase 3 cases: red against the old importer
#     (abort: no spaced preamble, no tiered displacement names).
#   - profiles_and_results_documents, Phase 3 cases: written before the
#     loader but not run red alone (the new fields did not compile yet), so
#     broken instead: deflection_per_thickness dropped, red here and in
#     the session's ISO 178 case; unknown validity quantities accepted, red.
#   - analysis_session_guides_and_analyses: the compliance case was first
#     red for a test-setup reason (table too stiff to move Ef by 1 %, and
#     too short for the four-point beam); the code was right.
#   - braunschweig_14125_agrees_with_testxpert: deflection read at
#     mid-span, 138 of 161 red; modulus window to 0.30 %, 67 red; an
#     exclusion whose reason no longer held, refused.
#   - main_window_walkthrough, Phase 3: the "Deflection unit" label left
#     as "Extension unit", red; the loading span not read, red. The
#     panel-width check was red on first run (389 px needed, 378 shown,
#     and 415 in bending): a real defect, fixed.
#   - chart_regions_labels_notes_and_modulus_window: red against stubs
#     (10 of 10) before the code. Then the sanitizer build found a use after
#     free in three of its own cases (a pointer into a temporary vector),
#     fixed in the test.
#   - main_window_walkthrough, figures: green on first run, so broken on
#     purpose: the dragged edge not reported (red); SVG text as outlines
#     (red); the report's chart rasterised (red: an image in the PDF);
#     the SVG post-processing removed (red: clipped transformed group).
#     Looking at the exported SVG found what no assertion had: VTK 9.5's
#     exporter clips the plots away in spec-following renderers, stretches
#     strokes and markers, names a different font, writes text anchors SVG
#     lacks and angles of the wrong sign. Each is rewritten in
#     ChartExport.cpp and guarded by an assertion on the SVG.

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

add_executable(test_statistics test_statistics.cpp)
target_link_libraries(test_statistics PRIVATE tf_analysis)
add_test(NAME engine_statistics_and_energy COMMAND test_statistics)

add_executable(test_tension_synthetic test_tension_synthetic.cpp)
target_link_libraries(test_tension_synthetic PRIVATE tf_analysis)
target_compile_definitions(test_tension_synthetic PRIVATE
    TF_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
add_test(NAME tension_on_synthetic_curves COMMAND test_tension_synthetic)

add_executable(test_curve test_curve.cpp)
target_link_libraries(test_curve PRIVATE tf_analysis)
target_compile_definitions(test_curve PRIVATE
    TF_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
add_test(NAME compression_and_bending_models COMMAND test_curve)

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

# --- The specimen and the test record (tf_core) ------------------------------
add_executable(test_specimen test_specimen.cpp)
target_link_libraries(test_specimen PRIVATE tf_core)
add_test(NAME specimen_geometry_and_json COMMAND test_specimen)

add_executable(test_record test_record.cpp)
target_link_libraries(test_record PRIVATE tf_core)
target_compile_definitions(test_record PRIVATE
    TF_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
add_test(NAME test_record_written_and_opened COMMAND test_record)

# --- What the chart shows besides the curve (tf_core) -----------------------
add_executable(test_annotations test_annotations.cpp)
target_link_libraries(test_annotations PRIVATE tf_core)
target_compile_definitions(test_annotations PRIVATE
    TF_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
add_test(NAME chart_regions_labels_notes_and_modulus_window COMMAND test_annotations)

# --- The analysis session behind the window (tf_core) -----------------------
add_executable(test_session test_session.cpp)
target_link_libraries(test_session PRIVATE tf_core)
target_compile_definitions(test_session PRIVATE
    TF_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
add_test(NAME analysis_session_guides_and_analyses COMMAND test_session)

add_executable(test_report test_report.cpp)
target_link_libraries(test_report PRIVATE tf_core)
target_compile_definitions(test_report PRIVATE
    TF_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
add_test(NAME report_carries_results_conformity_and_provenance COMMAND test_report)

# --- Validation: NPL TENSTAND (Phase 1 exit criterion) -----------------------
# The 15 Premium Quality files carry no licence statement, so they are not
# committed: a setup test fetches them into the build tree (or the cache
# directory below), pinned by SHA-256. The agreed values are facts from
# NPL's public report and are committed (tests/validation/).
option(TF_VALIDATION_DOWNLOADS "Fetch validation datasets that cannot be committed" ON)
set(TF_VALIDATION_CACHE "${CMAKE_BINARY_DIR}/validation-data" CACHE PATH
    "Where fetched validation datasets are kept between builds")
add_executable(validate_tenstand validate_tenstand.cpp)
target_link_libraries(validate_tenstand PRIVATE tf_core)
if(TF_VALIDATION_DOWNLOADS)
    file(READ ${CMAKE_CURRENT_SOURCE_DIR}/validation/tenstand-agreed.json tf_tenstand_json)
    string(JSON tf_tenstand_url GET "${tf_tenstand_json}" data url)
    string(JSON tf_tenstand_sha GET "${tf_tenstand_json}" data sha256)
    add_test(NAME fetch_tenstand
             COMMAND ${CMAKE_COMMAND} -DURL=${tf_tenstand_url} -DSHA256=${tf_tenstand_sha}
                     -DDEST=${TF_VALIDATION_CACHE}/tenstand
                     -P ${CMAKE_CURRENT_SOURCE_DIR}/validation/fetch.cmake)
    set_tests_properties(fetch_tenstand PROPERTIES FIXTURES_SETUP tenstand)
    add_test(NAME tenstand_within_en10002_annex_a5
             COMMAND validate_tenstand
                     ${CMAKE_CURRENT_SOURCE_DIR}/validation/tenstand-agreed.json
                     ${TF_VALIDATION_CACHE}/tenstand/data
                     ${CMAKE_BINARY_DIR}/tenstand-report.md)
    set_tests_properties(tenstand_within_en10002_annex_a5 PROPERTIES FIXTURES_REQUIRED tenstand)
else()
    message(STATUS "TF_VALIDATION_DOWNLOADS is OFF: the TENSTAND validation is not run")
endif()

# --- Validation: TU Braunschweig ISO 14125 (Phase 3 exit criterion) ---------
# CC BY 4.0, so it could be committed, but the 25 exports are 7.3 MB: they
# are fetched like TENSTAND (the archive pinned by SHA-256), and one file is
# committed as the importer's fixture for testXpert's dialect. testXpert's
# own results for each specimen are committed as transcribed facts.
add_executable(validate_braunschweig_14125 validate_braunschweig_14125.cpp)
target_link_libraries(validate_braunschweig_14125 PRIVATE tf_core)
if(TF_VALIDATION_DOWNLOADS)
    file(READ ${CMAKE_CURRENT_SOURCE_DIR}/validation/braunschweig-14125-agreed.json tf_bs_json)
    string(JSON tf_bs_url GET "${tf_bs_json}" data url)
    string(JSON tf_bs_sha GET "${tf_bs_json}" data sha256)
    add_test(NAME fetch_braunschweig_14125
             COMMAND ${CMAKE_COMMAND} -DURL=${tf_bs_url} -DSHA256=${tf_bs_sha}
                     -DDEST=${TF_VALIDATION_CACHE}/braunschweig-14125
                     -P ${CMAKE_CURRENT_SOURCE_DIR}/validation/fetch.cmake)
    set_tests_properties(fetch_braunschweig_14125 PROPERTIES FIXTURES_SETUP braunschweig)
    add_test(NAME braunschweig_14125_agrees_with_testxpert
             COMMAND validate_braunschweig_14125
                     ${CMAKE_CURRENT_SOURCE_DIR}/validation/braunschweig-14125-agreed.json
                     ${TF_VALIDATION_CACHE}/braunschweig-14125/data
                     ${CMAKE_BINARY_DIR}/braunschweig-14125-report.md)
    set_tests_properties(braunschweig_14125_agrees_with_testxpert PROPERTIES
        FIXTURES_REQUIRED braunschweig)
endif()

# --- Validation: BAM S355 (committed, CC BY 4.0) -----------------------------
add_executable(validate_bam_s355 validate_bam_s355.cpp)
target_link_libraries(validate_bam_s355 PRIVATE tf_core)
target_compile_definitions(validate_bam_s355 PRIVATE
    TF_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
add_test(NAME bam_s355_matches_reported_results COMMAND validate_bam_s355)

# --- The command line says what it is and what it cannot do yet -------------
add_test(NAME cli_reports_its_version
         COMMAND tf-analyse --version)
set_tests_properties(cli_reports_its_version PROPERTIES
    PASS_REGULAR_EXPRESSION "^tf-analyse ${PROJECT_VERSION}")

# The window's program answers --version with no display (packaging needs it).
add_test(NAME gui_reports_its_version_without_a_display COMMAND TenForge --version)
set_tests_properties(gui_reports_its_version_without_a_display PROPERTIES
    PASS_REGULAR_EXPRESSION "^TenForge ${PROJECT_VERSION} \\(build "
    ENVIRONMENT "DISPLAY=;WAYLAND_DISPLAY=;QT_QPA_PLATFORM=")

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
    PASS_REGULAR_EXPRESSION "give --area, or --width and --thickness")
add_test(NAME cli_without_an_area_exits_non_zero
         COMMAND tf-analyse --profile iso-527-1-2019 ${TF_SYN}/polymer.csv)
set_tests_properties(cli_without_an_area_exits_non_zero PROPERTIES WILL_FAIL TRUE)

# --- Walkthrough: the real window -------------------------------------------
add_executable(test_main_window_walkthrough test_main_window_walkthrough.cpp)
target_link_libraries(test_main_window_walkthrough PRIVATE tf_gui Qt6::Test)
target_compile_definitions(test_main_window_walkthrough PRIVATE
    TF_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
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
# A file dialog nobody answers would wait for ever: the timeout ends it.
set_tests_properties(main_window_walkthrough PROPERTIES TIMEOUT 120
    ENVIRONMENT "LIBGL_ALWAYS_SOFTWARE=1;QT_QPA_PLATFORM=xcb;WAYLAND_DISPLAY=;TF_WALKTHROUGH_XVFB=1")
# The test binary also puts itself under xvfb-run when started any other
# way (ensureHeadless() in the test), so a direct run never reaches the
# desktop either.
