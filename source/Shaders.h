#pragma once

#include "Taps.h"

/**
    Three passes and a frame store.

    1. **Iterator bank** (`kIteratorFragmentShader`) -- only for the Julia and
       Mandelbrot rigs, and only when something is asking for its picture. An
       escape-time evaluation, optionally in extended precision.
    2. **Loop** (`kLoopFragmentShader`) -- one trip round the rig, run once per
       field and NOT once per rendered frame. Reads the frame store, pulls it
       through the taps, adds what is being injected, runs the proc amp, and
       writes the frame store back.
    3. **Display** (`kDisplayFragmentShader`) -- the frame store to the output.
       Palette, bloom, the rescan for the Globe rig, and for the effect build the
       composite against the clip.

    ## Why this one has a framebuffer when the rest of the fleet avoids them

    Orrery's rule -- no FBO anywhere, because the SDK's is buggy -- cannot apply
    here. A feedback rig IS a frame store; there is no version of this plugin
    that does not keep the previous field. So the store is a ping-pong pair, and
    the two SDK bugs are handled rather than dodged: `Store` reimplements
    `Release()` because `ffglex::FFGLFBO::Release` tests `depthBufferID` where it
    meant `colorTextureID` and leaks the colour texture, and every pass sets its
    own viewport because `ScopedFBOBinding` restores the binding and not the
    viewport.

    ## The store is mipmapped, and that is a signal path decision

    `Focus` is a `textureLod` bias on the tap fetches. That is not a cheap
    approximation of a blur that ought to be a separate Gaussian pass -- it is
    the correct place for it. A soft lens is a low-pass filter *inside* the loop,
    and a low-pass filter inside a feedback loop is what stops a hot rig
    collapsing into single-pixel noise. Sharp and hot is static. Soft and hot is
    the thing everybody recognises as video feedback. Putting the blur after the
    loop instead would blur the picture and change nothing about what the loop
    does, which is the opposite of what the control is for.

    ## Premultiplied alpha

    Resolume hands over premultiplied clips and expects one back. The frame store
    is opaque -- a rig's screen has no alpha -- so `Opacity` and `Mix` are applied
    once, in the display pass, to colour and alpha together.
*/
namespace escapement
{
/// Shared by all three passes: an attributeless fullscreen triangle strip.
extern const char* const kFullscreenVertexShader;

/// The escape-time bank. Julia and Mandelbrot.
extern const char* const kIteratorFragmentShader;

/// One trip round the loop. Compiled once; the effect build defines
/// `ESCAPEMENT_EFFECT` so that Inject::Clip has a clip to read.
extern const char* const kLoopFragmentShader;

/// The frame store to the output.
extern const char* const kDisplayFragmentShader;

/// The tap array length written into the loop shader as a literal. Escapement.cpp
/// static_asserts that `kMaxTaps` still equals this, so raising the C++ constant
/// without raising the shader's is a build error rather than an array overrun.
inline constexpr int kShaderMaxTaps = 8;

static_assert( kShaderMaxTaps == kMaxTaps, "Shaders.cpp writes the tap array length as a literal" );

} // namespace escapement
