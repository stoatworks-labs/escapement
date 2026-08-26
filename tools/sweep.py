#!/usr/bin/env python3
"""Move every parameter and fail if any of them made no difference to the frame.

**This is the only thing in the repo that catches a dead control**, and it is
not a theoretical risk. A GLSL uniform whose name does not match the C++ is
silently ignored -- `glGetUniformLocation` returns -1 and `glUniform` on -1 is a
documented no-op -- so a slider can be stone dead while everything compiles,
links, loads and renders.

Nothing in `esctest --all` will catch it. Those checks pin the rig's clock, its
stability and its guards; a uniform that is never set still produces a stable,
guarded, frame-rate-independent picture.

## Sweeping a feedback rig is not like sweeping a stateless one

Two things make this harder here than in the rest of the fleet.

**Every render has to start from a fresh instance.** The frame store is the
instrument's memory, so a rig that has already been running is not in the same
state as one that has just been switched on, and two settings compared across a
single instance would differ because of the history rather than the setting.
`esctest` builds a new plugin per run, which is what makes this safe.

**Fields, not time.** The comparison is between two rigs that have gone round
the loop the same number of times. Left to the wall clock they would not have,
and every parameter would look live because no two frames would ever match.

## Why there is a context table

Most of these parameters are *supposed* to do nothing in the default rig, and a
naive sweep would report a dozen false failures:

- `Taps` and `Seed` are Kaleidoscope and Seeded only. Sierpinski has three maps
  whatever the Taps knob says.
- `Iterations`, `Julia X/Y`, `Escape Zoom/X/Y` and `Precision` are the iterator
  bank, which only runs for the Julia and Mandelbrot rigs or when the bank is
  what is being injected.
- `Tilt`, `Spin` and `Light` do nothing until `Sphere` is up.
- `Inject Level/Size/X/Y` do nothing while `Inject` is None.
- `Mirror Wedge` does nothing at a symmetry of 1, because there is one sector
  and nothing to mirror it against.
- `Palette Shift` does nothing on the Signal palette, which is not a lookup.
- `Field Rate` cannot change a render whose field count is pinned -- that is the
  whole point of pinning it -- so it is swept against the clock instead.

Usage::

    tools/sweep.py [--build BUILD_DIR] [--verbose]
"""

import argparse
import pathlib
import subprocess
import sys
import tempfile
import zlib

REPO = pathlib.Path(__file__).resolve().parent.parent

# Applied to every render: a rig that is actually running, with something in it
# to be changed. A dead rig makes every control look dead.
BASE = ["Gain=0.60", "Inject=1", "Inject Level=0.7", "Vignette=0.1"]

# How many fields each swept render runs. Enough for a change to work its way
# round the loop several times -- a rig only shows what a setting did after the
# picture has been through it -- and few enough that a 60-parameter sweep is
# not a coffee break.
FIELDS = "80"

EFFECT_ONLY = {"Mix", "Mask Mode"}
SOURCE_ONLY = set()

# What else has to be true for a parameter to be able to do anything.
CONTEXT = {
    # Rig-specific.
    "Taps":          ["Rig=5"],                       # Kaleidoscope chooses its own
    "Seed":          ["Rig=6"],                       # Seeded is the only rig built from one
    "Mirror Wedge":  ["Symmetry=0.3"],                # nothing to mirror at one sector

    # The iterator bank.
    # Gain=0 matters: these are only visible if the loop is NOT running on top
    # of them. At the base rig's 0.96 the store saturates to white within a
    # dozen fields and every one of these reads as dead -- which is exactly what
    # the first run of this sweep reported, and it was the sweep that was wrong.
    "Iterations":    ["Rig=8", "Gain=0.0", "Decay=0.2", "Inject=5"],
    "Julia X":       ["Rig=7", "Gain=0.0", "Decay=0.2", "Inject=5"],
    "Julia Y":       ["Rig=7", "Gain=0.0", "Decay=0.2", "Inject=5"],
    "Escape Zoom":   ["Rig=8", "Gain=0.0", "Decay=0.2", "Inject=5"],
    "Escape X":      ["Rig=8", "Gain=0.0", "Decay=0.2", "Inject=5", "Escape Zoom=0.3"],
    "Escape Y":      ["Rig=8", "Gain=0.0", "Decay=0.2", "Inject=5", "Escape Zoom=0.3"],
    # 1e6, not 1e10. Past about 1e9 a 2048-iteration budget starves and BOTH
    # precisions render a flat field, so the control reads as dead for a reason
    # that has nothing to do with precision.
    "Precision":     ["Rig=8", "Gain=0.0", "Decay=0.2", "Inject=5",
                      "Escape Zoom=0.46", "Iterations=1.0"],

    # The rescan. Nothing to tilt, spin or light while the picture is flat.
    "Tilt":          ["Sphere=1.0"],
    "Spin":          ["Sphere=1.0"],
    "Light":         ["Sphere=1.0"],

    # Injection.
    "Inject Level":  ["Inject=1"],
    "Inject Size":   ["Inject=1", "Inject Level=0.8"],
    "Inject X":      ["Inject=1", "Inject Level=0.8"],
    "Inject Y":      ["Inject=1", "Inject Level=0.8"],

    # A palette rotation needs a palette. Signal is the signal's own colour and
    # is not a lookup, so there is nothing to rotate.
    "Palette Shift": ["Palette=4"],

    # Saturation cannot be seen on a grey picture, and the default dot is white.
    "Saturation":    ["Inject=2"],                    # colour bars

    # A hue rotation of a white spot is a white spot.
    "Hue Rotate":    ["Inject=2"],

    # Camera moves need something with structure to move.
    "Focus":         ["Inject=3", "Inject Level=0.8"],
    "Lens":          ["Inject=3", "Inject Level=0.8"],
    "Rotate":        ["Inject=3", "Inject Level=0.8"],
    "Zoom":          ["Inject=3", "Inject Level=0.8"],
    "Pan X":         ["Inject=3", "Inject Level=0.8"],
    "Pan Y":         ["Inject=3", "Inject Level=0.8"],
    "Symmetry":      ["Inject=3", "Inject Level=0.8"],

    # Mask modes need the loop to have a picture with darks AND lights in it,
    # or Reveal and Hide are the same frame.
    "Mask Mode":     ["Inject=3", "Inject Level=0.8"],
    "Mix":           ["Inject=3", "Inject Level=0.8"],
}

# Parameters that need the host clock running rather than a pinned field count.
#
# Field Rate is the interesting one: with the fields pinned it CANNOT change the
# picture, by design, so sweeping it the ordinary way would report the control
# that defines the instrument as dead.
NEEDS_CLOCK = {"Speed", "Sync"}

# Parameters no rendered comparison can reach.
#
# Field Rate converts host seconds into a whole number of fields, and every
# render here PINS the field count so that two frames are comparable at all.
# Sweeping it against a pinned count asks whether a number that has been
# overridden still has an effect, and the answer is correctly no. It is covered
# by `esctest --clock`, which drives Clock::Advance directly.
SKIP_NAMES = {"Field Rate"}

SKIP_KINDS = {"buffer"}

SWEEP_VALUES = [0.0, 0.137, 0.611, 1.0]

OPTION_RANGE = {
    "Rig": 10,
    "Sync": 4,
    "Inject": 7,
    "Precision": 2,
    "Palette": 5,
    "Mask Mode": 4,
}


def read_png(path):
    """Decode a PNG to raw bytes. Enough of the format for our own writer's
    output -- 8-bit RGBA, one IDAT, filter 0 on every row."""
    data = path.read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError("not a PNG")

    pos = 8
    idat = b""
    while pos < len(data):
        length = int.from_bytes(data[pos:pos + 4], "big")
        kind = data[pos + 4:pos + 8]
        body = data[pos + 8:pos + 8 + length]
        if kind == b"IDAT":
            idat += body
        pos += 12 + length

    return zlib.decompress(idat)


def render(esctest, out, settings, effect, clock, verbose):
    args = [str(esctest), "--out", str(out), "--size", "320x180"]
    if effect:
        args.append("--effect")
    # Every run is a fresh instance with a cleared store, so the only thing that
    # differs between two renders is the setting being swept.
    args += ["--fields", "8" if clock else FIELDS]
    for setting in settings:
        args += ["--set", setting]

    if verbose:
        print("   ", " ".join(args))

    result = subprocess.run(args, capture_output=True, text=True)
    if result.returncode != 0:
        raise RuntimeError(f"esctest failed: {result.stderr.strip()}")

    return read_png(out)


def parameters(esctest, effect):
    """Name and default of every parameter, in declaration order."""
    args = [str(esctest), "--list"]
    if effect:
        args.append("--effect")

    result = subprocess.run(args, capture_output=True, text=True)
    if result.returncode != 0:
        raise RuntimeError(f"esctest --list failed: {result.stderr.strip()}")

    found = []
    for line in result.stdout.splitlines()[1:]:
        # id, name (may contain spaces), type, default
        parts = line.split()
        if len(parts) < 3:
            continue
        kind = parts[-2]
        name = " ".join(parts[1:-2])
        found.append((name, kind))


    # The About block is a text field and browser buttons, declared last. They
    # never touch a pixel, so sweeping them only buries a real dead control.
    for i, entry in enumerate(found):
        if entry[0] == "About":
            return found[:i]

    return found


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--build", default="build")
    parser.add_argument("--verbose", action="store_true")
    args = parser.parse_args()

    esctest = REPO / args.build / "esctest"
    if not esctest.exists():
        print(f"no esctest at {esctest} -- build first", file=sys.stderr)
        return 2

    dead = []
    checked = 0

    with tempfile.TemporaryDirectory() as tmp:
        tmp = pathlib.Path(tmp)

        for effect in (False, True):
            for name, kind in parameters(esctest, effect):
                if kind in SKIP_KINDS or name in SKIP_NAMES:
                    continue
                if effect and name not in EFFECT_ONLY:
                    continue
                if not effect and name in EFFECT_ONLY:
                    continue
                if effect and name in SOURCE_ONLY:
                    continue

                context = CONTEXT.get(name, [])
                clock = name in NEEDS_CLOCK
                base = BASE + context

                if name in OPTION_RANGE:
                    values = [float(i) for i in range(OPTION_RANGE[name])]
                else:
                    values = SWEEP_VALUES

                frames = []
                for value in values:
                    out = tmp / "sweep.png"
                    frames.append(
                        render(esctest, out, base + [f"{name}={value}"],
                               effect, clock, args.verbose))

                checked += 1
                if all(f == frames[0] for f in frames[1:]):
                    where = "effect" if effect else "source"
                    dead.append(f"{name} ({kind}, {where})")
                    print(f"  DEAD {name}")
                elif args.verbose:
                    print(f"  ok   {name}")

    print()
    if dead:
        print(f"sweep: {checked} parameters, {len(dead)} made no difference:")
        for entry in dead:
            print(f"  - {entry}")
        return 1

    print(f"sweep: {checked} parameters, all live")
    return 0


if __name__ == "__main__":
    sys.exit(main())
