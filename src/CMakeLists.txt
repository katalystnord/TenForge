# --- tf_analysis: the standards engine -------------------------------------
# Pure functions over arrays, Eigen only, NO Qt and no VTK (CLAUDE.md,
# settled decisions). It is the part every other target and every future
# binding stands on, and it must be usable without a GUI toolkit.
add_library(tf_analysis STATIC
    analysis/Primitives.cpp
    analysis/Tension.cpp
    analysis/Version.cpp
)
target_include_directories(tf_analysis PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(tf_analysis PUBLIC Eigen3::Eigen)
target_compile_definitions(tf_analysis PRIVATE TF_VERSION="${PROJECT_VERSION}")

# Configure-time guard: refuse a Qt or VTK dependency on the engine outright.
# The test `analysis_headers_compile_without_qt` covers the other route, a
# header that includes Qt without the link line saying so.
get_target_property(tf_analysis_links tf_analysis LINK_LIBRARIES)
foreach(lib IN LISTS tf_analysis_links)
    if(lib MATCHES "^(Qt|VTK)")
        message(FATAL_ERROR
            "tf_analysis must not depend on Qt or VTK (found ${lib}). "
            "See CLAUDE.md, settled decisions.")
    endif()
endforeach()

# --- tf_core: record, profiles, calibration, sequencer ----------------------
add_library(tf_core STATIC
    core/ApplicationInfo.cpp
    core/Import.cpp
    core/Profile.cpp
    core/Results.cpp
)
target_include_directories(tf_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(tf_core PUBLIC tf_analysis Qt6::Core)

# The build identity written into every results file. Taken at configure
# time (tools/run-tests.sh and the hook configure on every run).
execute_process(COMMAND git describe --always --dirty --abbrev=12
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                OUTPUT_VARIABLE TF_BUILD OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET)
if(NOT TF_BUILD)
    set(TF_BUILD "unknown")
endif()
target_compile_definitions(tf_core PRIVATE TF_VERSION="${PROJECT_VERSION}"
                                           TF_BUILD="${TF_BUILD}")

# The profiles shipped with TenForge, compiled in so the CLI and the GUI
# find them without an install step: loadProfile("iso-6892-1-2019").
file(GLOB TF_SHIPPED_PROFILES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/profiles/*.json)
qt_add_resources(tf_core "shipped_profiles"
    PREFIX "/profiles"
    BASE ${CMAKE_SOURCE_DIR}/profiles
    FILES ${TF_SHIPPED_PROFILES})

# --- tf_gui: the application window -----------------------------------------
add_library(tf_gui STATIC
    gui/MainWindow.cpp
)
target_include_directories(tf_gui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(tf_gui PUBLIC tf_core Qt6::Widgets Qt6::OpenGLWidgets
    ${VTK_LIBRARIES})
vtk_module_autoinit(TARGETS tf_gui MODULES ${VTK_LIBRARIES})

add_executable(TenForge gui/main.cpp)
target_link_libraries(TenForge PRIVATE tf_gui)
vtk_module_autoinit(TARGETS TenForge MODULES ${VTK_LIBRARIES})

# --- tf-analyse: the headless command line ----------------------------------
add_executable(tf-analyse cli/main.cpp)
target_link_libraries(tf-analyse PRIVATE tf_core)
