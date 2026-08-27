#include "Loop.h"

#include <cmath>

namespace escapement
{
float LoopGain( const TapSet& tapsBeforeCamera, float zoom, float gain, float decay )
{
	// AMPLITUDE, not geometry. These are two different questions about a rig and
	// conflating them is why the first draft's presets were all mistuned:
	//
	//   * Does the picture converge on a shape? -- `Contractivity()`. Geometric,
	//     a property of the glass, and nothing to do with brightness.
	//   * Does the picture survive? -- this. Amplitude, and nothing to do with
	//     what shape it settles into.
	//
	// A rig can contract geometrically and still saturate, and can be
	// amplitude-stable while its taps expand and smear the attractor away.
	//
	// The taps sum, so the worst case is a pixel where every tap lands on lit
	// content at once: `gain x sum of weights`. Decay is ADDED because what stays
	// put and what goes round the geometric path are parallel routes into the
	// same summing node -- a rig at 0.98 gain with 0.29 of persistence is running
	// at 1.27 while the control that says "gain" reads 0.98 and looks safe.
	//
	// The camera's zoom does not appear at all. It moves light around; it does
	// not amplify it.
	( void )zoom;

	float weightSum = 0.0f;
	for( int i = 0; i < tapsBeforeCamera.count; ++i )
		weightSum += tapsBeforeCamera.taps[ i ].weight;

	return gain * weightSum + decay;
}

void ApplyDrift( LoopParams& p, double phase, float contractivity )
{
	if( p.drift <= 0.0f )
		return;

	const double tau = 6.283185307179586;
	const float d    = p.drift;

	// How far the attractor moves for a given nudge. Clamped below 1 so a rig
	// sitting exactly at unity gain does not divide the hands by zero, and above
	// 0.02 so a runaway rig still gets some.
	const float loop     = contractivity * std::fabs( p.zoom );
	const float compliance = 1.0f - ( loop > 0.98f ? 0.98f : loop );

	// Depths are fractions of each control's USEFUL range, not of its slider.
	// Zoom's slider reaches 1.10 and nothing above about 1.03 is watchable, so
	// 0.010 is a large modulation of the part that matters and would be an
	// invisible one of the part that does not.
	//
	// They are quoted for a COMPLIANCE OF 1 and scaled down by how stiff the rig
	// actually is -- see the note in Loop.h. A mirror rig at 0.98 gets a fiftieth
	// of these; Sierpinski gets half.
	p.zoom += d * compliance * 0.100f * float( std::sin( tau * phase ) );
	p.rotate += d * compliance * 0.800f * float( std::sin( tau * phase * 0.732 ) );
	p.panX += d * compliance * 0.300f * float( std::sin( tau * phase * 0.517 ) );
	p.panY += d * compliance * 0.300f * float( std::sin( tau * phase * 0.313 + tau * 0.25 ) );

	// The colour walk gets a hand on it too, so that a rig which has settled
	// into greys -- where a hue rotation about the luma axis is a no-op, because
	// grey is a fixed point of it -- still has something changing.
	//
	// Small, and deliberately smaller than it was: hue rotation ACCUMULATES
	// round the loop, so a drift that looks modest per field walks a saturated
	// rig all the way round the wheel and parks it on whatever colour it is
	// passing. The Globe preset went flat magenta at 0.004.
	p.hueRotate += d * 0.0015f * float( std::sin( tau * phase * 0.211 ) );

	// Moving what is in front of the lens is the most effective of all of these
	// on a rig that is injecting anything, because the injection is the only
	// term in the loop that is not already converged.
	p.injectX += d * 0.45f * float( std::sin( tau * phase * 0.421 ) );
	p.injectY += d * 0.45f * float( std::sin( tau * phase * 0.277 + tau * 0.25 ) );

	// The escape rigs have no camera -- their gain is zero and the taps are
	// never sampled -- so drifting the camera would leave them exactly as dead
	// as before. What moves there is the constant itself: a Julia set whose c
	// wanders never settles, because it is a different Julia set every field.
	p.juliaX += d * 0.12f * float( std::sin( tau * phase * 0.611 ) );
	p.juliaY += d * 0.12f * float( std::sin( tau * phase * 0.389 + tau * 0.25 ) );

	// In units of the current view width, so a deep zoom wanders by as much of
	// the screen as a shallow one does rather than teleporting.
	p.escapeX += double( d ) * 0.25 * std::sin( tau * phase * 0.157 );
	p.escapeY += double( d ) * 0.25 * std::sin( tau * phase * 0.113 + tau * 0.25 );
}

TapSet Glass( const LoopParams& params )
{
	return Taps( params.rig, params.tapCount, params.seed );
}

LoopState Resolve( const TapSet& glass, LoopParams& params, double driftPhase )
{
	LoopState state;

	state.contractivity = Contractivity( glass );
	ApplyDrift( params, driftPhase, state.contractivity );

	state.loopGain = LoopGain( glass, params.zoom, params.gain, params.decay );

	const TapSet withCamera = WithCamera( glass, params.zoom, params.rotate, params.panX, params.panY );

	// Invert here, once per frame, rather than in the shader once per pixel.
	// A tap that cannot be inverted is dropped rather than divided by: a NaN in
	// a feedback buffer is permanent, because NaN times anything is NaN and the
	// buffer feeds itself for the life of the instance.
	state.inverseTaps.count = 0;
	state.weightSum         = 0.0f;

	for( int i = 0; i < withCamera.count; ++i )
	{
		Tap inverse;
		if( !Inverse( withCamera.taps[ i ], inverse ) )
		{
			++state.droppedTaps;
			continue;
		}

		state.inverseTaps.taps[ state.inverseTaps.count++ ] = inverse;
		state.weightSum += withCamera.taps[ i ].weight;
	}

	// Every rig has at least one tap before inversion, so this can only fire
	// when every one of them was singular. A seeded rig can do it; the picture
	// then has no geometric path at all and is whatever is injected, which is
	// a legible outcome rather than a black frame.
	if( state.weightSum <= 0.0f )
		state.weightSum = 1.0f;

	return state;
}

int Clock::Advance( double seconds, float fieldRate )
{
	if( !( seconds > 0.0 ) || !( fieldRate > 0.0f ) )
		return 0;

	carry += seconds * double( fieldRate );

	// A negative or absurd host time delta -- a composition being scrubbed, a
	// clock that wrapped -- must not turn into a huge field count or a negative
	// one. Clamping the result rather than the input keeps the accumulator
	// honest for the ordinary case.
	if( carry < 0.0 )
		carry = 0.0;

	int fields = int( carry );
	carry -= double( fields );

	if( fields > kMaxFieldsPerFrame )
	{
		fields = kMaxFieldsPerFrame;

		// Drop the backlog rather than paying it off over the following frames.
		// Paying it off means every frame after a stall also runs long, which is
		// how one stall becomes a stutter that lasts until the operator stops
		// watching.
		carry = 0.0;
	}

	return fields;
}

void Clock::Reset()
{
	carry = 0.0;
}

} // namespace escapement
