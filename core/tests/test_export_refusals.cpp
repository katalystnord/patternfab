// What every exporter refuses, and refuses one reason at a time.
//
// The first mutation sweep left survivors in every exporter's validation, all
// of the same two shapes, and both matter:
//
//   `<= 0` widened to `< 0`   Zero passes. A specimen with no width is not a
//                             degenerate pattern to be drawn as best we can;
//                             it is a pattern that cannot exist, and the file
//                             written for it is a file that says nothing.
//   two guards in series       Each catches what the other would let through, so
//                             "did it throw" cannot tell them apart. The stated
//                             REASON can, and it is what the person exporting
//                             reads.
//   `||` narrowed to `&&`     A single bad dimension slips through, because
//                             the guard now demands that ALL of them be bad.
//                             This is the one that a careless test cannot see:
//                             a fixture with everything set to zero passes an
//                             AND of the same conditions perfectly well.
//
// ⚑ So each case spoils exactly ONE field and leaves the rest valid. That is
// the whole design of this file, and it is the only arrangement that can tell
// an OR of refusals from an AND of them.
//
// Negative-checked by widening each guard in turn and watching the matching
// case go red; recorded per the regime rather than assumed.

#include <patternfab/DxfExport.h>
#include <patternfab/PngExport.h>
#include <patternfab/StlExport.h>
#include <patternfab/SvgExport.h>

#include <cstdlib>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

int failures = 0;

void check(bool condition, const std::string &message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << std::endl;
        ++failures;
    }
}

// ⚑ WHICH refusal, not merely that there was one. PngExport has two guards in
// series -- the parameters, then the pixel count they work out to -- and each
// catches everything the other would have let through, so "did it throw" cannot
// tell them apart and both survived the sweep. The message is the only evidence
// that distinguishes them, and it is also what the person exporting reads.
void checkRefusesBecause(const std::function<void()> &attempt, const std::string &fragment,
                         const std::string &message) {
    try {
        attempt();
    } catch (const std::runtime_error &e) {
        const std::string reason = e.what();
        check(reason.find(fragment) != std::string::npos,
              message + " (refused, but for the wrong stated reason: " + reason + ")");
        return;
    } catch (...) {
        check(false, message + " (refused, but not in the way the others do)");
        return;
    }
    check(false, message);
}

void checkRefuses(const std::function<void()> &attempt, const std::string &message) {
    try {
        attempt();
    } catch (const std::runtime_error &) {
        return;
    } catch (...) {
        check(false, message + " (refused, but not in the way the others do)");
        return;
    }
    check(false, message);
}

patternfab::Pattern makeValidPattern() {
    patternfab::Pattern pattern;
    pattern.params.specimenWidthMm = 30.0;
    pattern.params.specimenHeightMm = 20.0;
    pattern.params.imagingResolutionPxPerMm = 40.0;
    pattern.params.targetSpeckleSizeMm = 0.5;

    patternfab::Primitive circle;
    circle.shape = patternfab::PrimitiveShape::Circle;
    circle.centerXMm = 15.0;
    circle.centerYMm = 10.0;
    circle.radiusXMm = 0.25;
    circle.radiusYMm = 0.25;
    pattern.primitives.push_back(circle);
    return pattern;
}

// Somewhere the write would succeed if it got that far, so that a refusal is
// evidence of the guard rather than of an unwritable path.
std::string outputPath(const std::string &name) {
    return "/tmp/patternfab_refusal_" + name;
}

patternfab::ReliefParameters validRelief() {
    patternfab::ReliefParameters relief;
    relief.baseThicknessMm = 2.0;
    relief.bumpHeightMm = 0.8;
    relief.meshResolutionPerMm = 8.0;
    return relief;
}

void a_specimen_with_no_width_is_refused_by_every_exporter() {
    patternfab::Pattern pattern = makeValidPattern();
    pattern.params.specimenWidthMm = 0.0;

    checkRefuses([&] { patternfab::exportPatternToSvg(pattern, outputPath("w.svg")); },
                 "SVG export refuses a specimen with no width");
    checkRefuses([&] { patternfab::exportPatternToPng(pattern, outputPath("w.png"), 600.0); },
                 "PNG export refuses a specimen with no width");
    checkRefuses([&] { patternfab::exportPatternToStl(pattern, outputPath("w.stl"), validRelief()); },
                 "STL export refuses a specimen with no width");
}

void a_specimen_with_no_height_is_refused_by_every_exporter() {
    patternfab::Pattern pattern = makeValidPattern();
    pattern.params.specimenHeightMm = 0.0;

    checkRefuses([&] { patternfab::exportPatternToSvg(pattern, outputPath("h.svg")); },
                 "SVG export refuses a specimen with no height");
    checkRefuses([&] { patternfab::exportPatternToPng(pattern, outputPath("h.png"), 600.0); },
                 "PNG export refuses a specimen with no height");
    checkRefuses([&] { patternfab::exportPatternToStl(pattern, outputPath("h.stl"), validRelief()); },
                 "STL export refuses a specimen with no height");
}

void a_resolution_of_zero_is_refused_on_its_own() {
    // The specimen is entirely valid here: only the dpi is wrong, so this is
    // the case that catches the guard demanding every dimension be bad at once.
    const patternfab::Pattern pattern = makeValidPattern();

    checkRefusesBecause([&] { patternfab::exportPatternToPng(pattern, outputPath("dpi.png"), 0.0); },
                        "must be positive",
                        "PNG export refuses a dpi of zero while the specimen is fine, and says so");
    checkRefusesBecause([&] { patternfab::exportPatternToPng(pattern, outputPath("dpi.png"), -300.0); },
                        "must be positive",
                        "and a negative one");
}

void a_specimen_too_small_to_land_on_a_single_pixel_is_refused() {
    // ⚑ A second guard behind the first, and it is not redundant: the
    // dimensions and the dpi can each be positive and their PRODUCT still
    // round to nothing. A 0.01 mm specimen at 25 dpi is a tenth of a pixel.
    // Left through, QImage is asked for an image of no size and the file that
    // results is not a picture of anything.
    patternfab::Pattern pattern = makeValidPattern();
    pattern.params.specimenWidthMm = 0.01;
    pattern.params.specimenHeightMm = 0.01;

    checkRefusesBecause([&] { patternfab::exportPatternToPng(pattern, outputPath("tiny.png"), 25.0); },
                        "zero size",
                        "PNG export refuses a specimen that rounds away to no pixels, naming that "
                        "as the reason rather than blaming the parameters it was given");
}

void a_relief_with_no_thickness_or_no_height_is_refused() {
    const patternfab::Pattern pattern = makeValidPattern();

    patternfab::ReliefParameters flat = validRelief();
    flat.baseThicknessMm = 0.0;
    checkRefuses([&] { patternfab::exportPatternToStl(pattern, outputPath("base.stl"), flat); },
                 "STL export refuses a base plate of no thickness");

    patternfab::ReliefParameters unembossed = validRelief();
    unembossed.bumpHeightMm = 0.0;
    checkRefuses([&] { patternfab::exportPatternToStl(pattern, outputPath("bump.stl"), unembossed); },
                 "STL export refuses a stamp whose speckles do not rise");

    patternfab::ReliefParameters unmeshed = validRelief();
    unmeshed.meshResolutionPerMm = 0.0;
    checkRefuses([&] { patternfab::exportPatternToStl(pattern, outputPath("mesh.stl"), unmeshed); },
                 "STL export refuses a mesh with no samples in it");
}

void a_valid_pattern_is_not_refused_by_any_of_them() {
    // The other half of every refusal case: a guard that refuses everything
    // would satisfy all of the above and export nothing at all.
    const patternfab::Pattern pattern = makeValidPattern();
    try {
        patternfab::exportPatternToSvg(pattern, outputPath("ok.svg"));
        patternfab::exportPatternToDxf(pattern, outputPath("ok.dxf"));
        patternfab::exportPatternToPng(pattern, outputPath("ok.png"), 600.0);
        patternfab::exportPatternToStl(pattern, outputPath("ok.stl"), validRelief());
    } catch (const std::exception &e) {
        check(false, std::string("a valid pattern was refused: ") + e.what());
    }
}

} // namespace

int main() {
    a_specimen_with_no_width_is_refused_by_every_exporter();
    a_specimen_with_no_height_is_refused_by_every_exporter();
    a_resolution_of_zero_is_refused_on_its_own();
    a_specimen_too_small_to_land_on_a_single_pixel_is_refused();
    a_relief_with_no_thickness_or_no_height_is_refused();
    a_valid_pattern_is_not_refused_by_any_of_them();

    if (failures == 0) {
        std::cout << "test_export_refusals: all cases passed" << std::endl;
    }
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
