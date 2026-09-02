#pragma once

#include <vector>

namespace patternfab {

enum class PrimitiveShape { Circle, Ellipse, Polygon };

// All coordinates and dimensions are in millimeters (physical units), not
// pixels -- pixel-space inputs (the raster path) are converted at load time
// using PhysicalParameters::imagingResolutionPxPerMm.
struct Primitive {
    PrimitiveShape shape = PrimitiveShape::Circle;
    double centerXMm = 0.0;
    double centerYMm = 0.0;
    // Circle: radiusXMm == radiusYMm. Ellipse: both used. Polygon: unused,
    // geometry lives in vertices instead.
    double radiusXMm = 0.0;
    double radiusYMm = 0.0;
    std::vector<std::pair<double, double>> verticesMm;
};

struct PhysicalParameters {
    double specimenWidthMm = 0.0;
    double specimenHeightMm = 0.0;
    double imagingResolutionPxPerMm = 0.0;
    double targetSpeckleSizeMm = 0.0;
};

struct Pattern {
    PhysicalParameters params;
    std::vector<Primitive> primitives;
};

} // namespace patternfab
