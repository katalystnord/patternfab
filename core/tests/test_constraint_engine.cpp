#include <patternfab/ConstraintEngine.h>

#include <cmath>
#include <cstdlib>
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

patternfab::Primitive makeCircle(double x, double y, double radius) {
    patternfab::Primitive p;
    p.shape = patternfab::PrimitiveShape::Circle;
    p.centerXMm = x;
    p.centerYMm = y;
    p.radiusXMm = radius;
    p.radiusYMm = radius;
    return p;
}

void testBoundingBoxAndTiling() {
    patternfab::Pattern pattern;
    pattern.params.specimenWidthMm = 10.0;
    pattern.params.specimenHeightMm = 10.0;
    pattern.primitives.push_back(makeCircle(1.0, 1.0, 0.5));
    pattern.primitives.push_back(makeCircle(4.0, 4.0, 0.5));

    const auto bbox = patternfab::computeBoundingBox(pattern);
    check(bbox.minXMm == 0.5, "bbox minX");
    check(bbox.maxXMm == 4.5, "bbox maxX");

    // Pattern only spans 0.5..4.5 (4mm), specimen is 10mm -> requires tiling.
    check(patternfab::patternRequiresTiling(pattern), "small pattern requires tiling");

    // Enlarge the specimen requirement to match the pattern extent exactly.
    pattern.params.specimenWidthMm = 4.0;
    pattern.params.specimenHeightMm = 4.0;
    check(!patternfab::patternRequiresTiling(pattern), "pattern covering specimen does not require tiling");

    // Physical margin: a real speckle field's bounding box is inset from the
    // specimen edge by up to the speckle spacing (discrete dots, no ink past
    // the last one). With a target speckle size set, an inset within one
    // speckle must NOT be flagged as needing tiling...
    patternfab::Pattern inset;
    inset.params.specimenWidthMm = 10.0;
    inset.params.specimenHeightMm = 10.0;
    inset.params.targetSpeckleSizeMm = 0.5;
    inset.primitives.push_back(makeCircle(0.3, 0.3, 0.25)); // extent 0.05..9.95
    inset.primitives.push_back(makeCircle(9.7, 9.7, 0.25)); // (0.1mm short each side)
    check(!patternfab::patternRequiresTiling(inset),
          "field inset within one speckle does not require tiling");

    // ...but leaving an uncovered strip wider than a speckle still does.
    inset.params.specimenWidthMm = 12.0;
    check(patternfab::patternRequiresTiling(inset),
          "field short by more than one speckle requires tiling");
}

void testMinimumFeatureSize() {
    patternfab::Pattern pattern;
    pattern.primitives.push_back(makeCircle(0, 0, 0.1));  // diameter 0.2mm
    pattern.primitives.push_back(makeCircle(1, 1, 0.5));  // diameter 1.0mm

    patternfab::ManufacturingConstraints constraints;
    constraints.minFeatureSizeMm = 0.5;

    const auto violations = patternfab::checkMinimumFeatureSize(pattern, constraints);
    check(violations.size() == 1, "exactly one violation (the 0.2mm circle)");
    if (!violations.empty()) {
        check(violations[0].primitiveIndex == 0, "violation flags primitive 0");
    }

    constraints.minFeatureSizeMm = 0.0;
    check(patternfab::checkMinimumFeatureSize(pattern, constraints).empty(), "zero threshold disables check");
}

void testBleedCompensation() {
    patternfab::Pattern pattern;
    pattern.primitives.push_back(makeCircle(0, 0, 1.0));

    patternfab::ManufacturingConstraints constraints;
    constraints.bleedCompensationMm = 0.3;

    const auto compensated = patternfab::applyBleedCompensation(pattern, constraints);
    check(compensated.primitives[0].radiusXMm == 0.7, "radius reduced by bleed compensation");

    // Clamped at zero, not negative.
    constraints.bleedCompensationMm = 5.0;
    const auto clamped = patternfab::applyBleedCompensation(pattern, constraints);
    check(clamped.primitives[0].radiusXMm == 0.0, "radius clamped at zero");

    // Polygon + nonzero compensation must throw.
    patternfab::Pattern polygonPattern;
    patternfab::Primitive polygon;
    polygon.shape = patternfab::PrimitiveShape::Polygon;
    polygon.verticesMm = {{0, 0}, {1, 0}, {1, 1}};
    polygonPattern.primitives.push_back(polygon);

    bool threw = false;
    try {
        patternfab::applyBleedCompensation(polygonPattern, constraints);
    } catch (const std::runtime_error &) {
        threw = true;
    }
    check(threw, "polygon + nonzero compensation throws");

    // Zero compensation must NOT throw even with a polygon present.
    patternfab::ManufacturingConstraints noCompensation;
    bool didNotThrow = true;
    try {
        patternfab::applyBleedCompensation(polygonPattern, noCompensation);
    } catch (const std::runtime_error &) {
        didNotThrow = false;
    }
    check(didNotThrow, "polygon + zero compensation is a no-op, does not throw");
}

void testStencilBridging() {
    patternfab::Pattern pattern;
    pattern.primitives.push_back(makeCircle(0.0, 0.0, 1.0));
    pattern.primitives.push_back(makeCircle(2.1, 0.0, 1.0)); // gap = 0.1mm
    pattern.primitives.push_back(makeCircle(10.0, 10.0, 1.0)); // far away, gap large

    patternfab::ManufacturingConstraints constraints;
    constraints.minBridgeWidthMm = 0.2;

    const auto violations = patternfab::checkStencilBridging(pattern, constraints);
    check(violations.size() == 1, "exactly one bridging violation");
    if (!violations.empty()) {
        check(violations[0].primitiveIndexA == 0 && violations[0].primitiveIndexB == 1,
              "violation flags primitives 0 and 1");
        check(std::abs(violations[0].gapMm - 0.1) < 1e-9, "gap computed correctly");
    }

    constraints.minBridgeWidthMm = 0.0;
    check(patternfab::checkStencilBridging(pattern, constraints).empty(), "zero threshold disables check");

    // Overlapping primitives (negative gap) must also be flagged.
    patternfab::Pattern overlapping;
    overlapping.primitives.push_back(makeCircle(0.0, 0.0, 1.0));
    overlapping.primitives.push_back(makeCircle(0.5, 0.0, 1.0));
    patternfab::ManufacturingConstraints tightConstraints;
    tightConstraints.minBridgeWidthMm = 0.1;
    const auto overlapViolations = patternfab::checkStencilBridging(overlapping, tightConstraints);
    check(overlapViolations.size() == 1 && overlapViolations[0].gapMm < 0.0,
          "overlapping primitives flagged with negative gap");
}


// Everything below was written to kill mutants that survived the first sweep.
// They cluster at two kinds of place: the exact threshold, which no
// hand-picked example lands on, and the DIRECTION of a subtraction, which a
// symmetric fixture cannot see.

patternfab::Primitive makeEllipse(double x, double y, double rx, double ry) {
    patternfab::Primitive p;
    p.shape = patternfab::PrimitiveShape::Ellipse;
    p.centerXMm = x;
    p.centerYMm = y;
    p.radiusXMm = rx;
    p.radiusYMm = ry;
    return p;
}

void a_feature_exactly_at_the_minimum_is_allowed() {
    // ⚑ "Minimum feature size" is a floor a process can hold, not one it
    // cannot: a 0.15 mm feature on a 0.15 mm process is exactly what the
    // number permits. Reported as a violation, a pattern designed against the
    // spec fails its own spec.
    patternfab::Pattern pattern;
    pattern.params.specimenWidthMm = 10.0;
    pattern.params.specimenHeightMm = 10.0;
    pattern.primitives.push_back(makeCircle(5.0, 5.0, 0.075));   // 0.15 mm across

    patternfab::ManufacturingConstraints constraints;
    constraints.minFeatureSizeMm = 0.15;
    check(patternfab::checkMinimumFeatureSize(pattern, constraints).empty(),
          "a feature exactly at the minimum size is not a violation");

    constraints.minFeatureSizeMm = 0.1500001;
    check(patternfab::checkMinimumFeatureSize(pattern, constraints).size() == 1,
          "a feature a hair below the minimum is");
}

void a_gap_exactly_at_the_minimum_bridge_width_is_allowed() {
    patternfab::Pattern pattern;
    pattern.params.specimenWidthMm = 10.0;
    pattern.params.specimenHeightMm = 10.0;
    // Centres 3 mm apart, radius 1 each: a 1 mm gap of stencil between them.
    pattern.primitives.push_back(makeCircle(3.0, 5.0, 1.0));
    pattern.primitives.push_back(makeCircle(6.0, 5.0, 1.0));

    patternfab::ManufacturingConstraints constraints;
    constraints.minBridgeWidthMm = 1.0;
    check(patternfab::checkStencilBridging(pattern, constraints).empty(),
          "a bridge exactly at the minimum width holds");

    constraints.minBridgeWidthMm = 1.0000001;
    check(patternfab::checkStencilBridging(pattern, constraints).size() == 1,
          "a bridge a hair below it does not");
}

void a_constraint_of_zero_turns_that_check_off() {
    // Zero means "not specified", which is why the guard is <= and not <.
    // Under `<` the checks run against a threshold of zero, and the bridging
    // one then reports every OVERLAP in the pattern, since an overlap is a
    // negative gap and a negative gap is below zero. That is a real report of
    // a real thing, arriving from a constraint nobody set.
    patternfab::Pattern pattern;
    pattern.params.specimenWidthMm = 10.0;
    pattern.params.specimenHeightMm = 10.0;
    pattern.primitives.push_back(makeCircle(5.0, 5.0, 1.0));
    pattern.primitives.push_back(makeCircle(5.5, 5.0, 1.0));   // overlapping

    patternfab::ManufacturingConstraints unset;
    unset.minFeatureSizeMm = 0.0;
    unset.minBridgeWidthMm = 0.0;

    check(patternfab::checkMinimumFeatureSize(pattern, unset).empty(),
          "a minimum feature size of zero asks nothing of the pattern");
    check(patternfab::checkStencilBridging(pattern, unset).empty(),
          "a minimum bridge width of zero asks nothing of it either, overlaps included");
}

void a_pattern_is_measured_by_its_extent_not_by_where_it_sits() {
    // ⚑ The fixture sits away from the origin on purpose. With the width taken
    // as maxX PLUS minX rather than minus, a pattern nowhere near covering the
    // specimen reports itself as covering it, and the periodicity warning that
    // a tiled speckle field needs never fires. Centred on the origin, or
    // starting at it, the two arithmetics agree.
    // ⚑ ONE AXIS AT A TIME, because the two are joined by an OR and a fixture
    // short of covering BOTH is satisfied by either one of them. The first
    // version of this case failed to catch the mutant it was written for: the
    // height was uncovered too, so the OR reached the right answer through the
    // half that was still correct.
    patternfab::Pattern narrow;
    narrow.params.specimenWidthMm = 30.0;
    narrow.params.specimenHeightMm = 20.0;
    narrow.params.targetSpeckleSizeMm = 0.5;
    // 19 wide and the full 20 tall, hard against the far edge: only the width
    // falls short, and only if width means an extent rather than a position.
    narrow.primitives.push_back(makeCircle(10.0, 0.0, 0.0001));
    narrow.primitives.push_back(makeCircle(29.0, 20.0, 0.0001));
    check(patternfab::patternRequiresTiling(narrow),
          "a pattern that covers the height but not the width needs tiling");

    patternfab::Pattern short_;
    short_.params.specimenWidthMm = 30.0;
    short_.params.specimenHeightMm = 20.0;
    short_.params.targetSpeckleSizeMm = 0.5;
    short_.primitives.push_back(makeCircle(0.0, 10.0, 0.0001));
    short_.primitives.push_back(makeCircle(30.0, 14.0, 0.0001));
    check(patternfab::patternRequiresTiling(short_),
          "and one that covers the width but not the height needs it too");

    patternfab::Pattern covering;
    covering.params.specimenWidthMm = 30.0;
    covering.params.specimenHeightMm = 20.0;
    covering.params.targetSpeckleSizeMm = 0.5;
    covering.primitives.push_back(makeCircle(0.0, 0.0, 0.0001));
    covering.primitives.push_back(makeCircle(30.0, 20.0, 0.0001));
    check(!patternfab::patternRequiresTiling(covering),
          "a pattern reaching both edges is applied once, with no periodicity to warn about");
}

void a_bleed_allowance_is_taken_off_both_radii() {
    // Both, and independently: a compensation applied to one axis alone makes
    // every ellipse in the pattern a different shape than the one designed,
    // and a circle is the one primitive that cannot show it.
    patternfab::Pattern pattern;
    pattern.params.specimenWidthMm = 10.0;
    pattern.params.specimenHeightMm = 10.0;
    pattern.primitives.push_back(makeEllipse(5.0, 5.0, 1.0, 0.4));

    patternfab::ManufacturingConstraints constraints;
    constraints.bleedCompensationMm = 0.1;
    const patternfab::Pattern shrunk = patternfab::applyBleedCompensation(pattern, constraints);

    check(std::abs(shrunk.primitives[0].radiusXMm - 0.9) < 1e-12,
          "the long radius loses the bleed allowance");
    check(std::abs(shrunk.primitives[0].radiusYMm - 0.3) < 1e-12,
          "and so does the short one");

    // Never through zero into a negative radius, which is a shape no cutter
    // can be given.
    constraints.bleedCompensationMm = 5.0;
    const patternfab::Pattern gone = patternfab::applyBleedCompensation(pattern, constraints);
    check(gone.primitives[0].radiusXMm == 0.0 && gone.primitives[0].radiusYMm == 0.0,
          "a bleed larger than the speckle leaves nothing rather than a negative radius");
}

void the_gap_between_two_ellipses_is_measured_along_the_line_between_them() {
    // ⚑ The one fixture that can see the direction of dx and dy. Two ellipses side by side, long
    // axis horizontal: each reaches 2 mm towards the other, so centres 5 mm
    // apart leave a 1 mm gap. Point either direction the wrong way and each
    // reaches its SHORT radius instead, which reads as a 3 mm gap -- three
    // times the truth, and comfortably clear of a bridge width it actually
    // violates. Two circles cannot show this at all.
    //
    // It does NOT see the sign of the second primitive's own direction, and
    // nothing can: an axis-aligned ellipse's reach depends on |dirX| and
    // |dirY| only, and a polygon's not at all, so passing -ux for ux is
    // equivalent for every shape this tool has. Recorded rather than hunted.
    patternfab::Pattern pattern;
    pattern.params.specimenWidthMm = 20.0;
    pattern.params.specimenHeightMm = 20.0;
    pattern.primitives.push_back(makeEllipse(5.0, 10.0, 2.0, 0.5));
    pattern.primitives.push_back(makeEllipse(10.0, 10.0, 2.0, 0.5));

    patternfab::ManufacturingConstraints constraints;
    constraints.minBridgeWidthMm = 2.0;
    const auto violations = patternfab::checkStencilBridging(pattern, constraints);
    check(violations.size() == 1, "a 1 mm gap violates a 2 mm minimum bridge width");
    if (violations.size() == 1) {
        check(std::abs(violations[0].gapMm - 1.0) < 1e-9,
              "and the gap is measured along the line between the two, at 1 mm");
    }
}
} // namespace

int main() {
    testBoundingBoxAndTiling();
    testMinimumFeatureSize();
    testBleedCompensation();
    testStencilBridging();
    a_feature_exactly_at_the_minimum_is_allowed();
    a_gap_exactly_at_the_minimum_bridge_width_is_allowed();
    a_constraint_of_zero_turns_that_check_off();
    a_pattern_is_measured_by_its_extent_not_by_where_it_sits();
    a_bleed_allowance_is_taken_off_both_radii();
    the_gap_between_two_ellipses_is_measured_along_the_line_between_them();

    if (failures == 0) {
        std::cout << "OK: all patternfab-core constraint engine tests passed" << std::endl;
        return EXIT_SUCCESS;
    }
    std::cerr << failures << " test(s) failed" << std::endl;
    return EXIT_FAILURE;
}
