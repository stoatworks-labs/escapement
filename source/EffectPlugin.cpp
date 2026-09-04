#include "Escapement.h"

/**
    The effect: the same rig, with the clip in front of the lens.

    The clip is not a backdrop the picture is drawn over -- it can be **injected
    into the loop**, which is the thing this build exists for. A clip fed through
    a tap set comes back as its own attractor: the fractal is made out of the
    footage rather than laid on top of it, and because the loop keeps its frame
    store the picture goes on developing after the clip has moved on.

    Read the note in SourcePlugin.cpp on why this file is listed directly in its
    own target rather than in `escapement_core`.

    The plugin ID differs from the source's, and it has to: Resolume keys a saved
    composition's effect to that ID, so two plugins sharing one would make a
    composition ambiguous about which of them it meant.
*/

/**
    FFGL's `PluginInfoStruct` carries `char PluginName[ 16 ]`, **not null
    terminated**, so a longer name is silently cut off by the host and nothing
    plugin-side ever notices. `esctest --names` checks PARAMETER names for the
    same limit and cannot see this one: the plugin's name is not a parameter, it
    is an argument to the registration below.

    Caught by reading Arena's own log, which is the only place it appears:

        registered extension: 'Escapement Feedb' uid: ES02 category: 1

    Nineteen characters shipped as sixteen. This static_assert is the guard, and
    it is here rather than in a test because it is knowable at compile time.
*/
namespace
{
/// The name the host shows. Sixteen characters, and "Escapement Feedback" was
/// nineteen.
constexpr char kPluginName[] = "Escapement Feed";
static_assert( sizeof( kPluginName ) - 1 <= 16,
               "FFGL's PluginInfoStruct has char PluginName[16] and does not null-terminate it -- "
               "a longer name is truncated by the host and looks like a typo in the effects list" );

class EscapementEffect : public escapement::EscapementPlugin
{
public:
	EscapementEffect() :
		EscapementPlugin( true )
	{
	}
};
} // namespace

static CFFGLPluginInfo PluginInfo(
	PluginFactory< EscapementEffect >,                    // Create method
	"ES02",                                               // Plugin unique ID of maximum length 4
	kPluginName,                                          // Plugin name
	2,                                                    // API major version number
	1,                                                    // API minor version number
	0,                                                    // Plugin major version number
	1,                                                    // Plugin minor version number
	FF_EFFECT,                                            // Plugin type
	"Feeds the clip into a video feedback rig.\n\nNot a fractal generator with feedback bolted on. A model of an optical rig: a camera looking at a screen showing what the camera saw a field ago, through glass that splits the light into several paths, with a proc amp in the cable and an operator playing the gain against unity.\n\nNothing here draws a Sierpinski gasket. There are three half-scale taps on the vertices of a triangle, and the gasket is where the loop goes.\n\nA rig left alone goes still, correctly, and zooming does not rescue it. Drift is the operator's hands moving, and the only thing keeping the picture alive.\n\nStart from a Preset, at the bottom.",// Plugin description
	"Escapement FFGL effect"                              // About
);

extern "C" const char* EscapementEffectBuildStamp()
{
	return "escapement " ESCAPEMENT_VERSION " effect, built " __DATE__ " " __TIME__;
}
