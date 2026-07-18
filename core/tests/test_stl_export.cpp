#include <patternfab/StlExport.h>

#include <vtkNew.h>
#include <vtkPolyData.h>
#include <vtkSTLReader.h>

#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {

int failures = 0;

void check(bool condition, const std::string &message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << std::endl;
        ++failures;
    }
}

void testStlExport() {
    patternfab::Pattern pattern;
    pattern.params.specimenWidthMm = 10.0;
    pattern.params.specimenHeightMm = 10.0;

    patternfab::Primitive circle;
    circle.shape = patternfab::PrimitiveShape::Circle;
    circle.centerXMm = 5.0;
    circle.centerYMm = 5.0;
    circle.radiusXMm = 2.0;
    circle.radiusYMm = 2.0;
    pattern.primitives.push_back(circle);

    patternfab::ReliefParameters relief;
    relief.baseThicknessMm = 1.5;
    relief.bumpHeightMm = 0.5;
    relief.meshResolutionPerMm = 5.0; // 0.2mm grid spacing

    const std::string path = "test_pattern.stl";
    patternfab::exportPatternToStl(pattern, path, relief);

    vtkNew<vtkSTLReader> reader;
    reader->SetFileName(path.c_str());
    reader->Update();

    vtkPolyData *mesh = reader->GetOutput();
    check(mesh != nullptr, "STL file was read back");
    check(mesh->GetNumberOfPoints() > 0, "mesh has points");
    check(mesh->GetNumberOfCells() > 0, "mesh has cells");

    double bounds[6];
    mesh->GetBounds(bounds); // xmin,xmax, ymin,ymax, zmin,zmax

    check(std::abs(bounds[0] - 0.0) < 0.01, "x min at 0");
    check(std::abs(bounds[1] - 10.0) < 0.01, "x max at specimen width");
    check(std::abs(bounds[2] - 0.0) < 0.01, "y min at 0");
    check(std::abs(bounds[3] - 10.0) < 0.01, "y max at specimen height");
    check(std::abs(bounds[4] - 0.0) < 0.01, "z min at 0 (bottom cap)");
    check(std::abs(bounds[5] - (relief.baseThicknessMm + relief.bumpHeightMm)) < 0.05,
          "z max at base + bump height (highest point of dome)");

    // No relief anywhere must throw (zero bump height).
    patternfab::ReliefParameters zeroBump = relief;
    zeroBump.bumpHeightMm = 0.0;
    bool threw = false;
    try {
        patternfab::exportPatternToStl(pattern, "should_not_be_written.stl", zeroBump);
    } catch (const std::runtime_error &) {
        threw = true;
    }
    check(threw, "zero bump height throws");
}

} // namespace

int main() {
    testStlExport();

    if (failures == 0) {
        std::cout << "OK: all patternfab-core STL export tests passed" << std::endl;
        return EXIT_SUCCESS;
    }
    std::cerr << failures << " test(s) failed" << std::endl;
    return EXIT_FAILURE;
}
