// patternfab-cli: a thin end-to-end driver over patternfab-core.
//
// Runs one pattern through the entire fabrication pipeline exactly as the
// GUI (Phase 5) will orchestrate it: ingest -> constraint report -> bleed
// compensation -> all four export formats -> sensor-physics uncertainty.
// Its purpose is twofold: a manual smoke tool ("does a real pattern survive
// the whole chain?"), and a worked example of the core call sequence the GUI
// wraps.
//
// The process parameters below (constraints, DPI, relief, sensor noise) are
// illustrative demo values, NOT defaults baked into the library -- every one
// of them is caller-supplied precisely because it depends on the specific
// printer/laser/camera in use (see the header comments in ConstraintEngine.h,
// StlExport.h, UncertaintyEngine.h). The GUI will collect them from the user.

#include <patternfab/ConstraintEngine.h>
#include <patternfab/DxfExport.h>
#include <patternfab/PngExport.h>
#include <patternfab/Pattern.h>
#include <patternfab/StlExport.h>
#include <patternfab/SvgExport.h>
#include <patternfab/UncertaintyEngine.h>

#include <algorithm>
#include <patternfab/VectorInput.h>

#include <filesystem>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

int main(int argc, char *argv[]) {
    if (argc != 3) {
        std::cerr << "usage: patternfab-cli <pattern.json> <output-dir>\n"
                  << "  Runs a PatternFab vector pattern through the full\n"
                  << "  ingest -> constrain -> export -> uncertainty pipeline,\n"
                  << "  writing SVG/DXF/PNG/STL into <output-dir>.\n";
        return 2;
    }
    const std::string patternPath = argv[1];
    const fs::path outDir = argv[2];

    try {
        fs::create_directories(outDir);

        // --- 1. Ingest -------------------------------------------------
        const patternfab::Pattern pattern = patternfab::loadPatternFromVectorFile(patternPath);
        const auto &p = pattern.params;
        std::cout << "Loaded " << pattern.primitives.size() << " primitives\n"
                  << "  specimen        " << p.specimenWidthMm << " x " << p.specimenHeightMm << " mm\n"
                  << "  imaging res     " << p.imagingResolutionPxPerMm << " px/mm\n"
                  << "  target speckle  " << p.targetSpeckleSizeMm << " mm\n";

        const patternfab::BoundingBox bbox = patternfab::computeBoundingBox(pattern);
        std::cout << "  pattern extent  [" << bbox.minXMm << ", " << bbox.minYMm << "] .. ["
                  << bbox.maxXMm << ", " << bbox.maxYMm << "] mm\n";

        // --- 2. Constraint report -------------------------------------
        // Demo constraints: a resin-print stamp/stencil process.
        patternfab::ManufacturingConstraints constraints;
        constraints.minFeatureSizeMm = 0.15;      // finest reliable resin feature
        constraints.bleedCompensationMm = 0.03;   // ink/casting spread to erode away
        constraints.minBridgeWidthMm = 0.20;      // thinnest safe material web

        const patternfab::ConstraintReport report =
            patternfab::evaluateConstraints(pattern, constraints);
        std::cout << "\nConstraint report (demo process values):\n"
                  << "  requires tiling         " << (report.patternRequiresTiling ? "YES (periodicity risk)" : "no")
                  << "\n  min-feature violations   " << report.minimumFeatureSizeViolations.size()
                  << "\n  bridging violations      " << report.bridgingViolations.size() << "\n";

        // --- 3. Bleed compensation ------------------------------------
        const patternfab::Pattern compensated =
            patternfab::applyBleedCompensation(pattern, constraints);

        // --- 4. Exports ------------------------------------------------
        const std::string svg = (outDir / "pattern.svg").string();
        const std::string dxf = (outDir / "pattern.dxf").string();
        const std::string png = (outDir / "pattern.png").string();
        const std::string stl = (outDir / "pattern.stl").string();

        patternfab::exportPatternToSvg(compensated, svg);
        patternfab::exportPatternToDxf(compensated, dxf);

        const double dpi = 600.0; // direct-write / decal raster resolution
        patternfab::exportPatternToPng(compensated, png, dpi);

        patternfab::ReliefParameters relief;
        relief.baseThicknessMm = 2.0;
        relief.bumpHeightMm = 0.8;
        relief.meshResolutionPerMm = 8.0;
        patternfab::exportPatternToStl(compensated, stl, relief);

        std::cout << "\nExported:\n"
                  << "  " << svg << "\n"
                  << "  " << dxf << "\n"
                  << "  " << png << " (" << dpi << " dpi)\n"
                  << "  " << stl << " (base " << relief.baseThicknessMm
                  << " mm, bump " << relief.bumpHeightMm << " mm)\n";

        // --- 5. Sensor-physics uncertainty ----------------------------
        // Demo noise profile: a mid-range phone sensor at moderate ISO.
        patternfab::SensorNoiseProfile noise;
        noise.S = 1.2e-4;
        noise.O = 4.0e-6;
        const patternfab::UncertaintyMap umap =
            patternfab::computeUncertaintyMap(pattern, noise);
        // ⚑ Reported as a DISTRIBUTION, with the flat-pixel share named for
        // what it is. The obvious headline -- the fraction of pixels below
        // some confidence -- is near 100% for a good speckle pattern and a bad
        // one alike, because most pixels of any speckle pattern lie inside a
        // dot or in the background and carry no intensity gradient at all. A
        // number that cannot tell the two apart is worse than no number: it
        // reads as a failing grade on a pattern that is fine.
        //
        // What carries the information is the confidence where there IS an
        // edge to correlate, so the percentiles are printed too. No threshold
        // is invented here for what counts as good: that depends on the
        // correlation algorithm and the strain accuracy being chased, which is
        // why lowConfidenceFraction() takes the level from its caller.
        std::vector<double> sorted = umap.confidence;
        std::sort(sorted.begin(), sorted.end());
        const auto percentile = [&sorted](double fraction) {
            if (sorted.empty()) {
                return 0.0;
            }
            const size_t index = static_cast<size_t>(
                fraction * static_cast<double>(sorted.size() - 1));
            return sorted[index];
        };
        const double lowFrac = patternfab::lowConfidenceFraction(umap, 1.0);
        std::cout << "\nUncertainty map " << umap.widthPx << " x " << umap.heightPx
                  << " px, per-pixel confidence (gradient over sensor noise):\n"
                  << "  median            " << percentile(0.50) << "\n"
                  << "  90th percentile   " << percentile(0.90) << "\n"
                  << "  99th percentile   " << percentile(0.99) << "\n"
                  << "  below 1.0         " << (lowFrac * 100.0) << "% of pixels\n"
                  << "\n  The last line is not a quality score. Most pixels of any\n"
                  << "  speckle pattern are flat -- inside a dot or in the background --\n"
                  << "  and correlation works over subsets, not single pixels, so a high\n"
                  << "  share here is expected rather than a fault.\n";

        // --- 6. The same physics, as DIC's own figure ------------------
        // ⚑ The number above is dimensionless and its threshold is invented by
        // its caller, so it cannot be compared with anything -- least of all
        // with what the pattern achieves once fabricated. This one can: it is
        // sigma, a displacement noise floor in PIXELS, by the same definition
        // SurView measures after a run. Design-time and measured, same units,
        // side by side.
        const int subsetRadiusPx = patternfab::kSubsetRadiusPx;
        const patternfab::NoiseFloorMap floorMap =
            patternfab::computeNoiseFloorMap(pattern, noise, subsetRadiusPx);
        const patternfab::NoiseFloorSummary floor =
            patternfab::summariseNoiseFloor(floorMap);

        std::cout << "\nDisplacement noise floor at a " << subsetRadiusPx
                  << " px subset radius (DIC's sigma, lower is better):\n";
        if (floor.establishedCount == 0) {
            // ⚑ Not a zero and not a shrug. Nowhere on this pattern has
            // gradient in BOTH directions, so it cannot measure displacement at
            // all -- which a gradient-magnitude score cannot tell you.
            std::cout << "  none established anywhere. This pattern has no\n"
                      << "  gradient in one of the two directions, so it cannot\n"
                      << "  resolve displacement in that direction at all.\n";
        } else {
            // ⚑ A RANGE TO THE 95th PERCENTILE, and deliberately NOT the
            // worst value. A subset lying almost entirely on blank background
            // still has a trace of gradient, so it is established and its floor
            // is enormous -- on this sample the worst is about 945000 px, which
            // is arithmetic rather than information. SurView made exactly this
            // mistake once on a measured field, where one such point made an
            // excellent run report "at worst one part in 3".
            //
            // The share that established anything is the other half of the
            // story, and the more useful half for a designer: it says how much
            // of the specimen this pattern can measure at all.
            std::cout << "  " << floor.bestPx << " to " << floor.typicalPx
                      << " px for 95% of them\n"
                      << "  established at    " << floor.establishedCount << " of "
                      << floor.totalCount << " points ("
                      << (100.0 * floor.establishedCount / floor.totalCount)
                      << "% of the specimen)\n"
                      << "\n  An UPPER BOUND, not a prediction. This is an ideal\n"
                      << "  rendering; real ink, a real substrate and real focus all\n"
                      << "  make it worse. Measure the fabricated pattern and the\n"
                      << "  difference is the fabrication and imaging penalty.\n";
        }

        std::cout << "\nPipeline OK.\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
}
