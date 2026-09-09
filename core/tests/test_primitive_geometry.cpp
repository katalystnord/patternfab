// How big a speckle is, where its middle is, and how far it reaches.
//
// Every manufacturing constraint is decided from these three answers, and the
// first mutation sweep left survivors in all of them: the smallest dimension of
// a shape could have been its largest, the direction between two speckles could
// have pointed the wrong way, and a circle could have been measured by the
// ellipse formula and an ellipse by the circle one. A constraint report is a
// claim about whether a pattern can be MADE, so a wrong answer here is a
// stencil that cannot be cut, reported as fine.
//
// ⚑ Nothing here is square, round or centred on the origin, and that is the
// point. A circle of equal radii cannot tell rx from ry; a shape at the origin
// cannot tell a centre from an offset; and a symmetric one cannot tell a
// direction from its opposite. Every fixture below is deliberately lopsided.
//
// Negative checks, each applied to PrimitiveGeometry.h and the named case
// watched go red:
//   std::min(w, h) -> std::max      a 1 by 5 bar reports a feature size of 5,
//                                   so a stencil that cannot hold it passes.
//   the polygon/other dispatch
//     inverted                      a polygon is measured by radii it does not
//                                   have, which are zero, and every polygon
//                                   becomes infinitely fine.
//   rx == ry -> rx != ry            a circle takes the ellipse formula and an
//                                   ellipse the circle one; the circle still
//                                   comes out right, which is why the ellipse
//                                   case is the one that catches it.
//   hypot(x - cx, y - cy) -> (x + cx)  a polygon's reach is measured from a
//                                   point that is not its centre.

#include "PrimitiveGeometry.h"

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

patternfab::Primitive makeEllipse(double cx, double cy, double rx, double ry) {
    patternfab::Primitive p;
    p.shape = rx == ry ? patternfab::PrimitiveShape::Circle : patternfab::PrimitiveShape::Ellipse;
    p.centerXMm = cx;
    p.centerYMm = cy;
    p.radiusXMm = rx;
    p.radiusYMm = ry;
    return p;
}

// A bar 1 wide and 5 tall, its lower-left corner at (2, 3). Lopsided in every
// way a test here can use.
patternfab::Primitive makeBar() {
    patternfab::Primitive p;
    p.shape = patternfab::PrimitiveShape::Polygon;
    p.verticesMm = {{2.0, 3.0}, {3.0, 3.0}, {3.0, 8.0}, {2.0, 8.0}};
    return p;
}

void a_feature_size_is_the_smallest_dimension_a_cutter_has_to_hold() {
    // The narrow way through is what a laser or a knife has to manage, so the
    // smallest dimension is the whole point of the number.
    check(std::abs(patternfab::featureSizeMm(makeBar()) - 1.0) < 1e-12,
          "a bar 1 wide and 5 tall has a feature size of 1, not 5 and not 6");

    // ⚑ Straddling the origin, because min() hides an error in whichever
    // dimension is not the smallest: for a bar in the positive quadrant,
    // maxY PLUS minY is larger than the correct width and the min() picks the
    // width anyway. Across the origin the wrong arithmetic goes NEGATIVE and
    // wins, so the same bar reports a feature size no cutter could hold.
    patternfab::Primitive straddling = makeBar();
    straddling.verticesMm = {{-0.5, -3.0}, {0.5, -3.0}, {0.5, 2.0}, {-0.5, 2.0}};
    check(std::abs(patternfab::featureSizeMm(straddling) - 1.0) < 1e-12,
          "a bar across the origin is measured by its extent, not by its coordinates");

    check(std::abs(patternfab::featureSizeMm(makeEllipse(4.0, 4.0, 0.5, 2.0)) - 1.0) < 1e-12,
          "an ellipse is measured across its narrow axis, as a diameter");
    check(std::abs(patternfab::featureSizeMm(makeEllipse(4.0, 4.0, 1.5, 1.5)) - 3.0) < 1e-12,
          "a circle's feature size is its diameter");
}

void a_polygon_is_measured_from_its_own_middle() {
    const auto [cx, cy] = patternfab::effectiveCenter(makeBar());
    check(std::abs(cx - 2.5) < 1e-12 && std::abs(cy - 5.5) < 1e-12,
          "a polygon's middle is the average of its corners, not the origin");

    const auto [rx, ry] = patternfab::effectiveCenter(makeEllipse(7.0, 9.0, 1.0, 2.0));
    check(std::abs(rx - 7.0) < 1e-12 && std::abs(ry - 9.0) < 1e-12,
          "a round speckle's middle is the centre it was given");
}

void a_speckle_reaches_as_far_as_its_shape_allows_in_that_direction() {
    // A circle reaches the same distance whichever way you ask.
    const patternfab::Primitive circle = makeEllipse(4.0, 4.0, 1.5, 1.5);
    check(std::abs(patternfab::effectiveRadiusToward(circle, 1.0, 0.0) - 1.5) < 1e-12
              && std::abs(patternfab::effectiveRadiusToward(circle, 0.0, 1.0) - 1.5) < 1e-12,
          "a circle reaches its radius in every direction");

    // ⚑ An ellipse does not, and this is the case that catches a circle and an
    // ellipse swapping formulas: along x it reaches rx, along y it reaches ry,
    // and a single radius cannot be both.
    const patternfab::Primitive ellipse = makeEllipse(4.0, 4.0, 3.0, 1.0);
    check(std::abs(patternfab::effectiveRadiusToward(ellipse, 1.0, 0.0) - 3.0) < 1e-12,
          "an ellipse reaches its long radius along its long axis");
    check(std::abs(patternfab::effectiveRadiusToward(ellipse, 0.0, 1.0) - 1.0) < 1e-12,
          "and its short radius along its short axis");
    const double diagonal = patternfab::effectiveRadiusToward(ellipse, 0.6, 0.8);
    check(diagonal > 1.0 && diagonal < 3.0,
          "and something between the two on a diagonal");

    // A polygon is treated as a disc of its furthest corner: from (2.5, 5.5)
    // the corners are all hypot(0.5, 2.5) away.
    const double reach = patternfab::effectiveRadiusToward(makeBar(), 1.0, 0.0);
    check(std::abs(reach - std::hypot(0.5, 2.5)) < 1e-12,
          "a polygon reaches its furthest corner, measured from its own middle");
}

void a_bounding_box_grows_to_hold_what_it_is_given() {
    double minX = std::numeric_limits<double>::max();
    double minY = std::numeric_limits<double>::max();
    double maxX = std::numeric_limits<double>::lowest();
    double maxY = std::numeric_limits<double>::lowest();

    patternfab::expandForPrimitive(makeEllipse(10.0, 20.0, 1.0, 2.0), minX, minY, maxX, maxY);
    check(std::abs(minX - 9.0) < 1e-12 && std::abs(maxX - 11.0) < 1e-12
              && std::abs(minY - 18.0) < 1e-12 && std::abs(maxY - 22.0) < 1e-12,
          "a round speckle's box is its centre give or take each radius");

    patternfab::expandForPrimitive(makeBar(), minX, minY, maxX, maxY);
    check(std::abs(minX - 2.0) < 1e-12 && std::abs(minY - 3.0) < 1e-12,
          "a second primitive widens the box rather than replacing it");
    check(std::abs(maxX - 11.0) < 1e-12 && std::abs(maxY - 22.0) < 1e-12,
          "and leaves the sides it does not reach where they were");
}

} // namespace

int main() {
    a_feature_size_is_the_smallest_dimension_a_cutter_has_to_hold();
    a_polygon_is_measured_from_its_own_middle();
    a_speckle_reaches_as_far_as_its_shape_allows_in_that_direction();
    a_bounding_box_grows_to_hold_what_it_is_given();

    if (failures == 0) {
        std::cout << "test_primitive_geometry: all cases passed" << std::endl;
    }
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
