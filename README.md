# Escapement

**A video feedback rig for Resolume — and the fractals it settles into.**

Escapement is not a fractal generator with a feedback effect bolted on. It is a
model of an optical video feedback rig: a camera looking at a screen that is
showing what the camera saw a field ago, through glass that splits the light
into several paths, with a proc amp in the cable and an operator playing the
gain against unity.

The fractals are what that loop settles into. There is no Mandelbrot routine
driving the Mandelbrot rig's picture into a texture; there is a bank of
iterating amplifiers. There is no code anywhere that draws a Sierpinski gasket.
There are three half-scale taps on the vertices of a triangle, and the gasket is
where the loop goes.

Two plugins:

| | |
|---|---|
| **Escapement** | Source. A rig with nothing in front of the lens but what it makes itself. |
| **Escapement Feed** | Effect. The same rig with your clip in front of the lens — injected *into* the loop, so the fractal is built out of the footage. |

---

<!-- downloads:start -->

## Download

**[v0.1.2](https://github.com/stoatworks-labs/escapement/releases/tag/v0.1.2)** — prebuilt for macOS and Windows. Pick your platform:

<details>
<summary><b>macOS</b> — Universal (Apple Silicon + Intel)</summary>

| Build | Download | Size |
| --- | --- | --- |
| Universal (Apple Silicon + Intel) · .dmg disk image | [`escapement-0.1.2-macos-universal.dmg`](https://github.com/stoatworks-labs/escapement/releases/download/v0.1.2/escapement-0.1.2-macos-universal.dmg) | 453 KB |
| Universal (Apple Silicon + Intel) · .zip archive | [`escapement-macos-universal.zip`](https://github.com/stoatworks-labs/escapement/releases/latest/download/escapement-macos-universal.zip) | 399 KB |

</details>

<details>
<summary><b>Windows</b> — x64</summary>

| Build | Download | Size |
| --- | --- | --- |
| x64 · .exe installer | [`escapement-0.1.2-windows-x86_64-setup.exe`](https://github.com/stoatworks-labs/escapement/releases/download/v0.1.2/escapement-0.1.2-windows-x86_64-setup.exe) | 234 KB |
| x64 · .zip archive | [`escapement-windows-x86_64.zip`](https://github.com/stoatworks-labs/escapement/releases/latest/download/escapement-windows-x86_64.zip) | 247 KB |

</details>

All builds, checksums and release notes: [github.com/stoatworks-labs/escapement/releases](https://github.com/stoatworks-labs/escapement/releases).

macOS builds are signed and notarised and open normally. The Windows builds are unsigned, so SmartScreen warns once.

<!-- downloads:end -->

## What's in it

**Ten rigs.** Mirror, Sierpinski, Koch, Dragon, Barnsley Fern, Kaleidoscope,
Seeded, Julia, Mandelbrot, Globe.

**Endless zoom and pan, with no precision limit.** The camera's zoom is a
per-field multiplier applied to the picture, not a coordinate anyone stores.
After a thousand fields at 1.02 the magnification is 1.02¹⁰⁰⁰ — a number that
never exists anywhere in the plugin, so there is nothing to overflow and no
mantissa to run out of. Leave it running for an hour.

**A deep zoom where there *is* a coordinate.** The Julia and Mandelbrot rigs
hold one, so they have the limit the others do not: single precision runs out
around 1e5, and the Extended setting uses double-float arithmetic to reach about
1e13 for roughly three times the cost.

**Seeded rigs.** Type a number, get a rig. Every seed is framed, because a
seeded attractor's bounds are measured rather than assumed — seed 4021 settles
inside 1.45 frame units and seed 7 spreads across 2.68, and any single constant
would crop one and shrink the other to a speck.

**3D by rescan.** The Globe rig puts the rig's screen on a sphere and points the
camera at it. The loop underneath keeps running, so the texture on the globe is
alive rather than a still that spins.

**Twelve factory presets**, each a working operating point — because a rig has a
small set of settings that produce anything at all and a very large set that
produce a black frame or a white one, and forty sliders over a black frame is no
way in.

---

## Install

Download the release, unzip, and drop both bundles into:

- **macOS** — `~/Documents/Resolume Arena/Extra Effects/`
- **Windows** — `%USERPROFILE%\Documents\Resolume Arena\Extra Effects\`

Restart Resolume. `Escapement` appears under Sources; `Escapement Feed`
under Effects.

macOS builds are universal (Apple Silicon and Intel). There is no signed release
yet, so Gatekeeper will object once — see `docs/UNSIGNED.md`.

## Build

```bash
git clone --recursive https://github.com/stoatworks-labs/escapement
cd escapement
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
cmake --install build      # straight into Resolume's plugin folder
```

## Playing it

The one control that matters is **Gain**, and it matters because it has a
threshold in it. Below unity every trip round the loop is dimmer than the last
and the picture decays into whatever is being injected. Above unity it grows
until the clip stage stops it, and the shape it grows into is the attractor.
Unity is at 0.625 of the slider's travel, exactly, so you can find it in the
dark.

**Hue Rotate** is the difference between a picture and a performance. With it at
zero the loop converges to a fixed point and stops. With a little, it cannot
converge — a fixed point would have to be a colour that is its own rotation — so
the structure keeps re-entering itself and the colour walks the wheel forever.
Two thousandths of a turn per field is plenty.

**Focus** is a lens that is slightly soft, and it is inside the loop. Sharp and
hot breaks up into single-pixel noise; soft and hot is the thing everyone
recognises as video feedback. It is measured as a fraction of the frame, so a
preset looks the same on your preview and on a 4K output.

**Zoom below 1 gives you the tunnel.** A feedback tunnel is made of the screen's
own edge, re-photographed — so the camera has to be able to see past the frame,
which means pulling back, not pushing in. Above 1:1 you get expanding blooms
instead, which is a different and also good thing.

**Drift is what keeps it alive, and it is on by default.** A feedback loop with
every knob held still is a contraction mapping: it converges on its attractor and
then stops, for ever, and zooming does not help because a self-similar attractor
magnified is the same attractor. Real rigs never go still because a person is
always moving the cameras. Drift is that person — slow, wandering, and running at
mutually irrational rates so it never repeats. Turn it to zero and the rig will
settle within seconds, which is worth doing once just to see it happen.

**Field Rate** is the rig's own clock, not Resolume's. The picture evolves at
the same speed whether the host is managing 60 fps or dropping to 24.

---

## Credit where it is due

The rig this models is real: **The Light Herder**'s 4K Video Feedback Fractal
Device — cameras, 4K screens, half-mirrored glass and analogue knobs, with the
operator as part of the loop. Its video is
[here](https://youtu.be/koDCabeh5kQ), the claim in its description is *fractals
without a computer*, and that claim is this plugin's entire architecture. See
`ATTRIBUTIONS.md`.

## Licence

MIT. See `LICENSE`.

Parts of this repository were written with AI assistance; see the disclaimer in
`ATTRIBUTIONS.md`.
