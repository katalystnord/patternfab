#include <patternfab/RasterInput.h>
#include <patternfab/VectorInput.h>

#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkPNGWriter.h>

#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>

namespace {

int failures = 0;

void check(bool condition, const std::string &message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << std::endl;
        ++failures;
    }
}

void testVectorInput() {
    const std::string path = "test_pattern.json";
    std::ofstream file(path);
    file << R"({
        "physicalParameters": {
            "specimenWidthMm": 100.0,
            "specimenHeightMm": 100.0,
            "imagingResolutionPxPerMm": 40.0,
            "targetSpeckleSizeMm": 0.5
        },
        "primitives": [
            { "shape": "circle", "centerXMm": 12.3, "centerYMm": 45.6, "radiusMm": 0.25 },
            { "shape": "ellipse", "centerXMm": 1.0, "centerYMm": 2.0, "radiusXMm": 0.3, "radiusYMm": 0.2 },
            { "shape": "polygon", "verticesMm": [[0,0],[1,0],[1,1]] }
        ]
    })";
    file.close();

    const patternfab::Pattern pattern = patternfab::loadPatternFromVectorFile(path);

    check(pattern.params.specimenWidthMm == 100.0, "specimenWidthMm parsed");
    check(pattern.params.imagingResolutionPxPerMm == 40.0, "imagingResolutionPxPerMm parsed");
    check(pattern.primitives.size() == 3, "three primitives parsed");
    check(pattern.primitives[0].shape == patternfab::PrimitiveShape::Circle, "primitive 0 is circle");
    check(pattern.primitives[0].radiusXMm == 0.25, "circle radius parsed");
    check(pattern.primitives[1].shape == patternfab::PrimitiveShape::Ellipse, "primitive 1 is ellipse");
    check(pattern.primitives[2].shape == patternfab::PrimitiveShape::Polygon, "primitive 2 is polygon");
    check(pattern.primitives[2].verticesMm.size() == 3, "polygon has three vertices");
}

void drawFilledCircle(vtkImageData *image, int cx, int cy, int radiusPx) {
    for (int y = cy - radiusPx; y <= cy + radiusPx; ++y) {
        for (int x = cx - radiusPx; x <= cx + radiusPx; ++x) {
            const double dx = x - cx;
            const double dy = y - cy;
            if (dx * dx + dy * dy <= radiusPx * radiusPx) {
                image->SetScalarComponentFromDouble(x, y, 0, 0, 0.0); // black = ink
            }
        }
    }
}

void testRasterInput() {
    const int width = 200;
    const int height = 200;
    const int radiusPx = 10;

    vtkNew<vtkImageData> image;
    image->SetDimensions(width, height, 1);
    image->AllocateScalars(VTK_UNSIGNED_CHAR, 1);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            image->SetScalarComponentFromDouble(x, y, 0, 0, 255.0); // white background
        }
    }
    drawFilledCircle(image, 50, 50, radiusPx);
    drawFilledCircle(image, 150, 120, radiusPx);

    const std::string path = "test_pattern.png";
    vtkNew<vtkPNGWriter> writer;
    writer->SetFileName(path.c_str());
    writer->SetInputData(image);
    writer->Write();

    patternfab::PhysicalParameters params;
    params.specimenWidthMm = 5.0;
    params.specimenHeightMm = 5.0;
    params.imagingResolutionPxPerMm = 40.0; // width/height px at 40 px/mm -> 5mm
    params.targetSpeckleSizeMm = 0.5;

    const patternfab::Pattern pattern = patternfab::loadPatternFromRasterFile(path, params);

    check(pattern.primitives.size() == 2, "two blobs detected");
    if (pattern.primitives.size() == 2) {
        const double expectedRadiusMm = radiusPx / params.imagingResolutionPxPerMm;
        for (const auto &primitive : pattern.primitives) {
            check(std::abs(primitive.radiusXMm - expectedRadiusMm) < 0.02,
                  "blob radius close to expected (" + std::to_string(primitive.radiusXMm) + " vs " +
                      std::to_string(expectedRadiusMm) + ")");
        }
    }
}

} // namespace

int main() {
    testVectorInput();
    testRasterInput();

    if (failures == 0) {
        std::cout << "OK: all patternfab-core input tests passed" << std::endl;
        return EXIT_SUCCESS;
    }
    std::cerr << failures << " test(s) failed" << std::endl;
    return EXIT_FAILURE;
}
