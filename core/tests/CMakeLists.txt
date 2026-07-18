add_executable(patternfab-core-tests test_pattern_input.cpp)

target_link_libraries(patternfab-core-tests PRIVATE
    patternfab-core
    ${VTK_LIBRARIES}
)

vtk_module_autoinit(
    TARGETS patternfab-core-tests
    MODULES ${VTK_LIBRARIES}
)

add_test(NAME patternfab-core-tests COMMAND patternfab-core-tests)
