# Attributions

## The idea

Escapement is a model of an **optical video feedback rig**, and the rig it is
modelled on is real.

- **The Light Herder** — [thelightherder.com](https://www.thelightherder.com) —
  built the 4K Video Feedback Fractal Device: cameras pointed at 4K screens
  through half-mirrored glass, with analogue knobs and an operator moving the
  cameras by hand. Nothing about it is computed. The video that started this
  plugin is
  [*4K Analog Video Feedback Device Complete!*](https://youtu.be/koDCabeh5kQ),
  and the claim in its description — *fractals without a computer* — is the
  whole design brief: the fractals are the loop's attractor, not a formula
  anyone evaluates.

  No code, assets or measurements were taken from that device. What was taken is
  the architecture: taps through glass, a proc amp in the loop, and an operator
  playing gain against unity.

## The mathematics

The iterated function systems are used as published, and can be checked line by
line against their sources in `source/Taps.cpp`.

- **Sierpinski gasket**, **Koch curve** — standard similarity systems.
- **Heighway dragon** — the two-map system at 1/√2.
- **Barnsley fern** — Michael Barnsley, *Fractals Everywhere* (1988). The four
  maps are reproduced exactly, including the singular first map that draws the
  stem.
- **Hutchinson operator** — John E. Hutchinson, "Fractals and self similarity",
  *Indiana University Mathematics Journal* 30 (1981). The loop is a hardware
  realisation of it; `Taps.h` says where the inverse comes in.

The Mandelbrot deep-zoom point is the Misiurewicz point commonly used in the
deep-zoom literature.

## Code

- **FFGL SDK** — [github.com/resolume/ffgl](https://github.com/resolume/ffgl),
  pinned at `b1afaf9`. MIT.
- **OpenFX SDK subset** — vendored under `external/openfx`. BSD-3-Clause.
- **`lowbias32`** — Chris Wellons' 32-bit integer hash, in `source/Hash.h`.
  Public domain.
- **Double-float arithmetic** — Dekker's splitting and the two-sum/two-product
  algorithms, as used by every `df64` GLSL implementation.

## AI disclaimer

Parts of this repository were written with AI assistance (Claude). Everything
here was built and verified against a real Resolume install and the repo's own
offline harness; the traps recorded in `AGENTS.md` and `docs/NOTES.md` are ones
that were actually hit, not ones that were predicted.
