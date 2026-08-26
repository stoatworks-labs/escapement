#pragma once

#include <array>
#include <cstdint>

/**
    The taps: what the camera sees of the screen, and how many of it.

    A **tap** is one path from the screen back into the camera. A bare camera
    pointed at a monitor is one tap. Put a half-mirror in front of the lens and
    you get two, at different angles and scales. Put in a mirror wedge and you
    get as many as the wedge has faces. The Light Herder's device is a rig for
    building tap sets out of glass, and everything on its screens is what those
    taps settle into.

    A tap set is an **iterated function system**, and its attractor is a fractal.
    That is not an analogy and it is not how this file approximates one: three
    taps at half scale on the vertices of a triangle have the Sierpinski gasket
    as their attractor, and a loop carrying those three taps converges on it from
    any starting image at all, including noise. Nothing here evaluates a fractal.
    `Sierpinski()` returns three affine maps.

    ## Forward maps here, inverse maps in the shader

    Every map in this file is the **contraction** `w`, written the way the
    mathematics writes it: where a point in the frame *goes*. The Hutchinson
    operator that the loop is a hardware realisation of,

        (T f)(x) = max over i of  f( w_i^-1 (x) )

    needs the *inverse*, because a fragment shader pulls -- it asks "what was at
    the place this pixel came from" rather than pushing pixels forward. So the
    shader is handed `w_i^-1`, inverted once per frame by `Inverse()` on the CPU.

    Keeping the forward map as the thing that is written down matters more than
    it looks. The published constants for these systems are all forward maps, so
    a reader can check `Koch()` against a textbook line by line; a file full of
    pre-inverted matrices can only be checked by inverting them again. It also
    means `Contractivity()` is reading the number it claims to read -- the
    inverse of a contraction is an expansion, and its determinant would say the
    opposite of what the guard needs to know.

    ## Contractivity is a stability condition, not a style guide

    An IFS has an attractor only if every map contracts. A tap set carrying a map
    with `|det| >= 1` does not converge on anything: the loop's gain around that
    path exceeds one and the picture saturates to a flat field, which is what a
    real rig does when you zoom past 1:1 and let it run. `Contractivity()` returns
    the worst map's singular value so the loop can say so rather than the operator
    watching the screen go white and assuming the plugin has crashed.

    The camera's own zoom is deliberately NOT included in that number. It is the
    operator's hands, it is allowed to be an expansion, and being able to hold a
    rig just past 1:1 is most of what makes the endless zoom work -- see
    `Loop.h`.
*/
namespace escapement
{
/// One tap: an affine map of the frame, `p -> M p + t`, in frame coordinates
/// that run -1..1 across the SHORT edge with y up and the origin at the centre.
///
/// Stored row-major as `m[0] m[1]` over `m[2] m[3]`, which is the order the
/// shader's `mat2` wants and the order a textbook writes.
struct Tap
{
	float m[ 4 ] = { 1.0f, 0.0f, 0.0f, 1.0f };
	float tx     = 0.0f;
	float ty     = 0.0f;

	/// How much of the previous field this tap carries. A half-mirror splits
	/// the light; four taps through one do not each arrive at full strength.
	float weight = 1.0f;

	/// Per-tap hue offset in turns. A real half-mirror is not colour neutral
	/// and the dielectric coating shifts with angle of incidence, so the taps
	/// of one rig are not colour matched. Zero is a perfect mirror.
	float hue = 0.0f;
};

/// The most taps one rig can carry. Eight is where a mirror wedge stops being
/// something you could build and starts being a light box, and it is also the
/// point where the loop pass costs more than the rest of the plugin put
/// together -- every tap is another dependent texture fetch per pixel per field.
inline constexpr int kMaxTaps = 8;

/// A complete tap set.
struct TapSet
{
	std::array< Tap, kMaxTaps > taps = {};
	int count                        = 1;
};

/// The tap configurations the rig can be wired into.
///
/// The first six are pure tap sets: their whole behaviour is the maps this file
/// returns, and the fractal is the attractor of those maps. `Julia` and
/// `Mandelbrot` are not -- those two run the iterator bank instead, and appear
/// in this enum only because the operator picks all of them from one control.
/// `RigUsesTaps()` is the line between them.
enum class Rig
{
	Mirror = 0,  ///< One tap. A camera and a screen, which is the whole classic rig.
	Sierpinski,  ///< Three half-scale taps on a triangle. Gasket.
	Koch,        ///< Four third-scale taps, two of them turned +-60 degrees. Snowflake.
	Dragon,      ///< Two taps at 1/sqrt2 and +-45 degrees. Heighway dragon.
	Fern,        ///< Barnsley's four maps, including the degenerate one that draws the stem.
	Kaleido,     ///< N taps evenly spaced in rotation. A mirror wedge.
	Seeded,      ///< N contractions derived from the seed.
	Julia,       ///< Iterator bank, c fixed by the operator.
	Mandelbrot,  ///< Iterator bank, c taken from the pixel.
	Globe,       ///< One tap, re-projected onto a sphere and re-shot.

	Count
};

const char* RigName( Rig rig );

/// True when this rig's picture comes from the tap set, false when it comes from
/// the iterator bank. The two paths share the frame store, the proc amp and the
/// optics; they differ only in what feeds them.
bool RigUsesTaps( Rig rig );

/// True when this rig re-projects the field onto a sphere before re-shooting it.
bool RigIsRescan( Rig rig );

/// Build the tap set for a rig.
///
/// `count` is honoured only by the rigs that have a choosable number of taps
/// (`Kaleido`, `Seeded`); the named systems return the number of maps they are
/// defined to have, and asking Sierpinski for four taps gets three.
///
/// `seed` is used only by `Seeded`.
TapSet Taps( Rig rig, int count, uint32_t seed );

/// Invert an affine tap. Returns false and leaves `out` untouched when the map
/// is singular -- which `Fern()` really does return, its first map having
/// determinant zero because the stem of a fern is a line and not an area.
///
/// A singular tap is legal in the forward direction and cannot be pulled
/// through, so the loop drops it rather than dividing by zero and poisoning the
/// frame store with NaN for the life of the instance.
bool Inverse( const Tap& tap, Tap& out );

/// The largest singular value across the set: the factor by which the most
/// expansive tap stretches the frame, and so the loop gain around the worst
/// path.
///
/// Below 1 the set has an attractor. At or above 1 it does not, and the loop
/// will saturate no matter what the proc amp does.
float Contractivity( const TapSet& set );

/**
    Rescale and recentre a tap set so its attractor fills `extent` of the frame.

    The named rigs do not need this -- their attractors are fixed shapes and
    their framing constants are measured once and written down. A **seeded** rig
    does, because its attractor is not knowable before the seed is: four maps
    from seed 4021 settle inside 1.45 frame units and the same four from seed 7
    spread across 2.68, so any single constant either crops one seed or leaves
    the other a speck in the middle. An operator nudging the seed to find a rig
    they like is entitled to have every stop framed.

    The bounds are **measured, by playing the chaos game** on the set: iterate a
    point through randomly chosen maps and take the box it visits. That is a
    Monte Carlo estimate and it is deliberately not a bound -- the true attractor
    can reach slightly past what a finite orbit visited -- so `extent` leaves
    room. There is no closed form to prefer over it: the attractor of an
    arbitrary IFS has no analytic bounding box.

    Deterministic despite the name: the orbit is driven by `Hash32`, so the same
    set fits to the same box on every machine and the show laptop and the rack
    machine frame seed 4021 identically.
*/
TapSet FitToFrame( const TapSet& set, float extent );

/// The tap set a rig would have with the camera's own transform folded in.
///
/// Kept separate from `Taps()` because the camera is the operator and the taps
/// are the glass: `Taps()` alone is a property of the rig and is what the
/// harness checks against published constants, and this is what the loop
/// actually samples through.
///
/// `zoom` is per field and multiplies, `rotate` is radians per field, and
/// `panX`/`panY` are in frame units per field. All four are applied before the
/// taps, which is what makes them behave like moving the camera rather than
/// moving the screen.
TapSet WithCamera( const TapSet& set, float zoom, float rotate, float panX, float panY );

} // namespace escapement
