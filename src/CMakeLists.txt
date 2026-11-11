# --- tf_analysis: the standards engine -------------------------------------
# Pure functions over arrays, Eigen only, NO Qt and no VTK (CLAUDE.md,
# settled decisions). It is the part every other target and every future
# binding stands on, and it must be usable without a GUI toolkit.
add_library(tf_analysis STATIC
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
)
target_include_directories(tf_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(tf_core PUBLIC tf_analysis Qt6::Core)
target_compile_definitions(tf_core PRIVATE TF_VERSION="${PROJECT_VERSION}")

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
