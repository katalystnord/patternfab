// The summed-area table under every noise-floor figure.
//
// Written after the first mutation sweep, which found eleven mutants in these
// ten lines that the suite ran and checked nothing about: the four corners of
// the box sum could swap signs, and its +1 offsets could be dropped, and the
// numbers in the window would still have looked entirely reasonable. They were
// unreachable at the time -- an anonymous namespace inside UncertaintyEngine
// .cpp -- which is a structural defect rather than a fact to accept, so they
// moved into core/src/IntegralImage.h.
//
// The check is a PROPERTY over every rectangle rather than a handful of
// expected numbers: the table's answer must equal the loop it replaces. A
// worked example or two would have let the sign of one corner stay wrong for
// every box that happens to touch an edge, which is precisely the family of
// mutants that survived.
//
// Negative checks, each applied to IntegralImage.h and the case watched go red:
//   at(x0, y0) added -> subtracted        the overlap is subtracted twice; every
//                                         box not touching the top-left corner
//                                         disagrees.
//   at(x1 + 1, y1 + 1) -> at(x1, y1)      the box loses its last row and column,
//                                         and a 1x1 box reads zero.
//   the row above not accumulated         the table becomes a per-row running
//                                         total, and every box more than one
//                                         row tall disagrees.

#include "IntegralImage.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

int failures = 0;

void check(bool condition, const std::string &message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << std::endl;
        ++failures;
    }
}

// Deliberately not all ones: a field of equal values cannot tell a sum of the
// wrong PIXELS from a sum of the right ones, only a wrong count.
std::vector<double> makeField(int w, int h) {
    std::vector<double> field(static_cast<std::size_t>(w) * h);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            field[static_cast<std::size_t>(y) * w + x] = 1.0 + x * 3.0 + y * 11.0 + (x * y) * 0.5;
        }
    }
    return field;
}

double bruteForce(const std::vector<double> &field, int w, int x0, int y0, int x1, int y1) {
    double total = 0.0;
    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            total += field[static_cast<std::size_t>(y) * w + x];
        }
    }
    return total;
}

void every_box_reads_the_sum_of_the_pixels_it_covers() {
    const int w = 7;
    const int h = 5;
    const std::vector<double> field = makeField(w, h);
    const std::vector<double> integral = patternfab::integralImage(field, w, h);

    check(integral.size() == static_cast<std::size_t>(w + 1) * (h + 1),
          "the table carries the extra row and column that let an edge box be read");

    int boxes = 0;
    for (int y0 = 0; y0 < h; ++y0) {
        for (int y1 = y0; y1 < h; ++y1) {
            for (int x0 = 0; x0 < w; ++x0) {
                for (int x1 = x0; x1 < w; ++x1) {
                    const double fast = patternfab::boxSum(integral, w, x0, y0, x1, y1);
                    const double slow = bruteForce(field, w, x0, y0, x1, y1);
                    if (std::abs(fast - slow) > 1e-9) {
                        check(false, "the table disagrees with the loop it replaces at ("
                                         + std::to_string(x0) + "," + std::to_string(y0) + ")-("
                                         + std::to_string(x1) + "," + std::to_string(y1) + ")");
                        return;
                    }
                    ++boxes;
                }
            }
        }
    }
    // Guards the guard: a loop that ran zero times would report no
    // disagreements at all and pass.
    check(boxes == 420, "every rectangle in the field was compared");
}

void a_single_pixel_is_a_box_like_any_other() {
    const int w = 4;
    const int h = 3;
    const std::vector<double> field = makeField(w, h);
    const std::vector<double> integral = patternfab::integralImage(field, w, h);

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const double one = patternfab::boxSum(integral, w, x, y, x, y);
            check(std::abs(one - field[static_cast<std::size_t>(y) * w + x]) < 1e-9,
                  "a box of one pixel reads that pixel, not zero and not its neighbour");
        }
    }
}

void the_whole_field_is_the_sum_of_all_of_it() {
    const int w = 6;
    const int h = 4;
    const std::vector<double> field = makeField(w, h);
    const std::vector<double> integral = patternfab::integralImage(field, w, h);

    double total = 0.0;
    for (double value : field) {
        total += value;
    }
    check(std::abs(patternfab::boxSum(integral, w, 0, 0, w - 1, h - 1) - total) < 1e-9,
          "the box covering everything reads the sum of everything");
}

} // namespace

int main() {
    every_box_reads_the_sum_of_the_pixels_it_covers();
    a_single_pixel_is_a_box_like_any_other();
    the_whole_field_is_the_sum_of_all_of_it();

    if (failures == 0) {
        std::cout << "test_integral_image: all cases passed" << std::endl;
    }
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
