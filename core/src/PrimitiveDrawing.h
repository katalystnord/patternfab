#pragma once

#include "patternfab/Pattern.h"

#include <vector>

class QPainter;

namespace patternfab::detail {

// Internal helper shared by SvgExport and PngExport -- not a public API.
// Draws primitives filled black assuming painter's coordinate system already
// maps 1 unit to 1mm (callers apply their own scale/viewBox beforehand).
void drawPrimitivesMm(QPainter &painter, const std::vector<Primitive> &primitives);

} // namespace patternfab::detail
