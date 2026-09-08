#include "patternfab/StlExport.h"

#include "ReliefField.h"

#include <vtkCellArray.h>
#include <vtkLinearExtrusionFilter.h>
#include <vtkNew.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkSTLWriter.h>

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace patternfab {

void exportPatternToStl(const Pattern &pattern, const std::string &path, const ReliefParameters &relief) {
    const double widthMm = pattern.params.specimenWidthMm;
    const double heightMm = pattern.params.specimenHeightMm;
    if (widthMm <= 0.0 || heightMm <= 0.0 || relief.baseThicknessMm <= 0.0 || relief.bumpHeightMm <= 0.0 ||
        relief.meshResolutionPerMm <= 0.0) {
        throw std::runtime_error("exportPatternToStl: specimen dimensions and all ReliefParameters must be positive");
    }

    const int gridWidth = static_cast<int>(std::llround(widthMm * relief.meshResolutionPerMm)) + 1;
    const int gridHeight = static_cast<int>(std::llround(heightMm * relief.meshResolutionPerMm)) + 1;
    if (gridWidth < 2 || gridHeight < 2) {
        throw std::runtime_error("exportPatternToStl: meshResolutionPerMm too low for specimen size");
    }

    vtkNew<vtkPoints> points;
    points->SetNumberOfPoints(static_cast<vtkIdType>(gridWidth) * gridHeight);
    for (int j = 0; j < gridHeight; ++j) {
        const double y = (heightMm * j) / (gridHeight - 1);
        for (int i = 0; i < gridWidth; ++i) {
            const double x = (widthMm * i) / (gridWidth - 1);

            double bump = 0.0;
            for (const auto &primitive : pattern.primitives) {
                bump = std::max(bump, bumpHeightAt(x, y, primitive, relief.bumpHeightMm));
            }

            const vtkIdType pointId = static_cast<vtkIdType>(j) * gridWidth + i;
            points->SetPoint(pointId, x, y, relief.baseThicknessMm + bump);
        }
    }

    vtkNew<vtkCellArray> polys;
    for (int j = 0; j < gridHeight - 1; ++j) {
        for (int i = 0; i < gridWidth - 1; ++i) {
            const vtkIdType idx00 = static_cast<vtkIdType>(j) * gridWidth + i;
            const vtkIdType idx10 = idx00 + 1;
            const vtkIdType idx11 = idx00 + gridWidth + 1;
            const vtkIdType idx01 = idx00 + gridWidth;
            polys->InsertNextCell({idx00, idx10, idx11});
            polys->InsertNextCell({idx00, idx11, idx01});
        }
    }

    vtkNew<vtkPolyData> topSurface;
    topSurface->SetPoints(points);
    topSurface->SetPolys(polys);

    vtkNew<vtkLinearExtrusionFilter> extrusion;
    extrusion->SetInputData(topSurface);
    extrusion->SetExtrusionTypeToVectorExtrusion();
    extrusion->SetVector(0.0, 0.0, -1.0);
    // Extrudes down by exactly baseThicknessMm, so the bottom cap lands at
    // z=0 (the top surface's non-bumped points sit at z=baseThicknessMm).
    extrusion->SetScaleFactor(relief.baseThicknessMm);
    extrusion->SetCapping(true);

    vtkNew<vtkSTLWriter> writer;
    writer->SetFileName(path.c_str());
    writer->SetFileTypeToBinary();
    writer->SetInputConnection(extrusion->GetOutputPort());
    if (!writer->Write()) {
        throw std::runtime_error("exportPatternToStl: failed to write file: " + path);
    }
}

} // namespace patternfab
