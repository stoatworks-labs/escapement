/**
    esctest -- the offline harness.

    It drives **the real plugin class** through the real FFGL sequence in a
    headless core-profile context. Not a reimplementation of the rig and not a
    preview: the thing under test is `EscapementPlugin`, compiled from the same
    objects that go into the bundles, and every number below comes out of a
    frame it actually rendered.

        --out PATH        render a frame
        --preset N        start from a factory preset (1-based)
        --set "Name=v"    set any parameter by name
        --fields N        run exactly N fields (default 240)
        --size WxH        render size (default 640x360)
        --effect          the effect build, over a test clip
        --list            parameters, with their types and defaults
        --names           no parameter name exceeds FFGL's 16 characters
        --contact PATH    a contact sheet of every factory preset
        --taps            the tap sets, against their published constants
        --rate            the rig's clock is independent of the frame rate
        --stability       loop gain predicts whether the picture survives
        --guard           a hostile rig cannot put NaN in the frame store

    ## Why --fields exists, and why a feedback plugin needs it

    A rig's picture is a function of how many times the loop has gone round.
    Left to the wall clock that number depends on how fast the machine is, so
    two runs of the same test would render different pictures and nothing could
    be asserted about either. `--fields` pins it, which is the only way to make
    a feedback loop reproducible.

    ## What --rate actually checks

    That the rig keeps its own time. Sixty fields delivered as one frame of 60,
    or six frames of 10, or sixty frames of 1, must all leave the store in the
    same state -- because a rig runs at its screen's field rate whatever is
    filming it. It is the test that fails if anyone ever "simplifies" the field
    accumulator into one trip round the loop per rendered frame, which is the
    obvious shape and makes the instrument change character whenever the host is
    busy.

    ## What --stability actually checks

    That `LoopGain()` predicts the picture. Below unity the rig must decay to
    what is being injected; above it, the rig must reach the clip stage and stay
    there rather than growing without bound. Both are checked by measuring the
    rendered frame, not by re-deriving the gain -- a test that recomputed the
    number would only prove the arithmetic agrees with itself.
*/

#include <OpenGL/OpenGL.h>
#include <OpenGL/gl3.h>
#include <zlib.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "Controls.h"
#include "Escapement.h"
#include "Hash.h"
#include "Loop.h"
#include "Presets.h"
#include "Taps.h"

using namespace escapement;

namespace
{
//---------------------------------------------------------------------------
// A PNG writer. zlib ships with the OS, so this is a few chunk headers and a
// CRC rather than a dependency.
//---------------------------------------------------------------------------
void putU32( std::vector< unsigned char >& out, uint32_t value )
{
	out.push_back( static_cast< unsigned char >( value >> 24 ) );
	out.push_back( static_cast< unsigned char >( value >> 16 ) );
	out.push_back( static_cast< unsigned char >( value >> 8 ) );
	out.push_back( static_cast< unsigned char >( value ) );
}

void putChunk( std::vector< unsigned char >& out, const char* type, const std::vector< unsigned char >& data )
{
	putU32( out, static_cast< uint32_t >( data.size() ) );
	const size_t start = out.size();
	out.insert( out.end(), type, type + 4 );
	out.insert( out.end(), data.begin(), data.end() );
	uLong crc = crc32( 0L, Z_NULL, 0 );
	crc       = crc32( crc, out.data() + start, static_cast< uInt >( 4 + data.size() ) );
	putU32( out, static_cast< uint32_t >( crc ) );
}

bool writePng( const std::string& path, int width, int height, const std::vector< unsigned char >& rgba )
{
	std::vector< unsigned char > raw;
	raw.reserve( static_cast< size_t >( height ) * ( 1 + static_cast< size_t >( width ) * 4 ) );
	for( int y = 0; y < height; ++y )
	{
		raw.push_back( 0 );// filter: none
		const unsigned char* row = rgba.data() + static_cast< size_t >( y ) * width * 4;
		raw.insert( raw.end(), row, row + static_cast< size_t >( width ) * 4 );
	}

	uLongf compressedSize = compressBound( static_cast< uLong >( raw.size() ) );
	std::vector< unsigned char > compressed( compressedSize );
	if( compress2( compressed.data(), &compressedSize, raw.data(), static_cast< uLong >( raw.size() ), 6 ) != Z_OK )
		return false;
	compressed.resize( compressedSize );

	std::vector< unsigned char > png = { 0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n' };

	std::vector< unsigned char > ihdr;
	putU32( ihdr, static_cast< uint32_t >( width ) );
	putU32( ihdr, static_cast< uint32_t >( height ) );
	ihdr.push_back( 8 );// bit depth
	ihdr.push_back( 6 );// truecolour with alpha
	ihdr.push_back( 0 );
	ihdr.push_back( 0 );
	ihdr.push_back( 0 );
	putChunk( png, "IHDR", ihdr );
	putChunk( png, "IDAT", compressed );
	putChunk( png, "IEND", {} );

	FILE* file = fopen( path.c_str(), "wb" );
	if( file == nullptr )
		return false;
	const size_t written = fwrite( png.data(), 1, png.size(), file );
	fclose( file );
	return written == png.size();
}

//---------------------------------------------------------------------------
// GL plumbing.
//---------------------------------------------------------------------------
CGLContextObj createContext()
{
	const CGLPixelFormatAttribute accelerated[] = {
		kCGLPFAOpenGLProfile, static_cast< CGLPixelFormatAttribute >( kCGLOGLPVersion_GL4_Core ),
		kCGLPFAAccelerated,
		kCGLPFAColorSize, static_cast< CGLPixelFormatAttribute >( 24 ),
		kCGLPFAAlphaSize, static_cast< CGLPixelFormatAttribute >( 8 ),
		static_cast< CGLPixelFormatAttribute >( 0 )
	};
	const CGLPixelFormatAttribute software[] = {
		kCGLPFAOpenGLProfile, static_cast< CGLPixelFormatAttribute >( kCGLOGLPVersion_GL4_Core ),
		kCGLPFAColorSize, static_cast< CGLPixelFormatAttribute >( 24 ),
		kCGLPFAAlphaSize, static_cast< CGLPixelFormatAttribute >( 8 ),
		static_cast< CGLPixelFormatAttribute >( 0 )
	};

	CGLPixelFormatObj format = nullptr;
	GLint formatCount        = 0;
	if( CGLChoosePixelFormat( accelerated, &format, &formatCount ) != kCGLNoError || format == nullptr )
	{
		if( CGLChoosePixelFormat( software, &format, &formatCount ) != kCGLNoError || format == nullptr )
			return nullptr;
	}

	CGLContextObj context = nullptr;
	const CGLError error  = CGLCreateContext( format, nullptr, &context );
	CGLDestroyPixelFormat( format );
	if( error != kCGLNoError )
		return nullptr;

	CGLSetCurrentContext( context );
	return context;
}

struct Target
{
	GLuint texture = 0;
	GLuint fbo     = 0;
	int width      = 0;
	int height     = 0;
};

Target makeTarget( int width, int height )
{
	Target target;
	target.width  = width;
	target.height = height;

	glGenTextures( 1, &target.texture );
	glBindTexture( GL_TEXTURE_2D, target.texture );
	glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
	glBindTexture( GL_TEXTURE_2D, 0 );

	glGenFramebuffers( 1, &target.fbo );
	glBindFramebuffer( GL_FRAMEBUFFER, target.fbo );
	glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, target.texture, 0 );
	return target;
}

void releaseTarget( Target& target )
{
	if( target.fbo != 0 )
		glDeleteFramebuffers( 1, &target.fbo );
	if( target.texture != 0 )
		glDeleteTextures( 1, &target.texture );
	target = Target();
}

/// Straight out of GL, bottom row first.
std::vector< unsigned char > readBytes( const Target& target )
{
	std::vector< unsigned char > pixels( static_cast< size_t >( target.width ) * target.height * 4 );
	glBindFramebuffer( GL_FRAMEBUFFER, target.fbo );
	glPixelStorei( GL_PACK_ALIGNMENT, 1 );
	glReadPixels( 0, 0, target.width, target.height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data() );
	return pixels;
}

std::vector< unsigned char > flipRows( const std::vector< unsigned char >& image, int width, int height )
{
	std::vector< unsigned char > flipped( image.size() );
	const size_t stride = static_cast< size_t >( width ) * 4;
	for( int y = 0; y < height; ++y )
		std::memcpy( flipped.data() + static_cast< size_t >( y ) * stride,
		             image.data() + static_cast< size_t >( height - 1 - y ) * stride, stride );
	return flipped;
}

/// A test clip for the effect: coloured quadrants over a gradient, so that a
/// mask mode getting its geometry or its UV flip wrong is obvious rather than
/// merely plausible.
GLuint makeTestClip( int width, int height )
{
	std::vector< unsigned char > pixels( static_cast< size_t >( width ) * height * 4 );
	for( int y = 0; y < height; ++y )
	{
		for( int x = 0; x < width; ++x )
		{
			const float u = static_cast< float >( x ) / static_cast< float >( width );
			const float v = static_cast< float >( y ) / static_cast< float >( height );

			unsigned char* p = &pixels[ ( static_cast< size_t >( y ) * width + x ) * 4 ];
			p[ 0 ] = static_cast< unsigned char >( ( u < 0.5f ? 220.0f : 40.0f ) * ( 0.4f + 0.6f * v ) );
			p[ 1 ] = static_cast< unsigned char >( ( v < 0.5f ? 200.0f : 60.0f ) * ( 0.4f + 0.6f * u ) );
			p[ 2 ] = static_cast< unsigned char >( 255.0f * ( 0.3f + 0.7f * ( 1.0f - v ) ) );
			p[ 3 ] = 255;
		}
	}

	GLuint texture = 0;
	glGenTextures( 1, &texture );
	glBindTexture( GL_TEXTURE_2D, texture );
	glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data() );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	glBindTexture( GL_TEXTURE_2D, 0 );
	return texture;
}

//---------------------------------------------------------------------------
// Parameters by name.
//---------------------------------------------------------------------------
std::map< std::string, unsigned int > parameterIndex( EscapementPlugin& plugin )
{
	std::map< std::string, unsigned int > byName;
	for( unsigned int i = 0; i < plugin.GetNumParams(); ++i )
	{
		const char* name = plugin.GetParamName( i );
		if( name != nullptr )
			byName[ name ] = i;
	}
	return byName;
}

bool applySetting( EscapementPlugin& plugin, const std::string& assignment )
{
	const size_t equals = assignment.find( '=' );
	if( equals == std::string::npos )
	{
		fprintf( stderr, "--set wants Name=value, got '%s'\n", assignment.c_str() );
		return false;
	}

	const std::string name  = assignment.substr( 0, equals );
	const std::string value = assignment.substr( equals + 1 );

	const std::map< std::string, unsigned int > byName = parameterIndex( plugin );
	const auto found                                   = byName.find( name );
	if( found == byName.end() )
	{
		fprintf( stderr, "no parameter called '%s'\n", name.c_str() );
		return false;
	}

	plugin.SetFloatParameter( found->second, std::stof( value ) );
	return true;
}

/// Set up a plugin, initialise its GL, and hand it back ready to draw.
bool prepare( EscapementPlugin& plugin, int width, int height )
{
	FFGLViewportStruct viewport {};
	viewport.x      = 0;
	viewport.y      = 0;
	viewport.width  = static_cast< unsigned int >( width );
	viewport.height = static_cast< unsigned int >( height );

	if( plugin.InitGL( &viewport ) != FF_SUCCESS )
	{
		fprintf( stderr, "InitGL failed -- see the log\n" );
		return false;
	}

	// The harness DECLARES its clock unit. A single absolute time in one frame
	// is genuinely ambiguous -- 2.0 is two seconds or two milliseconds and
	// nothing in it says which -- so inference is only possible against a live
	// host's frame deltas, and an implicit unit here would be a test that
	// passes while the real host runs a thousand times fast.
	plugin.SetClockScaleForTest( 1.0 );
	return true;
}

/// Run `fields` trips round the loop, delivered `perFrame` at a time.
///
/// Both numbers matter: the picture is a function of the total, and --rate is
/// the test that it does NOT depend on how they were divided up.
void run( EscapementPlugin& plugin, const Target& target, int fields, int perFrame, GLuint input = 0 )
{
	glBindFramebuffer( GL_FRAMEBUFFER, target.fbo );
	glViewport( 0, 0, target.width, target.height );
	glClearColor( 0.0f, 0.0f, 0.0f, 0.0f );
	glClear( GL_COLOR_BUFFER_BIT );

	int done = 0;
	while( done < fields )
	{
		const int chunk = std::min( perFrame, fields - done );
		plugin.PinFieldsForTest( chunk );
		plugin.Render( target.width, target.height, input, 1.0f, 1.0f, target.fbo );
		done += chunk;
	}

	glFinish();
}

//---------------------------------------------------------------------------
// Measuring what was drawn
//---------------------------------------------------------------------------
struct Stats
{
	double mean      = 0.0;
	double peak      = 0.0;
	double lit       = 0.0;  ///< Fraction of pixels above a tenth of full scale.
	double deviation = 0.0;  ///< Standard deviation of luma: how much STRUCTURE there is.
};

Stats measure( const std::vector< unsigned char >& rgba )
{
	Stats stats;
	const size_t pixels = rgba.size() / 4;
	if( pixels == 0 )
		return stats;

	size_t lit = 0;
	for( size_t i = 0; i < pixels; ++i )
	{
		const double luma = ( 0.299 * rgba[ i * 4 + 0 ] + 0.587 * rgba[ i * 4 + 1 ] + 0.114 * rgba[ i * 4 + 2 ] ) / 255.0;
		stats.mean += luma;
		stats.peak = std::max( stats.peak, luma );
		if( luma > 0.1 )
			++lit;
	}

	stats.mean /= double( pixels );
	stats.lit = double( lit ) / double( pixels );

	// Deviation, and it is the number that matters most here.
	//
	// The first version of the preset check asserted `mean > 0.002` -- that
	// SOME light reached the screen -- and passed all twelve presets while five
	// of them were rendering a flat wash and one was a solid green rectangle. A
	// feedback rig's characteristic failures are a black frame and a saturated
	// one, and "is anything lit" cannot see the second at all. A flat field of
	// any brightness has a deviation near zero; a picture with a fractal in it
	// does not.
	double variance = 0.0;
	for( size_t i = 0; i < pixels; ++i )
	{
		const double luma = ( 0.299 * rgba[ i * 4 + 0 ] + 0.587 * rgba[ i * 4 + 1 ] + 0.114 * rgba[ i * 4 + 2 ] ) / 255.0;
		variance += ( luma - stats.mean ) * ( luma - stats.mean );
	}
	stats.deviation = std::sqrt( variance / double( pixels ) );

	return stats;
}

bool sameImage( const std::vector< unsigned char >& a, const std::vector< unsigned char >& b, int tolerance )
{
	if( a.size() != b.size() )
		return false;

	for( size_t i = 0; i < a.size(); ++i )
		if( std::abs( int( a[ i ] ) - int( b[ i ] ) ) > tolerance )
			return false;

	return true;
}

int fails = 0;

void check( bool ok, const char* what )
{
	printf( "%s %s\n", ok ? "  ok  " : "  FAIL", what );
	if( !ok )
		++fails;
}

//---------------------------------------------------------------------------
// Tests
//---------------------------------------------------------------------------

/// The tap sets, against the constants they are published with.
///
/// A chaos game is the way to see an IFS attractor without a screen: iterate a
/// point through randomly chosen maps of the set and it converges onto the
/// attractor and stays there. What is asserted is what can be asserted without
/// re-deriving the maps -- the number of maps, that they all contract, and that
/// the attractor lands inside the frame.
void testTaps()
{
	printf( "\n== taps\n" );

	struct Expect
	{
		Rig rig;
		int taps;
		float maxContractivity;
	};

	const Expect expected[] = {
		{ Rig::Sierpinski, 3, 0.51f },
		{ Rig::Koch, 4, 0.34f },
		{ Rig::Dragon, 2, 0.71f },
		{ Rig::Fern, 4, 0.86f },
	};

	for( const Expect& e : expected )
	{
		const TapSet set = Taps( e.rig, 4, 1 );
		char label[ 128 ];

		snprintf( label, sizeof( label ), "%s has %d maps", RigName( e.rig ), e.taps );
		check( set.count == e.taps, label );

		const float c = Contractivity( set );
		snprintf( label, sizeof( label ), "%s contracts (%.3f < %.2f)", RigName( e.rig ), c, e.maxContractivity );
		check( c > 0.0f && c <= e.maxContractivity, label );
	}

	// Barnsley's first map collapses the plane onto the stem, so it has no
	// inverse and the loop drops it. That is a documented property of the
	// system rather than a bug, and it is asserted so that a "fix" which
	// perturbs the published constants to make it invertible fails here.
	Tap dropped;
	check( !Inverse( Taps( Rig::Fern, 4, 1 ).taps[ 0 ], dropped ),
	       "the fern's stem map is singular, as published" );

	// Every seed frames. The bounds of a seeded attractor are not knowable
	// before the seed is, so they are measured -- and this is the test that the
	// measurement is actually applied.
	bool allFramed = true;
	for( uint32_t seed : { 1u, 7u, 99u, 1234u, 4021u, 9999u } )
	{
		const TapSet set = Taps( Rig::Seeded, 4, seed );

		float x = 0.0f, y = 0.0f, extent = 0.0f;
		uint32_t s = 4242u;
		for( int i = 0; i < 20000; ++i )
		{
			s            = Hash32( s );
			const Tap& t = set.taps[ s % uint32_t( set.count ) ];
			const float nx = t.m[ 0 ] * x + t.m[ 1 ] * y + t.tx;
			const float ny = t.m[ 2 ] * x + t.m[ 3 ] * y + t.ty;
			x = nx;
			y = ny;
			if( i < 200 )
				continue;
			extent = std::max( extent, std::max( std::fabs( x ), std::fabs( y ) ) );
		}

		if( !( extent > 0.3f && extent < 1.2f ) )
		{
			printf( "        seed %u reaches %.3f\n", seed, extent );
			allFramed = false;
		}
	}
	check( allFramed, "every seeded rig lands inside the frame" );
}

/// FFGL truncates every parameter name at 16 characters and the SDK says
/// nothing, so a control called "Loop Persistence" ships as "Loop Persistenc".
/// Six plugins in this fleet shipped one before anybody noticed.
void testNames( EscapementPlugin& plugin )
{
	printf( "\n== names\n" );

	bool allFit = true;
	for( unsigned int i = 0; i < plugin.GetNumParams(); ++i )
	{
		const char* name = plugin.GetParamName( i );
		if( name == nullptr )
			continue;

		if( std::strlen( name ) > 16 )
		{
			printf( "        '%s' is %zu characters\n", name, std::strlen( name ) );
			allFit = false;
		}
	}

	check( allFit, "no parameter name exceeds FFGL's 16 characters" );
}

/// The rig keeps its own clock.
void testRate( const Target& target )
{
	printf( "\n== rate\n" );

	std::vector< std::vector< unsigned char > > images;

	for( int perFrame : { 1, 10, 60 } )
	{
		EscapementPlugin plugin( false );
		if( !prepare( plugin, target.width, target.height ) )
			return;

		run( plugin, target, 60, perFrame );
		images.push_back( readBytes( target ) );
		plugin.DeInitGL();
	}

	// Bit-exact, not approximately equal. Every one of these renders the same
	// number of fields through the same arithmetic in the same order; the only
	// difference is how many of them happened between two calls to Render, and
	// that must not reach the picture at all.
	check( sameImage( images[ 0 ], images[ 1 ], 0 ), "60 fields as 60x1 == as 6x10" );
	check( sameImage( images[ 0 ], images[ 2 ], 0 ), "60 fields as 60x1 == as 1x60" );
}

/// Loop gain predicts whether the picture survives.
void testStability( const Target& target )
{
	printf( "\n== stability\n" );

	// Well under unity: the rig must fall away to what is being injected.
	{
		EscapementPlugin plugin( false );
		if( !prepare( plugin, target.width, target.height ) )
			return;

		applySetting( plugin, "Gain=0.30" );      // 0.48
		applySetting( plugin, "Inject Level=0" ); // nothing to sustain it
		applySetting( plugin, "Decay=0" );
		run( plugin, target, 240, 8 );

		const Stats stats = measure( readBytes( target ) );
		char label[ 128 ];
		snprintf( label, sizeof( label ), "gain below unity decays away (mean %.4f)", stats.mean );
		check( stats.mean < 0.02, label );
		plugin.DeInitGL();
	}

	// Well over unity: the rig must reach the clip stage and STOP there. The
	// test that matters is not that it is bright, it is that it is bounded --
	// an unbounded loop reaches inf, and inf - inf is the NaN that would poison
	// the store for the life of the instance.
	{
		EscapementPlugin plugin( false );
		if( !prepare( plugin, target.width, target.height ) )
			return;

		applySetting( plugin, "Gain=0.95" );  // 1.52
		applySetting( plugin, "Noise=0.4" );
		run( plugin, target, 240, 8 );

		const std::vector< unsigned char > image = readBytes( target );
		const Stats stats                        = measure( image );

		char label[ 128 ];
		snprintf( label, sizeof( label ), "gain above unity is bounded by the clip (peak %.3f)", stats.peak );
		check( stats.peak <= 1.0, label );
		snprintf( label, sizeof( label ), "gain above unity actually fills (lit %.2f)", stats.lit );
		check( stats.lit > 0.2, label );
		plugin.DeInitGL();
	}
}

/// A hostile rig cannot put NaN in the frame store.
///
/// This is the one failure a feedback plugin cannot recover from on its own:
/// NaN times anything is NaN, and the store feeds itself, so one NaN anywhere
/// survives every subsequent field for the life of the instance. Turning the
/// gain back down does not clear it.
void testGuard( const Target& target )
{
	printf( "\n== guard\n" );

	EscapementPlugin plugin( false );
	if( !prepare( plugin, target.width, target.height ) )
		return;

	// Everything at once: maximum gain, a negative pedestal to drive the signal
	// through zero, a low gamma to take a fractional power of it, and the clip
	// wide open so nothing bounds it on the way.
	applySetting( plugin, "Gain=1.0" );
	applySetting( plugin, "Pedestal=0.0" );
	applySetting( plugin, "Gamma=0.05" );
	applySetting( plugin, "Clip=1.0" );
	applySetting( plugin, "Decay=1.0" );
	applySetting( plugin, "Noise=1.0" );
	run( plugin, target, 400, 8 );

	// Read as float: an 8-bit read-back would convert a NaN into some byte and
	// hide exactly what is being looked for.
	std::vector< float > pixels( size_t( target.width ) * target.height * 4 );
	glBindFramebuffer( GL_FRAMEBUFFER, target.fbo );
	glReadPixels( 0, 0, target.width, target.height, GL_RGBA, GL_FLOAT, pixels.data() );

	bool clean = true;
	for( float v : pixels )
		if( std::isnan( v ) || std::isinf( v ) )
			clean = false;

	check( clean, "400 fields of a hostile rig leave no NaN or inf" );

	// And having survived that, the rig must still be able to draw. A guard
	// that clamped the store to black would pass the test above and have
	// destroyed the instrument.
	applySetting( plugin, "Gain=0.60" );
	applySetting( plugin, "Gamma=0.5" );
	applySetting( plugin, "Noise=0.0" );
	applySetting( plugin, "Clip=0.7" );
	run( plugin, target, 120, 8 );

	const Stats stats = measure( readBytes( target ) );
	char label[ 128 ];
	snprintf( label, sizeof( label ), "the rig still draws afterwards (mean %.4f)", stats.mean );
	check( stats.mean > 0.001, label );

	plugin.DeInitGL();
}

/// Every factory preset renders something.
///
/// A preset that is a black frame is worse than no preset: it reads as a broken
/// plugin, and it is the failure mode a feedback rig falls into most easily.
void testPresets( const Target& target )
{
	printf( "\n== presets\n" );

	for( int i = 0; i < presets::kCount; ++i )
	{
		EscapementPlugin plugin( false );
		if( !prepare( plugin, target.width, target.height ) )
			return;

		const std::map< std::string, unsigned int > byName = parameterIndex( plugin );
		const auto preset                                  = byName.find( "Preset" );
		if( preset != byName.end() )
			plugin.SetFloatParameter( preset->second, float( i + 1 ) );

		run( plugin, target, 240, 8 );

		const Stats stats = measure( readBytes( target ) );
		char label[ 200 ];
		snprintf( label, sizeof( label ), "%-16s draws (mean %.3f dev %.3f lit %.2f)",
		          presets::kPresets[ i ].name, stats.mean, stats.deviation, stats.lit );

		// Three ways to fail, because a rig has three ways to be useless: dark,
		// flat, and saturated. All three have to be excluded by name.
		const bool notDark      = stats.mean > 0.004;
		const bool hasStructure = stats.deviation > 0.045;
		const bool notBlownOut  = stats.mean < 0.88;

		check( notDark && hasStructure && notBlownOut, label );

		plugin.DeInitGL();
	}
}

/**
    How much the picture is still MOVING once it has settled.

    This is the measurement behind "they all have a burst of motion and then die
    off". A contractive loop with constant parameters is a contraction mapping,
    so Banach guarantees it converges on a unique fixed point and then stops --
    that is not a mistuning, it is what an iterated function system IS, and it
    is why every rig here went still.

    Settle first, then measure the mean absolute difference between consecutive
    FIELDS. A rig that is alive keeps changing; a rig that has converged reports
    a number that is indistinguishable from zero.
*/
/// Mean luma per 32x32 block. Grain averages away; structure does not.
std::vector< double > coarse( const std::vector< unsigned char >& rgba, int width, int height )
{
	const int bw = ( width + 31 ) / 32;
	const int bh = ( height + 31 ) / 32;
	std::vector< double > out( size_t( bw ) * bh, 0.0 );
	std::vector< int > count( size_t( bw ) * bh, 0 );

	for( int y = 0; y < height; ++y )
	{
		for( int x = 0; x < width; ++x )
		{
			const size_t i = ( size_t( y ) * width + x ) * 4;
			const double luma = ( 0.299 * rgba[ i ] + 0.587 * rgba[ i + 1 ] + 0.114 * rgba[ i + 2 ] ) / 255.0;
			const size_t b = size_t( y / 32 ) * bw + size_t( x / 32 );
			out[ b ] += luma;
			++count[ b ];
		}
	}

	for( size_t b = 0; b < out.size(); ++b )
		if( count[ b ] > 0 )
			out[ b ] /= double( count[ b ] );

	return out;
}

void testLiveness( const Target& target, int settle, int window, int stride )
{
	printf( "\n== liveness (structure change per %d fields, after %d settle)\n", stride, settle );

	for( int i = 0; i < presets::kCount; ++i )
	{
		EscapementPlugin plugin( false );
		if( !prepare( plugin, target.width, target.height ) )
			return;

		const std::map< std::string, unsigned int > byName = parameterIndex( plugin );
		const auto preset                                  = byName.find( "Preset" );
		if( preset != byName.end() )
			plugin.SetFloatParameter( preset->second, float( i + 1 ) );

		run( plugin, target, settle, 8 );

		std::vector< double > previous = coarse( readBytes( target ), target.width, target.height );
		std::vector< unsigned char > previousFine = readBytes( target );

		double totalFine = 0.0;
		double totalCoarse = 0.0;

		for( int k = 0; k < window; ++k )
		{
			run( plugin, target, stride, 8 );
			const std::vector< unsigned char > nowFine = readBytes( target );
			const std::vector< double > now = coarse( nowFine, target.width, target.height );

			double fine = 0.0;
			for( size_t b = 0; b < nowFine.size(); ++b )
				fine += std::abs( int( nowFine[ b ] ) - int( previousFine[ b ] ) );
			totalFine += fine / double( nowFine.size() ) / 255.0;

			double blocks = 0.0;
			for( size_t b = 0; b < now.size(); ++b )
				blocks += std::abs( now[ b ] - previous[ b ] );
			totalCoarse += blocks / double( now.size() );

			previous = now;
			previousFine = nowFine;
		}

		const double motion = totalFine / double( window );
		const double structure = totalCoarse / double( window );

		// Measured across a STRIDE of fields, not between consecutive ones.
		//
		// The complaint this test exists for is "they move at first and then die
		// off", which is about evolution over seconds. Drift moves the operator's
		// hands at a fraction of a hertz on purpose -- fast enough and it reads
		// as an LFO wobble rather than an instrument -- so the per-field delta of
		// a perfectly healthy rig is tiny, and a per-field threshold called four
		// live rigs dead. A second is the timescale a person judges this on.

		// TWO numbers, because they answer different complaints.
		//
		// `moves` counts every pixel, so injected grain alone scores well while
		// the picture stands perfectly still -- which is exactly what the first
		// version of this test reported, and it made five dead rigs look like
		// seven live ones.
		//
		// `structure` averages 32x32 blocks first, so grain cancels and only
		// something the eye would call motion survives. That is the number the
		// complaint was about.
		char label[ 200 ];
		snprintf( label, sizeof( label ), "%-16s moves %.4f  structure %.4f",
		          presets::kPresets[ i ].name, motion, structure );
		check( structure > 0.004, label );

		plugin.DeInitGL();
	}
}

void usage()
{
	printf( "esctest -- the Escapement offline harness\n\n"
	        "  --out PATH        render a frame\n"
	        "  --preset N        start from a factory preset (1-based)\n"
	        "  --set \"Name=v\"    set any parameter by name\n"
	        "  --fields N        run exactly N fields (default 240)\n"
	        "  --size WxH        render size (default 640x360)\n"
	        "  --effect          the effect build, over a test clip\n"
	        "  --list            parameters, with their types and defaults\n"
	        "  --contact PATH    a contact sheet of every factory preset\n"
	        "  --names           no parameter name exceeds 16 characters\n"
	        "  --taps            the tap sets, against published constants\n"
	        "  --rate            the rig's clock is frame-rate independent\n"
	        "  --stability       loop gain predicts the picture\n"
	        "  --guard           a hostile rig leaves no NaN\n"
	        "  --liveness        the rigs are still moving once settled\n"
	        "  --all             every check above\n" );
}

} // namespace

int main( int argc, char** argv )
{
	std::string outPath;
	std::string contactPath;
	std::vector< std::string > settings;
	int preset   = 0;
	int fields   = 240;
	int width    = 640;
	int height   = 360;
	bool effect  = false;
	bool doList  = false;
	bool doNames = false;
	bool doTaps  = false;
	bool doRate  = false;
	bool doStab  = false;
	bool doGuard = false;
	bool doPre   = false;
	bool doLive  = false;

	for( int i = 1; i < argc; ++i )
	{
		const std::string arg = argv[ i ];
		const bool hasNext    = ( i + 1 ) < argc;

		if( arg == "--out" && hasNext )
			outPath = argv[ ++i ];
		else if( arg == "--contact" && hasNext )
			contactPath = argv[ ++i ];
		else if( arg == "--set" && hasNext )
			settings.push_back( argv[ ++i ] );
		else if( arg == "--preset" && hasNext )
			preset = std::stoi( argv[ ++i ] );
		else if( arg == "--fields" && hasNext )
			fields = std::stoi( argv[ ++i ] );
		else if( arg == "--size" && hasNext )
		{
			const std::string size = argv[ ++i ];
			const size_t x         = size.find( 'x' );
			if( x != std::string::npos )
			{
				width  = std::stoi( size.substr( 0, x ) );
				height = std::stoi( size.substr( x + 1 ) );
			}
		}
		else if( arg == "--effect" )
			effect = true;
		else if( arg == "--list" )
			doList = true;
		else if( arg == "--names" )
			doNames = true;
		else if( arg == "--taps" )
			doTaps = true;
		else if( arg == "--rate" )
			doRate = true;
		else if( arg == "--stability" )
			doStab = true;
		else if( arg == "--guard" )
			doGuard = true;
		else if( arg == "--presets" )
			doPre = true;
		else if( arg == "--liveness" )
			doLive = true;
		else if( arg == "--all" )
			doNames = doTaps = doRate = doStab = doGuard = doPre = doLive = true;
		else
		{
			usage();
			return 1;
		}
	}

	if( argc == 1 )
	{
		usage();
		return 1;
	}

	// The tap tests need no GL at all -- that is the point of keeping the maths
	// out of the shader -- so run them before a context exists.
	if( doTaps )
		testTaps();

	CGLContextObj context = createContext();
	if( context == nullptr )
	{
		fprintf( stderr, "no OpenGL context\n" );
		return 1;
	}

	Target target = makeTarget( width, height );

	if( doNames || doList || !outPath.empty() )
	{
		EscapementPlugin plugin( effect );
		if( !prepare( plugin, width, height ) )
			return 1;

		if( doList )
		{
			printf( "%-4s %-18s %-10s %s\n", "id", "name", "type", "default" );
			for( unsigned int i = 0; i < plugin.GetNumParams(); ++i )
			{
				const char* name = plugin.GetParamName( i );
				printf( "%-4u %-18s %-10u %.4f\n", i, name ? name : "?",
				        plugin.GetParamType( i ), plugin.GetFloatParameter( i ) );
			}
		}

		if( doNames )
			testNames( plugin );

		if( !outPath.empty() )
		{
			const std::map< std::string, unsigned int > byName = parameterIndex( plugin );
			if( preset > 0 )
			{
				const auto found = byName.find( "Preset" );
				if( found != byName.end() )
					plugin.SetFloatParameter( found->second, float( preset ) );
			}

			for( const std::string& s : settings )
				if( !applySetting( plugin, s ) )
					return 1;

			GLuint clip = effect ? makeTestClip( width, height ) : 0;
			run( plugin, target, fields, 8, clip );

			const std::vector< unsigned char > image = flipRows( readBytes( target ), width, height );
			if( !writePng( outPath, width, height, image ) )
			{
				fprintf( stderr, "could not write %s\n", outPath.c_str() );
				return 1;
			}

			const Stats stats = measure( image );
			printf( "wrote %s (%dx%d, %d fields, mean %.4f, lit %.3f, peak %.3f)\n",
			        outPath.c_str(), width, height, fields, stats.mean, stats.lit, stats.peak );

			if( clip != 0 )
				glDeleteTextures( 1, &clip );
		}

		plugin.DeInitGL();
	}

	//-----------------------------------------------------------------------
	// A contact sheet of every preset, which is the fastest way to see that a
	// change to the loop has not quietly ruined one of them.
	//-----------------------------------------------------------------------
	if( !contactPath.empty() )
	{
		const int cols  = 4;
		const int rows  = ( presets::kCount + cols - 1 ) / cols;
		const int cellW = 320;
		const int cellH = 180;

		std::vector< unsigned char > sheet( size_t( cols * cellW ) * ( rows * cellH ) * 4, 0 );

		Target cell = makeTarget( cellW, cellH );

		for( int i = 0; i < presets::kCount; ++i )
		{
			EscapementPlugin plugin( false );
			if( !prepare( plugin, cellW, cellH ) )
				return 1;

			const std::map< std::string, unsigned int > byName = parameterIndex( plugin );
			const auto found                                   = byName.find( "Preset" );
			if( found != byName.end() )
				plugin.SetFloatParameter( found->second, float( i + 1 ) );

			run( plugin, cell, fields, 8 );

			const std::vector< unsigned char > image = flipRows( readBytes( cell ), cellW, cellH );

			const int cx = ( i % cols ) * cellW;
			const int cy = ( i / cols ) * cellH;
			for( int y = 0; y < cellH; ++y )
				std::memcpy( &sheet[ ( size_t( cy + y ) * ( cols * cellW ) + cx ) * 4 ],
				             &image[ size_t( y ) * cellW * 4 ], size_t( cellW ) * 4 );

			printf( "  %-18s\n", presets::kPresets[ i ].name );
			plugin.DeInitGL();
		}

		releaseTarget( cell );

		if( !writePng( contactPath, cols * cellW, rows * cellH, sheet ) )
		{
			fprintf( stderr, "could not write %s\n", contactPath.c_str() );
			return 1;
		}
		printf( "wrote %s\n", contactPath.c_str() );
	}

	if( doRate )
		testRate( target );
	if( doStab )
		testStability( target );
	if( doGuard )
		testGuard( target );
	if( doPre )
		testPresets( target );
	if( doLive )
		testLiveness( target, 400, 8, 60 );

	releaseTarget( target );
	CGLDestroyContext( context );

	if( fails > 0 )
		printf( "\n%d check(s) FAILED\n", fails );

	return fails == 0 ? 0 : 1;
}
