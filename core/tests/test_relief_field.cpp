// Which points a speckle covers, and how far it rises there.
//
// Written after the first mutation sweep put sixteen survivors in these two
// functions: the crossing test could have been inverted, its wrap-around edge
// dropped, its edge interpolation run backwards, and the STL suite would have
// stayed green while the stamp printed the wrong shape. They were unreachable
// at the time, in an anonymous namespace inside StlExport.cpp.
//
// ⚑ The cases are chosen so that a WRONG polygon test cannot pass them. A
// square alone is nearly useless here: almost any broken crossing test still
// answers a square correctly, because a square is its own bounding box. The
// concave notch and the wrap-around edge are what carry the weight.
//
// Negative checks, each applied to ReliefField.h and the named case watched go
// red:
//   inside seeded true          every answer inverts; the notch case reports
//                               the specimen covered where it is not.
//   j = n - 1 seeded n - 2      the closing edge is skipped and one crossing is
//                               lost. Only the L noticed, and only in its
//                               upright: the square and the triangle both
//                               still answered correctly, which is why the
//                               concave case is here.
//   (yi > y) != (yj > y) -> ==  every edge counts except the ones that cross,
//                               and nothing is ever inside.
//   the edge interpolation's
//     (y - yi) -> (y + yi)      the crossing lands at the wrong x, which only
//                               the slanted-edge case can see.
//   normalizedDistSq > 1.0
//     -> >=                     EQUIVALENT, and not chased: exactly on the rim
//                               the dome is sqrt(1 - 1) = 0, which is the same
//                               height the refusal returns.
//
// Two more mutants survive here on purpose, both equivalent by an argument
// rather than by not having found the input yet, so that nobody spends an
// evening hunting them (measured 2026-09-09: 20 of 25 viable killed):
//
//   (yi > y) -> (yi >= y)       A vertex sitting exactly on the scanline has
//                               two edges, and widening the comparison makes
//                               BOTH of them cross where neither did. Two
//                               crossings cancel in a parity count, so no
//                               polygon and no point can tell the difference.
//                               This robustness is why the convention is `>`.
//   x < xint -> x > xint        Counts the crossings to the RIGHT of the point
//                               instead of to the left. A closed ring puts an
//                               even number of crossings on any scanline, so
//                               the two counts always share a parity.
//
// And a third, added 2026-09-11 from the fresh sweep:
//
//   normalizedDistSq > 1.0      The dome's own rim. A point exactly ON it has
//     -> >= 1.0                 a normalised distance of 1, and the branch the
//                               guard skips would compute
//                               bumpHeight * sqrt(1 - 1), which is zero - the
//                               same zero the guard returns. The two paths meet
//                               exactly at the boundary, which is what makes a
//                               hemispherical cap continuous with the plate it
//                               sits on, so no point can tell them apart.

#include "ReliefField.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

int failures = 0;

void check(bool condition, const std::string &message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << std::endl;
        ++failures;
    }
}

using Ring = std::vector<std::pair<double, double>>;

void a_point_is_inside_a_square_only_where_the_square_is() {
    const Ring square{{0.0, 0.0}, {10.0, 0.0}, {10.0, 10.0}, {0.0, 10.0}};

    check(patternfab::pointInPolygon(5.0, 5.0, square), "the middle is inside");
    check(!patternfab::pointInPolygon(-1.0, 5.0, square), "left of it is outside");
    check(!patternfab::pointInPolygon(11.0, 5.0, square), "right of it is outside");
    check(!patternfab::pointInPolygon(5.0, -1.0, square), "below it is outside");
    check(!patternfab::pointInPolygon(5.0, 11.0, square), "above it is outside");
    check(!patternfab::pointInPolygon(5.0, 5.0, Ring{}), "a ring with no vertices encloses nothing");
}

void the_notch_of_a_concave_shape_is_outside_it() {
    // An L, drawn anticlockwise. Its bounding box is the full 10 by 10 square,
    // so a test that answers with the box alone -- which several of the
    // surviving mutants amount to -- calls the notch covered.
    const Ring ell{{0.0, 0.0}, {10.0, 0.0}, {10.0, 4.0},
                   {4.0, 4.0}, {4.0, 10.0}, {0.0, 10.0}};

    check(patternfab::pointInPolygon(2.0, 2.0, ell), "the corner of the L is inside");
    check(patternfab::pointInPolygon(8.0, 2.0, ell), "the foot of the L is inside");
    check(patternfab::pointInPolygon(2.0, 8.0, ell), "the upright of the L is inside");
    check(!patternfab::pointInPolygon(8.0, 8.0, ell),
          "the notch is outside the shape even though it is inside its bounding box");
}

void the_edge_that_closes_the_ring_counts_like_any_other() {
    // ⚑ The ring is OPEN: the edge from the last vertex back to the first is
    // walked by the j = n - 1 seed and by nothing else. Seeded wrong, this
    // triangle loses exactly one crossing and reports itself inside out.
    const Ring triangle{{0.0, 0.0}, {10.0, 0.0}, {0.0, 10.0}};

    check(patternfab::pointInPolygon(1.0, 1.0, triangle),
          "a point well inside the triangle is inside it");
    check(!patternfab::pointInPolygon(9.0, 9.0, triangle),
          "a point beyond the hypotenuse is outside, which only the closing edge can decide");
}

void a_slanted_edge_is_crossed_where_it_actually_lies() {
    // The hypotenuse runs from (0, 10) to (10, 0), so at y = 5 it sits at
    // x = 5. Points either side of that are the only thing that can catch an
    // interpolation computed with the wrong sign or the wrong difference.
    const Ring triangle{{0.0, 0.0}, {10.0, 0.0}, {0.0, 10.0}};

    check(patternfab::pointInPolygon(4.9, 5.0, triangle),
          "just inside the slanted edge is inside");
    check(!patternfab::pointInPolygon(5.1, 5.0, triangle),
          "just outside the slanted edge is outside");
}

patternfab::Primitive makeCircle(double cx, double cy, double r) {
    patternfab::Primitive circle;
    circle.shape = patternfab::PrimitiveShape::Circle;
    circle.centerXMm = cx;
    circle.centerYMm = cy;
    circle.radiusXMm = r;
    circle.radiusYMm = r;
    return circle;
}

void a_dome_is_tallest_at_its_centre_and_meets_the_plate_at_its_rim() {
    const patternfab::Primitive circle = makeCircle(5.0, 5.0, 2.0);
    const double bump = 0.8;

    check(std::abs(patternfab::bumpHeightAt(5.0, 5.0, circle, bump) - bump) < 1e-12,
          "the dome reaches the full bump height at its centre");
    check(patternfab::bumpHeightAt(7.0, 5.0, circle, bump) == 0.0,
          "the dome meets the base plate exactly at its rim");
    check(patternfab::bumpHeightAt(7.5, 5.0, circle, bump) == 0.0,
          "there is no height outside the speckle at all");

    // Hemispherical, not conical: half way out the height is sqrt(1 - 1/4),
    // which a linear cap would put at 1/2.
    const double half = patternfab::bumpHeightAt(6.0, 5.0, circle, bump);
    check(std::abs(half - bump * std::sqrt(0.75)) < 1e-12,
          "the dome is a hemispherical cap rather than a cone");
}

void an_ellipse_is_measured_against_each_of_its_own_radii() {
    patternfab::Primitive ellipse = makeCircle(5.0, 5.0, 2.0);
    ellipse.shape = patternfab::PrimitiveShape::Ellipse;
    ellipse.radiusXMm = 4.0;
    ellipse.radiusYMm = 1.0;

    check(patternfab::bumpHeightAt(8.0, 5.0, ellipse, 1.0) > 0.0,
          "three units out along the long axis is still on the speckle");
    check(patternfab::bumpHeightAt(5.0, 7.0, ellipse, 1.0) == 0.0,
          "two units out along the short axis is off it, which a single radius could not tell");
}

void a_speckle_with_no_size_raises_nothing() {
    // Both radii are refused independently: a primitive flat in one direction
    // is not a speckle, and dividing by it is how a not-a-number reaches the
    // mesh.
    patternfab::Primitive flat = makeCircle(5.0, 5.0, 2.0);
    flat.radiusXMm = 0.0;
    check(patternfab::bumpHeightAt(5.0, 5.0, flat, 1.0) == 0.0,
          "a speckle with no width raises nothing");

    // ⚑ Exactly zero, not merely negative, and on each radius in turn. A
    // radius of -1 is refused by any comparison that gets the DIRECTION right,
    // so it cannot tell `<= 0` from `< 0` -- and `< 0` divides by zero. That
    // survived the sweep on the second radius while the first was covered.
    patternfab::Primitive thin = makeCircle(5.0, 5.0, 2.0);
    thin.radiusYMm = 0.0;
    check(patternfab::bumpHeightAt(5.0, 5.0, thin, 1.0) == 0.0,
          "a speckle with no height raises nothing");

    patternfab::Primitive backwards = makeCircle(5.0, 5.0, 2.0);
    backwards.radiusYMm = -1.0;
    check(patternfab::bumpHeightAt(5.0, 5.0, backwards, 1.0) == 0.0,
          "a speckle with a negative radius raises nothing");
}

void a_polygon_speckle_has_a_flat_top() {
    patternfab::Primitive polygon;
    polygon.shape = patternfab::PrimitiveShape::Polygon;
    polygon.verticesMm = Ring{{0.0, 0.0}, {10.0, 0.0}, {10.0, 10.0}, {0.0, 10.0}};

    check(patternfab::bumpHeightAt(5.0, 5.0, polygon, 0.8) == 0.8,
          "a polygon rises to the full bump height everywhere inside it");
    check(patternfab::bumpHeightAt(1.0, 1.0, polygon, 0.8) == 0.8,
          "including near its edge, since its top is flat rather than domed");
    check(patternfab::bumpHeightAt(11.0, 5.0, polygon, 0.8) == 0.0,
          "and to nothing outside it");
}


void a_point_exactly_on_an_edge_is_decided_the_same_way_every_time() {
    // ⚑ Not an arbitrary tie-break, and worth pinning because a sampling grid
    // really does land exactly on a boundary: 10 mm at 8 samples per mm is 80
    // whole steps, not a near miss. The test counts the crossings to the RIGHT
    // of the point, so a shape owns its left boundary and not its right one.
    // Half-open either way is what matters -- two speckles that abut exactly
    // then neither overlap nor leave a gap between them -- but which way round
    // it falls is a fact about this code, checked here rather than assumed.
    const Ring square{{0.0, 0.0}, {10.0, 0.0}, {10.0, 10.0}, {0.0, 10.0}};

    check(patternfab::pointInPolygon(0.0, 5.0, square),
          "a point exactly on the left edge belongs to this shape");
    check(!patternfab::pointInPolygon(10.0, 5.0, square),
          "a point exactly on the right edge belongs to whatever lies right of it");
}
} // namespace

int main() {
    a_point_is_inside_a_square_only_where_the_square_is();
    the_notch_of_a_concave_shape_is_outside_it();
    the_edge_that_closes_the_ring_counts_like_any_other();
    a_slanted_edge_is_crossed_where_it_actually_lies();
    a_dome_is_tallest_at_its_centre_and_meets_the_plate_at_its_rim();
    an_ellipse_is_measured_against_each_of_its_own_radii();
    a_speckle_with_no_size_raises_nothing();
    a_polygon_speckle_has_a_flat_top();
    a_point_exactly_on_an_edge_is_decided_the_same_way_every_time();

    if (failures == 0) {
        std::cout << "test_relief_field: all cases passed" << std::endl;
    }
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
