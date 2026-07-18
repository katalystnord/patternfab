#pragma once

#include "patternfab/Pattern.h"

#include <string>

namespace patternfab {

// Loads a Pattern from PatternFab's own JSON schema:
//
// {
//   "physicalParameters": {
//     "specimenWidthMm": 100.0,
//     "specimenHeightMm": 100.0,
//     "imagingResolutionPxPerMm": 40.0,
//     "targetSpeckleSizeMm": 0.5
//   },
//   "primitives": [
//     { "shape": "circle", "centerXMm": 12.3, "centerYMm": 45.6, "radiusMm": 0.25 },
//     { "shape": "ellipse", "centerXMm": 1.0, "centerYMm": 2.0, "radiusXMm": 0.3, "radiusYMm": 0.2 },
//     { "shape": "polygon", "verticesMm": [[0,0],[1,0],[1,1]] }
//   ]
// }
//
// Throws std::runtime_error on malformed input (missing required field,
// unrecognized shape, unreadable file).
Pattern loadPatternFromVectorFile(const std::string &path);

} // namespace patternfab
