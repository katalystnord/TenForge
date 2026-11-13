cmake_minimum_required(VERSION 3.21)
project(TenForge VERSION 0.2.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)  # for editors (clangd); not used by the build

set(CMAKE_AUTOMOC ON)

# Address and undefined-behaviour sanitizers, for the class of defect no
# assertion can reach (an access one past the end changes nothing a test can
# see). Off by default and built in its own directory by tools/sanitize.sh,
# because the instrumentation changes the object code.
option(TF_SANITIZE "Build with address and undefined-behaviour sanitizers" OFF)
if(TF_SANITIZE)
    add_compile_options(-fsanitize=address,undefined -fno-sanitize-recover=all
                        -fno-omit-frame-pointer -g)
    add_link_options(-fsanitize=address,undefined)
endif()

find_package(Eigen3 3.4 REQUIRED NO_MODULE)
# Svg: drawing the exported chart into PDF reports as vectors, and into PNG.
find_package(Qt6 REQUIRED COMPONENTS Core Widgets OpenGLWidgets Svg Test)

# Ubuntu's VTK re-exports system libraries through its imported targets but
# leaves the consumer to locate them; found up front so VTK's own config
# succeeds (learned in SurView, same distro packages). MPI's C target only
# exists when the project enables C, hence LANGUAGES C CXX above: without it
# VTK::mpi names MPI::MPI_C and generation fails.
find_package(MPI QUIET)
find_package(jsoncpp QUIET)

find_package(VTK REQUIRED COMPONENTS
    CommonCore
    CommonDataModel
    RenderingCore
    RenderingOpenGL2          # OpenGL backend and object-factory overrides
    InteractionStyle
    GUISupportQt              # QVTKOpenGLNativeWidget: VTK inside a Qt widget
    # Charts are VTK's own, never Qt Charts: Qt Charts is GPL-3.0-only and
    # would force TenForge's licence to GPL.
    ChartsCore                # vtkChartXY
    ViewsContext2D            # vtkContextView
    RenderingContextOpenGL2   # the 2D context's OpenGL backend
    IOExport                  # vtkSVGExporter: the chart as a vector figure
)

enable_testing()

add_subdirectory(src)
add_subdirectory(tests)
