# escapement — orientation for another LLM (or a newcomer)

A video feedback rig, and the fractals it settles into, as two FFGL plugins for
Resolume Arena/Avenue.

`CLAUDE.md` is the command reference. This file is the *why*.

---

## The one idea

**The fractals are what the loop settles into. Nothing evaluates a fractal.**

Escapement models an optical video feedback rig: a camera looking at a screen
that is showing what the camera saw a field ago, through glass that splits the
path into several. That is a real machine — The Light Herder's device, in
`ATTRIBUTIONS.md` — and its whole claim is *fractals without a computer*.

The mathematics agrees with the machine, exactly. A **tap** is one path from the
screen back into the camera, and a set of taps is an **iterated function
system**, whose attractor is a fractal. Three half-scale taps on the vertices of
a triangle have the Sierpinski gasket as their attractor and converge on it from
any starting image at all, including sensor noise. So `Taps()` returns three
affine maps, the loop runs, and the gasket arrives. There is no gasket-drawing
code and there must never be one.

The rule it gives you, which is the same rule vectrix has in a different
domain: **if you are tempted to draw the thing, the rig is wrong somewhere.**

### What falls out of it

- **Endless zoom is free, and exact.** The camera's zoom is a per-field
  *multiplier* applied to the picture, not a coordinate anyone stores. After a
  thousand fields at 1.02 the effective magnification is 1.02^1000, a number
  that never exists anywhere: there is no accumulator to overflow and no float
  to run out of mantissa. This is why the escape-time rigs are the only part of
  the plugin that has a precision limit — they are the only part that holds a
  coordinate.
- **Endless pan, for the same reason.**
- **The Koch snowflake costs four taps, not twelve.** The twelve-map snowflake
  system will not fit in eight taps. A four-map Koch *curve* plus a three-fold
  rotational fold is the same picture, and the fold is a mirror wedge in front
  of the lens — a thing the rig could actually have.
- **The 3D globe is a rescan**, not a renderer. The rig's screen is a sphere and
  the camera is looking at it, so the loop underneath is an ordinary mirror rig
  and the texture on the globe is alive rather than being a still that spins.
- **The effect build makes a fractal out of your footage.** The clip is injected
  into the loop rather than laid behind the picture, so the attractor is built
  from the clip's own light.

---

## The shape of it

```
per rendered frame:
  1 UpdateClock()          host ms/s auto-detect, then delta seconds
  2 CurrentLoop()          0..1 params -> engineering units
  3 store.Ensure(w,h)      allocate; a RESIZE clears, a knob never does
  4 clock.Advance(dt)      -> a whole number of FIELDS at the rig's own rate
  5 Glass(params)          the tap set, once -- drift never changes which rig
  6 for each field:
      ApplyDrift + Resolve   the hands move BETWEEN fields, not between frames
      [if needed] iterator bank pass
      loop pass              (store -> store, ping-pong, mips)
  7 display pass           palette, bloom, rescan, composite with the clip
```

### Directories

- `source/Taps.{h,cpp}` — the glass. Pure maths, no GL, no host. Every published
  IFS is written the way its source writes it so it can be checked line by line.
- `source/Loop.{h,cpp}` — the rig's clock and its stability arithmetic. Also no
  GL, which is what lets the harness drive the real code.
- `source/Shaders.cpp` — three passes. The only place the picture exists.
- `source/Store.{h,cpp}` — the frame store. The one framebuffer in the fleet
  that is not a mistake.

---

## Traps

Ordered by how much time they cost.

### A rig left alone goes still, and no amount of tuning fixes it

This is the one that will be reported as a bug, and it is a theorem.

The loop is a contraction mapping. With constant parameters it has, by Banach,
**exactly one attracting fixed point**, and the iteration converges on it
geometrically. That is the whole basis of the plugin — it is why a Sierpinski
gasket assembles itself out of sensor noise — and it is also why, about ten
seconds later, nothing moves again. The attractor maps to itself. Every
subsequent field is the previous field.

Endless zoom does not rescue it, and it looks as though it should: the attractor
of a self-similar system, magnified by its own similarity ratio, is the same
attractor. A gasket zoomed by two is a gasket. The camera can zoom for an hour.

The only way out is for the **operator** to change, which on the rig this models
is literally a person: the Light Herder's device is played by somebody walking
the cameras around while it runs. `ApplyDrift` in Loop.cpp is those hands, it is
on by default in every preset, and `esctest --liveness` is the test that fails if
anyone turns it off or lets it stop reaching the picture.

Two things about it are easy to get wrong:

- **Depth has to scale with compliance.** A camera nudge `t` displaces an
  attractor by about `t / ( 1 - c )`. Sierpinski contracts by 2 and needs a
  shove; a mirror rig at 0.98 would be thrown off the screen by the same shove.
  Gentle drift tuned on the mirror rigs moved the IFS rigs by a thousandth of the
  frame per second, which is a still picture with extra arithmetic.
- **It has to advance per FIELD.** Drift makes the hands a function of time, so a
  field at phase 0.31 differs from one at 0.30. Advance it once per rendered
  frame and sixty fields delivered as one frame use one hand position where sixty
  frames of one use sixty — the picture then depends on the host's frame rate,
  which is the one thing this plugin promises it does not. `--rate` caught it.

### Autonomous life is a chroma instability, and a delay is not the answer

`Drift` keeps a rig alive by moving the operator's hands. There is a second way,
which needs no operator, and it is worth knowing which one it is because the
obvious candidate is wrong.

**What works: chroma gain above unity.** Saturation over 1 amplifies whatever
colour deviation a pixel has; `Focus` spreads it to the neighbours; the clip
stops it running away. Local gain, lateral coupling, a ceiling — a
reaction-diffusion system, whose domains are *colours*. It produces labyrinthine
patterns that keep reorganising indefinitely with every knob held still. That is
the whole of `Cell Structures`, and the band is narrow: 1.0 collapses to a dot,
1.1 patterns coarsely, 1.4 gives fine domains, 1.6 floods.

The garish magenta-and-green is not a fault to be tuned out. **The colour is the
mechanism** — the preset was first written with the saturation pulled down to
0.7 to calm it, and at 0.7 there is no reaction term and the picture dies.

**What does not work: a delay in the light path.** A real camera-to-screen path
is two to four fields long, and a delayed feedback loop with a saturating
amplifier ought to lose its stable fixed point and oscillate — that is the Ikeda
map, a model of light going round a ring cavity, which is exactly what this is.
It was built: the ring store, the second sampler, the control. It did nothing.
At best 0.00013 structural motion against 0.0, and a settled picture differed by
0.005 between one field of latency and four.

Two reasons, both plain afterwards:

- **Ikeda's nonlinearity is non-monotonic and a soft clip is not.** Monotone
  saturation with positive feedback is bistable: it settles at the ceiling and
  stays there. Sustained oscillation needs a fold or a sign reversal.
- **At a fixed point, every delayed copy is identical by definition.** A delay
  cannot change a picture that has already converged. It only alters transients,
  which is precisely the part of the run nobody is watching.

It was removed rather than shipped as a control costing a full float frame per
field of delay — 350 MB at 4K — to change nothing. If anyone brings it back, the
missing ingredient is a non-monotonic proc amp, not more delay.

### The taps sum. Averaging them deletes the attractor

`acc / tapCount` is the normalisation everyone reaches for and it is fatal. A
point on the gasket is reached by exactly **one** of the three maps, so dividing
by three hands it a third of its light every field — a loop gain of 0.33 wearing
a normalisation's clothes. Five rigs rendered pure black while the Gain control
read 1.12 and every test passed.

Summing is also what the glass does: a half-mirror splits a beam and the paths
**add** where they land. Nothing in an optical path divides by the number of
paths. Overlaps brighten and the soft clip is what stops them, which is where a
real amplifier stops them too.

### A tunnel is made of the screen edge, and needs Zoom BELOW 1

The classic feedback tunnel is not copies of what you put in front of the lens.
It is the **edge of the picture**, re-photographed: the camera sees the frame
boundary, that boundary appears inside the next field, and the field after has
both.

Above 1:1 every tap fetch lands *inside* the frame, so the camera never finds
its own edge, and an injected spot simply inflates into a smooth radial gradient
with no structure in it whatsoever. Four versions of the Mirror Tunnel preset
were spent tuning gain against this before the cause turned out to be the sign
of the zoom.

Below 1:1 the fetch reaches outside the frame, the shader returns nothing rather
than the clamped edge texel, and the nested frames appear.

### Loop gain and contractivity answer different questions

- *Does the picture converge on a shape?* — `Contractivity()`. Geometric, a
  property of the glass, nothing to do with brightness.
- *Does the picture survive?* — `LoopGain()`. Amplitude, nothing to do with what
  shape it settles into.

The first draft multiplied them together into one number and left Decay out of
it. Every preset was then tuned against a figure that said 0.98 while the rig
was actually running at 1.27, and every one of them blew out. Persistence is a
**parallel route into the same summing node**, so it adds; the camera's zoom
moves light around and does not amplify it, so it does not appear at all.

### `precise` is load-bearing in the df64 code, and nothing fails without it

Double-float arithmetic gets its extra bits from Dekker's split —
`cona - ( cona - a.x )`, which is only a no-op if you are allowed to
reassociate it. Apple's GLSL compiler is, and does.

The result compiles, links, runs, and **differs from the single-precision path**
— so comparing the two proves nothing — while silently having 24 bits again. It
was caught by looking at a picture: a 1e6 zoom came out in rectangular blocks
about eight pixels across, which is exactly the width at which a 24-bit mantissa
stops being able to tell two adjacent pixels apart at that magnification.

Marking every intermediate `precise` fixes it. Do not tidy them away.

### A starved iterator looks exactly like a precision failure

Both render a flat field. Past about 1e9 a 2048-iteration budget cannot resolve
anything near the boundary, so *both* precisions go flat and the Precision
control reads as dead. `tools/sweep.py` reported it as dead for this reason and
the sweep was what was wrong, not the plugin. When a deep zoom looks broken,
raise Iterations before suspecting the arithmetic.

### Focus is inside the loop, and that is the point

It is a `textureLod` bias on the tap fetches, not a blur pass. A low-pass filter
*inside* a feedback loop is what stops a hot rig collapsing into single-pixel
noise: sharp and hot is static, soft and hot is what everybody recognises as
video feedback. Moving it after the loop would blur the picture and change
nothing about what the loop does.

There is a floor to it. At LOD 2.6 the blur wipes detail faster than the gain
can rebuild it and the loop converges to a flat field — `Cell Structures` did
exactly that.

### NaN is permanent in a store that feeds itself

NaN times anything is NaN, and this buffer is its own input, so one NaN anywhere
survives every subsequent field for the life of the instance. It does not decay,
it cannot be cleared by turning the gain down, and it spreads through the taps
into its neighbours. Guarded in the loop pass **and** clamped, both, not either
— `esctest --guard` runs 400 fields of a deliberately hostile rig and then
checks the rig can still draw, because a guard that clamped the store to black
would pass the first half and have destroyed the instrument.

### A parameter change must NOT clear the frame store

The opposite of the fleet's GPU habit, and for vectrix's reason: what is going
round the loop is the instrument's memory and most of what the operator is
playing. Turn the gain down and the picture should fall away over a second the
way a real rig's does, not vanish between two frames. Only a resize and a fresh
instance clear it.

### A weak acceptance test passes a broken plugin

`--presets` originally asserted `mean > 0.002` — that *some* light reached the
screen. All twelve presets passed while five were rendering a flat wash and one
was a solid green rectangle. A feedback rig's characteristic failures are a
black frame and a saturated one, and "is anything lit" cannot see the second at
all. The check now measures the standard deviation of luma, and excludes dark,
flat and blown-out by name.

### The SDK's traps, all still live

- `ffglex::FFGLFBO::Release()` tests `depthBufferID` where it meant
  `colorTextureID` and leaks the colour texture. `Store` is raw GL rather than a
  subclass, which costs forty lines and sidesteps it.
- `ScopedFBOBinding` restores the binding and **not the viewport**. Every pass
  here sets its own, and the display pass restores the host's by hand from
  `pGL->HostFBO`.
- `SetParamInfo` clamps a default into 0..1 *before* `SetParamRange` can widen
  it, for `FF_TYPE_STANDARD` only.
- A display-only TEXT parameter **without** a `SetTextParameter` override makes
  `FF_INSTANTIATE_GL` fail for the whole plugin.
- `escapement_core` is an **OBJECT** library. In a STATIC archive the linker may
  drop `CFFGLPluginInfo`'s translation unit, giving a bundle that loads, exports
  `plugMain`, and reports no plugins.
- FFGL truncates parameter names at 16 characters and says nothing.
- Binding texture 0 to a sampler makes the Apple driver log
  "GLD_TEXTURE_INDEX_2D is unloadable" once per frame, burying anything else.

### The host's clock unit is measured, not guessed

FFGL never says what `SetTime` is in. Resolume sends milliseconds; the harness
sends seconds. `UpdateClock` measures the ratio of host time to `steady_clock`
over several frames and decides from that — the fleet's shared solution, copied
here deliberately rather than reinvented.
