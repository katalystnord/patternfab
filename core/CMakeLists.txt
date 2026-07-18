add_library(patternfab-core
    src/Core.cpp
    include/patternfab/Core.h
    src/VectorInput.cpp
    include/patternfab/VectorInput.h
    src/RasterInput.cpp
    include/patternfab/RasterInput.h
    include/patternfab/Pattern.h
    src/ConstraintEngine.cpp
    include/patternfab/ConstraintEngine.h
)

target_include_directories(patternfab-core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(patternfab-core PUBLIC
    ${VTK_LIBRARIES}
    dime::dime
    nlohmann_json::nlohmann_json
)

vtk_module_autoinit(
    TARGETS patternfab-core
    MODULES ${VTK_LIBRARIES}
)

enable_testing()
add_subdirectory(tests)
