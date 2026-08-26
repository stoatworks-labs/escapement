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
	"Escapement",                                               // Plugin name
	2,                                                          // API major version number
	1,                                                          // API minor version number
	0,                                                          // Plugin major version number
	1,                                                          // Plugin minor version number
	FF_SOURCE,                                                  // Plugin type
	"A video feedback rig and the fractals it settles into",     // Plugin description
	"Escapement FFGL source"                                    // About
);

extern "C" const char* EscapementSourceBuildStamp()
{
	return "escapement " ESCAPEMENT_VERSION " source, built " __DATE__ " " __TIME__;
}
