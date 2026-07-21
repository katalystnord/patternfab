#pragma once

#include <QVTKOpenGLNativeWidget.h>
#include <vtkSmartPointer.h>

class vtkRenderer;

// In-window 3D preview of the STL relief, rendered with VTK's OpenGL pipeline
// embedded in the Qt window (QVTKOpenGLNativeWidget). Loads the same STL that
// exportPatternToStl writes, so -- like the 2D SVG preview -- what's shown is
// exactly what's exported. The user can orbit/zoom with the mouse.
class ReliefPreviewWidget : public QVTKOpenGLNativeWidget {
    Q_OBJECT

public:
    explicit ReliefPreviewWidget(QWidget *parent = nullptr);

    // Load and display the STL at the given path (replacing any prior mesh).
    void showStl(const QString &path);
    void clear();

private:
    vtkSmartPointer<vtkRenderer> renderer_;
};
