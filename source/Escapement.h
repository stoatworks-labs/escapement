#pragma once

#include <FFGLSDK.h>

// After FFGLSDK.h, which is where FFUInt32 comes from.
#include "StoatworksAboutParams.h"

#include "Controls.h"
#include "Loop.h"
#include "Presets.h"
#include "Store.h"
#include "Taps.h"

/**
    Escapement -- a video feedback rig, and the fractals it settles into.

    What is worth knowing about how it works is in three other files and not
    repeated here:

    - **Taps.h** -- a tap is one path from the screen back into the camera, a tap
      set is an iterated function system, and the fractal is its attractor.
      Nothing evaluates a fractal.
    - **Loop.h** -- the rig keeps its own clock and goes round at its own field
      rate, not the host's frame rate. A parameter change must never clear the
      frame store.
    - **Shaders.h** -- three passes and why this is the one plugin in the fleet
      that has to have a framebuffer.

    This class is the part that talks to the host: it declares the parameters,
    turns them into a `LoopParams`, decides how many fields have gone by, and
    draws.

    Both plugins are this class. The source's loop can inject its own patterns;
    the effect's can inject the incoming clip, and composites the result against
    it. They differ by a constructor flag, a `#define` handed to the shader
    compiler, and their input count.

    See AGENTS.md for the traps.
*/
namespace escapement
{
class EscapementPlugin : public CFFGLPlugin
{
public:
	explicit EscapementPlugin( bool overInput );

	// CFFGLPlugin
	FFResult InitGL( const FFGLViewportStruct* vp ) override;
	FFResult ProcessOpenGL( ProcessOpenGLStruct* pGL ) override;
	FFResult DeInitGL() override;

	FFResult SetFloatParameter( unsigned int index, float value ) override;
	float GetFloatParameter( unsigned int index ) override;
	char* GetTextParameter( unsigned int index ) override;

	/// Declared only so the About line can accept its own default.
	/// `instantiateGL` pushes every declared default back through the setters on
	/// a fresh instance and deletes the instance if one fails, and
	/// CFFGLPlugin's SetTextParameter is a stub that returns exactly that
	/// failure -- so without this override no real host can load the plugin,
	/// while every offline harness here carries on passing.
	FFResult SetTextParameter( unsigned int index, const char* value ) override;

	FFResult SetTime( double time ) override;

	/// Render one frame at `width` x `height`, leaving the result in `hostFBO`.
	///
	/// `hostFBO` is passed rather than read back with `glGetIntegerv` because
	/// FFGL already hands it over and the read-back is a pipeline stall. It is
	/// also the one thing the loop passes have to be able to put back: the store
	/// binds its own framebuffer several times per frame, and a plugin that
	/// leaves somebody else's binding changed corrupts the NEXT plugin in the
	/// chain, which is a bug that gets reported against the wrong effect.
	///
	/// Exposed for the offline harness, which drives this class rather than a
	/// copy of it.
	void Render( int width, int height, GLuint inputTexture, float maxU, float maxV, GLuint hostFBO );

	/// The loop parameters as they would be resolved right now.
	LoopParams CurrentLoop() const;

	/// The parameter ids a preset covers, in presets::Param order. Handed out
	/// rather than copied into the harness, so a second list cannot go quietly
	/// out of step with this one.
	static const unsigned int* PresetParamIDsForTest( int& count );

	//---------------------------------------------------------------------
	// Test hooks.
	//
	// The harness DECLARES its clock unit rather than leaving UpdateClock to
	// infer one: a single absolute time handed over in one frame is genuinely
	// ambiguous, and inference is only possible against a live host's frame
	// deltas.
	//---------------------------------------------------------------------
	void SetClockScaleForTest( double scale );

	/// Run exactly `fields` trips round the loop on the next Render, whatever
	/// the clock says.
	///
	/// The harness needs this because the thing being tested about the loop is
	/// that N fields produce a particular picture, and a test that let the wall
	/// clock decide N would be testing the wall clock.
	void PinFieldsForTest( int fields );

	/// Fields run by the last Render.
	int LastFieldsForTest() const { return lastFields; }

	/// Loop state as last resolved -- gain, dropped taps, the inverse maps.
	const LoopState& LastStateForTest() const { return lastState; }

private:
	static constexpr unsigned int kPresetParamIDs[ presets::kParamCount ] = {
		PT_RIG, PT_TAPS, PT_SEED, PT_SYMMETRY, PT_FOLD_MIRROR,
		PT_ZOOM, PT_ROTATE, PT_PAN_X, PT_PAN_Y,
		PT_FOCUS, PT_LENS, PT_VIGNETTE,
		PT_GAIN, PT_PEDESTAL, PT_GAMMA, PT_HUE_ROTATE, PT_SATURATION, PT_CLIP, PT_NOISE, PT_DECAY,
		PT_INJECT, PT_INJECT_LEVEL, PT_INJECT_SIZE,
		PT_PALETTE, PT_SPHERE, PT_TILT, PT_SPIN,
		PT_ITERATIONS, PT_ESCAPE_ZOOM, PT_PRECISION
	};

	bool BuildShaders();

	/// Draw the fullscreen quad. Every pass is one of these.
	void DrawQuad();

	/// One trip round the loop, into the back of the store.
	void RunField( const LoopParams& params, const LoopState& state, int fieldIndex, GLuint clipTexture, float maxU, float maxV );

	/// The iterator bank into its own buffer. Only when something reads it.
	void RunIterator( const LoopParams& params, int width, int height );

	float presetValue( int presetIndex, unsigned int id ) const;
	bool hostIsRestatingItself( unsigned int index, float value );
	void seedHostValues();
	void applyPreset( int presetIndex );

	void UpdateClock();

	const bool overInput;

	ffglex::FFGLShader iteratorShader;
	ffglex::FFGLShader loopShader;
	ffglex::FFGLShader displayShader;

	/// A core profile refuses to draw with no vertex array bound, even when the
	/// vertex shader sources nothing and builds its quad from `gl_VertexID`.
	GLuint emptyVAO = 0;

	Store store;

	/// The iterator bank's output. A plain FBO pair is overkill -- nothing reads
	/// it while it is being written -- so this is one texture and one
	/// framebuffer.
	GLuint bankFBO     = 0;
	GLuint bankTexture = 0;
	int bankWidth      = 0;
	int bankHeight     = 0;

	Clock clock;
	int lastFields    = 0;
	int pinnedFields  = -1;
	LoopState lastState;

	/// Counts every field ever run by this instance. The loop shader's grain is
	/// hashed against it, so it has to keep counting rather than restarting each
	/// frame -- otherwise every field within one rendered frame gets identical
	/// noise, which correlates with itself round the loop and builds up as a
	/// fixed pattern rather than looking like grain.
	int fieldCounter = 0;

	float params[ PT_COUNT ] = {};

	/// What the HOST last sent for each parameter, which is not the same thing
	/// as what the plugin is rendering with. Keeping the host's own last word
	/// separately is what tells an operator's edit apart from the host
	/// restating a value it still believes in -- without which a preset drops
	/// straight back to Custom on the host's own echo.
	float hostValues[ PT_COUNT ] = {};
	bool hostValuesSeeded        = false;

	double clockScale   = 0.0;
	double lastRawTime  = -1.0;
	double lastWallTime = -1.0;
	double wallStart    = -1.0;
	double hostSeconds  = 0.0;
	double lastHostSeconds = -1.0;
	int secondsVotes    = 0;
	int millisVotes     = 0;
	bool hostTimeSeen   = false;

	/// Accumulated rescan spin, in turns. Integrated rather than computed as
	/// `time * rate` for the reason vectrix integrates its oscillator phase: the
	/// product jumps the whole history the instant the rate changes, so nudging
	/// Spin an hour into a set would snap the globe to a different angle.
	double spinPhase = 0.0;
};

} // namespace escapement
