#pragma once

#include "Controls.h"
#include "Taps.h"

#include <cstdint>

/**
    The loop: a rig's clock, and the state of its signal path.

    Everything in here is plain arithmetic with no OpenGL in it, which is what
    lets the offline harness drive the real code rather than a copy of it.

    ## The rig has its own clock

    A real feedback rig runs at the field rate of the screen inside it. Whatever
    is recording it, whatever the recording is played back at, the loop went
    round sixty times a second. That is not a detail: the number of trips round
    the loop **is** the number of times the tap set has been applied, and the
    attractor emerges after enough of them. A rig that went round the loop 24
    times a second would be a visibly different instrument.

    So `Advance()` takes the host's elapsed time and returns a whole number of
    fields at the rig's own rate. Resolume at 60 fps with the rig at 60 gets one
    field per frame; Resolume dropping to 24 gets two or three, and the picture
    evolves at the same rate as before. The alternative -- one trip round the
    loop per rendered frame -- makes the instrument change character whenever the
    machine is busy, which for something being recorded or run to a timecode is
    not a performance quirk but a bug.

    ## What is deliberately NOT reset

    **A parameter change must never clear the frame store.** This is the fleet's
    GPU habit reversed, and it is reversed for the same reason vectrix's delay
    line keeps its contents through a knob move: what is going round the loop is
    the instrument's memory, and it is most of what the operator is playing. Turn
    the gain down and the picture should fall away over the next second the way a
    real rig's does, not vanish between two frames. The only things that clear
    the store are a resize and a fresh instance.

    ## Two different questions, and they were conflated once

    **Does the picture converge on a shape?** That is `Contractivity()`, in
    Taps.h. Geometric, a property of the glass alone, and nothing to do with
    brightness. The camera's zoom multiplies it and is allowed to be an
    expansion -- holding a rig just past 1:1 is most of what makes an endless
    zoom.

    **Does the picture survive?** That is `LoopGain()`. Amplitude, and nothing to
    do with what shape it settles into. The taps SUM rather than averaging (see
    Shaders.cpp on why averaging deletes the attractor), so the worst case is
    `gain x sum of tap weights`, plus Decay, which adds because persistence is a
    parallel route into the same summing node.

    Running the two together in one number is what made the first set of presets
    read as safe at 0.98 while actually running at 1.27, and every one of them
    blew out.

    Above 1 the loop grows until the clip stage stops it, which is not a failure
    -- it is the operating point for most of what a rig is worth watching for.
    Escapement therefore does not clamp it. What it does is report it, so that
    "the picture went white" has a number attached to it in the diagnostics log
    rather than being a mystery.
*/
namespace escapement
{
/// The signal path, in engineering units, for one frame.
///
/// Filled from the host's 0..1 parameters by `Escapement::CurrentLoop()`. Every
/// per-field quantity in here has already been multiplied by Speed.
struct LoopParams
{
	Rig rig          = Rig::Mirror;
	int tapCount     = 3;
	uint32_t seed    = 1;
	int symmetry     = 1;     ///< Rotational fold order. 1 is no fold.
	bool foldMirror  = false; ///< Mirror each wedge, as a real wedge does.
	float fieldRate  = 60.0f; ///< Fields per second.

	// Camera, per field.
	float zoom   = 1.0f;
	float rotate = 0.0f;
	float panX   = 0.0f;
	float panY   = 0.0f;

	// The operator's hands.
	float drift     = 0.0f; ///< How far they wander. 0 is a rig left alone.
	float driftRate = 0.05f;///< Cycles per second.

	// Optics.
	float focus    = 0.0f;
	float lens     = 0.0f;
	float vignette = 0.0f;

	// Proc amp, per field.
	float gain       = 1.0f;
	float pedestal   = 0.0f;
	float gamma      = 1.0f;
	float hueRotate  = 0.0f;
	float saturation = 1.0f;
	float clip       = 1.0f;
	float noise      = 0.0f;
	float decay      = 0.0f;

	// Injection.
	Inject inject      = Inject::Dot;
	float injectLevel  = 1.0f;
	float injectSize   = 0.2f;
	float injectX      = 0.0f;
	float injectY      = 0.0f;

	// Iterator bank.
	int iterations     = 256;
	float juliaX       = -0.4f;
	float juliaY       = 0.6f;
	double escapeZoom  = 1.0;
	double escapeX     = 0.0;
	double escapeY     = 0.0;
	Precision precision = Precision::Single;

	// Rescan.
	float sphere = 0.0f;
	float tilt   = 0.0f;
	float spin   = 0.0f;
	float light  = 0.5f;

	// Output.
	Palette palette     = Palette::Signal;
	float paletteShift  = 0.0f;
	float opacity       = 1.0f;
	MaskMode maskMode   = MaskMode::Over;
	float mix           = 1.0f;
};

/// What the loop pass needs, resolved once per frame on the CPU.
struct LoopState
{
	/// The taps, already composed with the camera and already INVERTED, because
	/// a fragment shader pulls rather than pushes. See Taps.h.
	TapSet inverseTaps;

	/// Taps that could not be inverted and have been dropped. Barnsley's stem is
	/// the one that really happens; anything else here means a seeded rig has
	/// produced a degenerate map and is worth a line in the log.
	int droppedTaps = 0;

	/// Worst-case gain around the loop, camera and proc amp included.
	float loopGain = 1.0f;

	/// The tap set's own contraction, before the camera. What decides how stiff
	/// the attractor is against the hands -- see ApplyDrift.
	float contractivity = 1.0f;

	/// Sum of the surviving taps' weights. NOT a divisor -- the taps sum, and
	/// dividing by this is exactly the averaging that deletes the attractor.
	/// It is uploaded as a diagnostic and used by `LoopGain()`.
	float weightSum = 1.0f;
};

/// Worst-case AMPLITUDE gain around the loop: `gain x sum of tap weights`, plus
/// the persistence, which is a parallel route into the same summing node and
/// therefore adds rather than multiplying.
///
/// Deliberately NOT the same question as `Contractivity()`, which asks whether
/// the picture converges on a shape. This asks whether it survives. `zoom` is
/// accepted and ignored: the camera moves light around, it does not amplify it.
///
/// At or above 1 the picture grows until the clip stage stops it.
float LoopGain( const TapSet& tapsBeforeCamera, float zoom, float gain, float decay );

/**
    Move the operator's hands, at `phase` turns of the drift clock.

    ## Why this exists at all

    A loop whose parameters are constant is a contraction mapping with constant
    coefficients, and Banach's fixed point theorem then says exactly one thing
    about it: there is precisely one attracting fixed point and the iteration
    converges on it. Which is what the whole plugin is built on -- it is why the
    Sierpinski gasket arrives out of noise -- and it is also why, once it has
    arrived, **nothing ever moves again**. The attractor maps to itself. Every
    subsequent field is the same field.

    Endless zoom does not rescue it, and the reason is worth stating because it
    looks like it should: the attractor of a self-similar system zoomed by its
    own similarity ratio is the same attractor. A gasket magnified by two is a
    gasket. The camera can zoom for an hour and the picture will not change.

    So a rig that is left alone goes still, correctly, and the only way out is
    for the operator to change. On the device this models that is not a figure
    of speech -- the Light Herder's rig is played by somebody walking the cameras
    back and forth and turning knobs while it runs, and the video's own
    description says so. Drift is those hands.

    ## Mutually irrational rates

    The five modulations run at 1, 0.732, 0.517, 0.313 and 0.211 times the drift
    rate. No two of those have a rational ratio, so the combination has no
    period: the rig never returns to a state it has been in, and an operator
    watching for ten minutes never sees it repeat. Round them to 1, 0.75, 0.5,
    0.25 and the whole thing closes into a four-second loop.

    Pan Y runs a quarter cycle behind Pan X for vectrix's reason: in phase, every
    excursion is along the 45-degree diagonal, which reads as one shake rather
    than as wandering.

    ## The camera depths are scaled by the rig's COMPLIANCE, and have to be

    A nudge of the camera does not move an attractor by the size of the nudge.
    The attractor is the fixed point of the whole composed operator, so a
    per-field camera translation `t` displaces it by about `t / ( 1 - c )` where
    `c` is the tap set's contractivity -- and then it stops, because the
    displaced attractor is just as fixed as the old one was.

    That factor spans two orders of magnitude across the rigs here. Sierpinski
    contracts by 2, so it is **stiff**: it shrugs off anything small and needs a
    shove. A mirror rig running at 0.98 has `1 - c = 0.02`, so the same shove
    would throw the picture off the screen. A single depth cannot serve both, and
    the first attempt at drift proved it -- gentle enough for the mirror rigs, it
    moved the IFS rigs by about a thousandth of the frame per second, which is a
    still picture with extra arithmetic.

    So the camera depths below are quoted for a compliance of 1 and multiplied by
    `1 - c`. The injection, the Julia constant and the escape centre are NOT
    scaled: none of them is inside the geometric loop, so none of them is stiff.
*/
void ApplyDrift( LoopParams& params, double phase, float contractivity );

/// The glass alone: the tap set before the camera, and before the hands.
///
/// Split out because it is the expensive half -- a seeded rig plays a 20,000
/// step chaos game to measure its own framing -- and because drift never
/// changes it. One of these per FRAME; a `Resolve` per FIELD.
TapSet Glass( const LoopParams& params );

/**
    Build one field's loop state, moving the operator's hands on the way.

    Called **once per field, not once per frame**, and that is not a detail.
    Drift makes the operator's hands a function of time, so a field rendered at
    drift phase 0.31 is a different field from one rendered at 0.30 -- and if the
    hands only moved once per rendered frame, then sixty fields delivered as one
    frame of sixty would use one hand position where sixty frames of one would
    use sixty. The picture would depend on the host's frame rate, which is the
    one thing this plugin promises it does not. `esctest --rate` caught exactly
    that and is why this is shaped like this.

    `params` is taken by reference and comes back DRIFTED, because the caller
    needs the same drifted injection position and Julia constant that the taps
    were built from -- they go to the shader as uniforms.
*/
LoopState Resolve( const TapSet& glass, LoopParams& params, double driftPhase );

/**
    The rig's clock.

    Holds the fractional field left over between frames, so that a 60 Hz rig
    rendered at 50 fps runs 1.2 fields per frame as 1,1,2,1,1,2... rather than
    dropping the remainder and running slow.
*/
class Clock
{
public:
	/// Advance by `seconds` of host time and return how many whole fields the
	/// rig went round in that period.
	int Advance( double seconds, float fieldRate );

	/// Throw away the accumulated fraction. For a fresh instance and a resize --
	/// NOT for a parameter change.
	void Reset();

	/// The most fields one rendered frame may run.
	///
	/// A host that stalls for two seconds -- loading a clip, resizing, being
	/// debugged -- would otherwise come back asking for 120 trips round the
	/// loop in one frame, which takes long enough to cause the next stall. The
	/// picture is a second and a half behind where it "should" be, which for an
	/// instrument nobody is sequencing is not a thing anyone can see, and it
	/// recovers on the next frame instead of compounding.
	static constexpr int kMaxFieldsPerFrame = 8;

private:
	double carry = 0.0;
};

} // namespace escapement
