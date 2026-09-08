#pragma once

#include <cstddef>
#include <vector>

namespace patternfab {

// A summed-area table and the four-lookup box sum built on it.
//
// Internal to core (core/src, not the public include directory), but a header
// rather than an anonymous namespace inside UncertaintyEngine.cpp because a
// test cannot reach an anonymous namespace, and this arithmetic is under every
// noise-floor figure the tool reports. The first mutation sweep found eleven
// mutants living in these ten lines that no test noticed: the signs of the four
// corners could be swapped and the +1 offsets dropped, and every figure in the
// window would still have looked entirely reasonable.

// Row-major `field` of w*h values, returned as a (w+1)*(h+1) table whose
// entry (x, y) holds the sum of every field value strictly above and left of
// it. The extra row and column of zeros are what let a box touching the top or
// left edge be read without a special case.
inline std::vector<double> integralImage(const std::vector<double> &field, int w, int h)
{
    std::vector<double> sum(static_cast<std::size_t>(w + 1) * (h + 1), 0.0);
    for (int y = 0; y < h; ++y) {
        double rowRunning = 0.0;
        for (int x = 0; x < w; ++x) {
            rowRunning += field[static_cast<std::size_t>(y) * w + x];
            sum[static_cast<std::size_t>(y + 1) * (w + 1) + (x + 1)] =
                sum[static_cast<std::size_t>(y) * (w + 1) + (x + 1)] + rowRunning;
        }
    }
    return sum;
}

// The sum over the INCLUSIVE rectangle [x0, x1] x [y0, y1]. `w` is the width of
// the original field, not of the table.
inline double boxSum(const std::vector<double> &integral, int w, int x0, int y0, int x1, int y1)
{
    const auto at = [&](int x, int y) {
        return integral[static_cast<std::size_t>(y) * (w + 1) + x];
    };
    return at(x1 + 1, y1 + 1) - at(x0, y1 + 1) - at(x1 + 1, y0) + at(x0, y0);
}

} // namespace patternfab
