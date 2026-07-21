cmake_minimum_required(VERSION 3.21)
project(PatternFab VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Gui Widgets Svg)
# GUISupportQt deliberately excluded for now: Ubuntu's packaged VTK 9.1
# GUISupportQt module is linked against Qt5, not Qt6 (verified via ldd),
# and Phase 0 doesn't need the VTK-in-Qt-window embedding yet. Revisit when
# Phase 5 (GUI) actually needs it — either drop to Qt5 or build VTK from
# source with Qt6 support.
find_package(VTK REQUIRED COMPONENTS
    CommonCore CommonDataModel RenderingCore
    IOImage ImagingMorphological
    FiltersModeling IOGeometry
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
