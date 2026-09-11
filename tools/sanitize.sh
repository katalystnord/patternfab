#!/usr/bin/env bash
# Run the suite under the address and undefined-behaviour sanitizers.
#
# ⚑ WHAT THIS CATCHES THAT NOTHING ELSE DOES. A read or write one element past
# the end of a vector returns whatever sits there and changes nothing
# downstream: no assertion can see it, and a mutation sweep reports it as a
# survivor with nothing to be done. Fourteen of the survivors standing after the
# sweep of 2026-09-11 are exactly that, and this is what closes them.
#
# ⚑ Leak detection is OFF; see the comment on PATTERNFAB_SANITIZE in
# CMakeLists.txt for why that is a scoping decision and not a convenience.
#
# The window is left out: nothing in it is under test here, and building it
# needs a VTK whose Qt agrees with ours, which is not true everywhere.
set -euo pipefail

cd "$(dirname "$0")/.."

cmake -S . -B build-asan -G Ninja \
    -DPATTERNFAB_SANITIZE=ON \
    -DPATTERNFAB_BUILD_GUI=OFF
cmake --build build-asan

export ASAN_OPTIONS="detect_leaks=0:halt_on_error=1"
export UBSAN_OPTIONS="print_stacktrace=1"

ctest --test-dir build-asan --output-on-failure "$@"
