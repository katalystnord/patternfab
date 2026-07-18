cmake_minimum_required(VERSION 3.21)
project(PatternFab VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Svg)
find_package(VTK REQUIRED COMPONENTS CommonCore CommonDataModel RenderingCore GUISupportQt)

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
add_subdirectory(gui)
