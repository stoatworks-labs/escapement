#include "Taps.h"
#include "Hash.h"

#include <cmath>

namespace escapement
{
namespace
{
/// A similarity: scale, then rotate, then translate.
Tap Similarity( float scale, float radians, float tx, float ty )
{
	const float c = std::cos( radians ) * scale;
	const float s = std::sin( radians ) * scale;

	Tap tap;
	tap.m[ 0 ] = c;
	tap.m[ 1 ] = -s;
	tap.m[ 2 ] = s;
	tap.m[ 3 ] = c;
	tap.tx     = tx;
	tap.ty     = ty;
	return tap;
}

/**
    Rewrite a map that was published in some other coordinate system.

    Barnsley's fern is defined on a leaf about 5 units wide and 10 tall with its
    root at the origin; the Koch curve is defined on the unit interval. Neither
    is the frame. The temptation is to fix that by transforming the *points*
    somewhere in the loop, which does not work: the maps compose with each other
    frame after frame, so a coordinate change applied once at the end is applied
    once, while the maps go on iterating in the old system for ever.

    A change of basis has to be a **conjugation** -- `S w S^-1` -- because that
    is the map which does in the new system what `w` did in the old one, and it
    stays true through any number of compositions. Here `S` is `p -> k p + o`.
*/
Tap Conjugate( const Tap& w, float k, float ox, float oy )
{
	// The linear part is unchanged by a uniform scale: k M (1/k) = M. Only the
	// translation moves.
	Tap out = w;
	out.tx  = k * w.tx + ox - ( w.m[ 0 ] * ox + w.m[ 1 ] * oy );
	out.ty  = k * w.ty + oy - ( w.m[ 2 ] * ox + w.m[ 3 ] * oy );
	return out;
}

Tap Compose( const Tap& a, const Tap& b )
{
	// a after b: p -> a( b( p ) )
	Tap out;
	out.m[ 0 ] = a.m[ 0 ] * b.m[ 0 ] + a.m[ 1 ] * b.m[ 2 ];
	out.m[ 1 ] = a.m[ 0 ] * b.m[ 1 ] + a.m[ 1 ] * b.m[ 3 ];
	out.m[ 2 ] = a.m[ 2 ] * b.m[ 0 ] + a.m[ 3 ] * b.m[ 2 ];
	out.m[ 3 ] = a.m[ 2 ] * b.m[ 1 ] + a.m[ 3 ] * b.m[ 3 ];
	out.tx     = a.m[ 0 ] * b.tx + a.m[ 1 ] * b.ty + a.tx;
	out.ty     = a.m[ 2 ] * b.tx + a.m[ 3 ] * b.ty + a.ty;
	out.weight = a.weight * b.weight;
	out.hue    = a.hue + b.hue;
	return out;
}

constexpr float kPi = 3.14159265358979323846f;

} // namespace

const char* RigName( Rig rig )
{
	switch( rig )
	{
	case Rig::Mirror:     return "Mirror";
	case Rig::Sierpinski: return "Sierpinski";
	case Rig::Koch:       return "Koch";
	case Rig::Dragon:     return "Dragon";
	case Rig::Fern:       return "Fern";
	case Rig::Kaleido:    return "Kaleidoscope";
	case Rig::Seeded:     return "Seeded";
	case Rig::Julia:      return "Julia";
	case Rig::Mandelbrot: return "Mandelbrot";
	case Rig::Globe:      return "Globe";
	default:              return "?";
	}
}

bool RigUsesTaps( Rig rig )
{
	return rig != Rig::Julia && rig != Rig::Mandelbrot;
}

bool RigIsRescan( Rig rig )
{
	return rig == Rig::Globe;
}

TapSet Taps( Rig rig, int count, uint32_t seed )
{
	TapSet set;

	switch( rig )
	{
	case Rig::Mirror:
	case Rig::Julia:
	case Rig::Mandelbrot:
	case Rig::Globe:
	default:
	{
		// One tap, identity. The camera supplies everything: this is a lens
		// pointed at a screen and nothing else in the light path.
		//
		// Julia and Mandelbrot never sample this -- they run the iterator bank
		// -- but they are still handed a valid one-tap set so that every code
		// path downstream can assume `count >= 1` rather than testing the rig.
		set.count = 1;
		break;
	}

	case Rig::Sierpinski:
	{
		// Three half-scale maps whose fixed points are the vertices of an
		// equilateral triangle. w_i( x ) = ( x + v_i ) / 2, so w_i( v_i ) = v_i.
		//
		// The attractor is the gasket, and the reason it is the gasket rather
		// than a filled triangle is that the three half-scale copies of the
		// triangle cover it everywhere except the middle third -- the hole is
		// what the maps do not reach, at every scale.
		// The vertices sit on a circle of radius r, dropped by r/4 so that the
		// triangle's BOUNDING BOX is centred rather than its circumcentre. They
		// are not the same point -- the apex reaches r and the base only r/2 --
		// and centring the circle instead leaves the gasket visibly high in the
		// frame with a quarter of the picture empty underneath it.
		const float r  = 0.92f;
		const float cy = -r * 0.25f;
		const float v[ 3 ][ 2 ] = {
			{ 0.0f, r + cy },
			{ -r * 0.8660254f, -r * 0.5f + cy },
			{ r * 0.8660254f, -r * 0.5f + cy },
		};

		for( int i = 0; i < 3; ++i )
		{
			Tap tap;
			tap.m[ 0 ] = 0.5f;
			tap.m[ 1 ] = 0.0f;
			tap.m[ 2 ] = 0.0f;
			tap.m[ 3 ] = 0.5f;
			tap.tx     = 0.5f * v[ i ][ 0 ];
			tap.ty     = 0.5f * v[ i ][ 1 ];
			set.taps[ i ] = tap;
		}
		set.count = 3;
		break;
	}

	case Rig::Koch:
	{
		// The Koch curve on the unit interval: four third-scale maps, the
		// middle two turned +-60 degrees, which is the textbook system.
		//
		// This is the CURVE, one side of the snowflake. The snowflake is three
		// of these on the sides of a triangle -- twelve maps, more taps than
		// any rig here carries -- so it is not built from taps at all. The
		// three-fold symmetry fold does it instead, for the cost of one
		// coordinate wrap rather than eight more texture fetches, which is also
		// what a mirror wedge in front of the lens would do in the glass.
		// See `Loop.h` on the fold.
		const float third = 1.0f / 3.0f;
		Tap w[ 4 ];
		w[ 0 ] = Similarity( third, 0.0f, 0.0f, 0.0f );
		w[ 1 ] = Similarity( third, -kPi / 3.0f, third, 0.0f );
		w[ 2 ] = Similarity( third, kPi / 3.0f, 0.5f, -0.28867513f );
		w[ 3 ] = Similarity( third, 0.0f, 2.0f * third, 0.0f );

		// The bump points DOWN -- the rotations are negated against the usual
		// textbook pair, which draws it upwards. That sign is the difference
		// between a snowflake and a hexagram: the fold puts this curve on the
		// three sides of a triangle centred on the origin, and a bump that
		// points towards the centre makes the three sides cross each other
		// through the middle instead of enclosing it.
		//
		// The curve then runs from (0,0) to (1,0) bulging to y = -0.289. Scaled
		// by 1.6 and dropped to y = -0.4619, it becomes the lower side of a
		// triangle of side 1.6 whose incircle is 0.4619 and whose circumcircle
		// is 0.9238 -- and the bump's tip lands exactly on that circumcircle,
		// which is the check that the two constants belong to the same triangle.
		for( int i = 0; i < 4; ++i )
			set.taps[ i ] = Conjugate( w[ i ], 1.6f, -0.8f, -0.46188022f );

		set.count = 4;
		break;
	}

	case Rig::Dragon:
	{
		// Heighway's dragon: two maps at 1/sqrt2, turned 45 and 135 degrees.
		const float s = 0.70710678f;
		Tap w[ 2 ];
		w[ 0 ] = Similarity( s, kPi * 0.25f, 0.0f, 0.0f );
		w[ 1 ] = Similarity( s, kPi * 0.75f, 1.0f, 0.0f );

		// Measured attractor, not a remembered one: x in [-1.10, 1.15], y in
		// [-0.28, 1.22] before conjugation, which is half as wide again as the
		// frame and sits high. These constants are what `esctest --fit` reports
		// for it.
		for( int i = 0; i < 2; ++i )
			set.taps[ i ] = Conjugate( w[ i ], 1.0337f, -0.4307f, -0.1723f );

		set.count = 2;
		break;
	}

	case Rig::Fern:
	{
		// Barnsley's fern, exactly as published. The first map has determinant
		// zero -- it collapses the plane onto a segment, which is the stem --
		// and so it cannot be inverted and cannot be pulled through. `Inverse()`
		// reports that and the loop drops it; the stem is then missing, which
		// is the honest outcome of a rig that can only pull. It costs a thin
		// line at the bottom of a picture that is otherwise the fern.
		const float f[ 4 ][ 6 ] = {
			//  a       b       c       d       e     f
			{ 0.00f, 0.00f, 0.00f, 0.16f, 0.00f, 0.00f },
			{ 0.85f, 0.04f, -0.04f, 0.85f, 0.00f, 1.60f },
			{ 0.20f, -0.26f, 0.23f, 0.22f, 0.00f, 1.60f },
			{ -0.15f, 0.28f, 0.26f, 0.24f, 0.00f, 0.44f },
		};

		for( int i = 0; i < 4; ++i )
		{
			Tap w;
			w.m[ 0 ] = f[ i ][ 0 ];
			w.m[ 1 ] = f[ i ][ 1 ];
			w.m[ 2 ] = f[ i ][ 2 ];
			w.m[ 3 ] = f[ i ][ 3 ];
			w.tx     = f[ i ][ 4 ];
			w.ty     = f[ i ][ 5 ];

			// Fern space is about 0.8 wide and 1.6 tall with the root at y = 0.
			set.taps[ i ] = Conjugate( w, 0.1852f, -0.0239f, -0.7750f );
		}
		set.count = 4;
		break;
	}

	case Rig::Kaleido:
	{
		// A mirror wedge: N taps evenly spaced in rotation, at unit scale
		// because a mirror does not shrink what it reflects.
		//
		// That makes the set exactly non-contractive on its own, and it is
		// meant to. The camera's zoom is the only contraction in a kaleidoscope
		// rig, which is why `Contractivity()` deliberately excludes the camera
		// and `LoopGain()` in Loop.h is the number that decides stability.
		const int n = count < 2 ? 2 : ( count > kMaxTaps ? kMaxTaps : count );
		for( int i = 0; i < n; ++i )
		{
			set.taps[ i ]        = Similarity( 1.0f, 2.0f * kPi * float( i ) / float( n ), 0.0f, 0.0f );
			set.taps[ i ].weight = 1.0f / float( n );

			// Each bounce off a real mirror is another pass through its
			// coating, so the taps that have travelled furthest are the most
			// coloured. Small, and it is most of why a wedge rig looks like
			// glass rather than like a copy operation.
			set.taps[ i ].hue = 0.012f * float( i );
		}
		set.count = n;
		break;
	}

	case Rig::Seeded:
	{
		// N contractions from the seed.
		//
		// The scale range is the whole design of this rig. Below about 0.3 the
		// maps shrink faster than the frame can show and the attractor is a few
		// specks; above about 0.7 the copies overlap so heavily that the
		// attractor fills its own convex hull and looks like a smudge. Between
		// those it is reliably a fractal that has structure at the scales a
		// screen can resolve, which is the only test that matters for something
		// an operator is going to pick by nudging a number.
		const int n = count < 2 ? 2 : ( count > kMaxTaps ? kMaxTaps : count );
		for( int i = 0; i < n; ++i )
		{
			const uint32_t h0 = Hash2( uint32_t( i ) * 4u + 0u, seed );
			const uint32_t h1 = Hash2( uint32_t( i ) * 4u + 1u, seed );
			const uint32_t h2 = Hash2( uint32_t( i ) * 4u + 2u, seed );
			const uint32_t h3 = Hash2( uint32_t( i ) * 4u + 3u, seed );

			const float scale = 0.34f + 0.32f * Unit( h0 );
			const float angle = 2.0f * kPi * Unit( h1 );

			// Fixed points spread around a disc rather than uniformly over the
			// square: a map whose fixed point is in a corner pulls the whole
			// attractor into that corner and off the frame.
			const float rad = 0.75f * std::sqrt( Unit( h2 ) );
			const float arg = 2.0f * kPi * Unit( h3 );
			const float fx  = rad * std::cos( arg );
			const float fy  = rad * std::sin( arg );

			Tap tap = Similarity( scale, angle, 0.0f, 0.0f );

			// Place the translation so the map's fixed point lands on (fx,fy):
			// t = f - M f. Choosing the fixed point rather than the offset is
			// what keeps a seeded rig on screen -- the attractor always lies in
			// the convex hull of the fixed points.
			tap.tx = fx - ( tap.m[ 0 ] * fx + tap.m[ 1 ] * fy );
			tap.ty = fy - ( tap.m[ 2 ] * fx + tap.m[ 3 ] * fy );

			tap.weight = 1.0f;
			tap.hue    = 0.5f * Signed( Hash2( uint32_t( i ) + 977u, seed ) ) * 0.08f;

			set.taps[ i ] = tap;
		}
		set.count = n;

		// Measured, not assumed -- see FitToFrame().
		set = FitToFrame( set, 1.55f );
		break;
	}
	}

	return set;
}

TapSet FitToFrame( const TapSet& set, float extent )
{
	if( set.count < 1 )
		return set;

	float x = 0.0f, y = 0.0f;
	float x0 = 1e9f, x1 = -1e9f, y0 = 1e9f, y1 = -1e9f;
	uint32_t s = 0x9e3779b9u;

	// 20000 is where the box stops moving in the fourth decimal for every seed
	// tried. The first 200 are dropped: the orbit starts at the origin, which
	// is not on the attractor, and the points before it converges are the one
	// part of the walk that is not a sample of the shape being measured.
	for( int i = 0; i < 20000; ++i )
	{
		s            = Hash32( s );
		const Tap& t = set.taps[ s % uint32_t( set.count ) ];

		const float nx = t.m[ 0 ] * x + t.m[ 1 ] * y + t.tx;
		const float ny = t.m[ 2 ] * x + t.m[ 3 ] * y + t.ty;
		x              = nx;
		y              = ny;

		if( i < 200 )
			continue;

		x0 = x < x0 ? x : x0;
		x1 = x > x1 ? x : x1;
		y0 = y < y0 ? y : y0;
		y1 = y > y1 ? y : y1;
	}

	const float w = x1 - x0;
	const float h = y1 - y0;
	const float m = w > h ? w : h;

	// A set whose maps are contractions cannot produce a degenerate box unless
	// every map shares a fixed point, which a seeded set can do by coincidence.
	// Scaling that up by 1/tiny would throw the attractor across the universe.
	if( !( m > 1e-4f ) )
		return set;

	const float k  = extent / m;
	const float ox = -k * ( x0 + x1 ) * 0.5f;
	const float oy = -k * ( y0 + y1 ) * 0.5f;

	TapSet out = set;
	for( int i = 0; i < set.count; ++i )
		out.taps[ i ] = Conjugate( set.taps[ i ], k, ox, oy );

	return out;
}

bool Inverse( const Tap& tap, Tap& out )
{
	const float det = tap.m[ 0 ] * tap.m[ 3 ] - tap.m[ 1 ] * tap.m[ 2 ];

	// Not `det == 0`. A map can be numerically invertible and still useless:
	// inverting a determinant of 1e-9 gives entries around 1e9, the sample
	// coordinate leaves the frame by nine orders of magnitude, and what comes
	// back is the edge texel smeared across the picture. The cutoff is where
	// the inverse stops being able to address the frame at all.
	if( std::fabs( det ) < 1e-6f )
		return false;

	const float inv = 1.0f / det;

	out        = tap;
	out.m[ 0 ] = tap.m[ 3 ] * inv;
	out.m[ 1 ] = -tap.m[ 1 ] * inv;
	out.m[ 2 ] = -tap.m[ 2 ] * inv;
	out.m[ 3 ] = tap.m[ 0 ] * inv;
	out.tx     = -( out.m[ 0 ] * tap.tx + out.m[ 1 ] * tap.ty );
	out.ty     = -( out.m[ 2 ] * tap.tx + out.m[ 3 ] * tap.ty );
	return true;
}

float Contractivity( const TapSet& set )
{
	float worst = 0.0f;

	for( int i = 0; i < set.count; ++i )
	{
		const Tap& t = set.taps[ i ];

		// The largest singular value of a 2x2, in closed form. Not the
		// determinant and not the largest entry: the determinant is the area
		// factor and a map can preserve area while stretching one axis by ten,
		// which is an expansion the loop will find even though the determinant
		// says it is neutral.
		const float a = t.m[ 0 ], b = t.m[ 1 ], c = t.m[ 2 ], d = t.m[ 3 ];
		const float e = ( a * a + b * b + c * c + d * d ) * 0.5f;
		const float f = ( a * a + b * b - c * c - d * d ) * 0.5f;
		const float g = a * c + b * d;
		const float s = std::sqrt( e + std::sqrt( f * f + g * g ) );

		if( s > worst )
			worst = s;
	}

	return worst;
}

TapSet WithCamera( const TapSet& set, float zoom, float rotate, float panX, float panY )
{
	// The camera is applied BEFORE the taps -- `tap after camera` -- because it
	// is a move of the lens, and the glass is downstream of the lens. Composing
	// the other way round is a move of the screen, which is a different rig and
	// looks it: the taps then swing around the frame instead of the frame
	// swinging behind the taps.
	const Tap camera = Similarity( zoom, rotate, panX, panY );

	TapSet out = set;
	for( int i = 0; i < set.count; ++i )
		out.taps[ i ] = Compose( set.taps[ i ], camera );

	return out;
}

} // namespace escapement
