#include "Shaders.h"

namespace escapement
{
//---------------------------------------------------------------------------
// Shared vertex shader
//---------------------------------------------------------------------------
const char* const kFullscreenVertexShader = R"(#version 410 core

out vec2 vUV;

void main()
{
	// Attributeless. A triangle strip of four gives the corners in the order
	// (-1,-1) (1,-1) (-1,1) (1,1), which is what the bit tests below produce.
	vec2 c = vec2( ( gl_VertexID & 1 ) == 0 ? -1.0 : 1.0,
	               ( gl_VertexID & 2 ) == 0 ? -1.0 : 1.0 );

	vUV         = c * 0.5 + 0.5;
	gl_Position = vec4( c, 0.0, 1.0 );
}
)";

//---------------------------------------------------------------------------
// Iterator bank
//---------------------------------------------------------------------------
//
// The escape-time rigs. Everything else in this plugin is a loop over fields;
// this is a loop over iterations, and the difference is worth being plain about
// because it is the one place Escapement does evaluate a formula.
//
// It is still the same instrument. An analogue multiplier settles in
// nanoseconds, so a bank of four of them wired as a complex square with a
// summing amplifier really does run a few hundred iterations inside one video
// field -- which is why this belongs in the rig at all rather than being a
// different plugin. What comes out is a signal like any other and goes into the
// same proc amp, the same store and the same display.
//
const char* const kIteratorFragmentShader = R"(#version 410 core

in vec2 vUV;

uniform vec2 Resolution;
uniform float Aspect;

uniform int Iterations;
uniform int Mode;          // 0 Julia, 1 Mandelbrot
uniform vec2 JuliaC;

// The view, split high/low so that the same uniforms serve both precisions.
// In Single the .y halves are simply zero.
uniform vec2 CentreX;
uniform vec2 CentreY;
uniform vec2 Scale;
uniform bool Extended;

out vec4 fragColor;

//-------------------------------------------------------------------------
// Double-float arithmetic.
//
// Two floats carrying a high and a low part give about 48 bits of mantissa
// where one gives 24, which moves the point where a zoom turns to mush from
// around 1e5 out to around 1e13.
//
// Every intermediate is `precise`, and that keyword is the whole reason this
// works. `cona - ( cona - a.x )` is not a no-op: it is Dekker's split, and it is
// where the high half of the product comes from. Written as plain floats the
// compiler is entitled to reassociate it to zero, and Apple's does -- leaving
// arithmetic that still runs, still type-checks, still differs from the single
// path, and silently has 24 bits again.
//
// It was caught by the picture rather than by a test: a 1e6 zoom came out in
// rectangular blocks about eight pixels across, which is exactly the width at
// which 24-bit mantissas stop being able to tell two adjacent pixels apart at
// that magnification. Comparing single against extended does NOT catch it --
// the two paths round differently and their outputs differ either way.
//
// Do not remove `precise` to tidy these up. Nothing will fail to compile.
//-------------------------------------------------------------------------
vec2 dfAdd( vec2 a, vec2 b )
{
	precise float t1 = a.x + b.x;
	precise float e  = t1 - a.x;
	precise float t2 = ( ( b.x - e ) + ( a.x - ( t1 - e ) ) ) + a.y + b.y;
	precise float hi = t1 + t2;
	precise float lo = t2 - ( hi - t1 );
	return vec2( hi, lo );
}

vec2 dfMul( vec2 a, vec2 b )
{
	const float split = 8193.0;

	precise float cona = a.x * split;
	precise float conb = b.x * split;
	precise float a1   = cona - ( cona - a.x );
	precise float b1   = conb - ( conb - b.x );
	precise float a2   = a.x - a1;
	precise float b2   = b.x - b1;

	precise float c11 = a.x * b.x;
	precise float c21 = a2 * b2 + ( a2 * b1 + ( a1 * b2 + ( a1 * b1 - c11 ) ) );
	precise float c2  = a.x * b.y + a.y * b.x;

	precise float t1 = c11 + c2;
	precise float e  = t1 - c11;
	precise float t2 = a.y * b.y + ( ( c2 - e ) + ( c11 - ( t1 - e ) ) ) + c21;

	precise float hi = t1 + t2;
	precise float lo = t2 - ( hi - t1 );
	return vec2( hi, lo );
}

vec2 dfSet( float a )
{
	return vec2( a, 0.0 );
}

void main()
{
	// Frame coordinates: -1..1 across the SHORT edge, y up.
	vec2 p = vec2( ( vUV.x * 2.0 - 1.0 ) * Aspect, vUV.y * 2.0 - 1.0 );

	float smoothIter = 0.0;
	float escaped    = 0.0;
	float trap       = 1e9;

	if( Extended )
	{
		vec2 cx = dfAdd( CentreX, dfMul( Scale, dfSet( p.x ) ) );
		vec2 cy = dfAdd( CentreY, dfMul( Scale, dfSet( p.y ) ) );

		vec2 zx = ( Mode == 1 ) ? dfSet( 0.0 ) : cx;
		vec2 zy = ( Mode == 1 ) ? dfSet( 0.0 ) : cy;

		vec2 ax = ( Mode == 1 ) ? cx : dfSet( JuliaC.x );
		vec2 ay = ( Mode == 1 ) ? cy : dfSet( JuliaC.y );

		for( int i = 0; i < Iterations; ++i )
		{
			vec2 xx = dfMul( zx, zx );
			vec2 yy = dfMul( zy, zy );

			float r2 = xx.x + yy.x;
			if( r2 > 65536.0 )
			{
				// Smooth iteration count. The +1 - log2(log2 r) term removes the
				// banding that a bare integer count gives, and it is exact
				// enough in single precision even when the orbit was not: by the
				// time a point has escaped past 256 its low bits no longer
				// affect where the band edge falls.
				smoothIter = float( i ) + 1.0 - log2( log2( sqrt( r2 ) ) );
				escaped    = 1.0;
				break;
			}

			trap = min( trap, r2 );

			vec2 nx = dfAdd( dfAdd( xx, -yy ), ax );
			vec2 ny = dfAdd( dfMul( dfMul( zx, zy ), dfSet( 2.0 ) ), ay );
			zx      = nx;
			zy      = ny;
		}
	}
	else
	{
		vec2 c = vec2( CentreX.x, CentreY.x ) + Scale.x * p;
		vec2 z = ( Mode == 1 ) ? vec2( 0.0 ) : c;
		vec2 a = ( Mode == 1 ) ? c : JuliaC;

		for( int i = 0; i < Iterations; ++i )
		{
			float xx = z.x * z.x;
			float yy = z.y * z.y;
			float r2 = xx + yy;

			if( r2 > 65536.0 )
			{
				smoothIter = float( i ) + 1.0 - log2( log2( sqrt( r2 ) ) );
				escaped    = 1.0;
				break;
			}

			trap = min( trap, r2 );

			z = vec2( xx - yy, 2.0 * z.x * z.y ) + a;
		}
	}

	// The bank puts out a SIGNAL, not a picture: a scalar that the proc amp and
	// the palette downstream are free to do what they like with. Interior points
	// carry the orbit trap instead of zero, so the inside of the set has
	// structure to light rather than being a hole.
	// LOG spacing, not linear and not a root.
	//
	// Escape counts are distributed logarithmically: most of the plane leaves in
	// a handful of iterations and the interesting part takes hundreds. `fract( n
	// * k )` therefore puts every band edge out in the boring part and paints
	// all the structure in one flat colour -- a deep zoom came out as a uniform
	// orange rectangle. A square root fixes the deep end and breaks the shallow
	// one instead: it spreads the first ten iterations over half a cycle, which
	// wraps a Julia set in a bright halo that is not in the data.
	//
	// log2 matches how the counts actually fall, so one coefficient works at 1:1
	// and at 1e12 both.
	float v = escaped > 0.5
	              ? fract( log2( smoothIter + 1.0 ) * 0.5 )
	              : 0.35 * exp( -6.0 * sqrt( max( trap, 0.0 ) ) );

	fragColor = vec4( vec3( v ), 1.0 );
}
)";

//---------------------------------------------------------------------------
// The loop
//---------------------------------------------------------------------------
//
// One trip round the rig. Run once per FIELD; a rendered frame may run this
// none, once or several times -- see Loop.h on why the rig keeps its own clock.
//
// The order of the blocks below is the order the light and the signal actually
// go through them, and it is not interchangeable. The proc amp is after the
// taps because a proc amp is in the cable and the taps are in the glass; the
// clip is last because an amplifier runs out of rail at its output; the noise is
// added before the clip because sensor grain is upstream of the amplifier and
// therefore gets clipped along with everything else.
//
const char* const kLoopFragmentShader = R"(#version 410 core

in vec2 vUV;

uniform sampler2D Prev;
uniform sampler2D Bank;

#ifdef ESCAPEMENT_EFFECT
uniform sampler2D ClipTexture;
uniform vec2 MaxUV;
#endif

uniform vec2 Resolution;
uniform float Aspect;

// The taps, already composed with the camera and already inverted.
uniform vec4 TapM[ 8 ];   // m00 m01 m10 m11
uniform vec4 TapT[ 8 ];   // tx ty weight hue
uniform int TapCount;
uniform float WeightSum;

uniform int Symmetry;
uniform bool FoldMirror;

uniform float Focus;
uniform float Lens;
uniform float Vignette;

uniform float Gain;
uniform float Pedestal;
uniform float Gamma;
uniform float HueRot;
uniform float Saturation;
uniform float ClipLevel;
uniform float Noise;
uniform float Decay;

uniform int InjectMode;
uniform float InjectLevel;
uniform float InjectSize;
uniform vec2 InjectPos;

uniform int FieldIndex;

out vec4 fragColor;

const float kPi = 3.14159265358979;

//-------------------------------------------------------------------------
// An exact integer hash, matching Hash.h on the CPU. Not fract(sin(x)*k):
// that is transcendental and its result differs between drivers, so the grain
// would differ between the machine a look was built on and the one it is run
// on.
//-------------------------------------------------------------------------
uint Hash32( uint x )
{
	x ^= x >> 16;
	x *= 0x7feb352du;
	x ^= x >> 15;
	x *= 0x846ca68bu;
	x ^= x >> 16;
	return x;
}

float Unit( uint h )
{
	return float( h >> 8 ) * ( 1.0 / 16777216.0 );
}

//-------------------------------------------------------------------------
// Hue rotation about the luma axis, written out component by component.
//
// The usual way to write this is three mat3s added together, which is a trap in
// GLSL: mat3 constructors take COLUMNS, the published coefficient tables are
// written in ROWS, and the result of getting it wrong is a rotation that still
// looks like a hue rotation and is wrong by a transpose. Explicit dot products
// cannot be silently transposed.
//-------------------------------------------------------------------------
vec3 HueRotate( vec3 c, float turns )
{
	if( turns == 0.0 )
		return c;

	float a  = turns * 2.0 * kPi;
	float s  = sin( a );
	float co = cos( a );

	float m00 = 0.299 + 0.701 * co + 0.168 * s;
	float m01 = 0.587 - 0.587 * co + 0.330 * s;
	float m02 = 0.114 - 0.114 * co - 0.497 * s;

	float m10 = 0.299 - 0.299 * co - 0.328 * s;
	float m11 = 0.587 + 0.413 * co + 0.035 * s;
	float m12 = 0.114 - 0.114 * co + 0.292 * s;

	float m20 = 0.299 - 0.300 * co + 1.250 * s;
	float m21 = 0.587 - 0.588 * co - 1.050 * s;
	float m22 = 0.114 + 0.886 * co - 0.203 * s;

	return vec3( m00 * c.r + m01 * c.g + m02 * c.b,
	             m10 * c.r + m11 * c.g + m12 * c.b,
	             m20 * c.r + m21 * c.g + m22 * c.b );
}

//-------------------------------------------------------------------------
// The amplifier running out of rail.
//
// Linear to ClipLevel, then an exponential approach to 1. Not a clamp: a hard
// clamp has a corner in it, and a corner inside a feedback loop quantises the
// picture into flat plateaus with hard edges. The soft knee is why a hot rig
// blooms rather than posterising.
//-------------------------------------------------------------------------
float SoftClip( float v, float t )
{
	float a = abs( v );
	if( a <= t )
		return v;

	float k = max( 1.0 - t, 1e-4 );
	float y = t + k * ( 1.0 - exp( -( a - t ) / k ) );
	return v < 0.0 ? -y : y;
}

vec2 FrameToUV( vec2 p )
{
	return vec2( p.x / Aspect, p.y ) * 0.5 + 0.5;
}

//-------------------------------------------------------------------------
// The mirror wedge.
//
// A rotational fold of order N: everything the camera sees is one wedge,
// repeated. This is how the Koch snowflake is built -- a four-tap Koch CURVE
// plus a three-fold fold -- for the cost of one coordinate wrap instead of the
// eight extra taps the twelve-map snowflake system would need. See Taps.cpp.
//-------------------------------------------------------------------------
vec2 Fold( vec2 p )
{
	if( Symmetry <= 1 )
		return p;

	float seg   = 2.0 * kPi / float( Symmetry );
	float r     = length( p );
	float theta = atan( p.y, p.x );

	//---------------------------------------------------------------------
	// The wedge is centred on STRAIGHT DOWN, and that is not cosmetic.
	//
	// A fold reads its source from one canonical wedge and repeats it. The
	// obvious `mod( theta, seg )` makes that wedge start at zero -- pointing
	// right -- and the Koch rig then reads from a part of the frame its curve
	// is nowhere near, because the curve is built lying along the BOTTOM of
	// the triangle it belongs to. The result is a black frame, from a rig
	// whose taps are all correct and whose gain is fine, which is a
	// particularly expensive way to spend an evening.
	//
	// Centring the wedge on -90 degrees puts it at [-150, -30) for a
	// three-fold, which is exactly the arc the Koch curve occupies. Down is
	// also the right default for everything else: it is where a rig's content
	// sits when it has a horizon, and no rig here is built along the +x axis.
	//---------------------------------------------------------------------
	float phase = theta + kPi * 0.5 + seg * 0.5;
	float a     = mod( phase, seg );

	// A real wedge is made of mirrors, so alternate sectors arrive reversed.
	// Rotational-only is the setting that builds the snowflake; mirrored is the
	// one that looks like a kaleidoscope.
	if( FoldMirror )
		a = abs( a - seg * 0.5 ) + seg * 0.5;

	theta = a - kPi * 0.5 - seg * 0.5;

	return r * vec2( cos( theta ), sin( theta ) );
}

//-------------------------------------------------------------------------
// The lens.
//
// Applied to the coordinate the tap fetch is about to use, so it is INSIDE the
// loop and compounds: a barrel distortion of a barrel distortion of a barrel
// distortion is most of why a real rig's spirals curl the way they do. A lens
// correction applied once on the way out would look like a warped picture
// instead.
//-------------------------------------------------------------------------
vec2 LensWarp( vec2 p )
{
	if( Lens == 0.0 )
		return p;

	float r2 = dot( p, p );
	return p * ( 1.0 + Lens * 0.25 * r2 );
}

vec3 Inject( vec2 p )
{
	if( InjectMode == 0 || InjectLevel <= 0.0 )
		return vec3( 0.0 );

	vec2 q = ( p - InjectPos ) / max( InjectSize, 1e-4 );
	vec3 c = vec3( 0.0 );

	if( InjectMode == 1 )
	{
		// Dot. Gaussian rather than a disc: a hard-edged spot injects a step,
		// and a step going round a loop with gain comes back as a ring.
		c = vec3( exp( -dot( q, q ) * 2.0 ) );
	}
	else if( InjectMode == 2 )
	{
		// Colour bars.
		float b = floor( fract( q.x * 0.125 + 0.5 ) * 7.0 );
		vec3 bars[ 7 ] = vec3[ 7 ](
			vec3( 1.0, 1.0, 1.0 ), vec3( 1.0, 1.0, 0.0 ), vec3( 0.0, 1.0, 1.0 ),
			vec3( 0.0, 1.0, 0.0 ), vec3( 1.0, 0.0, 1.0 ), vec3( 1.0, 0.0, 0.0 ),
			vec3( 0.0, 0.0, 1.0 ) );
		c = bars[ int( clamp( b, 0.0, 6.0 ) ) ] * step( abs( q.y ), 1.0 ) * step( abs( q.x ), 4.0 );
	}
	else if( InjectMode == 3 )
	{
		// Grid. Shows what the taps are doing more plainly than anything else,
		// which is what it is for.
		vec2 g = abs( fract( q * 0.5 ) - 0.5 );
		float line = 1.0 - smoothstep( 0.0, 0.06, min( g.x, g.y ) );
		c = vec3( line ) * step( length( q ), 8.0 );
	}
	else if( InjectMode == 4 )
	{
		uint h = Hash32( uint( gl_FragCoord.x ) * 1973u + uint( gl_FragCoord.y ) * 9277u + uint( FieldIndex ) * 26699u );
		c      = vec3( Unit( h ), Unit( Hash32( h ) ), Unit( Hash32( Hash32( h ) ) ) );
	}
	else if( InjectMode == 5 )
	{
		c = texture( Bank, FrameToUV( q * InjectSize + InjectPos ) ).rgb;
	}
	else if( InjectMode == 6 )
	{
#ifdef ESCAPEMENT_EFFECT
		vec2 uv = FrameToUV( q * InjectSize + InjectPos );
		if( uv == clamp( uv, 0.0, 1.0 ) )
			c = texture( ClipTexture, uv * MaxUV ).rgb;
#endif
	}

	return c * InjectLevel;
}

void main()
{
	vec2 p = vec2( ( vUV.x * 2.0 - 1.0 ) * Aspect, vUV.y * 2.0 - 1.0 );

	vec2 folded = Fold( p );

	//---------------------------------------------------------------------
	// The taps.
	//
	// Each is an inverse map, so this asks "where in the previous field did
	// what belongs at this pixel come from". Fetches outside the frame return
	// nothing rather than the clamped edge texel: a real camera pointed
	// slightly off its screen sees the dark room, not an infinitely smeared
	// copy of the edge pixel, and clamping here is the single most recognisable
	// way to make a feedback plugin look like a shader instead of a rig.
	//---------------------------------------------------------------------
	vec3 acc = vec3( 0.0 );

	for( int i = 0; i < TapCount; ++i )
	{
		vec2 q = vec2( TapM[ i ].x * folded.x + TapM[ i ].y * folded.y + TapT[ i ].x,
		               TapM[ i ].z * folded.x + TapM[ i ].w * folded.y + TapT[ i ].y );

		q = LensWarp( q );

		vec2 uv = FrameToUV( q );
		if( uv != clamp( uv, 0.0, 1.0 ) )
			continue;

		vec3 s = textureLod( Prev, uv, Focus ).rgb;

		if( TapT[ i ].w != 0.0 )
			s = HueRotate( s, TapT[ i ].w );

		// The lens is dark at the edges, and being dark at the edges INSIDE a
		// loop is what stops the picture filling the frame edge to edge and
		// going flat. It is a stabiliser, not a look.
		if( Vignette > 0.0 )
		{
			float r = length( q );
			s *= 1.0 - Vignette * clamp( r * 0.7, 0.0, 1.0 );
		}

		acc += s * TapT[ i ].z;
	}

	//---------------------------------------------------------------------
	// The taps SUM. They are not averaged, and that is the difference between
	// this working and not.
	//
	// Averaging is the tempting normalisation -- three taps, divide by three,
	// the brightness stays put. It also destroys the attractor. A point on the
	// gasket is reached by exactly ONE of the three maps, so averaging hands it
	// a third of the light it had, every field: a loop gain of 0.33 dressed up
	// as a normalisation, and the picture goes black no matter what the proc amp
	// is set to. That is what the first contact sheet showed -- five rigs black
	// and nobody able to say why, because the gain control said 1.12.
	//
	// Summing is also what the glass does. A half-mirror splits a beam and the
	// two paths ADD where they land on the sensor; nothing in an optical path
	// divides by the number of paths. Overlaps therefore brighten, and the soft
	// clip is what stops them -- which is the correct place for it, because that
	// is where a real amplifier stops them too.
	//
	// `WeightSum` is still uploaded, and is still what a per-tap weight has to
	// be read against, but it is a diagnostic rather than a divisor: Kaleido
	// carries 1/N weights of its own because a mirror wedge really does split
	// the light N ways.
	//---------------------------------------------------------------------

	//---------------------------------------------------------------------
	// Persistence.
	//
	// What stays where it is, as distinct from what goes round the geometric
	// path. Phosphor decay and the camera's integration time do the same thing
	// and are the same term. Gain moves without smearing; Decay smears without
	// moving.
	//---------------------------------------------------------------------
	vec3 held = textureLod( Prev, vUV, 0.0 ).rgb * Decay;

	vec3 c = acc * Gain + held;

	c += vec3( Pedestal );

	c = HueRotate( c, HueRot );

	float luma = dot( c, vec3( 0.299, 0.587, 0.114 ) );
	c          = mix( vec3( luma ), c, Saturation );

	// Gamma on the magnitude, sign preserved. The signal can be negative here --
	// a negative pedestal puts it there deliberately -- and pow() of a negative
	// is undefined, which is one of the two ways this buffer gets its first NaN.
	c = sign( c ) * pow( abs( c ), vec3( Gamma ) );

	c += Inject( p );

	if( Noise > 0.0 )
	{
		uint h = Hash32( uint( gl_FragCoord.x ) * 6151u + uint( gl_FragCoord.y ) * 1543u + uint( FieldIndex ) * 3079u );
		c += ( vec3( Unit( h ), Unit( Hash32( h ) ), Unit( Hash32( Hash32( h ) ) ) ) - 0.5 ) * Noise;
	}

	c = vec3( SoftClip( c.r, ClipLevel ), SoftClip( c.g, ClipLevel ), SoftClip( c.b, ClipLevel ) );

	//---------------------------------------------------------------------
	// The NaN guard, and why it is not optional.
	//
	// This buffer feeds itself. NaN times anything is NaN, so a single NaN
	// anywhere survives every subsequent field for the life of the instance --
	// it does not decay, it cannot be cleared by turning the gain down, and it
	// spreads through the taps into its neighbours. One bad frame becomes a
	// plugin that is permanently broken until the host reloads it.
	//
	// The clamp is the same guard for the other direction: a rig held above
	// unity gain with the clip wide open can reach infinity in a few dozen
	// fields, and inf - inf is NaN.
	//---------------------------------------------------------------------
	if( any( isnan( c ) ) || any( isinf( c ) ) )
		c = vec3( 0.0 );

	fragColor = vec4( clamp( c, -8.0, 8.0 ), 1.0 );
}
)";

//---------------------------------------------------------------------------
// Display
//---------------------------------------------------------------------------
const char* const kDisplayFragmentShader = R"(#version 410 core

in vec2 vUV;

uniform sampler2D Store;

#ifdef ESCAPEMENT_EFFECT
uniform sampler2D ClipTexture;
uniform vec2 MaxUV;
uniform int MaskMode;
uniform float Blend;
#endif

uniform vec2 Resolution;
uniform float Aspect;

uniform int PaletteMode;
uniform float PaletteShift;
uniform float Opacity;
uniform float Bloom;

// Rescan -- the Globe rig.
uniform float Sphere;
uniform float Tilt;
uniform float SpinPhase;
uniform float Light;

out vec4 fragColor;

const float kPi = 3.14159265358979;

vec3 Palette( float v )
{
	v = fract( v + PaletteShift );

	if( PaletteMode == 1 )
	{
		// P3 amber. The green channel of a real amber phosphor does not track
		// the red linearly, which is why this is a curve and not a tint.
		return vec3( pow( v, 0.7 ), pow( v, 1.5 ) * 0.75, pow( v, 3.0 ) * 0.18 );
	}
	if( PaletteMode == 2 )
	{
		return vec3( pow( v, 1.6 ) * 0.55, pow( v, 1.1 ) * 0.85, pow( v, 0.6 ) );
	}
	if( PaletteMode == 3 )
	{
		// Black - red - orange - white.
		return clamp( vec3( v * 3.0, v * 3.0 - 1.0, v * 3.0 - 2.0 ), 0.0, 1.0 );
	}
	if( PaletteMode == 4 )
	{
		vec3 h = clamp( abs( mod( v * 6.0 + vec3( 0.0, 4.0, 2.0 ), 6.0 ) - 3.0 ) - 1.0, 0.0, 1.0 );
		return h;
	}

	return vec3( v );
}

vec3 Shade( vec2 uv, out float coverage )
{
	coverage = 1.0;

	vec3 signalColour;

	if( Sphere <= 0.0 )
	{
		vec3 s = texture( Store, uv ).rgb;

		// Bloom: the high mip levels ARE a blurred copy of the picture, already
		// paid for because the loop needs the mip chain for Focus. Halation on a
		// screen and flare in a lens are both a wide, dim copy of the bright
		// parts, which is what this adds.
		if( Bloom > 0.0 )
		{
			vec3 wide = textureLod( Store, uv, 4.0 ).rgb + textureLod( Store, uv, 6.0 ).rgb;
			s += wide * Bloom * 0.5;
		}

		signalColour = s;
	}
	else
	{
		//-----------------------------------------------------------------
		// The rescan.
		//
		// The rig's screen is a sphere and the camera is looking at it. The
		// intersection is analytic rather than ray marched -- it is one sphere,
		// and marching it would be a lot of work to arrive at the same quadratic.
		//-----------------------------------------------------------------
		vec2 p = vec2( ( uv.x * 2.0 - 1.0 ) * Aspect, uv.y * 2.0 - 1.0 );

		float r2 = dot( p, p );
		float radius = 0.92;

		if( r2 > radius * radius )
		{
			coverage = 0.0;
			return vec3( 0.0 );
		}

		float z = sqrt( max( radius * radius - r2, 0.0 ) );
		vec3 n  = vec3( p, z ) / radius;

		// Tilt, then spin. Spin second so the globe turns about its own axis
		// rather than about the camera's.
		float ct = cos( Tilt ), st = sin( Tilt );
		vec3 m   = vec3( n.x, ct * n.y - st * n.z, st * n.y + ct * n.z );

		float lon = atan( m.x, m.z ) / ( 2.0 * kPi ) + 0.5 + SpinPhase;
		float lat = asin( clamp( m.y, -1.0, 1.0 ) ) / kPi + 0.5;

		vec3 s = texture( Store, vec2( fract( lon ), clamp( lat, 0.0, 1.0 ) ) ).rgb;

		// Wrap the mapping back towards the flat picture as Sphere comes down,
		// so the control is a continuous move between the two rather than a
		// switch with nothing in between.
		s = mix( texture( Store, uv ).rgb, s, Sphere );

		// Lambert against a light over the camera's shoulder, plus a rim. A
		// self-lit screen is not a lit object, so the shading is deliberately
		// gentle: too much and it stops looking like a screen wrapped on a ball
		// and starts looking like a painted prop.
		vec3 lightDir = normalize( vec3( -0.4, 0.5, 0.75 ) );
		float lambert = max( dot( n, lightDir ), 0.0 );
		float rim     = pow( 1.0 - abs( n.z ), 3.0 );

		s *= mix( 1.0, 0.35 + 0.9 * lambert, Light * Sphere );
		s += vec3( rim ) * 0.12 * Light * Sphere;

		// The limb, so the ball has an edge rather than a staircase.
		float edge = smoothstep( radius, radius - 0.004, sqrt( r2 ) );
		coverage   = mix( 1.0, edge, Sphere );

		signalColour = s;
	}

	if( PaletteMode == 0 )
		return signalColour;

	// A palette is a lookup on ONE number, so the signal has to become one.
	// Luma rather than the maximum channel: the loop's hue rotation means the
	// channels take turns being the bright one, and a max would make the
	// palette flicker between them field to field.
	float v = dot( clamp( signalColour, 0.0, 4.0 ), vec3( 0.299, 0.587, 0.114 ) );
	return Palette( v );
}

void main()
{
	float coverage;
	vec3 c = Shade( vUV, coverage );

	c = clamp( c, 0.0, 4.0 );

	float a = Opacity * coverage;

#ifdef ESCAPEMENT_EFFECT
	vec4 clipColour = texture( ClipTexture, vUV * MaxUV );

	// The clip arrives premultiplied and leaves the same way.
	float loopLuma = clamp( dot( c, vec3( 0.299, 0.587, 0.114 ) ), 0.0, 1.0 );

	vec4 result;

	if( MaskMode == 0 )
	{
		vec4 over = vec4( c * a, a );
		result    = over + clipColour * ( 1.0 - over.a );
	}
	else if( MaskMode == 1 )
	{
		result = clipColour * loopLuma * a;
	}
	else if( MaskMode == 2 )
	{
		result = clipColour * ( 1.0 - loopLuma * a );
	}
	else
	{
		result = vec4( clipColour.rgb * c, clipColour.a );
	}

	fragColor = mix( clipColour, result, Blend );
#else
	fragColor = vec4( c * a, a );
#endif
}
)";

} // namespace escapement
