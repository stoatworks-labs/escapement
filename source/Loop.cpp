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

LoopState Resolve( const LoopParams& params )
{
	LoopState state;

	const TapSet glass = Taps( params.rig, params.tapCount, params.seed );

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
