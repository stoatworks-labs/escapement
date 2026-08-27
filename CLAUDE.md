# escapement

A video feedback rig, and the fractals it settles into, as **two** FFGL plugins
for Resolume Arena/Avenue: a source (`Escapement`) and an effect that feeds the
clip into the loop (`Escapement Feedback`). C++/GLSL, CMake MODULE → universal
`.bundle` (macOS) + Windows `.dll`. Public MIT repo.

Read `AGENTS.md` before changing the loop, the taps or the proc amp.

## Commands (CMake)
- Configure: `cmake -B build -DCMAKE_BUILD_TYPE=Release`
- Fast dev build: add `-DCMAKE_OSX_ARCHITECTURES=arm64`
- Build: `cmake --build build`
- Install both bundles to Resolume: `cmake --install build`
- Render a frame offline: `./build/esctest --out /tmp/f.png --fields 300`
- A factory preset: `./build/esctest --out /tmp/f.png --preset 3`
- The effect over a test clip: `./build/esctest --out /tmp/f.png --effect`
- Contact sheet of every preset: `./build/esctest --contact /tmp/sheet.png`
- List parameters: `./build/esctest --list`
- Set anything by name: `--set "Gain=0.6" --set "Rig=1"`

## Browser demo
- Serve it: `python3 -m http.server 8794 --directory demo`
- The shaders are copied from `source/Shaders.cpp`, not rewritten. Prove it:
  `python3 demo/tools/check_shaders.py`
- The kit under `demo/vendor/` is vendored from
  `stoatworks-backend/resolume-demo/kit/` — fix bugs THERE, not here.
- NOT deployed. `wrangler.toml` is written and untested; the About/project URLs
  it implies do not exist yet.

## Verify
- Everything: `tools/verify.sh`
- The tap sets against published constants: `./build/esctest --taps`
- The rig's clock is frame-rate independent: `./build/esctest --rate`
- Loop gain predicts the picture: `./build/esctest --stability`
- A hostile rig leaves no NaN: `./build/esctest --guard`
- Every preset draws something with structure in it: `./build/esctest --presets`
- Every preset is still MOVING once settled: `./build/esctest --liveness`
- Chroma gain above unity is what keeps a rig alive: `./build/esctest --reaction`
- How alive is one rig? `./build/esctest --motion --preset 11`
- No dead controls: `python3 tools/sweep.py`

## Notes
- **The fractals are the loop's attractor, not a formula.** A tap set is an
  iterated function system; `Taps()` returns three affine maps and the gasket
  arrives on its own. If you are tempted to draw a fractal, the rig is wrong
  somewhere.
- **The taps SUM. They are not averaged.** Averaging is a loop gain of 1/N
  dressed up as a normalisation and it deletes the attractor — five rigs
  rendered black before this was fixed.
- **A tunnel is made of the SCREEN EDGE**, re-photographed. It needs Zoom
  *below* 1 so the camera can see past the frame. Above 1:1 an injected spot
  inflates into a smooth disc with no structure in it.
- **The rig keeps its own clock.** Fields, not frames. `esctest --rate` is the
  test that fails if anyone makes it one trip round the loop per rendered frame.
- **A rig with the hands off goes still, and that is Banach's theorem, not a
  bug.** A contraction mapping with constant coefficients has exactly one
  attracting fixed point; the loop reaches its attractor and then maps it to
  itself for ever. Endless zoom does not help — a self-similar attractor
  magnified is the same attractor. **Drift** is the only thing that keeps a rig
  alive, it is on by default in every preset, and `--liveness` is the test.
- **Autonomous life is a CHROMA instability, not a delay.** Saturation above
  unity is the reaction term, Focus is the diffusion, and the clip is the
  ceiling: that is a reaction-diffusion system whose domains are colours. It is
  what `Cell Structures` is. Below 1.0 chroma gain the same rig collapses to a
  dot — `--reaction` is the pair that proves it.
- **A loop delay was tried and removed.** Latency ought to destabilise a loop
  (Ikeda), and it measurably does not here: a soft clip is monotone, so positive
  feedback through it is bistable and settles, and at a fixed point every delayed
  copy is identical anyway. It cost a float frame per field of delay and moved a
  settled picture by 0.005. See AGENTS.md.
- **Drift depth scales with the rig's compliance, `1 - c`.** A camera nudge moves
  an attractor by about `t/(1-c)`, which spans two orders of magnitude between
  Sierpinski (stiff) and a mirror rig at 0.98 (compliant). One depth cannot serve
  both; the first attempt proved it.
- **Anything animated runs on FIELD time**, never the host clock. Drift and spin
  advance by `fields / fieldRate`, per field, inside the loop — hoisting either
  to once per frame makes the picture depend on the host's frame rate and
  `--rate` fails.
- **A parameter change must NOT clear the frame store** — the opposite of the
  fleet's GPU habit. The store is the instrument's memory.
- **Every `df64` intermediate is `precise`.** Without it Apple's compiler
  reassociates Dekker's split away and the deep zoom silently has 24 bits. It
  does not fail to compile and it still differs from the single-precision path.
- **Loop gain and contractivity are different questions.** Amplitude vs
  geometry. Running them together is what mistuned every preset in the first
  draft.
- Focus is a `textureLod` bias on the tap fetches — a low-pass filter *inside*
  the loop, which is what stops a hot rig collapsing into single-pixel noise.
  Not a blur to be moved to the end.
- `sample`, `input`, `output`, `filter`, `common`, `active` are GLSL reserved
  words. Shader errors surface only at runtime, in the diagnostics log.
- FFGL truncates every parameter name at 16 characters. `esctest --names`.
- `SetParamInfo` clamps a STANDARD default into 0..1 before `SetParamRange` can
  widen it, so every numeric parameter is 0..1 and mapped in `Controls.cpp`.
- Override `SetTextParameter` to return FF_SUCCESS for the About block, or no
  host can instantiate the plugin at all.
- `escapement_core` is an OBJECT library, not STATIC — the plugin registers
  itself from a file-scope constructor nothing references by name.
- macOS build must be universal. Verify with `lipo`, never the build log.
- **The constructor defaults ARE the Mirror Tunnel preset, by hand.** They went
  out of step once — the preset was retuned and the defaults were not, so the
  plugin opened on a rig at 1.28 round the loop that saturated to white. Retune
  one, retune the other.
- **GLSL ES 3.00 has no `precise`**, so the browser demo's port strips it and a
  deep zoom there may be coarser than in the plugin. Disclosed on the page.
- Public repo. "Commit" = commit **and** push.

## Not done yet

- **OpenFX target.** The OFX SDK subset is vendored and the CMake pattern is in
  orrery, but Escapement's picture is a GPU feedback loop and an OFX host wants
  a CPU render of an arbitrary frame in arbitrary order — which a loop with
  history cannot answer without being reimplemented. Deliberately deferred.
- **CI, Windows build, plugin-bench expectation.** The web demo exists and runs
  locally; it has never been deployed.
- **`StoatworksAbout.h` is a hand-written placeholder.** Escapement has no entry
  in the website's `projects.json`, so `sync-about.py` cannot generate it and
  the four About buttons point at pages that do not exist yet.
- **Two presets want more work**: `Cell Structures` is stable but reads as flat
  wedges rather than cells, and `Mandelbrot Dive` is correct but harshly
  coloured.

## Diagnostics

`source/Diag.{h,cpp}` — log file only, no crash handler (this runs inside
Resolume), no bundle command. It covers the failures that all look identical
from outside ("it does nothing"): a shader that will not compile, a frame store
that would not allocate, and a tap dropped as singular.

    ~/Library/Logs/escapement/escapement.YYYY-MM-DD.log
