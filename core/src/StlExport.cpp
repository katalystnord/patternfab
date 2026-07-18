#include "patternfab/StlExport.h"

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

namespace {

bool pointInPolygon(double x, double y, const std::vector<std::pair<double, double>> &vertices) {
    bool inside = false;
    const std::size_t n = vertices.size();
    for (std::size_t i = 0, j = n - 1; i < n; j = i++) {
        const double xi = vertices[i].first;
        const double yi = vertices[i].second;
        const double xj = vertices[j].first;
        const double yj = vertices[j].second;
        const bool edgeCrossesScanline = (yi > y) != (yj > y);
        if (edgeCrossesScanline && (x < (xj - xi) * (y - yi) / (yj - yi) + xi)) {
            inside = !inside;
        }
    }
    return inside;
}

// Hemispherical-cap dome for circle/ellipse, flat top for polygon (see
// StlExport.h for why polygons don't get a true distance-based dome).
double bumpHeightAt(double x, double y, const Primitive &primitive, double bumpHeightMm) {
    if (primitive.shape == PrimitiveShape::Polygon) {
        return pointInPolygon(x, y, primitive.verticesMm) ? bumpHeightMm : 0.0;
    }
    if (primitive.radiusXMm <= 0.0 || primitive.radiusYMm <= 0.0) {
        return 0.0;
    }
    const double dx = (x - primitive.centerXMm) / primitive.radiusXMm;
    const double dy = (y - primitive.centerYMm) / primitive.radiusYMm;
    const double normalizedDistSq = dx * dx + dy * dy;
    if (normalizedDistSq > 1.0) {
        return 0.0;
    }
    return bumpHeightMm * std::sqrt(1.0 - normalizedDistSq);
}

} // namespace

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
