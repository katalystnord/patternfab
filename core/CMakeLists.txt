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
    src/SvgExport.cpp
    include/patternfab/SvgExport.h
    src/DxfExport.cpp
    include/patternfab/DxfExport.h
    src/PngExport.cpp
    include/patternfab/PngExport.h
    src/PrimitiveDrawing.cpp
    src/PrimitiveDrawing.h
    src/StlExport.cpp
    include/patternfab/StlExport.h
    src/UncertaintyEngine.cpp
    include/patternfab/UncertaintyEngine.h
)

target_include_directories(patternfab-core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(patternfab-core PUBLIC
    ${VTK_LIBRARIES}
    dime::dime
    nlohmann_json::nlohmann_json
    # Qt6::Gui (QPainter) + Qt6::Svg (QSvgGenerator) only -- NOT Qt6::Widgets.
    # Verified these run standalone with no QApplication/QGuiApplication
    # instance and no display (QT_QPA_PLATFORM=offscreen or unset both
    # tested). Keeps the "headless, unit-testable directly" property from
    # CLAUDE.md's Development path: that principle is about not depending on
    # interactive widgets, not about zero Qt linkage.
    Qt6::Gui
    Qt6::Svg
)

vtk_module_autoinit(
    TARGETS patternfab-core
    MODULES ${VTK_LIBRARIES}
)

enable_testing()
add_subdirectory(tests)
