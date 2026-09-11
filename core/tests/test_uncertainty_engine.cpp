#include <patternfab/UncertaintyEngine.h>

#include <cmath>
#include <cstdlib>
#include <string>
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

// Returns the message, or an empty string if nothing was thrown.
std::string refusalFor(const patternfab::Pattern &pattern) {
    patternfab::SensorNoiseProfile noise;
    try {
        patternfab::computeUncertaintyMap(pattern, noise);
    } catch (const std::runtime_error &error) {
        return error.what();
    }
    return std::string();
}

// ⚑ WHICH REFUSAL A READER GETS IS THE POINT, and the case above cannot see it:
// it asks only whether SOMETHING was thrown. A specimen of zero width slips
// past the parameter guard when its test is narrowed to "< 0", renders zero
// pixels wide, and is caught by the NEXT guard instead - which tells the user
// the imaging resolution is too low for the specimen, about a specimen that has
// no width at all. They would go and change a number that was never the
// problem.
//
// Each parameter on its own, at zero and below, because the guard is three
// conditions joined by OR and joining them with AND lets any single bad value
// through while all three cases still "throw".
void each_bad_parameter_is_refused_for_being_what_it_is() {
    const double zero = 0.0;
    const double negative = -1.0;

    for (const double bad : {zero, negative}) {
        patternfab::Pattern width = makeSingleCirclePattern();
        width.params.specimenWidthMm = bad;
        const std::string widthSaid = refusalFor(width);
        check(widthSaid.find("must be positive") != std::string::npos,
              "a specimen " + std::to_string(bad) + " mm wide was refused for "
              "the wrong reason: " + widthSaid);

        patternfab::Pattern height = makeSingleCirclePattern();
        height.params.specimenHeightMm = bad;
        const std::string heightSaid = refusalFor(height);
        check(heightSaid.find("must be positive") != std::string::npos,
              "a specimen " + std::to_string(bad) + " mm tall was refused for "
              "the wrong reason: " + heightSaid);

        patternfab::Pattern resolution = makeSingleCirclePattern();
        resolution.params.imagingResolutionPxPerMm = bad;
        const std::string resolutionSaid = refusalFor(resolution);
        check(resolutionSaid.find("must be positive") != std::string::npos,
              "an imaging resolution of " + std::to_string(bad) + " was refused "
              "for the wrong reason: " + resolutionSaid);
    }
}

// The other guard, and its boundary. A specimen that renders two pixels across
// is the smallest one a difference can be taken over at all, and it is measured
// rather than refused; one pixel is not, and says so in its own words.
void a_specimen_of_two_pixels_is_the_smallest_there_is() {
    patternfab::Pattern twoPx = makeSingleCirclePattern();
    twoPx.params.imagingResolutionPxPerMm = 0.2;   // 10 mm -> exactly 2 px
    check(refusalFor(twoPx).empty(),
          "a specimen rendering two pixels across was refused: " + refusalFor(twoPx));

    // ⚑ One axis at a time, because the guard is two conditions joined by OR
    // and joining them with AND refuses only a specimen too small in BOTH.
    patternfab::Pattern narrow = makeSingleCirclePattern();
    narrow.params.specimenWidthMm = 0.5;
    narrow.params.specimenHeightMm = 100.0;
    narrow.params.imagingResolutionPxPerMm = 2.0;   // 1 px by 200 px
    const std::string narrowSaid = refusalFor(narrow);
    check(narrowSaid.find("too low") != std::string::npos,
          "a specimen one pixel wide was not refused for being too small: "
              + narrowSaid);

    patternfab::Pattern flat = makeSingleCirclePattern();
    flat.params.specimenWidthMm = 100.0;
    flat.params.specimenHeightMm = 0.5;
    flat.params.imagingResolutionPxPerMm = 2.0;     // 200 px by 1 px
    const std::string flatSaid = refusalFor(flat);
    check(flatSaid.find("too low") != std::string::npos,
          "a specimen one pixel tall was not refused for being too small: "
              + flatSaid);
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

void testTheDescriptionSaysWhatTheNumberIsAndIsNot() {
    // The wording lives in core rather than in the widget, for the reason every
    // other rule here does: it is where a test can reach it. A number on a
    // screen is trusted, and this one needs three qualifications to be read
    // correctly at all.
    const auto map = patternfab::computeNoiseFloorMap(
        makeSpecklePattern(), constantNoise(0.01), 16);
    const std::string text =
        patternfab::describeNoiseFloor(map, patternfab::summariseNoiseFloor(map));

    const auto says = [&text](const char *phrase) {
        return text.find(phrase) != std::string::npos;
    };

    check(says("16"), "the description must state the subset radius it used; "
                      "the same pattern gives a different figure at another one");
    check(says("bound"), "it must say it is a bound rather than a prediction");
    // ⚑ Two numbers on one screen reading opposite ways is a trap SurView
    // already documented: the confidence map beside it is HIGHER-is-better,
    // and this one is lower. A reader carrying the habit across from one to
    // the other reads a bad pattern as a good one.
    check(says("lower is better"), "it must say which direction is better, "
                                   "because the figure beside it reads the other way");

    // ⚑ And it must NOT quote the worst value. On a real pattern that is a
    // subset lying almost entirely on blank background, about 945000 px, which
    // is arithmetic rather than information and describes a good pattern as a
    // disaster.
    check(!says("945"), "the description must not headline the worst subset");
}

void testTheDescriptionExplainsAnEmptyResultRatherThanShowingABlank() {
    // ⚑ The case that would otherwise reach a user as an empty box. A pattern
    // with no gradient in one direction establishes nothing anywhere, and
    // "nothing" needs a reason attached or it reads as a broken tool rather
    // than as a verdict on the pattern.
    const auto map = patternfab::computeNoiseFloorMap(
        makeStripePattern(true), constantNoise(0.01), 16);
    const auto summary = patternfab::summariseNoiseFloor(map);
    check(summary.establishedCount == 0, "the fixture establishes nothing");

    const std::string text = patternfab::describeNoiseFloor(map, summary);
    check(!text.empty(), "an empty result must still say something");
    check(text.find("direction") != std::string::npos,
          "it must say WHY nothing was established, which is that the pattern "
          "has no structure in one direction");
}

void testTheSubsetRadiusTravelsWithTheFigure() {
    // A noise floor without the subset it was computed at is not a number
    // anybody can use, so the map carries it rather than leaving a caller to
    // remember.
    const auto map = patternfab::computeNoiseFloorMap(
        makeSpecklePattern(), constantNoise(0.01), 12);
    check(map.subsetRadiusPx == 12, "the map states the subset radius it used");
}


void the_typical_figure_is_the_ninety_fifth_percentile_exactly() {
    // ⚑ Written against a map built by hand rather than rendered, because the
    // index arithmetic is what is under test and a rendered pattern cannot pin
    // it: `0.95 * (n - 1)` off by one is invisible in a field of similar
    // values, which is every real one. A hundred distinct values make it exact.
    //
    // Survived the first mutation sweep three ways -- `- 1` to `- 0`, to `- 2`,
    // and to `+ 1` -- with nothing to notice any of them.
    patternfab::NoiseFloorMap map;
    map.widthPx = 10;
    map.heightPx = 10;
    map.subsetRadiusPx = 16;
    map.sigmaPx.resize(100);
    for (int i = 0; i < 100; ++i) {
        map.sigmaPx[static_cast<std::size_t>(i)] = i + 1.0;
    }

    const auto summary = patternfab::summariseNoiseFloor(map);
    check(summary.establishedCount == 100, "every value counted");
    check(summary.bestPx == 1.0, "the best figure is the smallest");
    check(summary.worstPx == 100.0, "the worst figure is the largest");
    // llround(0.95 * 99) = 94, and the 95th of a hundred sorted values.
    check(summary.typicalPx == 95.0,
          "the typical figure is the value 95 per cent of established points beat");
}

void a_confidence_sitting_exactly_on_the_threshold_is_not_below_it() {
    // The boundary, which is the only place the comparison can be wrong and the
    // only place no rendered pattern will reliably land. A pixel AT the level
    // asked for meets it; counting it as low makes the share depend on the last
    // bit of a double.
    patternfab::UncertaintyMap map;
    map.widthPx = 2;
    map.heightPx = 1;
    map.confidence = {1.0, 2.0};

    check(patternfab::lowConfidenceFraction(map, 1.0) == 0.0,
          "a confidence equal to the threshold is not below it");
    check(patternfab::lowConfidenceFraction(map, 2.0) == 0.5,
          "a confidence under the threshold is below it");
}
} // namespace


// ⚑ THE GRADIENT IS CENTRAL, AND A CENTRAL DIFFERENCE HAS NO PREFERRED
// DIRECTION. Nothing checked that. The sweep of 2026-09-09 left mutants in all
// four of its terms - I(x+1), I(x-1), I(x,y+1), I(x,y-1) - and turning any one
// of them into the pixel itself makes a ONE-SIDED difference, halved: a
// confidence map that leans, everywhere, in the axis it was broken in. Every
// figure downstream is built from these numbers, and a lean of that kind looks
// like an ordinary map of an ordinary pattern.
//
// A circle centred in the specimen renders symmetrically about the half-pixel
// line between columns 99 and 100 (measured, not assumed: the asymmetry there
// is exactly zero, while about any neighbouring line it is 5.0). The physics
// has no preferred direction either, so the confidence map must carry that
// symmetry through - in BOTH axes, since each is computed by its own line of
// code and a rule about two axes needs to ask about two.
void the_confidence_map_leans_in_neither_direction() {
    const patternfab::Pattern pattern = makeSingleCirclePattern();
    patternfab::SensorNoiseProfile noise;
    noise.S = 0.0;
    noise.O = 0.01;

    const auto map = patternfab::computeUncertaintyMap(pattern, noise);
    check(map.widthPx == 200 && map.heightPx == 200,
          "the fixture no longer renders 200x200, so the mirror line has moved");

    // ⚑ ALL THE WAY TO THE EDGE, not just past the circle. The sampler clamps
    // a coordinate to the last row and column, and that clamp is symmetric:
    // pulled in by one pixel on the high side alone, the map loses its last
    // column and leans - which nothing sees if the walk stops in the middle of
    // the specimen. The circle's own edge is 40 px from the centre, so a walk
    // of 45 covers the gradient; a walk to 99 covers the clamp as well.
    // The sampler's own clamp is covered separately, by the case below: it
    // needs contrast AT the border, which this fixture has not got.
    double worstAcross = 0.0;
    double worstDown = 0.0;
    for (int d = 0; d <= 99; ++d) {
        worstAcross = std::max(worstAcross,
                               std::fabs(confidenceAt(map, 100 + d, 100)
                                         - confidenceAt(map, 99 - d, 100)));
        worstDown = std::max(worstDown,
                             std::fabs(confidenceAt(map, 100, 100 + d)
                                       - confidenceAt(map, 100, 99 - d)));
    }

    check(worstAcross < 1e-12,
          "the confidence map is not symmetric across the specimen: the "
          "horizontal gradient leans to one side, worst "
              + std::to_string(worstAcross));
    check(worstDown < 1e-12,
          "the confidence map is not symmetric down the specimen: the vertical "
          "gradient leans to one side, worst " + std::to_string(worstDown));
}



// ⚑ AND THE SAMPLER'S CLAMP, which the case above cannot reach. Reading outside
// the image clamps to the last row and column, and that clamp is symmetric.
// Pulled in by one pixel on the high side alone, the last column is sampled as
// though it were its neighbour, and a specimen speckled to its edges - the
// ordinary case - loses the gradient there. The fixture above cannot see it:
// its circle sits mid-specimen and the borders are blank, so there is nothing
// out there to sample wrongly.
//
// ⚑ IT HAS TO BE A PIXEL-ALIGNED SHAPE, and that cost an hour to learn. Two
// CIRCLES straddling the borders do not render as mirror images: the map came
// out asymmetric by 0.0196 under correct code, which is exactly half of one
// grey level in 255, because Qt antialiases a circle centred on pixel 0
// differently from one centred on pixel 200. Axis-aligned bars have their edges
// on exact pixel boundaries, so the rasteriser has nothing to round
// asymmetrically, and the measured asymmetry is then exactly zero.
void the_clamp_at_the_border_samples_the_border_itself() {
    patternfab::Pattern pattern;
    pattern.params.specimenWidthMm = 10.0;
    pattern.params.specimenHeightMm = 10.0;
    pattern.params.imagingResolutionPxPerMm = 20.0;

    // ⚑ A bar against each border, mirrored about the same half-pixel line the
    // case above uses - and stopping ONE COLUMN SHORT of it, which is the whole
    // point. A bar that runs past the edge leaves the border region uniformly
    // dark, and a clamp reading one pixel in then samples dark where it should
    // have sampled dark: no difference at all. Stopped short, the outermost
    // column is light against a dark neighbour, and the gradient the clamp
    // decides is exactly the one being asked about. px 1 to 29, and 171 to 199.
    // ⚑ AND AGAINST ALL FOUR BORDERS, because the clamp is two lines of code -
    // one per axis - and a fixture with contrast only at the left and right
    // leaves the vertical one asking nothing. A rule about two axes has to put
    // something on both.
    for (const std::pair<double, double> &span :
         {std::make_pair(0.05, 1.45), std::make_pair(8.55, 9.95)}) {
        patternfab::Primitive upright;
        upright.shape = patternfab::PrimitiveShape::Polygon;
        upright.verticesMm = {{span.first, 3.0}, {span.second, 3.0},
                              {span.second, 7.0}, {span.first, 7.0}};
        pattern.primitives.push_back(upright);

        patternfab::Primitive lying;
        lying.shape = patternfab::PrimitiveShape::Polygon;
        lying.verticesMm = {{3.0, span.first}, {7.0, span.first},
                            {7.0, span.second}, {3.0, span.second}};
        pattern.primitives.push_back(lying);
    }

    patternfab::SensorNoiseProfile noise;
    noise.S = 0.0;
    noise.O = 0.01;
    const auto map = patternfab::computeUncertaintyMap(pattern, noise);

    // There is real contrast in the last column, or the case asks nothing at
    // all -- which is precisely how the clamp went uncovered in the first place.
    double edgeConfidence = 0.0;
    for (int y = 0; y < map.heightPx; ++y)
        edgeConfidence = std::max(edgeConfidence, confidenceAt(map, map.widthPx - 1, y));
    check(edgeConfidence > 0.0,
          "the border carries no contrast, so this case cannot see the clamp");

    double worstAcross = 0.0;
    double worstDown = 0.0;
    for (int d = 0; d <= 99; ++d) {
        for (int other = 90; other <= 110; ++other) {
            worstAcross = std::max(worstAcross,
                                   std::fabs(confidenceAt(map, 100 + d, other)
                                             - confidenceAt(map, 99 - d, other)));
            worstDown = std::max(worstDown,
                                 std::fabs(confidenceAt(map, other, 100 + d)
                                           - confidenceAt(map, other, 99 - d)));
        }
    }
    check(worstAcross < 1e-12,
          "a specimen speckled to both side borders reads differently at one of "
          "them: the horizontal clamp is not symmetric, worst "
              + std::to_string(worstAcross));
    check(worstDown < 1e-12,
          "a specimen speckled to top and bottom reads differently at one of "
          "them: the vertical clamp is not symmetric, worst "
              + std::to_string(worstDown));
}


// ⚑ THE SUBSET'S PIXEL COUNT, WHICH SCALES EVERY FLOOR THE TOOL REPORTS. The
// mean noise variance is a sum over the subset divided by its area, written as
// (2r + 1) squared, and three mutants sat on that expression: 2r, 2r + 2, and
// 2r - 1. Each multiplies every sigma by a constant - 1.5 at radius 1, and
// still 3 per cent at radius 16 - and a floor wrong by a constant factor is
// exactly the kind of wrong that looks right: the map's shape, its percentile
// summary and its comparison against a measured run all survive it unchanged.
//
// THE ANSWER HERE IS DERIVED FROM THE PATTERN, NOT FROM THE CODE. A 1 mm square
// at 20 px/mm is a 20 px dot. Its two vertical edges are 20 px tall, and a
// central difference across a step from white to black gives 0.5 at each of the
// two columns either side of an edge, so
//
//     sum gx^2 = 2 edges * 2 columns * 20 rows * 0.5^2 = 20
//
// and the same down the other axis, so the weaker axis is 20 too. The sensor
// noise is constant (S = 0), so the subset's mean variance is exactly O,
// whatever the subset's area - and
//
//     sigma = sqrt(2 * O / 20) = sqrt(0.001) = 0.0316227766...
//
// ⚑ AND IT MUST NOT MOVE WITH THE RADIUS. All the gradient energy is inside the
// dot, so a larger subset adds only blank pixels: it adds nothing to the sum
// and nothing to the mean of a constant. A pixel count computed wrongly does
// depend on the radius, so the two checks catch it from opposite sides - one by
// its value, one by its behaviour.
void the_floor_of_an_enclosed_dot_is_the_one_its_edges_give() {
    patternfab::Pattern pattern;
    pattern.params.specimenWidthMm = 10.0;
    pattern.params.specimenHeightMm = 10.0;
    pattern.params.imagingResolutionPxPerMm = 20.0;

    patternfab::Primitive dot;
    dot.shape = patternfab::PrimitiveShape::Polygon;
    dot.verticesMm = {{4.5, 4.5}, {5.5, 4.5}, {5.5, 5.5}, {4.5, 5.5}};
    pattern.primitives.push_back(dot);

    patternfab::SensorNoiseProfile noise;
    noise.S = 0.0;
    noise.O = 0.01;

    const double edgeLengthPx = 20.0;
    const double expected = std::sqrt(2.0 * noise.O / edgeLengthPx);

    double first = 0.0;
    for (const int radius : {16, 20, 24}) {
        const auto map = patternfab::computeNoiseFloorMap(pattern, noise, radius);
        const double sigma = map.sigmaPx[static_cast<std::size_t>(100) * map.widthPx + 100];

        check(std::fabs(sigma - expected) < 1e-9,
              "a 20 px dot under constant noise gives a floor its own edges "
              "decide: expected " + std::to_string(expected) + ", got "
                  + std::to_string(sigma) + " at radius "
                  + std::to_string(radius));

        if (first == 0.0)
            first = sigma;
        check(std::fabs(sigma - first) < 1e-12,
              "the floor moved when the subset grew, though the pattern put no "
              "gradient in the pixels that were added: " + std::to_string(sigma)
                  + " against " + std::to_string(first));
    }
}

int main() {
    testGradientLocation();
    testNoiseReducesConfidence();
    testLowConfidenceFraction();
    testInvalidParameters();
    each_bad_parameter_is_refused_for_being_what_it_is();
    a_specimen_of_two_pixels_is_the_smallest_there_is();
    the_floor_of_an_enclosed_dot_is_the_one_its_edges_give();
    testNoiseFloorIsInPixelsAndImprovesWithContrast();
    testNoiseFloorTakesTheWorseAxis();
    testALargerSubsetLowersTheFloor();
    testFeaturelessAreaEstablishesNothing();
    testSummaryUsesAPercentileNotTheExtreme();
    testASubsetThatWouldRunOffTheImageIsRefused();
    testTheDescriptionSaysWhatTheNumberIsAndIsNot();
    testTheDescriptionExplainsAnEmptyResultRatherThanShowingABlank();
    testTheSubsetRadiusTravelsWithTheFigure();
    the_typical_figure_is_the_ninety_fifth_percentile_exactly();
    a_confidence_sitting_exactly_on_the_threshold_is_not_below_it();
    the_confidence_map_leans_in_neither_direction();
    the_clamp_at_the_border_samples_the_border_itself();

    if (failures == 0) {
        std::cout << "OK: all patternfab-core uncertainty engine tests passed" << std::endl;
        return EXIT_SUCCESS;
    }
    std::cerr << failures << " test(s) failed" << std::endl;
    return EXIT_FAILURE;
}
