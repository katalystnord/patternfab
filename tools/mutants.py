#!/usr/bin/env python3
"""mutants.py -- break the code on purpose and see whether anything notices.

Ported from SurView, which is the sibling tool and where this harness was
written; the operators, the three-way verdict and the reasoning below are its.
What differs here is only what gets built and which tests get run.

Coverage says which lines the suite EXECUTES. That is a much weaker claim than
it looks: a test can run a line, and assert nothing about what it did. This
asks the stronger question -- change the meaning of a line, and does a test go
red? A mutant that survives marks behaviour the suite watches run without
checking. PatternFab had eight passing test executables and no answer to that
question at all.

Three outcomes, kept apart because collapsing them flatters the score:
  KILLED     a test failed. The suite noticed.
  SURVIVED   everything still passed. The suite did not notice. <- the finding
  NOT VIABLE the mutant would not compile. It was never a real change in
             behaviour, so it is excluded from the score rather than counted
             as a kill, which is what claiming it would do.

Usage:
  tools/mutants.py                      # all of core, all operators
  tools/mutants.py --limit 40 --seed 1  # a quick sample
  tools/mutants.py --files core/src/ConstraintEngine.cpp
"""

import argparse
import json
import os
import random
import re
import shutil
import signal
import subprocess
import sys
import time
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent


# --- what counts as a mutation -------------------------------------------
#
# Each operator is (name, pattern, replacement). Patterns run against a MASKED
# copy of the source in which comments and string literals have been blanked
# out -- without that, a codebase commented as heavily as this one would spend
# its whole run mutating prose, and every one of those mutants would survive
# while meaning nothing at all.
OPERATORS = [
    # Relational boundaries: the classic off-by-one, and the single richest
    # source of real defects in grid and bounds arithmetic.
    ("relational", r"(?<![<>=!])<=(?!=)", "<"),
    ("relational", r"(?<![<>=!])>=(?!=)", ">"),
    ("relational", r"(?<![<>=!ei])<(?![<=])", "<="),
    ("relational", r"(?<![<>=!-])>(?![>=])", ">="),
    ("relational", r"(?<![<>=!])<(?![<=])", ">"),
    ("equality",   r"(?<![<>=!])==(?!=)", "!="),
    ("equality",   r"(?<![<>=!])!=(?!=)", "=="),

    # Arithmetic. Division is left alone as a replacement: swapping * for /
    # mostly produces divide-by-zero crashes rather than wrong answers, and a
    # crash is a kill for uninteresting reasons.
    ("arithmetic", r"(?<![+\-*/=<>!&|])\+(?![+=])", "-"),
    ("arithmetic", r"(?<![+\-*/=<>!&|])-(?![-=>])", "+"),

    # Boundary: the +1/-1 that turns an inclusive bound exclusive.
    ("boundary", r"\+ 1\b", "+ 2"),
    ("boundary", r"\+ 1\b", "+ 0"),
    ("boundary", r"- 1\b", "- 2"),
    ("boundary", r"- 1\b", "- 0"),

    # Logic.
    ("logical", r"&&", "||"),
    ("logical", r"\|\|", "&&"),

    # Constants and returns.
    ("constant", r"\btrue\b", "false"),
    ("constant", r"\bfalse\b", "true"),
]


def mask_source(text):
    """Blank comments and string literals, preserving every offset.

    Offsets must survive exactly, because mutation sites are found in the mask
    and applied to the original.
    """
    out = list(text)
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        if c == "/" and i + 1 < n and text[i + 1] == "/":
            while i < n and text[i] != "\n":
                out[i] = " "
                i += 1
        elif c == "/" and i + 1 < n and text[i + 1] == "*":
            out[i] = out[i + 1] = " "
            i += 2
            while i < n and not (text[i] == "*" and i + 1 < n and text[i + 1] == "/"):
                if text[i] != "\n":
                    out[i] = " "
                i += 1
            if i < n:
                out[i] = out[i + 1] = " "
                i += 2
        elif c in "\"'":
            quote = c
            i += 1
            while i < n and text[i] != quote:
                if text[i] == "\\":
                    out[i] = " "
                    i += 1
                if i < n and text[i] != "\n":
                    out[i] = " "
                i += 1
            if i < n:
                i += 1
        elif c == "#":
            # Preprocessor lines: mutating an #include or a guard produces
            # noise, not a behaviour change.
            while i < n and text[i] != "\n":
                out[i] = " "
                i += 1
        else:
            i += 1
    return "".join(out)


def find_mutants(path):
    text = path.read_text()
    masked = mask_source(text)
    found = []
    for name, pattern, replacement in OPERATORS:
        for m in re.finditer(pattern, masked):
            start, end = m.start(), m.end()
            original = text[start:end]
            new_text = text[:start] + replacement + text[end:]
            if new_text == text:
                continue
            line = text.count("\n", 0, start) + 1
            found.append({
                "file": str(path.relative_to(REPO)) if REPO in path.parents or path.parent == REPO else str(path),
                "path": path,
                "line": line,
                "operator": name,
                "from": original,
                "to": replacement,
                "start": start,
                "end": end,
                "source": text,
                "mutated": new_text,
                "context": text[text.rfind("\n", 0, start) + 1:text.find("\n", start)].strip()[:100],
            })
    return found


def load_survivors(result_path, root):
    """Rebuild the exact mutants an earlier run recorded as survivors.

    ⚑ Each one is verified against the file as it stands NOW: the offsets are
    from the run that recorded them, and any edit since has moved them. A
    mutant whose recorded `from` is no longer at its recorded offset is
    reported and skipped rather than applied somewhere it does not belong,
    which would silently measure a mutation nobody chose -- the same hazard as
    the harness editing a dirty tree.
    """
    data = json.loads(result_path.read_text())
    replayed = []
    moved = []
    for entry in data.get("survived", []):
        path = (root / entry["file"]).resolve()
        if not path.exists():
            moved.append(f"{entry['file']}: gone")
            continue
        text = path.read_text()
        start, end = entry["start"], entry["end"]
        if text[start:end] != entry["from"]:
            moved.append(f"{entry['file']}:{entry['line']} {entry['from']!r} has moved")
            continue
        replayed.append({
            "file": entry["file"],
            "path": path,
            "line": entry["line"],
            "operator": entry["operator"],
            "from": entry["from"],
            "to": entry["to"],
            "start": start,
            "end": end,
            "source": text,
            "mutated": text[:start] + entry["to"] + text[end:],
            "context": entry.get("context", ""),
        })

    if moved:
        print(f"=== {len(moved)} recorded survivors could not be replayed ===")
        for line in moved:
            print(f"  {line}")
        print("  (the source moved under them; sweep those files afresh)\n")
    return replayed


def tests_covering(path, root):
    """The tests that compile against the mutated file, as a ctest -R pattern.

    ⚑ WHY A MUTANT DOES NOT NEED THE WHOLE SUITE. A mutation in Sequence.cpp
    cannot be caught by a test that never compiles against it, and running all
    33 executables to find that out costs three minutes a mutant where two
    executables would cost three seconds. Measured on this project, 2026-09-10:
    a full sweep spends most of its time re-running tests that could not
    possibly notice.

    The set is derived from the include graph rather than from a hand-written
    map, so it cannot rot: a test that includes core/Sequence.h is a test that
    might notice a change to Sequence.cpp. Direct includes only, which makes it
    a lower bound on what could catch the mutant -- and that is the whole reason
    the caller treats a pass here as INCONCLUSIVE and re-runs the complete suite
    before recording a survivor. A narrow filter can only ever produce a false
    survivor, never a false kill, and a false survivor costs one more run.

    Returns None when nothing includes the header, which sends the mutant
    straight to the complete suite.
    """
    header = path.with_suffix(".h").name
    covering = []
    for case in sorted((root / "tests").glob("test_*.cpp")):
        if header in case.read_text():
            covering.append(case.stem)
    if not covering:
        return None
    return "^(" + "|".join(covering) + ")$"


# The child currently running, so a signal can take it down with us. ⚑ A sweep
# is long and gets interrupted; killed without this, the python exits and leaves
# ctest and every test binary it started running on. SurView found orphans from
# three separate killed sweeps still holding that machine at a load average of
# 31 on 2026-09-11.
_running = None


def run(cmd, cwd, timeout):
    global _running
    try:
        # Its own process group, so one kill reaches ctest AND the binaries it
        # started rather than orphaning them.
        p = subprocess.Popen(cmd, cwd=cwd, stdout=subprocess.DEVNULL,
                             stderr=subprocess.DEVNULL, start_new_session=True)
        _running = p
        try:
            return p.wait(timeout=timeout)
        finally:
            _running = None
    except subprocess.TimeoutExpired:
        kill_group(p)
        # A mutant that makes the suite hang is caught, not ignored: an
        # infinite loop is a behaviour change a user would certainly notice.
        return 124


def kill_group(process):
    """Take down a child and everything it started."""
    if process is None or process.poll() is not None:
        return
    try:
        os.killpg(os.getpgid(process.pid), signal.SIGKILL)
    except (ProcessLookupError, PermissionError):
        pass
    try:
        process.wait(timeout=10)
    except subprocess.TimeoutExpired:
        pass


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--files", nargs="*", help="source files to mutate")
    ap.add_argument("--limit", type=int, default=0, help="sample this many mutants")
    ap.add_argument("--seed", type=int, default=20260818)
    ap.add_argument("--timeout", type=int, default=600)
    ap.add_argument("--jobs", type=int, default=0,
                    help="build with this many parallel jobs. Default: however "
                         "many ninja chooses, which is every core on the "
                         "machine. Give a number when something else is using "
                         "the machine too -- a mutation run is sustained full "
                         "load and a poor neighbour at its natural width.")
    ap.add_argument("--json", help="write the full result to this file")
    ap.add_argument("--rerun", metavar="RESULT.JSON",
                    help="re-run exactly the survivors recorded in an earlier "
                         "run's --json, instead of generating mutants afresh. "
                         "⚑ This is the half that finds things: replaying a "
                         "SURVIVOR list against a suite that has since been "
                         "worked on says which of them are now closed, and it "
                         "is minutes rather than the hours a fresh sweep costs. "
                         "Re-proving known kills finds nothing and is why "
                         "SurView's sweeps were being stopped half-way.")
    args = ap.parse_args()

    root = REPO
    # ⚑ Its own build directory, never build-ninja. Sharing it means a mutant is
    # compiled into the tree the pre-commit hook and an editor build are also
    # using -- and in SurView, during the first real run, a commit happened
    # mid-mutation with a deliberately broken source file in the working tree.
    #
    # The window is left OUT of this build. It is the slowest half by far and
    # not one line of it is mutated: everything with logic worth checking lives
    # in core, which is also the only thing the tests link against.
    build = REPO / "build-mutants"
    configure = ["cmake", "-S", str(REPO), "-B", str(build), "-G", "Ninja",
                 "-DCMAKE_BUILD_TYPE=Release", "-DPATTERNFAB_BUILD_GUI=OFF"]
    # ⚑ Headers as well as sources. IntegralImage.h holds real arithmetic, and
    # globbing *.cpp alone would mean that moving code into a header -- which is
    # exactly what making it testable required -- silently removed it from the
    # sweep and improved the score for no reason at all.
    default_files = sorted((REPO / "core" / "src").glob("*.cpp")) \
        + sorted((REPO / "core" / "src").glob("*.h")) \
        + sorted((REPO / "core" / "include" / "patternfab").glob("*.h"))
    # The whole suite runs in well under a second, so there is nothing to
    # exclude and no fast/slow split to explain.
    #
    # ⚑ AND THAT IS WHY SurView'S LOCALISATION IS NOT PORTED HERE, which is
    # worth saying so nobody ports it later for symmetry. There, a mutant ran
    # all 33 test executables and cost three minutes, so running only the tests
    # that compile against the mutated file cut a sweep by more than half.
    # Measured here on 2026-09-11: the whole suite is 0.43 s, and a mutant's
    # cost is its REBUILD. Narrowing the test set would save four tenths of a
    # second and buy a way to be wrong about coverage. The same fix, in the same
    # family of tools, is right in one and pointless in the other.
    test_filter = []
    ctest_dir = build
    ctest_cwd = build

    files = [Path(f).resolve() for f in args.files] if args.files else default_files
    files = [f for f in files if f.exists()]
    if not files:
        print("mutants.py: no source files to mutate", file=sys.stderr)
        return 2

    # ⚑ A dirty tree is refused. This harness restores each file from a backup
    # it took itself, so an edit made while it runs is silently reverted -- and
    # if it dies between writing a mutant and restoring, an uncommitted change
    # would be indistinguishable from the damage.
    dirty = subprocess.run(["git", "status", "--porcelain", "--", "core"],
                           cwd=root, capture_output=True, text=True).stdout.strip()
    if dirty and not os.environ.get("MUTANTS_ALLOW_DIRTY"):
        print(f"mutants.py: {root} has uncommitted changes under core/. Commit "
              "or stash them first -- this harness edits those files and "
              "restores them, and cannot tell your changes from its own.\n",
              file=sys.stderr)
        print(dirty, file=sys.stderr)
        return 2

    # ⚑ mold, when it is on the machine. Most of a mutant's cost is not
    # compiling the one file that changed but relinking every test executable,
    # and the linker is where that time goes.
    if shutil.which("mold"):
        configure.append("-DCMAKE_EXE_LINKER_FLAGS=-fuse-ld=mold")
        print("Linking with mold.")

    build_args = ["--parallel", str(args.jobs)] if args.jobs > 0 else []

    print("=== Baseline: the suite must be green before anything is broken ===")
    subprocess.run(configure, check=True, stdout=subprocess.DEVNULL)
    if subprocess.run(["cmake", "--build", str(build)] + build_args,
                      stdout=subprocess.DEVNULL).returncode != 0:
        print("mutants.py: the tree does not build. Fix that first.", file=sys.stderr)
        return 2
    baseline = run(["ctest", "--test-dir", str(ctest_dir)] + test_filter,
                   ctest_cwd, args.timeout)
    if baseline != 0:
        print("mutants.py: the suite is RED before mutation. A run now would "
              "score every mutant as killed by a failure that was already "
              "there.", file=sys.stderr)
        return 2
    print("Baseline green.\n")

    if args.rerun:
        mutants = load_survivors(Path(args.rerun), root)
        if not mutants:
            print("mutants.py: nothing to replay", file=sys.stderr)
            return 2
    else:
        mutants = []
        for f in files:
            mutants.extend(find_mutants(f))
    total_found = len(mutants)

    if args.limit and args.limit < len(mutants):
        random.Random(args.seed).shuffle(mutants)
        mutants = mutants[:args.limit]

    print(f"=== {len(mutants)} mutants "
          f"({total_found} found across {len(files)} files) ===\n")

    # ⚑ AN INTERRUPTED SWEEP MUST LEAVE THE TREE AS IT FOUND IT. It edits the
    # source in place and restores from a backup held in memory, so a sweep
    # killed between writing a mutant and restoring leaves a deliberately broken
    # file behind - indistinguishable from work in progress, which is the same
    # hazard the dirty-tree check above refuses to risk. It happened in SurView
    # on 2026-09-10, twice.
    #
    # SIGTERM and SIGINT are handled; SIGKILL cannot be, which is why the
    # message says which signal to use.
    restoring = {}

    def put_everything_back(*_):
        for target, text in restoring.items():
            Path(target).write_text(text)
        kill_group(_running)
        print("\nmutants.py: interrupted; source restored and children stopped.",
              file=sys.stderr)
        sys.exit(130)

    signal.signal(signal.SIGINT, put_everything_back)
    signal.signal(signal.SIGTERM, put_everything_back)

    killed, survived, not_viable = [], [], []
    started = time.time()

    for i, m in enumerate(mutants, 1):
        path = m["path"]
        backup = path.read_text()
        restoring[str(path)] = backup
        try:
            path.write_text(m["mutated"])
            built = subprocess.run(["cmake", "--build", str(build)] + build_args,
                                   stdout=subprocess.DEVNULL,
                                   stderr=subprocess.DEVNULL).returncode
            if built != 0:
                not_viable.append(m)
                verdict = "not viable"
            else:
                rc = run(["ctest", "--test-dir", str(ctest_dir)] + test_filter,
                         ctest_cwd, args.timeout)
                if rc == 0:
                    survived.append(m)
                    verdict = "SURVIVED"
                else:
                    killed.append(m)
                    verdict = "killed"
        finally:
            path.write_text(backup)
            restoring.pop(str(path), None)

        rate = (time.time() - started) / i
        print(f"[{i}/{len(mutants)}] {verdict:10} "
              f"{Path(m['file']).name}:{m['line']} "
              f"{m['operator']}: {m['from']!r} -> {m['to']!r}"
              f"   (~{rate:.1f}s/mutant)")

    # Rebuild clean, so the tree is left as it was found.
    subprocess.run(["cmake", "--build", str(build)] + build_args, stdout=subprocess.DEVNULL)

    scored = len(killed) + len(survived)
    score = (100.0 * len(killed) / scored) if scored else 0.0

    print("\n" + "=" * 70)
    print(f"Mutation score: {score:.1f}%  ({len(killed)} killed / {scored} viable)")
    print(f"  killed     {len(killed)}")
    print(f"  SURVIVED   {len(survived)}")
    print(f"  not viable {len(not_viable)}  (did not compile; excluded from the score)")

    if survived:
        print("\n--- Survivors: behaviour the suite runs but does not check ---")
        for m in survived:
            print(f"  {m['file']}:{m['line']}  "
                  f"{m['operator']}: {m['from']!r} -> {m['to']!r}")
            print(f"      {m['context']}")

    if args.json:
        Path(args.json).write_text(json.dumps({
            "score": score,
            "killed": len(killed),
            "survived": [{k: v for k, v in m.items()
                          if k not in ("path", "source", "mutated")}
                         for m in survived],
            "not_viable": len(not_viable),
        }, indent=2))
        print(f"\nFull result: {args.json}")

    return 0


if __name__ == "__main__":
    # Line-buffered, so a run redirected to a file shows progress as it goes.
    # The first full run printed nothing for twenty minutes and looked hung.
    sys.stdout.reconfigure(line_buffering=True)
    sys.exit(main())
