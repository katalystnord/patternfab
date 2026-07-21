#include "ReliefPreviewWidget.h"

#include <vtkActor.h>
#include <vtkCamera.h>
#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkNew.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>
#include <vtkSTLReader.h>

ReliefPreviewWidget::ReliefPreviewWidget(QWidget *parent) : QVTKOpenGLNativeWidget(parent) {
    vtkNew<vtkGenericOpenGLRenderWindow> window;
    setRenderWindow(window);

    renderer_ = vtkSmartPointer<vtkRenderer>::New();
    renderer_->SetBackground(1.0, 1.0, 1.0); // white, matching the 2D preview ground
    renderer_->SetBackground2(0.90, 0.92, 0.95);
    renderer_->GradientBackgroundOn();
    renderWindow()->AddRenderer(renderer_);
}

void ReliefPreviewWidget::showStl(const QString &path) {
    renderer_->RemoveAllViewProps();

    vtkNew<vtkSTLReader> reader;
    reader->SetFileName(path.toUtf8().constData());
    reader->Update();

    vtkNew<vtkPolyDataMapper> mapper;
    mapper->SetInputConnection(reader->GetOutputPort());
    mapper->ScalarVisibilityOff();

    vtkNew<vtkActor> actor;
    actor->SetMapper(mapper);
    actor->GetProperty()->SetColor(0.35, 0.37, 0.40); // resin-grey relief
    actor->GetProperty()->SetSpecular(0.3);
    actor->GetProperty()->SetSpecularPower(20.0);
    renderer_->AddActor(actor);

    // Look down at a shallow angle so the raised speckle domes read as 3D.
    renderer_->ResetCamera();
    renderer_->GetActiveCamera()->Elevation(-55.0);
    renderer_->GetActiveCamera()->Azimuth(25.0);
    renderer_->GetActiveCamera()->OrthogonalizeViewUp();
    renderer_->ResetCameraClippingRange();
    renderWindow()->Render();
}

void ReliefPreviewWidget::clear() {
    renderer_->RemoveAllViewProps();
    renderWindow()->Render();
}
