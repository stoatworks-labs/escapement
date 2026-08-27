#include "Escapement.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <string>

#include "Diag.h"
#include "Shaders.h"

namespace escapement
{
namespace
{
/// Read an option parameter. Option parameters do NOT hold 0..1: the host stores
/// the element value the operator chose, so this is an index and clamping it is
/// the only thing standing between a stale composition and an out-of-range enum.
int Option( float value, int count )
{
	const int i = static_cast< int >( std::lround( value ) );
	return std::min( count - 1, std::max( 0, i ) );
}

float Clamp01( float v )
{
	return std::min( 1.0f, std::max( 0.0f, v ) );
}

/// Insert defines after the `#version` line, which must be first in a GLSL
/// source.
std::string WithDefines( const char* shader, const char* defines )
{
	std::string source( shader );
	if( defines == nullptr || *defines == '\0' )
		return source;

	const size_t afterVersion = source.find( '\n' );
	if( afterVersion != std::string::npos )
		source.insert( afterVersion + 1, defines );

	return source;
}

const char* const kEffectDefine = "#define ESCAPEMENT_EFFECT 1\n";

/// Frames that must agree before the host's clock unit is settled.
constexpr int kClockVotes = 4;

/// Wall clock, to calibrate the host's against. Steady rather than system, so
/// nothing here moves if the machine's clock is corrected.
double wallSeconds()
{
	using namespace std::chrono;
	static const steady_clock::time_point start = steady_clock::now();
	return duration_cast< duration< double > >( steady_clock::now() - start ).count();
}

} // namespace

// The buttons are declared one per link, so the run in the enum and the run the
// block actually has must agree.
static_assert( PT_COUNT - PT_ABOUT_TEXT == stoatworks::about::kParamCount,
               "the About run no longer matches StoatworksAbout.h -- "
               "add or remove a PT_ABOUT_BUTTON_n to match" );

constexpr unsigned int EscapementPlugin::kPresetParamIDs[ presets::kParamCount ];

EscapementPlugin::EscapementPlugin( bool overInput ) :
	overInput( overInput )
{
	// The source has no input; the effect takes one.
	SetMinInputs( overInput ? 1 : 0 );
	SetMaxInputs( overInput ? 1 : 0 );

	//-----------------------------------------------------------------------
	// Defaults.
	//
	// Set BEFORE the parameters are declared, because SetOptionParamInfo takes
	// the default as an argument and reads it from here.
	//
	// The opening position is the Mirror Tunnel preset, VALUE FOR VALUE, and it
	// is chosen rather than inherited: a feedback rig at its neutral settings --
	// unity gain, no zoom, nothing injected -- is a black frame, and a plugin
	// that shows nothing when you drop it on a layer reads as broken.
	//
	// It has to be kept in step with that preset by hand, and it was not: the
	// preset was retuned and these were left behind, so the plugin shipped
	// opening on a rig running at 1.28 round the loop that saturated to a flat
	// white frame within two seconds -- the OTHER way a feedback plugin reads as
	// broken. The browser demo is what made it obvious, because a demo shows the
	// defaults and nothing else until you touch something.
	//
	// If you retune "Mirror Tunnel" in Presets.h, retune these with it.
	//-----------------------------------------------------------------------
	params[ PT_RIG ]         = static_cast< float >( Rig::Mirror );
	params[ PT_TAPS ]        = 0.30f;   // 4 taps, for the rigs that choose
	params[ PT_SEED ]        = 0.402f;  // seed 4021
	params[ PT_SYMMETRY ]    = 0.0f;    // no fold
	params[ PT_FOLD_MIRROR ] = 0.0f;
	params[ PT_FIELD_RATE ]  = 0.782f;  // 60 fields a second

	params[ PT_ZOOM ]     = 0.208f;     // 0.98 per field -- BELOW one, see below
	params[ PT_ROTATE ]   = 0.699f;     // 0.01 rad per field
	params[ PT_PAN_X ]    = 0.5f;       // exactly zero
	params[ PT_PAN_Y ]    = 0.5f;
	params[ PT_SYNC ]     = static_cast< float >( Sync::Free );
	params[ PT_SPEED ]    = 0.5f;       // exactly 1.0
	params[ PT_FOCUS ]    = 0.085f;     // 0.43% of the short edge
	params[ PT_LENS ]     = 0.5f;       // exactly zero
	params[ PT_VIGNETTE ] = 0.08f;

	// Drift is ON by default, and that is a decision rather than a taste.
	// A rig with the hands off converges and stops -- see ApplyDrift in Loop.h --
	// so a plugin whose default is drift 0 is a plugin that dies about ten
	// seconds after an operator drops it on a layer, which is precisely the
	// complaint this was added to answer.
	params[ PT_DRIFT ]      = 0.45f;
	params[ PT_DRIFT_RATE ] = 0.520f;  // 0.08 Hz -- most of a minute to come round

	params[ PT_GAIN ]       = 0.600f;   // 0.96
	params[ PT_PEDESTAL ]   = 0.5f;     // exactly zero
	params[ PT_GAMMA ]      = 0.5f;     // exactly 1.0
	params[ PT_HUE_ROTATE ] = 0.658f;   // 0.002 turns per field
	params[ PT_SATURATION ] = 0.55f;    // 1.1
	params[ PT_CLIP ]       = 0.786f;   // 0.85
	params[ PT_NOISE ]      = 0.0f;
	params[ PT_DECAY ]      = 0.050f;   // 0.049 -- 1.01 round the loop

	params[ PT_INJECT ]       = static_cast< float >( Inject::Grid );
	params[ PT_INJECT_LEVEL ] = 0.550f; // 0.19
	params[ PT_INJECT_SIZE ]  = 0.720f; // 0.37 of the short edge
	params[ PT_INJECT_X ]     = 0.5f;   // exactly zero
	params[ PT_INJECT_Y ]     = 0.5f;

	params[ PT_ITERATIONS ]   = 0.571f; // 256
	params[ PT_JULIA_X ]      = 0.360f;
	params[ PT_JULIA_Y ]      = 0.640f;
	params[ PT_ESCAPE_ZOOM ]  = 0.0f;   // 1:1
	params[ PT_ESCAPE_X ]     = 0.5f;
	params[ PT_ESCAPE_Y ]     = 0.5f;
	params[ PT_PRECISION ]    = static_cast< float >( Precision::Single );

	params[ PT_SPHERE ] = 0.0f;
	params[ PT_TILT ]   = 0.611f;       // 0.35 rad
	params[ PT_SPIN ]   = 0.612f;       // 0.05 turns a second
	params[ PT_LIGHT ]  = 0.5f;

	params[ PT_PALETTE ]       = static_cast< float >( Palette::Signal );
	params[ PT_PALETTE_SHIFT ] = 0.0f;
	params[ PT_OPACITY ]       = 1.0f;

	params[ PT_MASK_MODE ] = static_cast< float >( MaskMode::Over );
	params[ PT_MIX ]       = 1.0f;

	//-----------------------------------------------------------------------
	// Declaration. This order is the order the host shows them in.
	//-----------------------------------------------------------------------
	SetOptionParamInfo( PT_RIG, "Rig", static_cast< int >( Rig::Count ), params[ PT_RIG ] );
	for( unsigned int i = 0; i < static_cast< unsigned int >( Rig::Count ); ++i )
		SetParamElementInfo( PT_RIG, i, RigName( static_cast< Rig >( i ) ), static_cast< float >( i ) );

	SetParamInfof( PT_TAPS, "Taps", FF_TYPE_STANDARD );
	SetParamInfof( PT_SEED, "Seed", FF_TYPE_STANDARD );
	SetParamInfof( PT_SYMMETRY, "Symmetry", FF_TYPE_STANDARD );
	SetParamInfo( PT_FOLD_MIRROR, "Mirror Wedge", FF_TYPE_BOOLEAN, false );
	SetParamInfof( PT_FIELD_RATE, "Field Rate", FF_TYPE_STANDARD );

	SetParamInfof( PT_ZOOM, "Zoom", FF_TYPE_STANDARD );
	SetParamInfof( PT_ROTATE, "Rotate", FF_TYPE_STANDARD );
	SetParamInfof( PT_PAN_X, "Pan X", FF_TYPE_STANDARD );
	SetParamInfof( PT_PAN_Y, "Pan Y", FF_TYPE_STANDARD );

	SetOptionParamInfo( PT_SYNC, "Sync", static_cast< int >( Sync::Count ), params[ PT_SYNC ] );
	for( unsigned int i = 0; i < static_cast< unsigned int >( Sync::Count ); ++i )
		SetParamElementInfo( PT_SYNC, i, SyncName( static_cast< Sync >( i ) ), static_cast< float >( i ) );

	SetParamInfof( PT_SPEED, "Speed", FF_TYPE_STANDARD );
	SetParamInfof( PT_FOCUS, "Focus", FF_TYPE_STANDARD );
	SetParamInfof( PT_LENS, "Lens", FF_TYPE_STANDARD );
	SetParamInfof( PT_VIGNETTE, "Vignette", FF_TYPE_STANDARD );
	SetParamInfof( PT_DRIFT, "Drift", FF_TYPE_STANDARD );
	SetParamInfof( PT_DRIFT_RATE, "Drift Rate", FF_TYPE_STANDARD );

	SetParamInfof( PT_GAIN, "Gain", FF_TYPE_STANDARD );
	SetParamInfof( PT_PEDESTAL, "Pedestal", FF_TYPE_STANDARD );
	SetParamInfof( PT_GAMMA, "Gamma", FF_TYPE_STANDARD );
	SetParamInfof( PT_HUE_ROTATE, "Hue Rotate", FF_TYPE_STANDARD );
	SetParamInfof( PT_SATURATION, "Saturation", FF_TYPE_STANDARD );
	SetParamInfof( PT_CLIP, "Clip", FF_TYPE_STANDARD );
	SetParamInfof( PT_NOISE, "Noise", FF_TYPE_STANDARD );
	SetParamInfof( PT_DECAY, "Decay", FF_TYPE_STANDARD );

	SetOptionParamInfo( PT_INJECT, "Inject", static_cast< int >( Inject::Count ), params[ PT_INJECT ] );
	for( unsigned int i = 0; i < static_cast< unsigned int >( Inject::Count ); ++i )
		SetParamElementInfo( PT_INJECT, i, InjectName( static_cast< Inject >( i ) ), static_cast< float >( i ) );

	SetParamInfof( PT_INJECT_LEVEL, "Inject Level", FF_TYPE_STANDARD );
	SetParamInfof( PT_INJECT_SIZE, "Inject Size", FF_TYPE_STANDARD );
	SetParamInfof( PT_INJECT_X, "Inject X", FF_TYPE_STANDARD );
	SetParamInfof( PT_INJECT_Y, "Inject Y", FF_TYPE_STANDARD );

	SetParamInfof( PT_ITERATIONS, "Iterations", FF_TYPE_STANDARD );
	SetParamInfof( PT_JULIA_X, "Julia X", FF_TYPE_STANDARD );
	SetParamInfof( PT_JULIA_Y, "Julia Y", FF_TYPE_STANDARD );
	SetParamInfof( PT_ESCAPE_ZOOM, "Escape Zoom", FF_TYPE_STANDARD );
	SetParamInfof( PT_ESCAPE_X, "Escape X", FF_TYPE_STANDARD );
	SetParamInfof( PT_ESCAPE_Y, "Escape Y", FF_TYPE_STANDARD );

	SetOptionParamInfo( PT_PRECISION, "Precision", static_cast< int >( Precision::Count ), params[ PT_PRECISION ] );
	for( unsigned int i = 0; i < static_cast< unsigned int >( Precision::Count ); ++i )
		SetParamElementInfo( PT_PRECISION, i, PrecisionName( static_cast< Precision >( i ) ), static_cast< float >( i ) );

	SetParamInfof( PT_SPHERE, "Sphere", FF_TYPE_STANDARD );
	SetParamInfof( PT_TILT, "Tilt", FF_TYPE_STANDARD );
	SetParamInfof( PT_SPIN, "Spin", FF_TYPE_STANDARD );
	SetParamInfof( PT_LIGHT, "Light", FF_TYPE_STANDARD );

	SetOptionParamInfo( PT_PALETTE, "Palette", static_cast< int >( Palette::Count ), params[ PT_PALETTE ] );
	for( unsigned int i = 0; i < static_cast< unsigned int >( Palette::Count ); ++i )
		SetParamElementInfo( PT_PALETTE, i, PaletteName( static_cast< Palette >( i ) ), static_cast< float >( i ) );

	SetParamInfof( PT_PALETTE_SHIFT, "Palette Shift", FF_TYPE_STANDARD );
	SetParamInfof( PT_OPACITY, "Opacity", FF_TYPE_STANDARD );

	SetOptionParamInfo( PT_MASK_MODE, "Mask Mode", static_cast< int >( MaskMode::Count ), params[ PT_MASK_MODE ] );
	for( unsigned int i = 0; i < static_cast< unsigned int >( MaskMode::Count ); ++i )
		SetParamElementInfo( PT_MASK_MODE, i, MaskModeName( static_cast< MaskMode >( i ) ), static_cast< float >( i ) );

	SetParamInfof( PT_MIX, "Mix", FF_TYPE_STANDARD );

	// Factory presets. Element 0 is Custom; picking anything else copies that
	// preset's values into the covered parameters and raises value events so the
	// host re-reads the sliders. Editing a covered slider flips back to Custom.
	SetOptionParamInfo( PT_PRESET, "Preset", 1 + presets::kCount, params[ PT_PRESET ] );
	SetParamElementInfo( PT_PRESET, 0, "Custom", 0.0f );
	for( int i = 0; i < presets::kCount; ++i )
		SetParamElementInfo( PT_PRESET, 1 + i, presets::kPresets[ i ].name, static_cast< float >( 1 + i ) );

	//-----------------------------------------------------------------------
	// Groups. SetParamGroup collapses *runs* of same-group parameters, which is
	// why the ids in Controls.h have to stay in this order.
	//-----------------------------------------------------------------------
	for( unsigned int id = PT_RIG; id <= PT_FIELD_RATE; ++id )
		SetParamGroup( id, "Rig" );
	for( unsigned int id = PT_ZOOM; id <= PT_DRIFT_RATE; ++id )
		SetParamGroup( id, "Camera" );
	for( unsigned int id = PT_GAIN; id <= PT_DECAY; ++id )
		SetParamGroup( id, "Loop" );
	for( unsigned int id = PT_INJECT; id <= PT_INJECT_Y; ++id )
		SetParamGroup( id, "Inject" );
	for( unsigned int id = PT_ITERATIONS; id <= PT_PRECISION; ++id )
		SetParamGroup( id, "Iterator" );
	for( unsigned int id = PT_SPHERE; id <= PT_LIGHT; ++id )
		SetParamGroup( id, "Rescan" );
	for( unsigned int id = PT_PALETTE; id <= PT_MIX; ++id )
		SetParamGroup( id, "Output" );
	SetParamGroup( PT_PRESET, "Preset" );

	// The About block. Declared inline rather than through a helper, because
	// SetParamInfo is protected on CFFGLPlugin and nothing outside the class can
	// call it.
	SetParamInfo( PT_ABOUT_TEXT, "About", FF_TYPE_TEXT, stoatworks::about::defaultText() );
	{
		FFUInt32 aboutId = PT_ABOUT_TEXT + 1;
		for( const auto& b : stoatworks::about::buttons() )
			SetParamInfo( aboutId++, b.label, FF_TYPE_EVENT, false );
	}
	for( unsigned int id = PT_ABOUT_TEXT; id < PT_COUNT; ++id )
		SetParamGroup( id, "About" );
}

//---------------------------------------------------------------------------
// GL lifetime
//---------------------------------------------------------------------------
bool EscapementPlugin::BuildShaders()
{
	const char* defines = overInput ? kEffectDefine : "";

	const std::string loopFragment    = WithDefines( kLoopFragmentShader, defines );
	const std::string displayFragment = WithDefines( kDisplayFragmentShader, defines );

	if( !iteratorShader.Compile( kFullscreenVertexShader, kIteratorFragmentShader ) )
	{
		diag::error( "iterator shader would not compile" );
		return false;
	}

	if( !loopShader.Compile( kFullscreenVertexShader, loopFragment.c_str() ) )
	{
		diag::error( "loop shader would not compile" );
		return false;
	}

	if( !displayShader.Compile( kFullscreenVertexShader, displayFragment.c_str() ) )
	{
		diag::error( "display shader would not compile" );
		return false;
	}

	// A uniform that does not resolve is a silent no-op -- glUniform on location
	// -1 is documented to do nothing -- and the symptom here is a rig whose taps
	// are all identity, which looks exactly like a rig with the gain too low.
	if( loopShader.FindUniform( "TapM" ) < 0 || loopShader.FindUniform( "TapT" ) < 0 )
	{
		diag::error( "loop shader linked but TapM/TapT did not resolve" );
		return false;
	}

	return true;
}

FFResult EscapementPlugin::InitGL( const FFGLViewportStruct* vp )
{
	diag::init();

	if( !BuildShaders() )
	{
		std::string gl = "GL vendor=";
		const GLubyte* vendor   = glGetString( GL_VENDOR );
		const GLubyte* renderer = glGetString( GL_RENDERER );
		const GLubyte* version  = glGetString( GL_VERSION );
		gl += vendor ? reinterpret_cast< const char* >( vendor ) : "?";
		gl += " renderer=";
		gl += renderer ? reinterpret_cast< const char* >( renderer ) : "?";
		gl += " version=";
		gl += version ? reinterpret_cast< const char* >( version ) : "?";
		diag::error( gl );
		return FF_FAIL;
	}

	glGenVertexArrays( 1, &emptyVAO );

	clock.Reset();
	fieldCounter = 0;
	return FF_SUCCESS;
}

FFResult EscapementPlugin::DeInitGL()
{
	iteratorShader.FreeGLResources();
	loopShader.FreeGLResources();
	displayShader.FreeGLResources();

	store.Destroy();

	if( bankFBO != 0 )
	{
		glDeleteFramebuffers( 1, &bankFBO );
		bankFBO = 0;
	}
	if( bankTexture != 0 )
	{
		glDeleteTextures( 1, &bankTexture );
		bankTexture = 0;
	}
	bankWidth  = 0;
	bankHeight = 0;

	if( emptyVAO != 0 )
	{
		glDeleteVertexArrays( 1, &emptyVAO );
		emptyVAO = 0;
	}

	return FF_SUCCESS;
}

//---------------------------------------------------------------------------
// Parameters
//---------------------------------------------------------------------------
void EscapementPlugin::seedHostValues()
{
	if( hostValuesSeeded )
		return;

	for( unsigned int i = 0; i < PT_COUNT; ++i )
		hostValues[ i ] = params[ i ];

	hostValuesSeeded = true;
}

float EscapementPlugin::presetValue( int presetIndex, unsigned int id ) const
{
	if( presetIndex < 1 || presetIndex > presets::kCount )
		return -1.0f;

	const presets::Preset& preset = presets::kPresets[ presetIndex - 1 ];

	for( int i = 0; i < presets::kParamCount; ++i )
		if( kPresetParamIDs[ i ] == id )
			return preset.values[ i ];

	return -1.0f;
}

bool EscapementPlugin::hostIsRestatingItself( unsigned int index, float value )
{
	seedHostValues();

	const bool same = hostValues[ index ] == value;
	hostValues[ index ] = value;
	return same;
}

void EscapementPlugin::applyPreset( int presetIndex )
{
	if( presetIndex < 1 || presetIndex > presets::kCount )
		return;

	const presets::Preset& preset = presets::kPresets[ presetIndex - 1 ];

	for( int i = 0; i < presets::kParamCount; ++i )
	{
		const float v = preset.values[ i ];
		if( v < 0.0f )
			continue;

		const unsigned int id = kPresetParamIDs[ i ];
		params[ id ]          = v;
		hostValues[ id ]      = v;

		// Tell the host to re-read the slider. Resolume does not always act on
		// it, which is exactly why `hostValues` exists -- see the note on it in
		// Escapement.h.
		RaiseParamEvent( id, FF_EVENT_FLAG_VALUE );
	}
}

FFResult EscapementPlugin::SetFloatParameter( unsigned int index, float value )
{
	if( index >= PT_COUNT )
		return FF_FAIL;

	seedHostValues();

	// The About buttons open a browser and store nothing, so they are handled
	// before any of the bookkeeping below: pressing one is not the operator
	// editing a control.
	if( index >= PT_ABOUT_TEXT )
		return stoatworks::about::handleParam( index - PT_ABOUT_TEXT, value ) ? FF_SUCCESS : FF_FAIL;

	if( index == PT_PRESET )
	{
		const int chosen = Option( value, 1 + presets::kCount );
		params[ PT_PRESET ] = static_cast< float >( chosen );
		applyPreset( chosen );
		return FF_SUCCESS;
	}

	const bool restating = hostIsRestatingItself( index, value );

	params[ index ] = value;

	// An operator moving a slider a preset has an opinion about means they have
	// taken over, so drop to Custom. The host restating a value it already held
	// is not that, and treating it as such is what made presets un-selectable
	// in the fleet before the two were told apart.
	if( !restating && params[ PT_PRESET ] != 0.0f )
	{
		const int active = Option( params[ PT_PRESET ], 1 + presets::kCount );
		const float held = presetValue( active, index );

		if( held >= 0.0f && held != value )
		{
			params[ PT_PRESET ] = 0.0f;
			RaiseParamEvent( PT_PRESET, FF_EVENT_FLAG_VALUE );
		}
	}

	return FF_SUCCESS;
}

float EscapementPlugin::GetFloatParameter( unsigned int index )
{
	if( index >= PT_COUNT )
		return 0.0f;

	return params[ index ];
}

char* EscapementPlugin::GetTextParameter( unsigned int index )
{
	if( index == PT_ABOUT_TEXT )
	{
		// Function-local rather than a member: the line is built from
		// compile-time facts, so it is the same for every instance, and the
		// host only needs the pointer to outlive the call.
		static const std::string text = stoatworks::about::textParam( 0 );
		return const_cast< char* >( text.c_str() );
	}

	return CFFGLPlugin::GetTextParameter( index );
}

FFResult EscapementPlugin::SetTextParameter( unsigned int index, const char* value )
{
	// See the declaration: the base class fails, and a failed default deletes
	// the instance. The About line is display-only, so there is genuinely
	// nothing to store -- but it has to say so successfully.
	if( index == PT_ABOUT_TEXT )
		return FF_SUCCESS;

	return CFFGLPlugin::SetTextParameter( index, value );
}

const unsigned int* EscapementPlugin::PresetParamIDsForTest( int& count )
{
	count = presets::kParamCount;
	return kPresetParamIDs;
}

//---------------------------------------------------------------------------
// Clock
//---------------------------------------------------------------------------
FFResult EscapementPlugin::SetTime( double time )
{
	hostTime     = time;
	hostTimeSeen = true;
	return FF_SUCCESS;
}

void EscapementPlugin::SetClockScaleForTest( double scale )
{
	clockScale = scale;
}

void EscapementPlugin::PinFieldsForTest( int fields )
{
	pinnedFields = fields;
}

void EscapementPlugin::UpdateClock()
{
	// FFGL never says what unit SetTime arrives in, and hosts disagree: Resolume
	// sends MILLISECONDS while the offline harness sends seconds. Reading it raw
	// is a thousand times fast on the one host that matters and exactly right on
	// the one that gets tested, which is how it stays hidden.
	//
	// So measure rather than guess. steady_clock says how much real time passed,
	// the host says how much host time passed, and the ratio names the unit
	// outright. Nothing plausible sits between 1 and 1000, so both bands are
	// wide and a frame fitting neither simply does not vote.
	const double wallNow = wallSeconds();
	if( wallStart < 0.0 )
		wallStart = wallNow;

	// Never read `hostTime` before the host has set it: CFFGLPlugin's
	// constructor leaves it uninitialised.
	const double raw = hostTimeSeen ? hostTime : -1.0;

	if( clockScale == 0.0 && raw >= 0.0 && lastRawTime >= 0.0 && lastWallTime >= 0.0 )
	{
		const double hostDelta = raw - lastRawTime;
		const double wallDelta = wallNow - lastWallTime;

		if( hostDelta > 0.0 && wallDelta >= 0.0005 )
		{
			const double ratio = hostDelta / wallDelta;
			if( ratio > 0.1 && ratio < 10.0 )
				++secondsVotes;
			else if( ratio > 100.0 && ratio < 10000.0 )
				++millisVotes;

			if( secondsVotes >= kClockVotes || millisVotes >= kClockVotes )
			{
				clockScale = millisVotes > secondsVotes ? 0.001 : 1.0;
				diag::info( std::string( "host clock is " )
				            + ( clockScale == 0.001 ? "milliseconds" : "seconds" ) );
			}
		}
	}

	if( raw >= 0.0 )
		lastRawTime = raw;
	lastWallTime = wallNow;

	hostSeconds = ( raw >= 0.0 && clockScale != 0.0 ) ? raw * clockScale : wallNow - wallStart;
}

//---------------------------------------------------------------------------
// Parameters to engineering units
//---------------------------------------------------------------------------
LoopParams EscapementPlugin::CurrentLoop() const
{
	LoopParams p;

	p.rig        = static_cast< Rig >( Option( params[ PT_RIG ], static_cast< int >( Rig::Count ) ) );
	p.tapCount   = TapsFromParam( params[ PT_TAPS ] );
	p.seed       = SeedFromParam( params[ PT_SEED ] );
	p.symmetry   = SymmetryFromParam( params[ PT_SYMMETRY ] );
	p.foldMirror = params[ PT_FOLD_MIRROR ] >= 0.5f;
	p.fieldRate  = FieldRateFromParam( params[ PT_FIELD_RATE ] );

	// Speed multiplies the camera's moves and nothing else. It is the operator
	// walking the camera faster, not the rig running at a different field rate:
	// changing the field rate would change how many times the taps have been
	// applied, which is a different instrument rather than the same one moving
	// quicker.
	const Sync sync   = static_cast< Sync >( Option( params[ PT_SYNC ], static_cast< int >( Sync::Count ) ) );
	float speed       = SpeedFromParam( params[ PT_SPEED ] );

	if( sync == Sync::Manual )
		speed = 0.0f;
	else if( sync == Sync::Beat )
		speed *= std::max( 0.1f, static_cast< float >( bpm ) / 120.0f );
	else if( sync == Sync::Bar )
		speed *= std::max( 0.1f, static_cast< float >( bpm ) / 480.0f );

	// Zoom is a MULTIPLIER, so Speed scales its distance from 1 rather than the
	// number itself. Doubling the speed of a zoom of 1.02 is 1.04, not 2.04.
	p.zoom   = 1.0f + ( ZoomFromParam( params[ PT_ZOOM ] ) - 1.0f ) * speed;
	p.rotate = RotateFromParam( params[ PT_ROTATE ] ) * speed;
	p.panX   = PanFromParam( params[ PT_PAN_X ] ) * speed;
	p.panY   = PanFromParam( params[ PT_PAN_Y ] ) * speed;

	p.drift     = DriftFromParam( params[ PT_DRIFT ] );
	p.driftRate = DriftRateFromParam( params[ PT_DRIFT_RATE ] );

	p.focus    = FocusFromParam( params[ PT_FOCUS ] );
	p.lens     = LensFromParam( params[ PT_LENS ] );
	p.vignette = Clamp01( params[ PT_VIGNETTE ] );

	p.gain       = GainFromParam( params[ PT_GAIN ] );
	p.pedestal   = PedestalFromParam( params[ PT_PEDESTAL ] );
	p.gamma      = GammaFromParam( params[ PT_GAMMA ] );
	p.hueRotate  = HueRotateFromParam( params[ PT_HUE_ROTATE ] );
	p.saturation = SaturationFromParam( params[ PT_SATURATION ] );
	p.clip       = ClipFromParam( params[ PT_CLIP ] );
	p.noise      = NoiseFromParam( params[ PT_NOISE ] );
	p.decay      = DecayFromParam( params[ PT_DECAY ] );

	p.inject      = static_cast< Inject >( Option( params[ PT_INJECT ], static_cast< int >( Inject::Count ) ) );
	p.injectLevel = InjectLevelFromParam( params[ PT_INJECT_LEVEL ] );
	p.injectSize  = InjectSizeFromParam( params[ PT_INJECT_SIZE ] );
	p.injectX     = InjectPosFromParam( params[ PT_INJECT_X ] );
	p.injectY     = InjectPosFromParam( params[ PT_INJECT_Y ] );

	// The source has no clip, so Inject::Clip would be an empty texture. Fall
	// back rather than showing black: an operator who moved a composition from
	// the effect to the source has not asked for the picture to disappear.
	if( !overInput && p.inject == Inject::Clip )
		p.inject = Inject::Dot;

	p.iterations = IterationsFromParam( params[ PT_ITERATIONS ] );
	p.juliaX     = JuliaFromParam( params[ PT_JULIA_X ] );
	p.juliaY     = JuliaFromParam( params[ PT_JULIA_Y ] );
	p.escapeZoom = EscapeZoomFromParam( params[ PT_ESCAPE_ZOOM ] );
	p.escapeX    = EscapeCentreFromParam( params[ PT_ESCAPE_X ] );
	p.escapeY    = EscapeCentreFromParam( params[ PT_ESCAPE_Y ] );
	p.precision  = static_cast< Precision >( Option( params[ PT_PRECISION ], static_cast< int >( Precision::Count ) ) );

	p.sphere = SphereFromParam( params[ PT_SPHERE ] );
	p.tilt   = TiltFromParam( params[ PT_TILT ] );
	p.spin   = SpinFromParam( params[ PT_SPIN ] );
	p.light  = Clamp01( params[ PT_LIGHT ] );

	// The Globe rig IS the rescan, so picking it turns the sphere on rather than
	// leaving the operator to find a second control that the rig's name already
	// promised.
	if( p.rig == Rig::Globe && p.sphere <= 0.0f )
		p.sphere = 1.0f;

	p.palette      = static_cast< Palette >( Option( params[ PT_PALETTE ], static_cast< int >( Palette::Count ) ) );
	p.paletteShift = PaletteShiftFromParam( params[ PT_PALETTE_SHIFT ] );
	p.opacity      = Clamp01( params[ PT_OPACITY ] );
	p.maskMode     = static_cast< MaskMode >( Option( params[ PT_MASK_MODE ], static_cast< int >( MaskMode::Count ) ) );
	p.mix          = Clamp01( params[ PT_MIX ] );

	return p;
}

//---------------------------------------------------------------------------
// Rendering
//---------------------------------------------------------------------------
void EscapementPlugin::DrawQuad()
{
	glBindVertexArray( emptyVAO );
	glDrawArrays( GL_TRIANGLE_STRIP, 0, 4 );
	glBindVertexArray( 0 );
}

void EscapementPlugin::RunIterator( const LoopParams& p, int width, int height )
{
	if( bankTexture == 0 || width != bankWidth || height != bankHeight )
	{
		if( bankTexture != 0 )
			glDeleteTextures( 1, &bankTexture );
		if( bankFBO != 0 )
			glDeleteFramebuffers( 1, &bankFBO );

		glGenTextures( 1, &bankTexture );
		glBindTexture( GL_TEXTURE_2D, bankTexture );
		glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_HALF_FLOAT, nullptr );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
		glBindTexture( GL_TEXTURE_2D, 0 );

		glGenFramebuffers( 1, &bankFBO );
		bankWidth  = width;
		bankHeight = height;
	}

	glBindFramebuffer( GL_FRAMEBUFFER, bankFBO );
	glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, bankTexture, 0 );
	glViewport( 0, 0, width, height );

	glUseProgram( iteratorShader.GetGLID() );

	const float aspect = ( height > 0 ) ? float( width ) / float( height ) : 1.0f;

	iteratorShader.Set( "Resolution", float( width ), float( height ) );
	iteratorShader.Set( "Aspect", aspect );
	iteratorShader.Set( "Iterations", p.iterations );
	iteratorShader.Set( "Mode", p.rig == Rig::Mandelbrot ? 1 : 0 );
	iteratorShader.Set( "JuliaC", p.juliaX, p.juliaY );

	// The view, split into a high and a low float so that one pair of uniforms
	// serves both precisions. In Single the low halves are simply ignored.
	//
	// The centre is offset by the operator's controls in units of the CURRENT
	// view width, so that dragging Escape X moves the picture by the same amount
	// on screen whatever the magnification -- which at 1e12 is the difference
	// between a usable control and one that teleports.
	// The Mandelbrot base is a known deep-zoom point rather than the middle of
	// the set. At 1:1 the difference is invisible -- the whole set is on screen
	// either way -- and at 1e6 it is the difference between structure and a flat
	// field, because almost every point of the plane is boring at that
	// magnification and the interior of the set is the most boring of all. The
	// first draft centred on (-0.6, 0), which is inside the cardioid: the deep
	// zoom preset rendered a uniform orange rectangle.
	//
	// This is the Misiurewicz point the deep-zoom literature uses; the operator
	// steers away from it with Escape X/Y.
	const double kDeepX = -0.743643887037151;
	const double kDeepY = 0.131825904205330;

	const double scale   = 1.4 / p.escapeZoom;
	const double centreX = ( p.rig == Rig::Mandelbrot ? kDeepX : 0.0 ) + p.escapeX * scale;
	const double centreY = ( p.rig == Rig::Mandelbrot ? kDeepY : 0.0 ) + p.escapeY * scale;

	const float cxHi = float( centreX );
	const float cyHi = float( centreY );
	const float sHi  = float( scale );

	iteratorShader.Set( "CentreX", cxHi, float( centreX - double( cxHi ) ) );
	iteratorShader.Set( "CentreY", cyHi, float( centreY - double( cyHi ) ) );
	iteratorShader.Set( "Scale", sHi, float( scale - double( sHi ) ) );
	iteratorShader.Set( "Extended", p.precision == Precision::Extended ? 1 : 0 );

	glDisable( GL_BLEND );
	DrawQuad();
}

void EscapementPlugin::RunField( const LoopParams& p, const LoopState& state, int fieldIndex, GLuint clipTexture, float maxU, float maxV )
{
	store.BindForWrite();

	glUseProgram( loopShader.GetGLID() );

	const float aspect = ( store.Height() > 0 ) ? float( store.Width() ) / float( store.Height() ) : 1.0f;

	glActiveTexture( GL_TEXTURE0 );
	glBindTexture( GL_TEXTURE_2D, store.Front() );
	loopShader.Set( "Prev", 0 );

	// Only when there is one. Binding texture 0 to a sampler is legal and makes
	// the Apple driver log "GLD_TEXTURE_INDEX_2D is unloadable" once per frame,
	// which buries anything else in the log that mattered.
	if( bankTexture != 0 )
	{
		glActiveTexture( GL_TEXTURE1 );
		glBindTexture( GL_TEXTURE_2D, bankTexture );
		loopShader.Set( "Bank", 1 );
	}

	if( overInput )
	{
		glActiveTexture( GL_TEXTURE2 );
		glBindTexture( GL_TEXTURE_2D, clipTexture );
		loopShader.Set( "ClipTexture", 2 );
		loopShader.Set( "MaxUV", maxU, maxV );
	}

	glActiveTexture( GL_TEXTURE0 );

	loopShader.Set( "Resolution", float( store.Width() ), float( store.Height() ) );
	loopShader.Set( "Aspect", aspect );

	// The tap arrays. Uploaded as two vec4 arrays rather than a struct array:
	// GLSL's std140 rules for arrays of structs are a trap nobody needs, and two
	// flat arrays cannot be laid out wrongly.
	float tapM[ kShaderMaxTaps * 4 ] = {};
	float tapT[ kShaderMaxTaps * 4 ] = {};

	const int count = std::min( state.inverseTaps.count, kShaderMaxTaps );
	for( int i = 0; i < count; ++i )
	{
		const Tap& t = state.inverseTaps.taps[ i ];
		tapM[ i * 4 + 0 ] = t.m[ 0 ];
		tapM[ i * 4 + 1 ] = t.m[ 1 ];
		tapM[ i * 4 + 2 ] = t.m[ 2 ];
		tapM[ i * 4 + 3 ] = t.m[ 3 ];
		tapT[ i * 4 + 0 ] = t.tx;
		tapT[ i * 4 + 1 ] = t.ty;
		tapT[ i * 4 + 2 ] = t.weight;
		tapT[ i * 4 + 3 ] = t.hue;
	}

	const GLint tapMLoc = loopShader.FindUniform( "TapM" );
	const GLint tapTLoc = loopShader.FindUniform( "TapT" );
	if( tapMLoc >= 0 )
		glUniform4fv( tapMLoc, kShaderMaxTaps, tapM );
	if( tapTLoc >= 0 )
		glUniform4fv( tapTLoc, kShaderMaxTaps, tapT );

	loopShader.Set( "TapCount", count );
	loopShader.Set( "WeightSum", state.weightSum );

	loopShader.Set( "Symmetry", p.symmetry );
	loopShader.Set( "FoldMirror", p.foldMirror ? 1 : 0 );

	// Frame-relative to texels, here, because this is where the raster size is
	// known. See FocusFromParam.
	loopShader.Set( "Focus", FocusLod( p.focus, store.Width(), store.Height() ) );
	loopShader.Set( "Lens", p.lens );
	loopShader.Set( "Vignette", p.vignette );

	loopShader.Set( "Gain", p.gain );
	loopShader.Set( "Pedestal", p.pedestal );
	loopShader.Set( "Gamma", p.gamma );
	loopShader.Set( "HueRot", p.hueRotate );
	loopShader.Set( "Saturation", p.saturation );
	loopShader.Set( "ClipLevel", p.clip );
	loopShader.Set( "Noise", p.noise );
	loopShader.Set( "Decay", p.decay );

	loopShader.Set( "InjectMode", static_cast< int >( p.inject ) );
	loopShader.Set( "InjectLevel", p.injectLevel );
	loopShader.Set( "InjectSize", p.injectSize );
	loopShader.Set( "InjectPos", p.injectX, p.injectY );

	loopShader.Set( "FieldIndex", fieldIndex );

	glDisable( GL_BLEND );
	DrawQuad();

	store.Swap();
	store.GenerateMips();
}

void EscapementPlugin::Render( int width, int height, GLuint inputTexture, float maxU, float maxV, GLuint hostFBO )
{
	if( !loopShader.IsReady() || !displayShader.IsReady() )
		return;

	UpdateClock();

	LoopParams p = CurrentLoop();

	// A resize is the one thing that clears the store. Nothing that can be
	// reached from a knob does -- see Loop.h.
	const bool resized = ( width != store.Width() || height != store.Height() );
	if( !store.Ensure( width, height ) )
		return;
	if( resized )
		clock.Reset();

	//---------------------------------------------------------------------
	// How many trips round the loop this frame is worth.
	//---------------------------------------------------------------------
	double elapsed = 0.0;
	if( lastHostSeconds >= 0.0 )
		elapsed = hostSeconds - lastHostSeconds;
	lastHostSeconds = hostSeconds;

	int fields = pinnedFields >= 0 ? pinnedFields : clock.Advance( elapsed, p.fieldRate );

	// The very first frame after a resize or an instantiation has no elapsed
	// time behind it and would run no fields at all, leaving one black frame on
	// screen. One field is not a fudge: the rig has been switched on, and a rig
	// that has been on for zero seconds has still had one field through it.
	if( fields == 0 && resized )
		fields = 1;

	lastFields = fields;

	// The hands move on FIELD time, not on wall time.
	//
	// `fields / fieldRate` is how many seconds the RIG thinks have passed, which
	// is the same as the host's seconds whenever the rig is keeping up and is
	// deliberately not the same when it is not. Everything else in this plugin
	// is a function of how many times the loop has gone round, and drift has to
	// be as well, or a rendered frame stops being a function of the field count:
	// pinning the fields in the harness would leave the hands moving at whatever
	// speed the machine happened to render at, and 400 fields offline -- which
	// take about a fifth of a second of wall clock -- would drift by a fifth of
	// a second's worth instead of by 400 fields' worth. That is exactly what it
	// did, and it made four live rigs measure as dead.
	//
	// Applied AFTER CurrentLoop and before Resolve, so the taps are built from
	// the drifted camera rather than the parked one.
	const double riggedSeconds = ( p.fieldRate > 0.0f ) ? double( fields ) / double( p.fieldRate ) : 0.0;

	// The glass, once. Drift never changes which rig this is.
	const TapSet glass = Glass( p );

	// Seconds of rig time per FIELD, which is what the hands advance by between
	// one trip round the loop and the next.
	const double perField = ( p.fieldRate > 0.0f ) ? 1.0 / double( p.fieldRate ) : 0.0;

	// The spin is integrated, not computed from the clock -- `time * rate` would
	// rescale the whole history the instant the Spin knob moved -- and it runs on
	// field time for the same reason drift does, just above.
	spinPhase += double( p.spin ) * riggedSeconds;
	spinPhase = spinPhase - std::floor( spinPhase );

	const bool needsBank = !RigUsesTaps( p.rig ) || p.inject == Inject::Iterator;

	//---------------------------------------------------------------------
	// The loop, one field at a time, with the hands moving between them.
	//
	// The bank is inside this loop too, not outside it: its picture is a
	// function of the drifted Julia constant, so hoisting it would make the
	// escape rigs depend on the host's frame rate in the same way the taps
	// would. It is bounded by Clock::kMaxFieldsPerFrame, so the worst case is
	// eight bank passes in a stalled frame rather than an unbounded number.
	//---------------------------------------------------------------------
	for( int i = 0; i < fields; ++i )
	{
		LoopParams field = p;

		double phase = driftPhase + double( p.driftRate ) * perField * double( i + 1 );
		phase        = phase - std::floor( phase );

		lastState = Resolve( glass, field, phase );

		if( i == 0 && lastState.droppedTaps > 0 && p.rig != Rig::Fern )
			diag::warn( "rig dropped " + std::to_string( lastState.droppedTaps ) + " singular tap(s)" );

		if( needsBank )
			RunIterator( field, width, height );

		RunField( field, lastState, fieldCounter + i, inputTexture, maxU, maxV );
	}

	fieldCounter += fields;

	driftPhase += double( p.driftRate ) * perField * double( fields );
	driftPhase = driftPhase - std::floor( driftPhase );

	//---------------------------------------------------------------------
	// Display.
	//
	// Back to the host's framebuffer and the host's viewport. Both, by hand:
	// nothing in the SDK restores the viewport, and the store's passes set
	// their own.
	//---------------------------------------------------------------------
	glBindFramebuffer( GL_FRAMEBUFFER, hostFBO );
	glViewport( 0, 0, width, height );

	glUseProgram( displayShader.GetGLID() );

	const float aspect = ( height > 0 ) ? float( width ) / float( height ) : 1.0f;

	glActiveTexture( GL_TEXTURE0 );
	glBindTexture( GL_TEXTURE_2D, store.Front() );
	displayShader.Set( "Store", 0 );

	if( overInput )
	{
		glActiveTexture( GL_TEXTURE1 );
		glBindTexture( GL_TEXTURE_2D, inputTexture );
		displayShader.Set( "ClipTexture", 1 );
		displayShader.Set( "MaxUV", maxU, maxV );
		displayShader.Set( "MaskMode", static_cast< int >( p.maskMode ) );
		displayShader.Set( "Blend", p.mix );
	}

	glActiveTexture( GL_TEXTURE0 );

	displayShader.Set( "Resolution", float( width ), float( height ) );
	displayShader.Set( "Aspect", aspect );
	displayShader.Set( "PaletteMode", static_cast< int >( p.palette ) );
	displayShader.Set( "PaletteShift", p.paletteShift );
	displayShader.Set( "Opacity", p.opacity );

	// Bloom rides on the vignette control rather than having one of its own.
	// Halation is a property of the screen and the lens, and an operator who has
	// already told the rig how much its lens flares does not want to be asked a
	// second time in different words.
	displayShader.Set( "Bloom", p.vignette * 0.5f );

	displayShader.Set( "Sphere", p.sphere );
	displayShader.Set( "Tilt", p.tilt );
	displayShader.Set( "SpinPhase", float( spinPhase ) );
	displayShader.Set( "Light", p.light );

	glEnable( GL_BLEND );
	glBlendEquation( GL_FUNC_ADD );
	glBlendFunc( GL_ONE, GL_ONE_MINUS_SRC_ALPHA );

	DrawQuad();

	glUseProgram( 0 );
	glBindTexture( GL_TEXTURE_2D, 0 );
}

FFResult EscapementPlugin::ProcessOpenGL( ProcessOpenGLStruct* pGL )
{
	int width           = 0;
	int height          = 0;
	GLuint inputTexture = 0;
	float maxU          = 1.0f;
	float maxV          = 1.0f;

	if( overInput )
	{
		if( pGL == nullptr || pGL->numInputTextures < 1 || pGL->inputTextures[ 0 ] == nullptr )
			return FF_FAIL;

		const FFGLTextureStruct& texture = *pGL->inputTextures[ 0 ];
		inputTexture                     = texture.Handle;
		width                            = texture.Width;
		height                           = texture.Height;

		const FFGLTexCoords coords = GetMaxGLTexCoords( texture );
		maxU                       = coords.s;
		maxV                       = coords.t;
	}
	else
	{
		width  = static_cast< int >( currentViewport.width );
		height = static_cast< int >( currentViewport.height );
	}

	if( width <= 0 || height <= 0 )
		return FF_FAIL;

	Render( width, height, inputTexture, maxU, maxV, pGL != nullptr ? pGL->HostFBO : 0 );
	return FF_SUCCESS;
}

} // namespace escapement
