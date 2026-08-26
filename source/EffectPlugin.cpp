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
namespace
{
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
	"Escapement Feedback",                                // Plugin name
	2,                                                    // API major version number
	1,                                                    // API minor version number
	0,                                                    // Plugin major version number
	1,                                                    // Plugin minor version number
	FF_EFFECT,                                            // Plugin type
	"Feeds the clip into a video feedback rig",            // Plugin description
	"Escapement FFGL effect"                              // About
);

extern "C" const char* EscapementEffectBuildStamp()
{
	return "escapement " ESCAPEMENT_VERSION " effect, built " __DATE__ " " __TIME__;
}
