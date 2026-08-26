# escapement — notes

Repo-local facts. Cross-cutting ones live in
[fleet-notes](https://github.com/stoatworks-labs/fleet-notes).

---

## Where this came from (2026-08-26)

Brief was "a Resolume plugin that generates fractal graphics — Mandelbrot,
Sierpinski, Koch, seeded, endless zoom and pan, 3D presets such as a globe",
with a follow-up steer: **emulate an analogue signal chain rather than being
purely digitally generative**, and a reference —
[The Light Herder's 4K Video Feedback Fractal Device](https://youtu.be/koDCabeh5kQ).

That reference changed the architecture completely and for the better. The
device makes fractals with cameras, 4K screens and half-mirrored glass, and its
description claims *fractals without a computer*. Taking that literally gives
the design: a tap set is an iterated function system, its attractor is the
fractal, and the loop is a hardware realisation of the Hutchinson operator. It
also answers "endless zoom" exactly — a per-field zoom multiplier has no
coordinate to run out of precision on — which the obvious escape-time
implementation does not.

Every listed fractal survived the change. The only part that still evaluates a
formula is the iterator bank, and that is modelled as analogue multipliers,
which is fair: an analogue multiplier settles in nanoseconds, so a few hundred
iterations inside one video field is what the hardware would actually do.

## The bugs that cost real time

All four were found by **looking at rendered frames**, not by a test. The tests
were passing throughout.

**The taps were averaged.** `acc /= tapCount` looked like a normalisation and
was a loop gain of 1/N. Five rigs rendered black with the Gain control reading
1.12. Fixed by summing, which is also what a half-mirror does.

**The tunnel needed Zoom below 1.** Four rounds of gain tuning went into the
Mirror Tunnel preset before the cause turned out to be the sign of the zoom: a
tunnel is made of the *screen edge* re-photographed, so the camera has to see
past the frame. Above 1:1 it never does, and an injected spot inflates into a
smooth gradient.

**The fold read from the wrong wedge.** `mod( theta, seg )` puts the canonical
wedge at the +x axis; the Koch curve is built lying along the bottom of its
triangle. Three-fold symmetry therefore read from a part of the frame with
nothing in it and the snowflake was black — from a rig whose taps were all
correct. The wedge is now centred on straight down.

**`precise` was missing from the df64 code.** Apple's GLSL compiler reassociates
Dekker's split away. It compiles, runs, and still differs from the
single-precision path — so an A/B comparison proves nothing — while silently
having 24 bits. Caught by a 1e6 zoom coming out in ~8-pixel rectangular blocks,
which is exactly the width at which 24 bits stops separating adjacent pixels at
that magnification.

## The test that was too weak

`--presets` first asserted `mean > 0.002` and passed all twelve presets while
five were flat washes and one was a solid green rectangle. A feedback rig's
characteristic failures are a black frame *and a saturated one*, and "is
anything lit" cannot see the second. It now measures the standard deviation of
luma and excludes dark, flat and blown-out separately.

Worth generalising: for anything whose failure mode includes saturation, a
brightness floor is not an acceptance test.

## Sweeping a feedback plugin

`tools/sweep.py` needed two things the rest of the fleet's sweeps do not:

- **A fresh instance per render.** The frame store is state, so two settings
  compared across one instance differ because of history.
- **A pinned field count**, or no two frames ever match and everything looks
  live.

And its context table had two entries that were wrong rather than the plugin:
the bank's controls were invisible under a saturating loop (needs `Gain=0`), and
`Precision` read as dead at 1e10 because a 2048-iteration budget starves there
and *both* precisions go flat. `Field Rate` is excluded outright — it cannot
change a render whose field count is pinned, which is what pinning means.

## Plugin IDs

`ES01` source, `ES02` effect. Checked against every other ID in `~/Projects/
resolume` before choosing.

## Still open

- **No OpenFX target.** The pattern is vendored and orrery's CMake shows how,
  but an OFX host renders arbitrary frames in arbitrary order, and a loop with
  history cannot answer that without being reimplemented as something else.
  Deliberately deferred rather than half-built.
- **No web demo, CI, Windows build or plugin-bench expectation** yet.
- **`source/StoatworksAbout.h` is hand-written.** Escapement has no entry in the
  website's `projects.json`, so `sync-about.py` cannot generate it. The four
  About buttons point at pages that do not exist. Add the project and re-run the
  sync before any release.
- **Never opened in Resolume.** Both bundles are installed and pass the offline
  harness, the shader gate and the sweep, but no field session has happened —
  and the fleet's own experience is that first contact with the real host finds
  what no test can.
- **Two presets want another pass.** `Cell Structures` is stable but reads as
  flat wedges rather than cells; `Mandelbrot Dive` is correct and harshly
  coloured.
