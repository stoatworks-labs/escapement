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

## The browser demo (2026-08-27)

Built on the shared kit from `stoatworks-backend/resolume-demo`. Four findings.

**GLSL ES 3.00 has no `precise` keyword.** It arrived in ES only at 3.20, so
WebGL2 rejects the double-float code outright with "'precise' : undeclared
identifier" and a syntax error on the type after it. Handled in the kit's
`port()`, which is where the 410-core-to-ES-3.00 differences live — but it is
the only thing `port()` does that **removes a guarantee rather than translating
a spelling**, because a compiler that reassociates Dekker's split leaves 24 bits
where the plugin has 48. Disclosed on the page rather than papered over.

**The constructor defaults had gone stale, and the demo is what showed it.** The
Mirror Tunnel preset was retuned during the first session and the defaults were
left behind, so the plugin opened on a rig running at 1.28 round the loop that
saturated to a flat white frame in about two seconds. Nothing in `verify.sh`
looked at the defaults — every preset test applies a preset first. A demo shows
the defaults and nothing else until you touch something, which is exactly the
case the test suite had no coverage for.

**The demo is the fleet's only STATEFUL demo page.** Every other one renders a
pure function of (parameters, time). Here the loop advances on elapsed time, so
a redraw from moving a slider while paused runs no fields, and Restart clears
the store because a feedback loop cannot be rewound.

**Two Browser-pane traps cost time, both already in fleet-notes.** A hidden or
non-painting pane throttles `requestAnimationFrame` to nothing, so a stateful
page is silently *under-run* and looks wrong when it is only paused — force
paints with repeated screenshots before judging the picture. And a full-page
screenshot scrolls the document: with a `<select>` focused, that scroll changes
its value, so the rig silently switched from Mirror to Kaleidoscope between two
reads and looked like a wandering-parameter bug. Blur the active element first.

## They all died after a few seconds (2026-08-27)

Reported from watching them: a burst of motion, then nothing. Measured with a
new `esctest --liveness` before touching anything — seven of twelve presets had a
structural change of literally 0.0000 per field, and the five that "moved" were
flickering injected grain while the picture stood still. The first version of
that metric counted every pixel and made those five look healthy; averaging
32x32 blocks first, so grain cancels, is what made the problem legible.

The cause is Banach's fixed point theorem and there is no tuning that avoids it —
see AGENTS.md. The fix is `ApplyDrift`, the operator's hands, on by default
everywhere. Three things it took to make it actually work:

**Drift on wall time made the harness lie.** The phase advanced on host seconds
while the harness pins the field count, so 400 offline fields — a fifth of a
second of wall clock — drifted by a fifth of a second's worth. Four live rigs
measured as dead. Everything animated now advances on `fields / fieldRate`.

**Depth has to scale with the rig's compliance.** Gentle drift tuned on the
mirror rigs did nothing at all to the IFS rigs, and raising it five times over
still did nothing: a camera nudge displaces an attractor by `t/(1-c)`, and
Sierpinski's `1-c` is 0.5 where a mirror rig's is 0.02. Scaling by `1-c` moved
every IFS rig into life in one go.

**Per-frame drift broke frame-rate independence**, and `--rate` caught it within
a minute. Drift and the iterator bank both moved inside the per-field loop.

The Globe preset was rebuilt on the way: it had been left with a zoom above 1:1
from before the tunnel finding, so its loop filled to a uniform field and the
sphere came out as a flat magenta ball. A rescan needs something to WRAP, and a
loop that has filled has no texture left in it.

## The delay that did nothing, and the instability that did (2026-08-27)

`Cell Structures` was the last weak preset — stable, but flat wedges rather than
cells — and the proposed fix was a **loop delay**: a real light path is two to
four fields long, and a delayed loop with a saturating amplifier should lose its
fixed point and oscillate (Ikeda). Built it properly: ring store, a second
sampler so the taps read the delayed field while persistence reads the newest
(without that split, a delay of 2 gives `x[n] = f(x[n-2])`, two independent
interleaved sequences that flicker), the control, the memory budget.

**It did nothing.** A scan across delay 1..4 and gain gave at best 0.00013
structural motion, and a settled picture moved by 0.005 between one field of
latency and four. Two reasons: a soft clip is monotone, and monotone saturation
under positive feedback is bistable rather than oscillatory; and at a fixed point
every delayed copy is identical, so a delay can only change transients. Reverted.

The scan did find the real mechanism, in the column I had not been looking at.
Motion appeared when the loop was near unity gain with noise and blur — and
bisecting the difference between a configuration that patterned and one that
died isolated it to **saturation**: 1.0 dies, 1.1 patterns, 1.4 gives fine
domains. It is a **chroma instability**. Chroma gain above one is the reaction
term, Focus is the diffusion, the clip is the ceiling, and the domains are
colours. `Cell Structures` is now that, and it is a proper labyrinthine Turing
pattern that reorganises for ever with the hands off.

Three hours of it went on chasing a hypothesis that measurement killed in ten
minutes once the right thing was measured. The lesson worth keeping is the
shape of `--reaction`: a mechanism test is a PAIR, one setting that must work and
one that must fail, or it only proves that something moved.

Two smaller traps in the same session: the preset first read `Saturation=0.7` as
a colour choice when it was the mechanism, and the working scan configuration
inherited three defaults I had not set (`rotate` among them) which the preset
then zeroed — comparing a preset against a `--set` run means comparing every
value, not the ones that were typed.

## Focus was measured in texels (2026-08-27)

Noticed while porting the demo and confirmed by rendering one preset at two
rasters: Cell Structures gave **8 domain boundaries across the frame at 320x180
and 36 at 1280x720**. `Focus` is a `textureLod` bias, a lod is a number of
texels, and the blur is the diffusion length of a reaction-diffusion system --
so the size of everything the loop grew was a function of the output resolution.
Every preset here had been tuned on a small offline render, which means every one
of them would have looked wrong on a real output.

Fixed by making `Focus` a fraction of the short edge, with `FocusLod` converting
it where the raster is known, and converting each preset's value so the tuned
look is preserved. The ratio is now 1.30, and what remains is a floor rather than
an error: a blur cannot be finer than one texel, so a small raster cannot be as
sharp in frame terms as a large one. `--scale` holds it under 1.6.

Two things fell out of the units change, both worth remembering. The `--reaction`
test hard-coded `Focus=0.20` from when it was a lod, and when the units changed
underneath it the half that is supposed to prove the rig DIES quietly got
livelier than its own threshold -- a test that pins a magic number in a unit
someone else owns will rot silently. And `Mandelbrot Dive` dropped below the
liveness gate purely because its palette changed from Ember to Ice: the metric is
luma-based, and a smoother palette reads as less motion for the same picture.

`Mandelbrot Dive` also moved to the Ice palette. Ember is three clamped ramps,
which suits the broad bands of a 1:1 view and posterises a 1e6 zoom -- where
almost every pixel is near the boundary -- into flat orange and black. Julia
keeps Ember, where the bands really are broad.

## First contact with Arena, both platforms (2026-08-27)

Tested in Resolume Arena 7.27.1 on macOS, and on the win-lab VM under Mesa
llvmpipe via `plugin-bench/arena`. The expectation file is
`plugin-bench/arena/expect/escapement.json`.

**It found one shipped bug immediately, before anything was even driven.** The
effect's own NAME was truncated: FFGL's `PluginInfoStruct` carries
`char PluginName[16]`, not null terminated, and "Escapement Feedback" is
nineteen. Arena's log is the only place it is visible --
`registered extension: 'Escapement Feedb'` -- because the SDK takes the name as
a `const char*`, stores what it likes and never complains. Renamed to
"Escapement Feed" and guarded with a `static_assert` in both registration
files. `esctest --names` checks PARAMETER names for exactly this limit and
cannot reach this one: the plugin's name is not a parameter.

Everything else passed on both hosts:

- Loads and registers with the right uid and category (3 source, 1 effect).
- 52 controls, matching name, order, type, range and default against the
  plugin's own declarations.
- The only names at the 16-character limit are the shared About buttons, both
  complete.
- Renders real content (std 66.8), and the picture keeps changing -- 28 levels
  of frame delta over three seconds, which is the drift work confirmed in the
  host rather than in the harness.
- The effect takes a clip, and `Inject = Clip` really does feed the footage into
  the loop.
- Mesa's GLSL compiler accepts the shaders, which is genuine evidence about the
  source that macOS alone cannot give. It says nothing about NVIDIA or AMD.
- The plugin's own log confirms `host clock is milliseconds`, so the clock-unit
  detection is doing its job in the real host.

Windows gate: **15 passed, 0 failed, 0 skipped.**

### Arena restarts on its own on win-lab, and the gate cannot see it

The first gate run died mid-sweep with the REST API refusing connections, and
Arena's log ended abruptly -- which fleet-notes calls the signature of a crash.
It was not this plugin:

- No BugSplat dump was ever written, for any of the restarts.
- Sweeping all 52 controls by hand, every option and both extremes, killed
  nothing.
- Mounting the plugin eight times over does not leak: Arena's memory FALLS,
  2502 MB to 1460 MB.
- The full gate then ran clean end to end.

Arena's launch history on that box reads 01:56, 02:13, 02:40 -- and the 02:40
restart happened **during the gate run that reported "Arena survived the run"**.
The gate checks liveness at the end, so a restart mid-run passes it: a real
crash in the middle of a sweep would be reported as a pass. That is worth
fixing in the bench, and it is worth knowing before trusting an `alive` PASS.

## sync.sh cannot see any repo (found 2026-08-27, NOT fixed)

`stoatworks-backend/resolume-demo/sync.sh` computes
`projects="$(cd "${here}/../.." && pwd)"`, which after the repo reorganisation
resolves to `~/Projects/infrastructure` — but the plugin repos are under
`~/Projects/resolume`. Every one of its 17 entries hits the `no demo/` branch
and is skipped.

`--check` therefore prints "kit is in sync across 17 repo(s)" and **exits 0
while comparing nothing**, which is the failure mode that matters: it is the
drift gate for CI and release, and it has been passing vacuously. Escapement's
`demo/vendor/` was copied by hand for that reason.

## Plugin IDs

`ES01` source, `ES02` effect. Checked against every other ID in `~/Projects/
resolume` before choosing.

## Still open

- **No OpenFX target.** The pattern is vendored and orrery's CMake shows how,
  but an OFX host renders arbitrary frames in arbitrary order, and a loop with
  history cannot answer that without being reimplemented as something else.
  Deliberately deferred rather than half-built.
- **No CI, Windows build or plugin-bench expectation** yet. The web demo exists
  and runs locally; it has never been deployed, and `wrangler.toml` is untested.
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
