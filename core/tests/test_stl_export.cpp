#include <patternfab/StlExport.h>

#include <vtkNew.h>
#include <vtkPolyData.h>
#include <vtkCell.h>
#include <vtkPoints.h>
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


void the_mesh_is_as_fine_as_the_resolution_asked_for() {
    // ⚑ The existing cases check the mesh's BOUNDS and that it has some cells
    // at all, and a stamp meshed at half the resolution asked for satisfies
    // both perfectly: same size, same shape, fewer triangles. So the grid
    // arithmetic survived every mutation the suite could throw at it, and a
    // "+ 1" that decides how many samples a millimetre gets could quietly
    // become "+ 0".
    //
    // The count is derived HERE from the resolution alone -- qw quads across
    // is round(width * resolution), with no "+ 1" anywhere in the expectation
    // -- so it cannot agree with the code by sharing its mistake. The
    // structure it counts: qw by qh quads on the top, two triangles each; the
    // same again on the bottom cap the extrusion adds; and two triangles for
    // each of the 2(qw + qh) quads around the walls.
    //
    // A companion case checking that no triangle has zero area was written and
    // then DELETED, which is worth recording so it is not written again: it
    // could not be made to fail. vtkSTLWriter already drops a degenerate
    // triangle, so the property is guaranteed by the library rather than by
    // this code, and a case that cannot go red is false comfort. The count is
    // what catches a corrupted index anyway -- a quad indexed onto its own
    // corner reached 1440 cells against 1088.
    patternfab::Pattern pattern;
    pattern.params.specimenWidthMm = 10.0;
    pattern.params.specimenHeightMm = 6.0;

    patternfab::ReliefParameters relief;
    relief.baseThicknessMm = 2.0;
    relief.bumpHeightMm = 0.8;
    relief.meshResolutionPerMm = 2.0;

    const std::string path = "/tmp/patternfab_mesh_resolution.stl";
    patternfab::exportPatternToStl(pattern, path, relief);

    vtkNew<vtkSTLReader> reader;
    reader->SetFileName(path.c_str());
    reader->Update();
    vtkPolyData *mesh = reader->GetOutput();

    const long qw = std::lround(pattern.params.specimenWidthMm * relief.meshResolutionPerMm);
    const long qh = std::lround(pattern.params.specimenHeightMm * relief.meshResolutionPerMm);
    const long expected = 4 * qw * qh + 4 * (qw + qh);
    check(mesh->GetNumberOfCells() == expected,
          "the mesh holds the triangles a " + std::to_string(qw) + " by " + std::to_string(qh)
              + " grid makes: expected " + std::to_string(expected) + ", got "
              + std::to_string(mesh->GetNumberOfCells()));

    // Doubling the resolution quadruples the surface, which no fixed grid can
    // fake and no single count can pin on its own.
    relief.meshResolutionPerMm = 4.0;
    patternfab::exportPatternToStl(pattern, path, relief);
    vtkNew<vtkSTLReader> finer;
    finer->SetFileName(path.c_str());
    finer->Update();
    const long expectedFiner = 4 * (2 * qw) * (2 * qh) + 4 * (2 * qw + 2 * qh);
    check(finer->GetOutput()->GetNumberOfCells() == expectedFiner,
          "and twice the resolution gives four times the surface");
}

} // namespace

int main() {
    the_mesh_is_as_fine_as_the_resolution_asked_for();
    testStlExport();

    if (failures == 0) {
        std::cout << "OK: all patternfab-core STL export tests passed" << std::endl;
        return EXIT_SUCCESS;
    }
    std::cerr << failures << " test(s) failed" << std::endl;
    return EXIT_FAILURE;
}
