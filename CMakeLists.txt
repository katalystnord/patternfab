cmake_minimum_required(VERSION 3.21)
project(PatternFab VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# The GUI is optional, and that is not a preference -- it is the only way to
# build at all on a distribution whose VTK links GUISupportQt against Qt5.
# patternfab-core and patternfab-cli need no Qt widgets and no VTK-Qt bridge,
# so they stay buildable everywhere; only the window needs the two to agree.
option(PATTERNFAB_BUILD_GUI "Build the patternfab-gui application" ON)

# Gui (QPainter) and Svg (QSvgGenerator) are what core itself needs. Widgets
# and OpenGLWidgets are the window's alone -- OpenGLWidgets because
# QVTKOpenGLNativeWidget derives from QOpenGLWidget.
find_package(Qt6 REQUIRED COMPONENTS Gui Svg)
if(PATTERNFAB_BUILD_GUI)
    find_package(Qt6 REQUIRED COMPONENTS Widgets OpenGLWidgets)
endif()

set(PATTERNFAB_VTK_COMPONENTS
    CommonCore CommonDataModel RenderingCore RenderingOpenGL2
    IOImage ImagingMorphological
    FiltersModeling IOGeometry
)
# GUISupportQt + RenderingOpenGL2 drive the in-window VTK 3D relief preview
# (Phase 5.2). This was excluded through Phase 0-5.1 because Ubuntu's VTK 9.1
# built GUISupportQt against Qt5; on 26.04's VTK 9.5 it is built against Qt6,
# so the embedding path is now available.
if(PATTERNFAB_BUILD_GUI)
    list(APPEND PATTERNFAB_VTK_COMPONENTS GUISupportQt)
endif()

find_package(VTK REQUIRED COMPONENTS ${PATTERNFAB_VTK_COMPONENTS})

# Say which Qt this VTK was built against, in one sentence, BEFORE CMake says
# it in ten. VTK::GUISupportQt carries INTERFACE_QT_MAJOR_VERSION as a
# compatibility-required property, so a Qt5-built one against our Qt6::Gui
# fails at GENERATE time -- after "Configuring done", with nine repetitions of
# "The INTERFACE_QT_MAJOR_VERSION property of Qt6::Gui does not match" and no
# mention of VTK, Qt5, or what to do about it. That is what CI reported for
# every run on ubuntu-latest (24.04, VTK 9.1).
if(PATTERNFAB_BUILD_GUI)
    get_target_property(_pf_vtk_qt_major VTK::GUISupportQt INTERFACE_QT_MAJOR_VERSION)
    if(_pf_vtk_qt_major AND NOT _pf_vtk_qt_major STREQUAL "6")
        message(FATAL_ERROR
            "This VTK's GUISupportQt is built against Qt${_pf_vtk_qt_major}, "
            "but PatternFab is a Qt6 application, and one process cannot hold "
            "both. Ubuntu 24.04 and older ship such a VTK; 26.04's VTK 9.5 is "
            "built against Qt6.\n"
            "Build without the window instead: "
            "cmake -B build -S . -DPATTERNFAB_BUILD_GUI=OFF\n"
            "patternfab-core, patternfab-cli and the whole test suite build "
            "and run fine that way.")
    endif()
endif()

find_package(nlohmann_json REQUIRED)

find_library(DIME_LIBRARY NAMES dime)
find_path(DIME_INCLUDE_DIR NAMES dime/Base.h)
if(NOT DIME_LIBRARY OR NOT DIME_INCLUDE_DIR)
    message(FATAL_ERROR "dime library not found - install libdime-dev")
endif()
add_library(dime::dime UNKNOWN IMPORTED)
set_target_properties(dime::dime PROPERTIES
    IMPORTED_LOCATION "${DIME_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${DIME_INCLUDE_DIR}"
)

# Testing is enabled at the TOP level deliberately. Called from core/ instead,
# as it was, CTest writes its test list only under build/core, and `ctest` run
# from the build root -- which is where everyone runs it -- answers "No tests
# were found" while eight passing test executables sit one directory down.
enable_testing()

add_subdirectory(core)
add_subdirectory(cli)
if(PATTERNFAB_BUILD_GUI)
    add_subdirectory(gui)
endif()
