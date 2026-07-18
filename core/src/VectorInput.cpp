#include "patternfab/VectorInput.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <stdexcept>

namespace patternfab {

namespace {

Primitive parsePrimitive(const nlohmann::json &j) {
    Primitive primitive;
    const std::string shape = j.at("shape").get<std::string>();

    if (shape == "circle") {
        primitive.shape = PrimitiveShape::Circle;
        primitive.centerXMm = j.at("centerXMm").get<double>();
        primitive.centerYMm = j.at("centerYMm").get<double>();
        primitive.radiusXMm = j.at("radiusMm").get<double>();
        primitive.radiusYMm = primitive.radiusXMm;
    } else if (shape == "ellipse") {
        primitive.shape = PrimitiveShape::Ellipse;
        primitive.centerXMm = j.at("centerXMm").get<double>();
        primitive.centerYMm = j.at("centerYMm").get<double>();
        primitive.radiusXMm = j.at("radiusXMm").get<double>();
        primitive.radiusYMm = j.at("radiusYMm").get<double>();
    } else if (shape == "polygon") {
        primitive.shape = PrimitiveShape::Polygon;
        for (const auto &vertex : j.at("verticesMm")) {
            primitive.verticesMm.emplace_back(vertex.at(0).get<double>(), vertex.at(1).get<double>());
        }
    } else {
        throw std::runtime_error("Unrecognized primitive shape: " + shape);
    }

    return primitive;
}

} // namespace

Pattern loadPatternFromVectorFile(const std::string &path) {
    std::ifstream file(path);
    if (!file) {
        throw std::runtime_error("Cannot open pattern file: " + path);
    }

    nlohmann::json j;
    file >> j;

    Pattern pattern;

    const auto &params = j.at("physicalParameters");
    pattern.params.specimenWidthMm = params.at("specimenWidthMm").get<double>();
    pattern.params.specimenHeightMm = params.at("specimenHeightMm").get<double>();
    pattern.params.imagingResolutionPxPerMm = params.at("imagingResolutionPxPerMm").get<double>();
    pattern.params.targetSpeckleSizeMm = params.at("targetSpeckleSizeMm").get<double>();

    for (const auto &primitiveJson : j.at("primitives")) {
        pattern.primitives.push_back(parsePrimitive(primitiveJson));
    }

    return pattern;
}

} // namespace patternfab
