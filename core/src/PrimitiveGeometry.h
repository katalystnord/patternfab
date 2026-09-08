#pragma once

#include "patternfab/Pattern.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace patternfab {

// Per-primitive geometry: how big a speckle is, where its middle is, and how
// far it reaches in a given direction. Every manufacturing constraint is
// decided from these three answers.
//
// Internal to core (core/src, not the public include directory), but a header
// rather than an anonymous namespace inside ConstraintEngine.cpp so that a test
// can reach them. The first mutation sweep left survivors in every one: the
// smallest dimension of a shape could have been its largest, the direction
// between two speckles could have pointed the wrong way, and a circle could
// have been measured by the ellipse formula and an ellipse by the circle one.
// A constraint report is a claim about whether a pattern can be MADE, so a
// wrong answer here is a stencil that cannot be cut.

inline void expandForPrimitive(const Primitive &primitive, double &minX, double &minY, double &maxX, double &maxY) {
    if (primitive.shape == PrimitiveShape::Polygon) {
        for (const auto &[x, y] : primitive.verticesMm) {
            minX = std::min(minX, x);
            minY = std::min(minY, y);
            maxX = std::max(maxX, x);
            maxY = std::max(maxY, y);
        }
    } else {
        minX = std::min(minX, primitive.centerXMm - primitive.radiusXMm);
        minY = std::min(minY, primitive.centerYMm - primitive.radiusYMm);
        maxX = std::max(maxX, primitive.centerXMm + primitive.radiusXMm);
        maxY = std::max(maxY, primitive.centerYMm + primitive.radiusYMm);
    }
}

// Approximate feature size for a single primitive: exact smallest dimension
// for circle/ellipse, bounding-box-based approximation for polygon (see
// ConstraintEngine.h for why the approximation is acceptable here).
inline double featureSizeMm(const Primitive &primitive) {
    if (primitive.shape == PrimitiveShape::Polygon) {
        double minX = std::numeric_limits<double>::max();
        double minY = std::numeric_limits<double>::max();
        double maxX = std::numeric_limits<double>::lowest();
        double maxY = std::numeric_limits<double>::lowest();
        expandForPrimitive(primitive, minX, minY, maxX, maxY);
        return std::min(maxX - minX, maxY - minY);
    }
    return 2.0 * std::min(primitive.radiusXMm, primitive.radiusYMm);
}

// Centroid for polygon, center point otherwise -- the reference point used
// for gap-distance and direction calculations in checkStencilBridging.
inline std::pair<double, double> effectiveCenter(const Primitive &primitive) {
    if (primitive.shape == PrimitiveShape::Polygon) {
        double cx = 0.0;
        double cy = 0.0;
        for (const auto &[x, y] : primitive.verticesMm) {
            cx += x;
            cy += y;
        }
        const double n = static_cast<double>(primitive.verticesMm.size());
        return {cx / n, cy / n};
    }
    return {primitive.centerXMm, primitive.centerYMm};
}

// Effective radius of primitive in the direction (dirX, dirY) (a unit
// vector, pointing away from the primitive's effective center). See
// ConstraintEngine.h for the per-shape approximation rationale.
inline double effectiveRadiusToward(const Primitive &primitive, double dirX, double dirY) {
    if (primitive.shape == PrimitiveShape::Polygon) {
        const auto [cx, cy] = effectiveCenter(primitive);
        double maxDist = 0.0;
        for (const auto &[x, y] : primitive.verticesMm) {
            maxDist = std::max(maxDist, std::hypot(x - cx, y - cy));
        }
        return maxDist;
    }
    if (primitive.radiusXMm == primitive.radiusYMm) {
        return primitive.radiusXMm;
    }
    // Axis-aligned ellipse polar radius formula.
    const double rx = primitive.radiusXMm;
    const double ry = primitive.radiusYMm;
    const double denom = std::hypot(ry * dirX, rx * dirY);
    return (rx * ry) / denom;
}

} // namespace patternfab
