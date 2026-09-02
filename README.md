# PatternFab

Takes an optimized, non-periodic DIC speckle pattern and exports
method-aware physical fabrication files - STL/relief for a 3D-printed stamp
or mold, SVG/DXF for a laser-cut stencil, PNG at correct DPI for
direct-write or decal transfer - with manufacturing constraints (minimum
feature size, bridging, bleed compensation, non-periodicity) baked in.

It does not generate patterns or score their quality. That ground is covered
elsewhere. The gap PatternFab fills is the bridge from an optimized digital
pattern to a physical fabrication file, which nothing else in the landscape
currently does.

Built on Qt (GUI, SVG export) and VTK (mesh/relief pipeline, STL export), as
a headless `patternfab-core` library wrapped by a thin `patternfab-gui`
application and a `patternfab-cli` driver.

See [CLAUDE.md](CLAUDE.md) for scope, foundation rationale, the fabrication
methods and their citations, and the development path.

## What works today

The whole chain, end to end: read a pattern, apply manufacturing constraints,
and write every export format.

- **Input** - vector geometry read directly (JSON speckle primitives: shape,
  centre, radius, plus specimen scale, imaging resolution and target speckle
  size), or a raster PNG/TIFF via contour and blob detection.
- **Constraints** - minimum feature size, bleed compensation, stamp and
  stencil bridging, and a tiling check that refuses to inject periodicity
  into a pattern whose whole value is being non-periodic.
- **Exports** - SVG and DXF for a laser-cut stencil, PNG at a caller-supplied
  DPI for direct-write or decal transfer, and STL relief for a 3D-printed
  stamp or mold.
- **Sensor-physics uncertainty** - a design-time per-pixel DIC correlation
  confidence, combining local intensity gradient with a real camera sensor
  noise model, so a pattern can be judged before it is ever fabricated.
  Available in `patternfab-core` and `patternfab-cli`; not yet surfaced in
  the GUI.
- **GUI** - open a pattern, set the manufacturing constraints, read the
  constraint report, and export any of the four formats, with a 2D stencil
  preview and an in-window VTK 3D relief preview of the stamp.

## Build

Needs Qt 6, VTK 9, [dime](https://github.com/coin3d/dime) and
nlohmann/json. On Ubuntu 26.04:

```sh
sudo apt install libvtk9-dev libvtk9-qt-dev \
    qt6-base-dev qt6-base-dev-tools qt6-svg-dev \
    libdime-dev nlohmann-json3-dev

cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

Then open the sample pattern, a 175-speckle Poisson-disc field over a
30 x 20 mm specimen:

```sh
./build/gui/patternfab-gui examples/sample_pattern.json
```

or run the same pattern through the whole chain without a window:

```sh
./build/cli/patternfab-cli examples/sample_pattern.json /tmp/out
```

**The window needs a VTK whose `GUISupportQt` is built against Qt6.**
Ubuntu 24.04 and older ship one built against Qt5, and one process cannot
hold both. The build says so in a sentence and tells you the way round it:

```sh
cmake -S . -B build -DPATTERNFAB_BUILD_GUI=OFF
```

`patternfab-core`, `patternfab-cli` and the whole test suite build and run
that way on any distribution.

## Sister tool

PatternFab and [SurView DIC](https://github.com/katalystnord/SurView) are two
ends of one workflow: the speckle pattern PatternFab makes is the pattern
SurView measures. Same Qt and VTK stack, same licence, same ecosystem.

- **PatternFab** fabricates the pattern. This tool.
- **SurView DIC** measures it. Speckle images in, displacement and strain
  fields out, into ParaView and FreeCAD.

## License

LGPL-2.1-or-later. See [LICENSE](LICENSE).
