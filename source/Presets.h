#pragma once

/**
    Factory presets.

    A feedback rig has a **small** set of settings that produce anything at all
    and a very large set that produce a black frame or a white one. That is not a
    failure of the design, it is what the instrument is: gain either side of
    unity is the difference between a picture that dies and one that runs away,
    and the interesting settings are a hair off the boundary. An operator handed
    forty sliders and a black frame has no way in.

    So these are the way in. Every one of them is a working operating point,
    reached by moving one control at a time from a rig that was already running,
    and each is a place from which nudging anything is rewarding rather than
    fatal.

    Values are host 0..1, because that is what the host stores and what a preset
    has to write. The physical quantity each one maps to is in the comment beside
    it -- those are the numbers that were actually chosen, and the 0..1 value is
    the inverse of Controls.cpp's mapping for it. Changing a range in Controls.cpp
    without recomputing these silently moves every preset.

    `-1` means the preset has no opinion and leaves the operator's value alone.
*/
namespace escapement::presets
{
/// The parameters a preset covers, in the order `kPresetParamIDs` in
/// Escapement.h binds them to host ids.
enum Param
{
	P_RIG = 0,
	P_TAPS,
	P_SEED,
	P_SYMMETRY,
	P_FOLD_MIRROR,
	P_ZOOM,
	P_ROTATE,
	P_PAN_X,
	P_PAN_Y,
	P_FOCUS,
	P_LENS,
	P_VIGNETTE,
	P_GAIN,
	P_PEDESTAL,
	P_GAMMA,
	P_HUE_ROTATE,
	P_SATURATION,
	P_CLIP,
	P_NOISE,
	P_DECAY,
	P_INJECT,
	P_INJECT_LEVEL,
	P_INJECT_SIZE,
	P_PALETTE,
	P_SPHERE,
	P_TILT,
	P_SPIN,
	P_ITERATIONS,
	P_ESCAPE_ZOOM,
	P_PRECISION,

	// Appended rather than filed under Camera where they belong, so that the
	// twelve rows below did not all have to be re-counted by hand when drift
	// arrived. The order here is the order kPresetParamIDs binds them.
	P_DRIFT,
	P_DRIFT_RATE,

	kParamCount
};

struct Preset
{
	const char* name;
	float values[ kParamCount ];
};

//---------------------------------------------------------------------------
// The table.
//
// Reading the columns: rig, taps, seed, symmetry, mirror, zoom, rotate, pan x,
// pan y, focus, lens, vignette, gain, pedestal, gamma, hue, saturation, clip,
// noise, decay, inject, level, size, palette, sphere, tilt, spin, iterations,
// escape zoom, precision, drift, drift rate.
//
// Every preset carries drift, and none of them carries zero. A rig with the
// hands off is a contraction mapping with constant coefficients: it converges on
// its attractor and then stands perfectly still, for ever. See ApplyDrift in
// Loop.h. A factory preset that dies ten seconds after it is chosen is not a
// working operating point, whatever it looks like in a screenshot.
//---------------------------------------------------------------------------
inline constexpr Preset kPresets[] = {
	//-------------------------------------------------------------------
	// The classic. One camera, one screen, zoomed a hair past 1:1 so the
	// picture crawls inwards for ever. This is the rig everybody has
	// accidentally built by pointing a camcorder at a television, and it is
	// the right first thing to see.
	//-------------------------------------------------------------------
	{ "Mirror Tunnel",
	  { 0.0f,   // Mirror
	    -1.0f, -1.0f,
	    0.0f,   // no fold
	    0.0f,
	    0.208f, // zoom 0.98 -- BELOW one, and that is the whole preset.
	            //
	            // A tunnel is not made of copies of what you put in front of the
	            // lens. It is made of the SCREEN EDGE, re-photographed: the
	            // camera sees the edge of the picture, that edge appears inside
	            // the next field, and the one after that has both. Above 1:1 the
	            // camera never sees its own edge -- every fetch lands inside the
	            // frame -- and an injected spot just inflates into a smooth disc
	            // with no structure in it at all, which is what the first four
	            // versions of this preset did.
	    0.699f, // rotate 0.01 rad per field -- what turns nested frames into a spiral
	    0.5f, 0.5f,
	    0.085f, // focus 0.2 -- nearly sharp, so the nested edges stay edges
	    0.5f,
	    0.08f,  // vignette. Gentle: it is inside the loop, so it compounds.
	    0.600f, // gain 0.96
	    0.5f, 0.5f,
	    0.658f, // hue 0.002 turns per field -- the colour never settles
	    0.55f,  // saturation 1.1
	    0.786f, // clip 0.85
	    0.0f,
	    0.050f, // decay 0.05 -- almost none. Persistence smears the nested edges
	            // into each other, and the edges are the picture.
	    3.0f,   // inject: grid -- something that REACHES THE FRAME EDGE, so there
	            // is an edge for the camera to find
	    0.550f, // level 0.19
	    0.720f, // size 0.37
	    0.0f,   // palette: the signal's own colour
	    0.0f, 0.5f, 0.5f, -1.0f, -1.0f, -1.0f , 0.50f, 0.523f } },

	//-------------------------------------------------------------------
	// Three half-scale taps on a triangle. The gasket is the attractor, so
	// it arrives on its own from whatever is in the store -- including
	// nothing but sensor grain, which is the setting worth watching it from.
	//-------------------------------------------------------------------
	{ "Sierpinski",
	  { 1.0f,   // Sierpinski
	    -1.0f, -1.0f, 0.0f, 0.0f,
	    0.5f,   // zoom exactly 1.0 -- the taps supply all the contraction
	    0.5f, 0.5f, 0.5f,
	    0.088f,  // focus 0.25 -- nearly sharp: the gasket wants its edges
	    0.5f,
	    0.10f,
	    0.700f, // gain 1.12 -- over unity, so the attractor fills rather than fades
	    0.5f, 0.5f,
	    0.640f, // hue 0.0016 per field
	    0.5f,
	    0.857f, // clip 0.9
	    0.10f,  // a little grain, so there is always something to converge from
	    0.0f,   // no persistence: the attractor is not a trail
	    4.0f,   // inject: noise
	    0.260f, // level 0.040
	    1.0f,
	    0.0f, 0.0f, 0.5f, 0.5f, -1.0f, -1.0f, -1.0f , 0.50f, 0.435f } },

	//-------------------------------------------------------------------
	// Four third-scale taps and a three-fold fold. The taps make the Koch
	// CURVE; the fold puts it on all three sides of a triangle. See Taps.cpp
	// on why the bump has to point outwards.
	//-------------------------------------------------------------------
	{ "Koch Snowflake",
	  { 2.0f,   // Koch
	    -1.0f, -1.0f,
	    0.18f,  // symmetry 3 -- this is what makes it a snowflake
	    0.0f,   // rotational fold, NOT mirrored
	    0.5f, 0.5f, 0.5f, 0.5f,
	    0.085f,  // focus 0.2
	    0.5f, 0.08f,
	    0.812f, // gain 1.30 -- a curve is measure zero and needs more than an area does
	    0.5f, 0.5f,
	    0.600f, // hue 0.0008 per field
	    0.5f, 0.857f,
	    0.10f, 0.153f, // a little persistence, so the thin line accumulates
	    4.0f,   // inject: noise
	    0.140f, // level 0.021
	    1.0f,
	    0.0f, 0.0f, 0.5f, 0.5f, -1.0f, -1.0f, -1.0f , 0.50f, 0.435f } },

	{ "Dragon",
	  { 3.0f,
	    -1.0f, -1.0f, 0.0f, 0.0f,
	    0.5f,
	    0.699f, // a slow rotation, because the dragon is worth turning
	    0.5f, 0.5f,
	    0.091f, 0.5f, 0.12f,
	    0.680f, // gain 1.09
	    0.5f, 0.5f, 0.640f, 0.5f, 0.857f,
	    0.10f, 0.0f,
	    4.0f,   // inject: noise
	    0.260f, // level 0.021
	    1.0f,
	    2.0f,   // Ice
	    0.0f, 0.5f, 0.5f, -1.0f, -1.0f, -1.0f , 0.95f, 0.435f } },

	//-------------------------------------------------------------------
	// Barnsley's fern. One of its four maps is singular -- the stem is a
	// line, not an area -- so the loop drops it and the stem is missing.
	// That is the honest result of a rig that can only pull; see Taps.cpp.
	//-------------------------------------------------------------------
	{ "Barnsley Fern",
	  { 4.0f,
	    -1.0f, -1.0f, 0.0f, 0.0f,
	    0.5f, 0.5f, 0.5f, 0.5f,
	    0.085f, 0.5f, 0.0f,
	    0.560f, // gain 0.90. Barnsley is the awkward one: its big map carries most
	           // of the frond on its own and wants ~1, while the three maps overlap
	           // heavily where they meet and blow out well before that.
	    0.5f, 0.5f,
	    0.560f, 0.45f,
	    0.500f, // clip 0.65 -- a low ceiling, because the three maps pile up
	            // where the fronds meet and the stem is already missing
	    0.12f, 0.0f,
	    4.0f, 0.200f, 1.0f,
	    0.0f, 0.0f, 0.5f, 0.5f, -1.0f, -1.0f, -1.0f , 1.00f, 0.520f } },

	//-------------------------------------------------------------------
	// A mirror wedge. The taps are unit-scale rotations, so the ONLY
	// contraction in this rig is the camera's zoom -- which is why the zoom
	// here is below 1 rather than above it, and why moving it past 1 whites
	// the picture out.
	//-------------------------------------------------------------------
	{ "Kaleidoscope",
	  { 5.0f,   // Kaleido
	    0.60f,  // 6 taps
	    -1.0f,
	    0.0f, 0.0f,
	    0.329f, // zoom 0.996 -- the only contraction in the rig
	    0.640f, // a slow turn
	    0.5f, 0.5f,
	    0.148f, 0.5f, 0.30f,
	    0.625f, // gain 1.00
	    0.5f, 0.5f,
	    0.680f, // hue 0.0026 per field
	    0.60f, 0.786f,
	    0.02f, 0.153f, // decay 0.15
	    1.0f,   // inject: dot
	    0.780f, 0.420f,
	    0.0f, 0.0f, 0.5f, 0.5f, -1.0f, -1.0f, -1.0f , 0.45f, 0.523f } },

	//-------------------------------------------------------------------
	// A rig built from a number. Nudge the seed for a different one --
	// every seed is framed, because a seeded set's bounds are measured
	// rather than assumed. See FitToFrame in Taps.h.
	//-------------------------------------------------------------------
	{ "Seeded Rig",
	  { 6.0f,   // Seeded
	    0.30f,  // 4 taps
	    0.402f, // seed 4021
	    0.0f, 0.0f,
	    0.5f, 0.5f, 0.5f, 0.5f,
	    0.091f, 0.5f, 0.200f,
	    0.594f, 0.5f, 0.5f, 0.620f, 0.5f, 0.857f,
	    0.10f, 0.0f,
	    4.0f,   // inject: noise
	    0.140f, // level 0.021
	    1.0f,
	    4.0f,   // Spectrum
	    0.0f, 0.5f, 0.5f, -1.0f, -1.0f, -1.0f , 0.55f, 0.435f } },

	//-------------------------------------------------------------------
	// The iterator bank, with c fixed. The loop is still running -- the
	// bank's picture is injected into it, so the Julia set has a rig's
	// persistence and colour walk on top of it rather than being a still.
	//-------------------------------------------------------------------
	{ "Julia Drift",
	  { 7.0f,   // Julia
	    -1.0f, -1.0f, 0.0f, 0.0f,
	    0.5f, 0.5f, 0.5f, 0.5f,
	    0.0f, 0.5f, 0.10f,
	    0.0f,   // gain 0 -- nothing goes round the geometric path
	    0.5f, 0.5f, 0.620f, 0.5f, 1.0f,
	    0.0f,
	    0.55f,  // decay 0.54 -- the bank's picture smears in time
	    5.0f,   // inject: the bank
	    0.869f, 1.0f,
	    3.0f,   // Ember
	    0.0f, 0.5f, 0.5f,
	    0.571f, // 256 iterations
	    0.0f,   // no zoom
	    0.0f , 0.60f, 0.523f } },// single precision is plenty at 1:1

	//-------------------------------------------------------------------
	// The deep zoom. Extended precision, because single runs out of
	// resolution around 1e5 and this preset starts past that.
	//-------------------------------------------------------------------
	{ "Mandelbrot Dive",
	  { 8.0f,   // Mandelbrot
	    -1.0f, -1.0f, 0.0f, 0.0f,
	    0.5f, 0.5f, 0.5f, 0.5f,
	    0.0f, 0.5f, 0.10f,
	    0.0f, 0.5f, 0.5f,
	    0.560f, 0.5f, 1.0f, 0.0f,
	    0.20f,
	    5.0f, 0.869f, 1.0f,
	    2.0f,   // Ice. Ember posterises a deep zoom into flat orange and black:
	            // its curve is three clamped ramps, which is right for the broad
	            // bands of a 1:1 view and far too contrasty for the filigree at
	            // 1e6, where almost every pixel is near the boundary. Julia keeps
	            // Ember, where the bands ARE broad.
	    0.0f, 0.5f, 0.5f,
	    0.850f, // 1060 iterations -- a deep zoom needs them, and starving it
	            // looks exactly like a precision failure: flat, blocky, wrong
	    0.462f, // 1e6 magnification
	    1.0f , 1.00f, 0.520f  } },// Extended

	//-------------------------------------------------------------------
	// The rescan: the rig's screen is a sphere and the camera is looking at
	// it. The loop underneath is an ordinary mirror rig, so the texture on
	// the globe is alive rather than a still being spun.
	//-------------------------------------------------------------------
	{ "Globe",
	  { 9.0f,   // Globe
	    -1.0f, -1.0f, 0.0f, 0.0f,
	    0.208f, // zoom 0.98 -- the SAME rig as Mirror Tunnel underneath, because
	            // that is what a rescan is: an ordinary feedback loop, re-shot off
	            // a curved screen. Built once with a zoom above 1:1 instead, it
	            // filled to a uniform field and the globe came out as a flat
	            // magenta ball with nothing on it -- a sphere needs something to
	            // WRAP, and a loop that has filled has no texture left in it.
	    0.699f, // rotate 0.01 rad per field
	    0.5f, 0.5f,
	    0.085f, // focus 0.2 -- sharp enough to keep the nested edges
	    0.5f,
	    0.20f,  // vignette
	    0.600f, // gain 0.96
	    0.5f, 0.5f,
	    0.658f, // hue 0.002 turns per field
	    0.55f,
	    0.786f, // clip 0.85
	    0.0f,
	    0.050f, // decay 0.05 -- 1.01 round the loop
	    3.0f,   // inject: grid, so there is an edge for the camera to find
	    0.550f, // level 0.19
	    0.720f, // size 0.37
	    1.0f,   // Phosphor. NOT the signal's own colour, and the reason is the
	            // hue rotation: it turns about the LUMA axis, and grey is a fixed
	            // point of that. A rig injecting a white grid therefore stays
	            // grey however far the colour walks, and a grey ball with soft
	            // shading on it reads as a render of nothing. A palette maps the
	            // luma it does have into something to look at.
	    1.0f,   // fully spherical
	    0.611f, // tilt 0.35 rad
	    0.612f, // spin 0.05 turns a second
	    -1.0f, -1.0f, -1.0f, 0.40f, 0.435f } },

	//-------------------------------------------------------------------
	// Hot, soft and just over unity: the operating point where a rig stops
	// resolving into a shape and starts producing the cell-like structures
	// the Light Herder's device is full of. Focus is doing the work -- a
	// low-pass filter inside a loop with gain is what makes structure at a
	// scale rather than noise at a pixel.
	//-------------------------------------------------------------------
	{ "Cell Structures",
	  { 0.0f,
	    -1.0f, -1.0f,
	    0.0f, 0.0f,
	    0.300f, // zoom 0.9955 -- pulled back a hair, so the frame edge drains it
	    0.699f, // rotate 0.01 rad per field -- and it is NOT decoration. Without
	            // it the pattern has nothing stirring it, the domains lock, and
	            // the whole thing collapses to a dot within a few hundred fields.
	            // Found by setting it to zero and watching that happen.
	    0.5f, 0.5f,
	    0.148f, // focus 1.0 -- the diffusion term. Soft enough to couple
	            // neighbouring pixels, sharp enough that the gain can rebuild
	            // what it blurs away
	    0.5f,
	    0.10f,
	    0.660f, // gain 1.056 -- just over unity, and the whole preset is this
	            // number. This is a reaction-diffusion system: local gain above
	            // one, lateral coupling from the blur, and a low ceiling to stop
	            // it. What comes out is a Turing pattern -- labyrinthine domains
	            // that keep reorganising. The band is NARROW: at 1.048 it
	            // collapses to a dot and at 1.15 it saturates to a flat field.
	    0.5f, 0.5f,
	    0.700f, // hue 0.004 per field
	    0.700f, // saturation 1.40 -- ABOVE UNITY, and this is the reaction term.
	            // The instability is in the CHROMA, not the luma: chroma gain
	            // over one amplifies whatever colour deviation a pixel has, the
	            // blur spreads it to its neighbours, and the clip stops it. That
	            // is a reaction-diffusion system, and its domains are colours.
	            // Below 1.0 there is no reaction and the picture collapses to a
	            // dot -- 1.00 dies, 1.1 patterns coarsely, 1.4 gives fine domains. Found by
	            // bisecting it, after the preset was first written at 0.7 on the
	            // assumption that the garish colour was a fault to be tuned out.
	            // The colour IS the mechanism.
	    0.500f, // clip 0.65
	    0.25f,  // noise -- the forcing. With nothing injected this is the only
	            // thing feeding the loop, and it is what the pattern grows from
	    0.0f,   // no persistence: it smooths the domain walls away
	    0.0f,   // inject: nothing. The loop finds its own picture
	    0.0f, -1.0f,
	    0.0f, 0.0f, 0.5f, 0.5f, -1.0f, -1.0f, -1.0f, 0.20f, 0.520f } },

	//-------------------------------------------------------------------
	// Zoom and rotation together, which is what turns a tunnel into a
	// spiral: a point moving inwards while the frame turns traces one.
	//-------------------------------------------------------------------
	{ "Galaxy",
	  { 0.0f,
	    -1.0f, -1.0f, 0.0f, 0.0f,
	    0.240f, // zoom 0.9885 -- pulled back, so the nested frames recede
	    0.760f, // rotate 0.026 rad per field
	    0.5f, 0.5f,
	    0.112f, 0.46f, 0.18f,
	    0.620f, // gain 0.99
	    0.5f, 0.5f,
	    0.690f, 0.62f, 0.786f,
	    0.03f, 0.100f, // decay 0.098
	    1.0f, 0.608f, 0.300f,
	    2.0f,   // Ice
	    0.0f, 0.5f, 0.5f, -1.0f, -1.0f, -1.0f , 0.50f, 0.523f } },
};

inline constexpr int kCount = int( sizeof( kPresets ) / sizeof( kPresets[ 0 ] ) );

} // namespace escapement::presets
