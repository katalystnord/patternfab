add_executable(patternfab-core-tests-input test_pattern_input.cpp)
target_link_libraries(patternfab-core-tests-input PRIVATE
    patternfab-core
    ${VTK_LIBRARIES}
)
vtk_module_autoinit(
    TARGETS patternfab-core-tests-input
    MODULES ${VTK_LIBRARIES}
)
add_test(NAME patternfab-core-tests-input COMMAND patternfab-core-tests-input)

add_executable(patternfab-core-tests-constraints test_constraint_engine.cpp)
target_link_libraries(patternfab-core-tests-constraints PRIVATE
    patternfab-core
)
add_test(NAME patternfab-core-tests-constraints COMMAND patternfab-core-tests-constraints)

add_executable(patternfab-core-tests-svg test_svg_export.cpp)
target_link_libraries(patternfab-core-tests-svg PRIVATE
    patternfab-core
)
add_test(NAME patternfab-core-tests-svg COMMAND patternfab-core-tests-svg)

add_executable(patternfab-core-tests-dxf test_dxf_export.cpp)
target_link_libraries(patternfab-core-tests-dxf PRIVATE
    patternfab-core
)
add_test(NAME patternfab-core-tests-dxf COMMAND patternfab-core-tests-dxf)

add_executable(patternfab-core-tests-png test_png_export.cpp)
target_link_libraries(patternfab-core-tests-png PRIVATE
    patternfab-core
)
add_test(NAME patternfab-core-tests-png COMMAND patternfab-core-tests-png)

add_executable(patternfab-core-tests-stl test_stl_export.cpp)
target_link_libraries(patternfab-core-tests-stl PRIVATE
    patternfab-core
    ${VTK_LIBRARIES}
)
vtk_module_autoinit(
    TARGETS patternfab-core-tests-stl
    MODULES ${VTK_LIBRARIES}
)
add_test(NAME patternfab-core-tests-stl COMMAND patternfab-core-tests-stl)

add_executable(patternfab-core-tests-uncertainty test_uncertainty_engine.cpp)
target_link_libraries(patternfab-core-tests-uncertainty PRIVATE
    patternfab-core
)
add_test(NAME patternfab-core-tests-uncertainty COMMAND patternfab-core-tests-uncertainty)
