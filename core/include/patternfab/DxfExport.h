#pragma once

#include "patternfab/Pattern.h"

#include <string>

namespace patternfab {

// Writes pattern.primitives as DXF entities via libdime: circles as CIRCLE,
// ellipses as ELLIPSE, polygons as a closed POLYLINE. Coordinates are in mm
// (DXF itself is unitless; consuming CAD/CAM software is expected to be
// told the drawing units are mm, standard practice for this format).
//
// Throws std::runtime_error if the file can't be opened or dime's model
// write fails.
void exportPatternToDxf(const Pattern &pattern, const std::string &path);

} // namespace patternfab
