#include "Controls.h"

#include <cmath>

namespace escapement
{
namespace
{
float Clamp01( float v )
{
	return v < 0.0f ? 0.0f : ( v > 1.0f ? 1.0f : v );
}

/// An exponential map from 0..1 onto lo..hi. Both must be positive.
float Exponential( float v, float lo, float hi )
{
	return lo * std::pow( hi / lo, Clamp01( v ) );
}

} // namespace

const char* InjectName( Inject inject )
{
	switch( inject )
	{
	case Inject::None:     return "None";
	case Inject::Dot:      return "Dot";
	case Inject::Bars:     return "Bars";
	case Inject::Grid:     return "Grid";
	case Inject::Noise:    return "Noise";
	case Inject::Iterator: return "Iterator";
	case Inject::Clip:     return "Clip";
	default:               return "?";
	}
}

const char* MaskModeName( MaskMode mode )
{
	switch( mode )
	{
	case MaskMode::Over:      return "Over";
	case MaskMode::Reveal:    return "Reveal";
	case MaskMode::Hide:      return "Hide";
	case MaskMode::Colourise: return "Colourise";
	default:                  return "?";
	}
}

const char* PaletteName( Palette palette )
{
	switch( palette )
	{
	case Palette::Signal:   return "Signal";
	case Palette::Phosphor: return "Phosphor";
	case Palette::Ice:      return "Ice";
	case Palette::Ember:    return "Ember";
	case Palette::Spectrum: return "Spectrum";
	default:                return "?";
	}
}

const char* SyncName( Sync sync )
{
	switch( sync )
	{
	case Sync::Free:   return "Free";
	case Sync::Beat:   return "Beat";
	case Sync::Bar:    return "Bar";
	case Sync::Manual: return "Manual";
	default:           return "?";
	}
}

const char* PrecisionName( Precision precision )
{
	switch( precision )
	{
	case Precision::Single:   return "Single";
	case Precision::Extended: return "Extended";
	default:                  return "?";
	}
}

float SignedFromParam( float value, float range, float shape )
{
	const float t = Clamp01( value ) * 2.0f - 1.0f;

	// `t == 0` exactly at `value == 0.5` exactly, and pow() preserves that: the
	// centre detent is not a tolerance test anywhere, it is this identity.
	const float mag = std::pow( std::fabs( t ), shape );
	return ( t < 0.0f ? -mag : mag ) * range;
}

int TapsFromParam( float value )
{
	const int n = 2 + int( Clamp01( value ) * 6.999f );
	return n > 8 ? 8 : n;
}

uint32_t SeedFromParam( float value )
{
	const int n = 1 + int( Clamp01( value ) * 9998.999f );
	return uint32_t( n );
}

int SymmetryFromParam( float value )
{
	const int n = 1 + int( Clamp01( value ) * 11.999f );
	return n > 12 ? 12 : n;
}

float FieldRateFromParam( float value )
{
	return Exponential( value, 5.0f, 120.0f );
}

float ZoomFromParam( float value )
{
	// Shape 3: at a tenth of the way off centre the zoom is 1.0001, which is a
	// crawl you can watch for a minute; at half way off centre it is 1.0125,
	// which crosses the frame in a few seconds. A linear control spends its
	// first tenth passing through everything anyone would use.
	return 1.0f + SignedFromParam( value, 0.10f, 3.0f );
}

float RotateFromParam( float value )
{
	return SignedFromParam( value, 0.10f, 2.5f );
}

float PanFromParam( float value )
{
	return SignedFromParam( value, 0.05f, 2.5f );
}

float SpeedFromParam( float value )
{
	// A dead zone at the bottom so that "stopped" is reachable by dragging to
	// the end rather than by landing on it.
	if( value < 0.02f )
		return 0.0f;

	return std::pow( 4.0f, Clamp01( value ) * 2.0f - 1.0f );
}

float FocusFromParam( float value )
{
	return Clamp01( value ) * 5.0f;
}

float LensFromParam( float value )
{
	return SignedFromParam( value, 1.0f, 1.6f );
}

float GainFromParam( float value )
{
	// Linear, and 0.625 of the travel is exactly 1.0 -- 1.6 * 0.625 == 1. The
	// oscillation threshold being at a round fraction of the slider is worth
	// more than a curve here: it is the one setting an operator needs to be able
	// to find again in the dark.
	return Clamp01( value ) * 1.6f;
}

float PedestalFromParam( float value )
{
	return SignedFromParam( value, 0.1f, 2.0f );
}

float GammaFromParam( float value )
{
	// 0.4 and 2.5 are reciprocals, so the logarithmic midpoint is exactly 1.0
	// and the centre detent costs no special case.
	return Exponential( value, 0.4f, 2.5f );
}

float HueRotateFromParam( float value )
{
	return SignedFromParam( value, 0.02f, 2.0f );
}

float SaturationFromParam( float value )
{
	return Clamp01( value ) * 2.0f;
}

float ClipFromParam( float value )
{
	return 0.3f + Clamp01( value ) * 0.7f;
}

float NoiseFromParam( float value )
{
	const float v = Clamp01( value );
	return v * v * 0.08f;
}

float DecayFromParam( float value )
{
	return Clamp01( value ) * 0.98f;
}

float InjectLevelFromParam( float value )
{
	if( value < 0.005f )
		return 0.0f;

	return Exponential( value, 0.01f, 2.0f );
}

float InjectSizeFromParam( float value )
{
	return Exponential( value, 0.01f, 1.5f );
}

float InjectPosFromParam( float value )
{
	return SignedFromParam( value, 1.5f, 1.0f );
}

int IterationsFromParam( float value )
{
	return int( Exponential( value, 16.0f, 2048.0f ) + 0.5f );
}

float JuliaFromParam( float value )
{
	// Shape 1.6 rather than 1: the interesting Julia constants are clustered
	// near the boundary of the Mandelbrot set, which for both components means
	// the middle of this range rather than its ends.
	return SignedFromParam( value, 2.0f, 1.6f );
}

double EscapeZoomFromParam( float value )
{
	return std::pow( 10.0, double( Clamp01( value ) ) * 13.0 );
}

double EscapeCentreFromParam( float value )
{
	return double( SignedFromParam( value, 2.0f, 1.0f ) );
}

float SphereFromParam( float value )
{
	return Clamp01( value );
}

float TiltFromParam( float value )
{
	return SignedFromParam( value, 1.57079633f, 1.0f );
}

float SpinFromParam( float value )
{
	return SignedFromParam( value, 1.0f, 2.0f );
}

float PaletteShiftFromParam( float value )
{
	return Clamp01( value );
}

} // namespace escapement
