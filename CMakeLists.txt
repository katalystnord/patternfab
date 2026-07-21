cmake_minimum_required(VERSION 3.21)
project(PatternFab VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# OpenGLWidgets: QVTKOpenGLNativeWidget derives from QOpenGLWidget, which lives
# in this Qt6 module.
find_package(Qt6 REQUIRED COMPONENTS Gui Widgets Svg OpenGLWidgets)
# GUISupportQt + RenderingOpenGL2 drive the in-window VTK 3D relief preview
# (Phase 5.2). This was excluded through Phase 0-5.1 because Ubuntu's VTK 9.1
# built GUISupportQt against Qt5; on 26.04's VTK 9.5 it is built against Qt6,
# so the embedding path is now available.
find_package(VTK REQUIRED COMPONENTS
    CommonCore CommonDataModel RenderingCore RenderingOpenGL2
    IOImage ImagingMorphological
    FiltersModeling IOGeometry
    GUISupportQt
)

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

add_subdirectory(core)
add_subdirectory(cli)
add_subdirectory(gui)
