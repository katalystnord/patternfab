#include "patternfab/DxfExport.h"

#include <dime/Layer.h>
#include <dime/Model.h>
#include <dime/Output.h>
#include <dime/entities/Circle.h>
#include <dime/entities/Ellipse.h>
#include <dime/entities/Polyline.h>
#include <dime/entities/Vertex.h>
#include <dime/sections/EntitiesSection.h>
#include <dime/util/Linear.h>

#include <cmath>
#include <stdexcept>
#include <vector>

namespace patternfab {

void exportPatternToDxf(const Pattern &pattern, const std::string &path) {
    dimeModel model;
    if (!model.init()) {
        throw std::runtime_error("exportPatternToDxf: failed to initialize dime model");
    }

    // model.addEntity() looks up the model's "ENTITIES" section by name and
    // silently no-ops if none exists -- init() does not create one, so it
    // must be added explicitly (verified against dime's actual source; the
    // header/doc comments don't mention this requirement at all).
    model.insertSection(new dimeEntitiesSection);

    const dimeLayer *layer = model.addLayer("PatternFab");

    // dimeModel takes ownership of every entity added via addEntity() (and
    // of each dimeVertex added via dimePolyline::setCoordVertices) --
    // deleted when the model is destroyed, so no manual cleanup here.
    for (const auto &primitive : pattern.primitives) {
        if (primitive.shape == PrimitiveShape::Circle) {
            auto *circle = new dimeCircle;
            circle->setCenter(dimeVec3f(primitive.centerXMm, primitive.centerYMm, 0.0));
            circle->setRadius(primitive.radiusXMm);
            circle->setLayer(layer);
            model.addEntity(circle);
        } else if (primitive.shape == PrimitiveShape::Ellipse) {
            auto *ellipse = new dimeEllipse;
            ellipse->setCenter(dimeVec3f(primitive.centerXMm, primitive.centerYMm, 0.0));
            ellipse->setMajorAxisEndpoint(dimeVec3f(primitive.radiusXMm, 0.0, 0.0));
            ellipse->setMinorMajorRatio(primitive.radiusYMm / primitive.radiusXMm);
            ellipse->setStartParam(0.0);
            ellipse->setEndParam(2.0 * M_PI);
            ellipse->setLayer(layer);
            model.addEntity(ellipse);
        } else { // Polygon
            if (primitive.verticesMm.empty()) {
                continue;
            }
            const int numVertices = static_cast<int>(primitive.verticesMm.size());
            std::vector<dimeVertex *> vertices(numVertices);
            for (int i = 0; i < numVertices; ++i) {
                auto *vertex = new dimeVertex;
                vertex->setCoords(
                    dimeVec3f(primitive.verticesMm[i].first, primitive.verticesMm[i].second, 0.0));
                vertices[i] = vertex;
            }
            auto *polyline = new dimePolyline;
            polyline->setCoordVertices(vertices.data(), numVertices);
            polyline->setFlags(dimePolyline::CLOSED);
            polyline->setLayer(layer);
            model.addEntity(polyline);
        }
    }

    dimeOutput out;
    if (!out.setFilename(path.c_str())) {
        throw std::runtime_error("exportPatternToDxf: failed to open output file: " + path);
    }
    if (!model.write(&out)) {
        throw std::runtime_error("exportPatternToDxf: dime model write failed: " + path);
    }
}

} // namespace patternfab
