# PatternFab - Context File

## Scope

PatternFab takes an optimized, non-periodic DIC (Digital Image Correlation)
speckle pattern, plus physical measurement parameters (specimen scale,
imaging resolution, target speckle size), and exports method-aware physical
fabrication files - STL/relief (3D-printed stamp or mold), SVG/DXF (laser-cut
stencil), or PNG at correct DPI (direct-write or decal transfer) - with real
manufacturing constraints baked in (minimum feature size, stamp bridging,
bleed compensation, non-periodicity preserved across any tiling).

It does not generate patterns or compute quality metrics - that ground is
already covered elsewhere. The gap PatternFab fills is purely the bridge from
an optimized digital pattern to a physical fabrication file, which nothing
else in the landscape currently does.

## Foundation

C++, built on **Qt + VTK**, deliberately consistent with the open-source
engineering-tool landscape it sits in (FreeCAD, VTK itself, Range3).

- **Qt** - GUI, and also the SVG export path: `QPainter` + `QSvgGenerator`
  writes SVG natively, no separate rendering library needed. `QPainterPath`
  already provides boolean path ops (`united()`, `subtracted()`,
  `intersected()`) for bridging/stencil-bridge geometry.
- **VTK** - 3D mesh/relief pipeline and STL export (`vtkSTLWriter`,
  heightmap-to-mesh via elevation/warp filters).
- **dime** - DXF export (native C++, BSD-3-Clause, part of the Coin3D
  ecosystem that FreeCAD is also built on).

## Fabrication methods - cite properly, don't claim invention

The physical fabrication methods themselves are published, not original to
this project:

- **Stamps** (3D-printed resin mold, silicone cast): Diani, Geraud, Coq et
  al., "Stamps for Pattern Applications for DIC or Markers Tracking,"
  *Experimental Techniques* (Springer, 2026), DOI 10.1007/s40799-026-00875-z.
- **Direct-write** (modified 3D printer with a syringe dispensing ink,
  Poisson-disc-distributed dots - not "inkjet"): Yang, Tao, Franck, "Smart
  DIC Patterns via 3D Printing," *Experimental Mechanics* (2021), DOI
  10.1007/s11340-021-00720-x.
- **Water-slide/tattoo decal transfer**: Quino et al., "Speckle patterns for
  DIC in challenging scenarios: rapid application and impact endurance,"
  *Measurement Science and Technology* (IOP, 2020), DOI
  10.1088/1361-6501/abaae8. Confirmed stable under impact loading and large
  strain.
- Commercial precedent that hand-applied kits work: Correlated Solutions'
  VIC Speckle Application Kit (six stamps + rocker applicator).

PatternFab's contribution is the software bridge from pattern to fabrication
file, not any of these physical techniques.

## Method-by-scale guidance

- Spray-through-stencil is the weakest reproducible method (edge bleed, halo,
  clogging). Prefer stamp or transfer instead.
- Stamp (3D-printed, or silicone cast from a resin-printed mold) for
  medium/large speckle.
- Direct-write or decal transfer for fine speckle.
- FDM alone cannot do fine features (0.4 mm nozzle). Resin-print the mold,
  cast silicone.

## The periodicity trap (design against this)

Good DIC patterns must be random and non-periodic, or correlation aliases
onto the wrong period. Stamps and stencils are repeatable by design, so
tiling a small one injects periodicity. Design a single large randomized
(Poisson-disc) field that covers the region in one application, or a
provably non-periodic tile.

## Sensor-physics uncertainty

Use the camera's measured noise (Android Camera2 `SENSOR_NOISE_PROFILE`, per
capture, per gain) to produce physically-grounded per-pixel confidence,
rather than pattern-only heuristics. A key differentiator - absent from the
rest of the field.

## License

PatternFab is licensed **LGPL-2.1-or-later**, matching FreeCAD's choice in
the same ecosystem, and the same framing used in SurView DIC. Fully
open-source, no proprietary/open-core tier.

Reasons:

- Protects against the scenario this project actually cares about - someone
  taking the whole application, rebranding it, and reselling it closed-source
  - about as effectively as GPL would, since there's no "larger work" for a
  whole-app fork to hide behind: the copyleft obligation to share modified
  source applies to the rebranded fork itself.
- Stays welcoming to outside contributors, who already trust and understand
  LGPL from FreeCAD, VTK, and the rest of this ecosystem.
- Leaves a narrow, accepted gap: someone could extract a specific PatternFab
  module into their own separate closed product, keeping only that module's
  source open. Accepted trade-off for the contributor-friendliness and
  ecosystem fit above.
- All chosen dependencies are license-compatible: Qt (LGPLv3 essential
  modules), VTK (BSD-3-Clause), and dime (BSD-3-Clause) all permit
  combination with a differently-licensed larger work.

## How this project is run

Carried over from SurView DIC deliberately: same author, same standards. Where
anything else in this file conflicts with this section, this section wins.

### The one command

```bash
cmake --build build-ninja && (cd build-ninja && ctest --output-on-failure)
```

Eight test executables over `patternfab-core`, covering pattern input, the
constraint engine, all four export paths and the whole pipeline end to end.

⚑ `enable_testing()` belongs at the TOP level, not in `core/`. Called from
`core/` as it was until 2026-09-02, CTest writes its test list only under
`build/core`, and `ctest` from the build root answers "No tests were found"
while eight passing executables sit one directory down. CI hid that for months
by pointing at `build/core/tests` explicitly: green and unreachable at once.

### The hook

```bash
git config core.hooksPath tools/git-hooks      # once per clone
```

Refuses a commit touching C++ or CMake that does not build and pass the suite.
Skipping is deliberate and visible: `git commit --no-verify`.

### No em-dashes, ever

**David, 2026-08-18**, stated for SurView DIC and carried here 2026-09-02 when
PatternFab became a public sister tool. There are to be no em-dashes - nor any
other non-ASCII dash - in this code base, in code, comments, documentation or
user-facing text.

Enforced, not remembered: `tools/check-no-em-dashes.sh` runs in the pre-commit
hook and as CI's first job. It is a check rather than a note because an
em-dash is invisible in review - near-identical to the hyphen it should have
been, in a diff, in a terminal and in a code font - so it is exactly the kind
of rule that erodes one character at a time. The 31 swept out of this
repository all arrived that way, as did the 179 out of SurView.

What to use instead depends on where it sits:

| where | substitute | why |
|---|---|---|
| C++ comment | `--` | matches SurView and the engine fork |
| user-facing string | `-` | `--` in text a user reads looks like a typo |
| Markdown | `-` | |

The check covers the whole non-ASCII dash family (U+2010, 2011, 2013, 2014,
2212), not only the em-dash: forbidding one character of a family that all
look identical just leaves the rest to arrive in its place.

## Development path

Architecture: a headless **`patternfab-core`** library (no Qt-GUI
dependency, unit-testable directly) wrapped by a thin **`patternfab-gui`**
Qt application. Keeps the fabrication math embeddable elsewhere later,
independent of any particular UI.

Phases, in order:

0. **Scaffolding** - CMake build, Qt+VTK+dime linked and building empty,
   LICENSE/README, CI skeleton.
1. **Input ingestion** - canonical internal representation (speckle
   primitives: shape, center, radius) plus the physical parameter form
   (specimen scale, resolution, target speckle size). Two input paths:
   vector geometry read directly when the source pattern already has it;
   raster (PNG/TIFF) fallback via contour/blob detection when it doesn't.
2. **Constraint engine** - method-agnostic core operating on the canonical
   representation: non-periodicity check across tiling, bleed compensation,
   minimum-feature-size flagging per target process.
3. **Export pipelines**, built in order of geometric complexity:
   1. SVG/DXF (2D vector, stencil bridging) - validates the constraint
      engine cheaply.
   2. PNG at correct DPI (direct-write/decal) - mostly a rendering step
      once (1) works.
   3. STL/relief (stamp/mold) - hardest: primitives → heightfield/mesh,
      plus stamp-bridging, via VTK's warp/elevation filters →
      `vtkSTLWriter`.
4. **Sensor-physics uncertainty** - deliberately last; additive/analysis
   layer, not required for a usable core export workflow.
5. **GUI** - Qt shell (parameter form, VTK preview pane, export actions)
   wrapping the `patternfab-core` library from phases 1-3.
