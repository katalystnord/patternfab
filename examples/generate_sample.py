#!/usr/bin/env python3
"""Generate examples/sample_pattern.json: a realistic, non-periodic DIC
speckle field in PatternFab's own vector JSON schema (see VectorInput.h).

Deterministic (fixed seed) so the checked-in sample is reproducible and the
integration test that consumes it stays stable. This is a dev-time tool, not
part of the build. Regenerate with:  python3 examples/generate_sample.py

The field is a Bridson Poisson-disc distribution — the single large
randomized field CLAUDE.md's "periodicity trap" section calls for, covering
the whole specimen in one application rather than tiling a small motif.
"""

import json
import math
import random

# Physical setup: a small coupon, imaged at a typical DIC resolution.
SPECIMEN_W_MM = 30.0
SPECIMEN_H_MM = 20.0
RESOLUTION_PX_PER_MM = 40.0      # 30x20 mm -> 1200x800 px
TARGET_SPECKLE_MM = 0.5          # speckle diameter -> radius ~0.25 mm
SEED = 20260721

speckle_radius_mm = TARGET_SPECKLE_MM / 2.0
# Poisson-disc min center spacing: ~3x the target speckle size keeps roughly
# the 50% coverage DIC patterns aim for without speckles merging.
min_dist_mm = TARGET_SPECKLE_MM * 3.0


def poisson_disc(width, height, min_dist, k=30, rng=None):
    """Bridson's algorithm: blue-noise points at least min_dist apart."""
    rng = rng or random.Random()
    cell = min_dist / math.sqrt(2)
    gw = int(math.ceil(width / cell))
    gh = int(math.ceil(height / cell))
    grid = [[None] * gw for _ in range(gh)]
    samples = []
    active = []

    def grid_coords(p):
        return int(p[0] / cell), int(p[1] / cell)

    def fits(p):
        if not (0 <= p[0] < width and 0 <= p[1] < height):
            return False
        gx, gy = grid_coords(p)
        for yy in range(max(gy - 2, 0), min(gy + 3, gh)):
            for xx in range(max(gx - 2, 0), min(gx + 3, gw)):
                q = grid[yy][xx]
                if q is not None and (q[0] - p[0]) ** 2 + (q[1] - p[1]) ** 2 < min_dist ** 2:
                    return False
        return True

    first = (rng.uniform(0, width), rng.uniform(0, height))
    samples.append(first)
    active.append(first)
    gx, gy = grid_coords(first)
    grid[gy][gx] = first

    while active:
        idx = rng.randrange(len(active))
        origin = active[idx]
        placed = False
        for _ in range(k):
            ang = rng.uniform(0, 2 * math.pi)
            rad = rng.uniform(min_dist, 2 * min_dist)
            cand = (origin[0] + rad * math.cos(ang), origin[1] + rad * math.sin(ang))
            if fits(cand):
                samples.append(cand)
                active.append(cand)
                cgx, cgy = grid_coords(cand)
                grid[cgy][cgx] = cand
                placed = True
                break
        if not placed:
            active.pop(idx)
    return samples


def main():
    rng = random.Random(SEED)
    # Inset by one radius so no speckle is clipped by the specimen edge.
    pts = poisson_disc(
        SPECIMEN_W_MM - 2 * speckle_radius_mm,
        SPECIMEN_H_MM - 2 * speckle_radius_mm,
        min_dist_mm,
        rng=rng,
    )
    primitives = []
    for (x, y) in pts:
        # Small per-speckle radius jitter (+/-15%): real applied speckle is
        # never perfectly uniform, and it exercises the min-feature check.
        r = speckle_radius_mm * rng.uniform(0.85, 1.15)
        primitives.append({
            "shape": "circle",
            "centerXMm": round(x + speckle_radius_mm, 4),
            "centerYMm": round(y + speckle_radius_mm, 4),
            "radiusMm": round(r, 4),
        })

    doc = {
        "physicalParameters": {
            "specimenWidthMm": SPECIMEN_W_MM,
            "specimenHeightMm": SPECIMEN_H_MM,
            "imagingResolutionPxPerMm": RESOLUTION_PX_PER_MM,
            "targetSpeckleSizeMm": TARGET_SPECKLE_MM,
        },
        "primitives": primitives,
    }
    with open("examples/sample_pattern.json", "w") as f:
        json.dump(doc, f, indent=2)
        f.write("\n")
    print(f"wrote examples/sample_pattern.json: {len(primitives)} speckles")


if __name__ == "__main__":
    main()
