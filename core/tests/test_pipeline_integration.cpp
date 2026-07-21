// End-to-end integration test: the real checked-in sample pattern
// (examples/sample_pattern.json) run through the entire pipeline the GUI
// will orchestrate -- ingest -> constrain -> bleed -> all four exports ->
// uncertainty. Unlike the per-module unit tests, this proves the stages
// actually compose on a realistic 175-speckle non-periodic field loaded
// from disk via the real vector-input path, and that every export produces
// a valid, non-empty artifact.
//
// PATTERNFAB_SAMPLE_PATTERN is defined by CMake as the absolute path to the
// checked-in sample, so the test doesn't depend on the working directory.

#include <patternfab/ConstraintEngine.h>
#include <patternfab/DxfExport.h>
#include <patternfab/PngExport.h>
#include <patternfab/Pattern.h>
#include <patternfab/StlExport.h>
#include <patternfab/SvgExport.h>
#include <patternfab/UncertaintyEngine.h>
#include <patternfab/VectorInput.h>

#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkPNGReader.h>
#include <vtkPolyData.h>
#include <vtkSTLReader.h>

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

namespace {

int failures = 0;

void check(bool condition, const std::string &message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << std::endl;
        ++failures;
    }
}

std::uintmax_t fileSize(const std::string &path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    return f ? static_cast<std::uintmax_t>(f.tellg()) : 0;
}

bool fileStartsWith(const std::string &path, const std::string &prefix) {
    std::ifstream f(path, std::ios::binary);
    std::string buf(prefix.size(), '\0');
    f.read(buf.data(), static_cast<std::streamsize>(prefix.size()));
    return f && buf == prefix;
}

void testFullPipeline() {
    // --- Ingest the real sample file ---------------------------------
    const patternfab::Pattern pattern =
        patternfab::loadPatternFromVectorFile(PATTERNFAB_SAMPLE_PATTERN);
    check(pattern.primitives.size() > 100,
          "sample loaded with a realistic speckle count");
    check(pattern.params.specimenWidthMm > 0 && pattern.params.specimenHeightMm > 0,
          "specimen dimensions positive");

    // --- Constraint report -------------------------------------------
    patternfab::ManufacturingConstraints constraints;
    constraints.minFeatureSizeMm = 0.15;
    constraints.bleedCompensationMm = 0.03;
    constraints.minBridgeWidthMm = 0.20;
    const patternfab::ConstraintReport report =
        patternfab::evaluateConstraints(pattern, constraints);
    // The sample field is designed to cover the whole specimen in one pass,
    // so it must NOT require tiling (the periodicity trap).
    check(!report.patternRequiresTiling,
          "single-field sample does not require tiling");

    // --- Bleed compensation shrinks radii ----------------------------
    const patternfab::Pattern compensated =
        patternfab::applyBleedCompensation(pattern, constraints);
    check(compensated.primitives.size() == pattern.primitives.size(),
          "bleed compensation preserves primitive count");
    check(compensated.primitives[0].radiusXMm < pattern.primitives[0].radiusXMm,
          "bleed compensation shrinks radius");

    // --- Exports produce valid artifacts -----------------------------
    const std::string svg = "e2e_pattern.svg";
    const std::string dxf = "e2e_pattern.dxf";
    const std::string png = "e2e_pattern.png";
    const std::string stl = "e2e_pattern.stl";

    patternfab::exportPatternToSvg(compensated, svg);
    check(fileSize(svg) > 0, "SVG non-empty");
    check(fileStartsWith(svg, "<?xml"), "SVG has XML header");

    patternfab::exportPatternToDxf(compensated, dxf);
    check(fileSize(dxf) > 0, "DXF non-empty");

    const double dpi = 300.0;
    patternfab::exportPatternToPng(compensated, png, dpi);
    vtkNew<vtkPNGReader> pngReader;
    pngReader->SetFileName(png.c_str());
    pngReader->Update();
    int dims[3];
    pngReader->GetOutput()->GetDimensions(dims);
    // 30 mm at 300 dpi = 30/25.4*300 ~= 354 px wide.
    check(dims[0] > 300 && dims[1] > 200, "PNG rasterized at expected pixel size");

    patternfab::ReliefParameters relief;
    relief.baseThicknessMm = 2.0;
    relief.bumpHeightMm = 0.8;
    relief.meshResolutionPerMm = 6.0;
    patternfab::exportPatternToStl(compensated, stl, relief);
    vtkNew<vtkSTLReader> stlReader;
    stlReader->SetFileName(stl.c_str());
    stlReader->Update();
    vtkPolyData *mesh = stlReader->GetOutput();
    check(mesh->GetNumberOfPoints() > 0 && mesh->GetNumberOfCells() > 0,
          "STL mesh has geometry");
    double bounds[6];
    mesh->GetBounds(bounds);
    check(std::abs(bounds[5] - (relief.baseThicknessMm + relief.bumpHeightMm)) < 0.05,
          "STL top at base + bump height");

    // --- Uncertainty over the same pattern ---------------------------
    patternfab::SensorNoiseProfile noise;
    noise.S = 1.2e-4;
    noise.O = 4.0e-6;
    const patternfab::UncertaintyMap umap =
        patternfab::computeUncertaintyMap(pattern, noise);
    check(umap.widthPx > 0 && umap.heightPx > 0, "uncertainty map has extent");
    check(umap.confidence.size() ==
              static_cast<std::size_t>(umap.widthPx) * umap.heightPx,
          "uncertainty map fully populated");
    const double lowFrac = patternfab::lowConfidenceFraction(umap, 1.0);
    check(lowFrac >= 0.0 && lowFrac <= 1.0, "low-confidence fraction in [0,1]");
}

} // namespace

int main() {
    testFullPipeline();

    if (failures == 0) {
        std::cout << "OK: patternfab-core end-to-end pipeline test passed" << std::endl;
        return EXIT_SUCCESS;
    }
    std::cerr << failures << " test(s) failed" << std::endl;
    return EXIT_FAILURE;
}
