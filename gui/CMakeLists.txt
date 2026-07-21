add_executable(patternfab-gui
    src/main.cpp
    src/MainWindow.cpp
    src/MainWindow.h
    src/PreviewWidget.cpp
    src/PreviewWidget.h
)

target_link_libraries(patternfab-gui PRIVATE
    patternfab-core
    Qt6::Widgets
    Qt6::Svg
)

vtk_module_autoinit(
    TARGETS patternfab-gui
    MODULES ${VTK_LIBRARIES}
)
