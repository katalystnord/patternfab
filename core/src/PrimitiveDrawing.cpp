#include "PrimitiveDrawing.h"

#include <QPainter>
#include <QPainterPath>

namespace patternfab::detail {

void drawPrimitivesMm(QPainter &painter, const std::vector<Primitive> &primitives) {
    painter.setPen(Qt::NoPen);
    painter.setBrush(Qt::black);

    for (const auto &primitive : primitives) {
        if (primitive.shape == PrimitiveShape::Polygon) {
            if (primitive.verticesMm.empty()) {
                continue;
            }
            QPainterPath path;
            path.moveTo(primitive.verticesMm[0].first, primitive.verticesMm[0].second);
            for (std::size_t i = 1; i < primitive.verticesMm.size(); ++i) {
                path.lineTo(primitive.verticesMm[i].first, primitive.verticesMm[i].second);
            }
            path.closeSubpath();
            painter.drawPath(path);
        } else {
            painter.drawEllipse(QPointF(primitive.centerXMm, primitive.centerYMm), primitive.radiusXMm,
                                 primitive.radiusYMm);
        }
    }
}

} // namespace patternfab::detail
