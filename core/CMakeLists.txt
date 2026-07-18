add_library(patternfab-core
    src/Core.cpp
    include/patternfab/Core.h
)

target_include_directories(patternfab-core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(patternfab-core PUBLIC
    ${VTK_LIBRARIES}
    dime::dime
)

vtk_module_autoinit(
    TARGETS patternfab-core
    MODULES ${VTK_LIBRARIES}
)
