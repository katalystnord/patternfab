#include "patternfab/RasterInput.h"

#include <vtkDataArray.h>
#include <vtkImageAlgorithm.h>
#include <vtkImageConnectivityFilter.h>
#include <vtkImageData.h>
#include <vtkImageReader2.h>
#include <vtkNew.h>
#include <vtkPNGReader.h>
#include <vtkPointData.h>
#include <vtkSmartPointer.h>
#include <vtkTIFFReader.h>

#include <algorithm>
#include <cmath>
#include <map>
#include <stdexcept>

namespace patternfab {

namespace {

// Naive fixed threshold: pixels at or below this are treated as speckle ink
// on a lighter background. Not adaptive (no Otsu) -- a v1 baseline.
constexpr double kForegroundMax = 127.0;

struct BlobAccumulator {
    double sumX = 0.0;
    double sumY = 0.0;
    long count = 0;
};

std::string lowercaseExtension(const std::string &path) {
    const auto dot = path.find_last_of('.');
    if (dot == std::string::npos) {
        return "";
    }
    std::string ext = path.substr(dot + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });
    return ext;
}

} // namespace

Pattern loadPatternFromRasterFile(const std::string &path, const PhysicalParameters &physicalParams) {
    vtkSmartPointer<vtkImageAlgorithm> reader;
    const std::string ext = lowercaseExtension(path);
    if (ext == "png") {
        reader = vtkSmartPointer<vtkPNGReader>::New();
    } else if (ext == "tif" || ext == "tiff") {
        reader = vtkSmartPointer<vtkTIFFReader>::New();
    } else {
        throw std::runtime_error("Unsupported raster format (expected .png/.tif/.tiff): " + path);
    }
    vtkImageReader2::SafeDownCast(reader)->SetFileName(path.c_str());
    reader->Update();

    vtkImageData *image = vtkImageData::SafeDownCast(reader->GetOutputDataObject(0));
    if (!image || !image->GetPointData()->GetScalars()) {
        throw std::runtime_error("Failed to read raster pattern file: " + path);
    }
    if (image->GetPointData()->GetScalars()->GetNumberOfComponents() != 1) {
        throw std::runtime_error("Raster pattern must be single-channel grayscale: " + path);
    }

    vtkNew<vtkImageConnectivityFilter> connectivity;
    connectivity->SetInputConnection(reader->GetOutputPort());
    connectivity->SetScalarRange(0.0, kForegroundMax);
    connectivity->SetExtractionModeToAllRegions();
    connectivity->SetLabelModeToSizeRank();
    connectivity->Update();

    vtkImageData *labeled = connectivity->GetOutput();
    int dims[3];
    labeled->GetDimensions(dims);

    std::map<int, BlobAccumulator> blobs;
    for (int y = 0; y < dims[1]; ++y) {
        for (int x = 0; x < dims[0]; ++x) {
            const int label = static_cast<int>(labeled->GetScalarComponentAsDouble(x, y, 0, 0));
            if (label <= 0) {
                continue; // background
            }
            BlobAccumulator &blob = blobs[label];
            blob.sumX += x;
            blob.sumY += y;
            blob.count += 1;
        }
    }

    Pattern pattern;
    pattern.params = physicalParams;

    const double pxToMm = 1.0 / physicalParams.imagingResolutionPxPerMm;
    for (const auto &[label, blob] : blobs) {
        Primitive primitive;
        primitive.shape = PrimitiveShape::Circle;
        primitive.centerXMm = (blob.sumX / blob.count) * pxToMm;
        primitive.centerYMm = (blob.sumY / blob.count) * pxToMm;

        const double areaMm2 = blob.count * pxToMm * pxToMm;
        const double radiusMm = std::sqrt(areaMm2 / M_PI);
        primitive.radiusXMm = radiusMm;
        primitive.radiusYMm = radiusMm;

        pattern.primitives.push_back(primitive);
    }

    return pattern;
}

} // namespace patternfab
