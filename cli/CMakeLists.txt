add_executable(patternfab-cli main.cpp)

target_link_libraries(patternfab-cli PRIVATE
    patternfab-core
)

vtk_module_autoinit(
    TARGETS patternfab-cli
    MODULES ${VTK_LIBRARIES}
)
