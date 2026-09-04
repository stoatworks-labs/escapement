#include "Escapement.h"

/**
    The generator: a rig with nothing in front of the lens but what it makes
    itself.

    **This file is listed directly in the EscapementSource target, not in
    escapement_core.** Both plugins share the class; what they do not share is
    the `CFFGLPluginInfo` below, and putting either registration in the shared
    library would register both plugins into both bundles.

    It is also why the shared library is an OBJECT library rather than a STATIC
    one. `CFFGLPluginInfo` registers itself from a file-scope constructor and
    nothing ever references it by name, so in an archive the linker is entitled
    to drop the whole translation unit -- giving a bundle that loads, exports
    `plugMain`, and reports that it contains no plugins.

        nm -gU Escapement.bundle/Contents/MacOS/Escapement | grep plugMain
*/
/// The name the host shows. See EffectPlugin.cpp: FFGL's PluginInfoStruct has
/// `char PluginName[ 16 ]` and does not null-terminate it.
constexpr char kPluginName[] = "Escapement";
static_assert( sizeof( kPluginName ) - 1 <= 16,
               "FFGL truncates the plugin name at 16 characters" );

namespace
{
class EscapementSource : public escapement::EscapementPlugin
{
public:
	EscapementSource() :
		EscapementPlugin( false )
	{
	}
};
} // namespace

static CFFGLPluginInfo PluginInfo(
	PluginFactory< EscapementSource >,                          // Create method
	"ES01",                                                     // Plugin unique ID of maximum length 4
	kPluginName,                                                // Plugin name
	2,                                                          // API major version number
	1,                                                          // API minor version number
	0,                                                          // Plugin major version number
	1,                                                          // Plugin minor version number
	FF_SOURCE,                                                  // Plugin type
	"A video feedback rig, and the fractals it settles into.\n\nNot a fractal generator with a feedback effect bolted on. A model of an optical rig: a camera looking at a screen showing what the camera saw a field ago, through glass that splits the light into several paths, and an operator playing the gain against unity.\n\nNothing here draws a Sierpinski gasket. There are three half-scale taps on the vertices of a triangle, and the gasket is where the loop goes.\n\nA rig left alone goes still, correctly, and zooming does not rescue it. Drift is the operator's hands moving, and the only thing keeping the picture alive.\n\nStart from a Preset, at the bottom.",// Plugin description
	"Escapement FFGL source"                                    // About
);

extern "C" const char* EscapementSourceBuildStamp()
{
	return "escapement " ESCAPEMENT_VERSION " source, built " __DATE__ " " __TIME__;
}
