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
