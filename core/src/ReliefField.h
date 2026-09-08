#pragma once

#include "patternfab/Pattern.h"

#include <cmath>
#include <cstddef>
#include <utility>
#include <vector>

namespace patternfab {

// The height field a 3D relief is meshed from: which points a speckle covers,
// and how far it rises there.
//
// Internal to core (core/src, not the public include directory), but a header
// rather than an anonymous namespace inside StlExport.cpp so that a test can
// reach it. Sixteen mutants lived in these two functions and the first mutation
// sweep watched every one of them survive -- the crossing test could have been
// inverted, its wrap-around edge dropped, and its edge interpolation run
// backwards, and the only evidence would have been a stamp that prints the
// wrong shape.

// Crossing-number point-in-polygon. `vertices` is the open ring: the edge from
// the last vertex back to the first is walked by the j = n - 1 seed rather than
// by a repeated vertex.
inline bool pointInPolygon(double x, double y,
                           const std::vector<std::pair<double, double>> &vertices)
{
    bool inside = false;
    const std::size_t n = vertices.size();
    for (std::size_t i = 0, j = n - 1; i < n; j = i++) {
        const double xi = vertices[i].first;
        const double yi = vertices[i].second;
        const double xj = vertices[j].first;
        const double yj = vertices[j].second;
        const bool edgeCrossesScanline = (yi > y) != (yj > y);
        if (edgeCrossesScanline && (x < (xj - xi) * (y - yi) / (yj - yi) + xi)) {
            inside = !inside;
        }
    }
    return inside;
}

// Hemispherical-cap dome for circle/ellipse, flat top for polygon (see
// StlExport.h for why polygons do not get a true distance-based dome).
inline double bumpHeightAt(double x, double y, const Primitive &primitive, double bumpHeightMm)
{
    if (primitive.shape == PrimitiveShape::Polygon) {
        return pointInPolygon(x, y, primitive.verticesMm) ? bumpHeightMm : 0.0;
    }
    if (primitive.radiusXMm <= 0.0 || primitive.radiusYMm <= 0.0) {
        return 0.0;
    }
    const double dx = (x - primitive.centerXMm) / primitive.radiusXMm;
    const double dy = (y - primitive.centerYMm) / primitive.radiusYMm;
    const double normalizedDistSq = dx * dx + dy * dy;
    if (normalizedDistSq > 1.0) {
        return 0.0;
    }
    return bumpHeightMm * std::sqrt(1.0 - normalizedDistSq);
}

} // namespace patternfab
