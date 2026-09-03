#include <patternfab/UncertaintyEngine.h>

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

patternfab::Pattern makeSingleCirclePattern() {
    patternfab::Pattern pattern;
    pattern.params.specimenWidthMm = 10.0;
    pattern.params.specimenHeightMm = 10.0;
    pattern.params.imagingResolutionPxPerMm = 20.0;

    patternfab::Primitive circle;
    circle.shape = patternfab::PrimitiveShape::Circle;
    circle.centerXMm = 5.0;
    circle.centerYMm = 5.0;
    circle.radiusXMm = 2.0;
    circle.radiusYMm = 2.0;
    pattern.primitives.push_back(circle);
    return pattern;
}

double confidenceAt(const patternfab::UncertaintyMap &map, int x, int y) {
    return map.confidence[static_cast<std::size_t>(y) * map.widthPx + x];
}

void testGradientLocation() {
    const patternfab::Pattern pattern = makeSingleCirclePattern();
    patternfab::SensorNoiseProfile noise;
    noise.S = 0.0;
    noise.O = 0.01; // small constant noise floor

    const auto map = patternfab::computeUncertaintyMap(pattern, noise);
    check(map.widthPx == 200 && map.heightPx == 200, "map sized to specimen * imaging resolution");

    const double pxPerMm = pattern.params.imagingResolutionPxPerMm;
    // Circle edge: at radius 2mm from center (5,5) along +X, i.e. x=7mm, y=5mm.
    const int edgeX = static_cast<int>(7.0 * pxPerMm);
    const int edgeY = static_cast<int>(5.0 * pxPerMm);
    // Circle interior (uniform black) and far background (uniform white).
    const int interiorX = static_cast<int>(5.0 * pxPerMm);
    const int interiorY = static_cast<int>(5.0 * pxPerMm);
    const int backgroundX = static_cast<int>(0.5 * pxPerMm);
    const int backgroundY = static_cast<int>(0.5 * pxPerMm);

    const double edgeConfidence = confidenceAt(map, edgeX, edgeY);
    const double interiorConfidence = confidenceAt(map, interiorX, interiorY);
    const double backgroundConfidence = confidenceAt(map, backgroundX, backgroundY);

    check(edgeConfidence > interiorConfidence,
          "confidence at circle edge exceeds confidence in uniform interior");
    check(edgeConfidence > backgroundConfidence,
          "confidence at circle edge exceeds confidence in uniform background");
    check(interiorConfidence < 0.01 * edgeConfidence, "uniform interior has near-zero confidence relative to edge");
}

void testNoiseReducesConfidence() {
    const patternfab::Pattern pattern = makeSingleCirclePattern();
    const double pxPerMm = pattern.params.imagingResolutionPxPerMm;
    const int edgeX = static_cast<int>(7.0 * pxPerMm);
    const int edgeY = static_cast<int>(5.0 * pxPerMm);

    patternfab::SensorNoiseProfile lowNoise;
    lowNoise.O = 0.01;
    patternfab::SensorNoiseProfile highNoise;
    highNoise.O = 1.0;

    const auto lowNoiseMap = patternfab::computeUncertaintyMap(pattern, lowNoise);
    const auto highNoiseMap = patternfab::computeUncertaintyMap(pattern, highNoise);

    check(confidenceAt(lowNoiseMap, edgeX, edgeY) > confidenceAt(highNoiseMap, edgeX, edgeY),
          "higher sensor noise reduces confidence at the same location");
}

void testLowConfidenceFraction() {
    patternfab::UncertaintyMap map;
    map.widthPx = 2;
    map.heightPx = 2;
    map.confidence = {0.1, 0.5, 0.9, 2.0};

    check(std::abs(patternfab::lowConfidenceFraction(map, 1.0) - 0.75) < 1e-9,
          "three of four pixels below threshold 1.0");
    check(std::abs(patternfab::lowConfidenceFraction(map, 0.0) - 0.0) < 1e-9, "nothing below zero threshold");

    patternfab::UncertaintyMap empty;
    check(patternfab::lowConfidenceFraction(empty, 1.0) == 0.0, "empty map returns zero, not a crash");
}

void testInvalidParameters() {
    patternfab::Pattern pattern = makeSingleCirclePattern();
    pattern.params.imagingResolutionPxPerMm = 0.0;
    patternfab::SensorNoiseProfile noise;

    bool threw = false;
    try {
        patternfab::computeUncertaintyMap(pattern, noise);
    } catch (const std::runtime_error &) {
        threw = true;
    }
    check(threw, "zero imagingResolutionPxPerMm throws");
}

// --- the displacement noise floor, in pixels -------------------------------
//
// WHY THIS EXISTS. The confidence map above is sound physics reported as a
// dimensionless ratio whose threshold the caller invents, so it has no absolute
// meaning and cannot be compared with anything -- including with what the
// pattern actually achieves once it is fabricated and photographed. DIC's own
// figure is sigma, a displacement NOISE FLOOR in pixels, and it is what SurView
// measures after a run. Reported in the same units by the same definition, a
// design-time number and a measured one can finally be set side by side, and
// the difference between them is the fabrication and imaging penalty.
//
// ⚑ IT IS A BOUND, NOT A PREDICTION. This renders an ideal pattern; a real one
// is printed with real ink on a real substrate and photographed slightly out of
// focus. The measured figure will be worse, and that is the point.
//
// NEGATIVE CHECK (2026-09-03). Each rule removed in turn and the suite re-run.
// All six turned at least one case red:
//
//   gradient magnitude instead of the weaker axis -> ...TakesTheWorseAxis
//   the square root dropped                       -> ...ImprovesWithContrast
//   unestablished reported as zero                -> ...EstablishesNothing
//   summary takes the maximum                     -> ...UsesAPercentile...
//   subset radius not carried on the map          -> ...TravelsWithTheFigure
//   border subsets clipped instead of refused     -> ...WouldRunOffTheImage...
//
// ⚑ The last of those caught NOTHING at first. Clipping a subset at the border
// sums over fewer pixels and so reports a different subset's figure, wearing
// the radius that was asked for -- and every case here passed regardless.
// testASubsetThatWouldRunOffTheImageIsRefused was written afterwards, asserting
// the exact interior count, and is what turns it red.

patternfab::Pattern makeStripePattern(bool vertical) {
    // Bars in one direction only. Every gradient points the same way.
    patternfab::Pattern pattern;
    pattern.params.specimenWidthMm = 10.0;
    pattern.params.specimenHeightMm = 10.0;
    pattern.params.imagingResolutionPxPerMm = 20.0;

    for (int i = 0; i < 10; ++i) {
        patternfab::Primitive bar;
        bar.shape = patternfab::PrimitiveShape::Polygon;
        const double a = i * 1.0;
        const double b = a + 0.5;
        if (vertical) {
            bar.verticesMm = {{a, 0.0}, {b, 0.0}, {b, 10.0}, {a, 10.0}};
        } else {
            bar.verticesMm = {{0.0, a}, {10.0, a}, {10.0, b}, {0.0, b}};
        }
        pattern.primitives.push_back(bar);
    }
    return pattern;
}

patternfab::Pattern makeSpecklePattern() {
    // A crude but genuinely two-dimensional field of dots.
    patternfab::Pattern pattern;
    pattern.params.specimenWidthMm = 10.0;
    pattern.params.specimenHeightMm = 10.0;
    pattern.params.imagingResolutionPxPerMm = 20.0;

    for (int row = 0; row < 12; ++row) {
        for (int col = 0; col < 12; ++col) {
            patternfab::Primitive dot;
            dot.shape = patternfab::PrimitiveShape::Circle;
            // Offset alternate rows so the pattern is not a plain lattice.
            dot.centerXMm = 0.4 + col * 0.8 + (row % 2 ? 0.4 : 0.0);
            dot.centerYMm = 0.4 + row * 0.8;
            dot.radiusXMm = dot.radiusYMm = 0.22;
            pattern.primitives.push_back(dot);
        }
    }
    return pattern;
}

patternfab::SensorNoiseProfile constantNoise(double standardDeviation) {
    patternfab::SensorNoiseProfile noise;
    noise.S = 0.0;
    noise.O = standardDeviation * standardDeviation;
    return noise;
}

void testNoiseFloorIsInPixelsAndImprovesWithContrast() {
    const auto weak = patternfab::computeNoiseFloorMap(
        makeSpecklePattern(), constantNoise(0.02), 16);
    const auto strong = patternfab::computeNoiseFloorMap(
        makeSpecklePattern(), constantNoise(0.005), 16);

    const auto weakSummary = patternfab::summariseNoiseFloor(weak);
    const auto strongSummary = patternfab::summariseNoiseFloor(strong);

    check(weakSummary.establishedCount > 0, "noise floor established somewhere");
    check(strongSummary.typicalPx < weakSummary.typicalPx,
          "less sensor noise must give a finer noise floor");

    // ⚑ Proportional, not merely ordered. sigma scales with the noise standard
    // deviation, so a quarter of the noise is a quarter of the floor. An
    // implementation that forgot the square root would give a sixteenth.
    const double ratio = weakSummary.typicalPx / strongSummary.typicalPx;
    check(std::abs(ratio - 4.0) < 0.2,
          "noise floor must scale linearly with sensor noise standard deviation");
}

void testNoiseFloorTakesTheWorseAxis() {
    // ⚑ THE CASE THAT JUSTIFIES THE WHOLE CHANGE, and it is more damning than
    // "worse". A pattern of bars in one direction has enormous gradient energy
    // across the bars and NONE along them, so it cannot measure displacement in
    // that direction at all -- and sigma, taking the weaker axis, therefore
    // establishes no noise floor anywhere on it. Nothing, not merely a poor
    // number.
    //
    // Measured, on the patterns below: bars establish a floor at 0 of 40000
    // points while a speckle establishes one at 70.6 per cent of them (the rest
    // being the border, where a subset cannot be placed).
    const auto vertical = patternfab::summariseNoiseFloor(
        patternfab::computeNoiseFloorMap(makeStripePattern(true), constantNoise(0.01), 16));
    const auto horizontal = patternfab::summariseNoiseFloor(
        patternfab::computeNoiseFloorMap(makeStripePattern(false), constantNoise(0.01), 16));
    const auto speckle = patternfab::summariseNoiseFloor(
        patternfab::computeNoiseFloorMap(makeSpecklePattern(), constantNoise(0.01), 16));

    check(vertical.establishedCount == 0,
          "bars in one direction must establish no noise floor at all");
    check(horizontal.establishedCount == 0,
          "the same, whichever way the bars run");
    check(speckle.establishedCount > speckle.totalCount / 2,
          "a two-directional pattern must establish a floor over most of itself");

    // ⚑ AND THE OLD METRIC IS FOOLED BY EXACTLY THIS PATTERN, which is why the
    // new one had to be added rather than the old one merely rescaled. The
    // confidence map takes the gradient MAGNITUDE, so it cannot see that all
    // the gradient points one way: it rates bars at about 95 per cent of a
    // proper speckle's score while they measure nothing.
    const auto stripeConfidence =
        patternfab::computeUncertaintyMap(makeStripePattern(true), constantNoise(0.01));
    const auto speckleConfidence =
        patternfab::computeUncertaintyMap(makeSpecklePattern(), constantNoise(0.01));
    const auto meanOf = [](const patternfab::UncertaintyMap &map) {
        double total = 0.0;
        for (double c : map.confidence) {
            total += c;
        }
        return total / static_cast<double>(map.confidence.size());
    };
    check(meanOf(stripeConfidence) > 0.9 * meanOf(speckleConfidence),
          "this case only means something while the confidence map still rates "
          "bars as nearly as good as speckle; if that changes, revisit why");
}

void testALargerSubsetLowersTheFloor() {
    // More pixels in the sum, so a finer floor. The pattern is unchanged; this
    // is the correlation setting, and it is why the figure cannot be quoted
    // without the subset radius it was computed at.
    const auto small = patternfab::summariseNoiseFloor(
        patternfab::computeNoiseFloorMap(makeSpecklePattern(), constantNoise(0.01), 8));
    const auto large = patternfab::summariseNoiseFloor(
        patternfab::computeNoiseFloorMap(makeSpecklePattern(), constantNoise(0.01), 24));

    check(large.typicalPx < small.typicalPx,
          "a larger subset must give a finer noise floor");
}

void testFeaturelessAreaEstablishesNothing() {
    // ⚑ Not a zero. Zero is the FLATTERING reading -- it claims a perfect
    // measurement -- and a blank region is where a pattern is at its worst.
    // The same rule SurView keeps for a measured noise floor, which is written
    // as not-a-number rather than zero wherever it was never established.
    patternfab::Pattern blank;
    blank.params.specimenWidthMm = 10.0;
    blank.params.specimenHeightMm = 10.0;
    blank.params.imagingResolutionPxPerMm = 20.0;   // no primitives at all

    const auto map = patternfab::computeNoiseFloorMap(blank, constantNoise(0.01), 16);
    const auto summary = patternfab::summariseNoiseFloor(map);

    check(summary.establishedCount == 0,
          "a blank specimen must establish no noise floor anywhere");
    for (double sigma : map.sigmaPx) {
        check(std::isnan(sigma), "an unestablished noise floor must be not-a-number");
    }
}

void testSummaryUsesAPercentileNotTheExtreme() {
    // ⚑ A lesson already paid for in SurView: a single bad subset -- one that
    // happens to sit on blank background -- set the headline figure for an
    // otherwise excellent pattern, because the summary took the maximum. The
    // typical figure is a percentile, so one hopeless corner cannot speak for
    // the whole specimen.
    patternfab::Pattern pattern = makeSpecklePattern();
    // Blank out one corner by shrinking the dots there to nothing.
    for (auto &primitive : pattern.primitives) {
        if (primitive.centerXMm < 2.0 && primitive.centerYMm < 2.0) {
            primitive.radiusXMm = primitive.radiusYMm = 0.0;
        }
    }

    const auto map = patternfab::computeNoiseFloorMap(pattern, constantNoise(0.01), 16);
    const auto summary = patternfab::summariseNoiseFloor(map);

    check(summary.establishedCount > 0, "the good part still establishes a floor");
    check(summary.typicalPx < summary.worstPx,
          "the typical figure must not be the worst one");
    check(summary.bestPx <= summary.typicalPx, "best is not worse than typical");
}

void testASubsetThatWouldRunOffTheImageIsRefused() {
    // ⚑ Added because a negative check found NOTHING catching it. Clipping a
    // subset at the border instead of refusing it sums over fewer pixels, so it
    // silently reports a DIFFERENT subset's figure -- and reports it as though
    // it were the one asked for, indistinguishable from an interior value.
    //
    // The property is exact: for a pattern with gradient throughout, a floor is
    // established at precisely the points where a full subset fits, and nowhere
    // else. 200 by 200 at radius 16 leaves 168 by 168, which is 28224.
    const int radius = 16;
    const auto map = patternfab::computeNoiseFloorMap(
        makeSpecklePattern(), constantNoise(0.01), radius);
    const auto summary = patternfab::summariseNoiseFloor(map);

    const int interior = (map.widthPx - 2 * radius) * (map.heightPx - 2 * radius);
    check(summary.establishedCount == interior,
          "a floor must be established exactly where a whole subset fits");
    check(summary.establishedCount < summary.totalCount,
          "the border must not be establishing anything");
}

void testTheSubsetRadiusTravelsWithTheFigure() {
    // A noise floor without the subset it was computed at is not a number
    // anybody can use, so the map carries it rather than leaving a caller to
    // remember.
    const auto map = patternfab::computeNoiseFloorMap(
        makeSpecklePattern(), constantNoise(0.01), 12);
    check(map.subsetRadiusPx == 12, "the map states the subset radius it used");
}

} // namespace

int main() {
    testGradientLocation();
    testNoiseReducesConfidence();
    testLowConfidenceFraction();
    testInvalidParameters();
    testNoiseFloorIsInPixelsAndImprovesWithContrast();
    testNoiseFloorTakesTheWorseAxis();
    testALargerSubsetLowersTheFloor();
    testFeaturelessAreaEstablishesNothing();
    testSummaryUsesAPercentileNotTheExtreme();
    testASubsetThatWouldRunOffTheImageIsRefused();
    testTheSubsetRadiusTravelsWithTheFigure();

    if (failures == 0) {
        std::cout << "OK: all patternfab-core uncertainty engine tests passed" << std::endl;
        return EXIT_SUCCESS;
    }
    std::cerr << failures << " test(s) failed" << std::endl;
    return EXIT_FAILURE;
}
