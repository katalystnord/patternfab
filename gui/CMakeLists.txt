add_executable(patternfab-gui
    src/main.cpp
    src/MainWindow.cpp
    src/MainWindow.h
    src/PreviewWidget.cpp
    src/PreviewWidget.h
    src/ReliefPreviewWidget.cpp
    src/ReliefPreviewWidget.h
)

target_link_libraries(patternfab-gui PRIVATE
    patternfab-core
    Qt6::Widgets
    Qt6::Svg
    Qt6::OpenGLWidgets
    VTK::GUISupportQt
    VTK::RenderingOpenGL2
)

vtk_module_autoinit(
    TARGETS patternfab-gui
    MODULES ${VTK_LIBRARIES}
)
