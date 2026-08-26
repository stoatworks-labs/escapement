#pragma once

#include <cstdint>

/**
    Host parameters, and what they mean.

    **Every numeric parameter Escapement declares is a plain 0..1 float**, even
    where it stands for a field rate in hertz, a rotation in degrees or a seed.
    `CFFGLPluginManager::SetParamInfo` clamps an `FF_TYPE_STANDARD` default into
    0..1 *before* returning, and `SetParamRange` can only be called afterwards
    because it finds the parameter by id -- so a parameter declared in degrees
    cannot declare a default in degrees. There is no `SetParamDefault`. A default
    of 90 becomes 1, silently, and the plugin starts up wrong in a way no build
    step notices. The range lives here, in the conversion, and the host only ever
    sees 0..1.

    **No parameter name may exceed 16 characters.** FFGL truncates at 16 and the
    SDK does not warn, so `Persistence` is fine and `Loop Persistence` ships as
    `Loop Persistenc`. `esctest --names` fails the build over it rather than an
    operator finding it.

    ## Why so many of these are centred

    Zoom, Rotate, Pan and Hue Rotate are all **per field** and all have to be
    able to be exactly zero. A feedback rig sitting at a zoom of 1.0000 is
    stationary and one at 1.0004 crawls inwards for ever; the interesting
    settings are a hair off nothing at all, and there is no way to find them on a
    slider whose zero is at one end. So those four map the centre of the slider
    to exactly zero -- `SignedFromParam` -- and put a wide, curved range either
    side of it. Nothing in a feedback rig is used at both ends of its travel; it
    is used at the middle, gently.
*/
namespace escapement
{
/// What is fed into the loop alongside what is already going round it.
///
/// A feedback rig with nothing injected is not black -- it is whatever noise the
/// sensor makes, amplified by the loop until it is the picture, which is a real
/// and rather beautiful mode of a real rig. Everything else here is something
/// put in front of the camera.
enum class Inject
{
	None = 0,  ///< Sensor grain only. The loop finds its own picture.
	Dot,       ///< A soft spot. The classic thing to wave in front of the lens.
	Bars,      ///< Colour bars, because a video rig has them and they hue-rotate well.
	Grid,      ///< A line grid. Shows the tap geometry more plainly than anything else.
	Noise,     ///< Static, reseeded every field.
	Iterator,  ///< The iterator bank's picture -- one fractal fed into another's loop.
	Clip,      ///< The incoming clip. Effect build only; the source has no clip.

	Count
};

const char* InjectName( Inject inject );

/// How the loop's picture is combined with the clip. Effect build only.
enum class MaskMode
{
	Over = 0,   ///< The loop's picture over the clip.
	Reveal,     ///< The clip, seen only where the loop's picture is bright.
	Hide,       ///< The clip, hidden where the loop's picture is bright.
	Colourise,  ///< The clip, tinted by the loop's picture.

	Count
};

const char* MaskModeName( MaskMode mode );

/// How the loop's signal is coloured on the way out.
enum class Palette
{
	Signal = 0, ///< The loop's own RGB, untouched. What the rig actually carries.
	Phosphor,   ///< Amber, on the curve a P3 phosphor actually has.
	Ice,        ///< Blue-white through cyan.
	Ember,      ///< Black through red and orange to white -- the escape-time classic.
	Spectrum,   ///< Full hue sweep. Cycles endlessly under Hue Rotate.

	Count
};

const char* PaletteName( Palette palette );

/// Where the camera's motion rate comes from.
enum class Sync
{
	Free = 0,  ///< Seconds. Speed multiplies the per-field camera moves.
	Beat,      ///< The host's beat.
	Bar,       ///< The host's bar.
	Manual,    ///< Speed is ignored and the camera does not move on its own.

	Count
};

const char* SyncName( Sync sync );

/// How precisely the iterator bank holds its position.
enum class Precision
{
	Single = 0, ///< One float per component. Runs out of resolution near 1e5.
	Extended,   ///< Two floats per component, ~1e13. Costs roughly 3x the pass.

	Count
};

const char* PrecisionName( Precision precision );

/**
    Parameter ids.

    The declaration order in Escapement.cpp is the order they appear in the host,
    and the groups depend on consecutive ids staying consecutive -- `SetParamGroup`
    collapses *runs* of same-group parameters, so reordering these silently splits
    a group into two.
*/
enum ParamId : unsigned int
{
	// Rig -- the glass, and how many of it.
	PT_RIG = 0,
	PT_TAPS,
	PT_SEED,
	PT_SYMMETRY,
	PT_FOLD_MIRROR,
	PT_FIELD_RATE,

	// Camera -- the operator's hands.
	PT_ZOOM,
	PT_ROTATE,
	PT_PAN_X,
	PT_PAN_Y,
	PT_SYNC,
	PT_SPEED,
	PT_FOCUS,
	PT_LENS,
	PT_VIGNETTE,

	// Loop -- the proc amp, which is where a rig is actually played.
	PT_GAIN,
	PT_PEDESTAL,
	PT_GAMMA,
	PT_HUE_ROTATE,
	PT_SATURATION,
	PT_CLIP,
	PT_NOISE,
	PT_DECAY,

	// Injection -- what is put in front of the lens.
	PT_INJECT,
	PT_INJECT_LEVEL,
	PT_INJECT_SIZE,
	PT_INJECT_X,
	PT_INJECT_Y,

	// Iterator bank -- the escape-time rigs only.
	PT_ITERATIONS,
	PT_JULIA_X,
	PT_JULIA_Y,
	PT_ESCAPE_ZOOM,
	PT_ESCAPE_X,
	PT_ESCAPE_Y,
	PT_PRECISION,

	// Rescan -- the Globe rig only.
	PT_SPHERE,
	PT_TILT,
	PT_SPIN,
	PT_LIGHT,

	// Output.
	PT_PALETTE,
	PT_PALETTE_SHIFT,
	PT_OPACITY,

	// Both builds declare both of these so a composition can be moved between
	// the source and the effect without the parameter list shifting underneath
	// it; the source has no clip to mask against and ignores them.
	PT_MASK_MODE,
	PT_MIX,

	// Preset. Declared after the real controls so their ids -- which a saved
	// composition refers to -- do not shift under existing users.
	PT_PRESET,

	// -- The Stoatworks About block ------------------------------------------
	//
	// One display-only text line, then one button per link the block carries.
	// Last in the enum so no saved composition's parameter ids shift.
	PT_ABOUT_TEXT,
	PT_ABOUT_BUTTON_1,
	PT_ABOUT_BUTTON_2,
	PT_ABOUT_BUTTON_3,
	PT_ABOUT_BUTTON_4,
	PT_COUNT
};

//---------------------------------------------------------------------------
// Conversions. Each is the only place its range is written down.
//---------------------------------------------------------------------------

/// A slider whose centre is exactly zero, curved either side.
///
/// `pow( |v|, shape )` with shape > 1 puts fine resolution near the middle,
/// which is the only part of a feedback control anyone uses. Exact zero at
/// exactly 0.5 is load-bearing rather than tidy: it is the difference between a
/// rig you can park and one that drifts imperceptibly for the whole set.
float SignedFromParam( float value, float range, float shape );

/// Number of taps, for the rigs that let you choose. 2..8.
int TapsFromParam( float value );

/// The seed. 1..9999 -- an integer, so nudging the slider gives a different rig
/// rather than an imperceptibly different one.
uint32_t SeedFromParam( float value );

/// Order of the rotational fold applied before the taps. 1..12, where 1 is no
/// fold. This is a mirror wedge in front of the lens, and it is how the Koch
/// snowflake is made out of a four-tap Koch curve -- see Taps.cpp.
int SymmetryFromParam( float value );

/// How fast the rig's own clock runs, in fields per second. 5..120.
///
/// **Not the host's frame rate.** A real rig runs at its display's field rate
/// whatever is recording it, and this plugin's picture must evolve at the same
/// speed whether Resolume is managing 60 fps or 24. `Loop::Advance` converts the
/// host's elapsed time into a whole number of fields at this rate.
float FieldRateFromParam( float value );

/// Per-field zoom, as a multiplier. 0.90..1.10, exactly 1.0 at the centre.
///
/// The range looks narrow and is not: 1.02 per field at 60 fields a second is a
/// zoom of 3.3x every second, which is already faster than anything watchable.
/// The useful settings live in the third decimal place, which is why this is the
/// most heavily curved control in the plugin.
float ZoomFromParam( float value );

/// Per-field rotation in radians. +-0.1 rad (about 5.7 degrees), zero centred.
float RotateFromParam( float value );

/// Per-field pan in frame units. +-0.05, zero centred.
float PanFromParam( float value );

/// Camera motion multiplier. 0..4, exponential, exactly 1 at the centre.
float SpeedFromParam( float value );

/// Defocus, as a mip level. 0..5.
///
/// A real rig's focus is the single most powerful control on it, because a lens
/// that is slightly soft is a low-pass filter INSIDE the loop -- and a low-pass
/// filter inside a feedback loop is what stops the picture breaking up into
/// single-pixel noise as the gain comes up. Sharp and hot is static; soft and
/// hot is the thing everyone recognises as video feedback.
float FocusFromParam( float value );

/// Lens distortion. -1..1, zero centred: negative is barrel, positive pincushion.
float LensFromParam( float value );

/// Loop gain, per field. 0..1.6, with exactly 1.0 at 0.625 of the travel.
///
/// **This is the control the whole plugin is about.** Below 1 every trip round
/// the loop is dimmer than the last and the picture decays to whatever is being
/// injected. Above 1 it grows until the clip stage stops it, and the shape it
/// grows into is the attractor. The transition is not gentle and it is not
/// meant to be.
float GainFromParam( float value );

/// Black level added each field. -0.1..0.1, zero centred.
float PedestalFromParam( float value );

/// Proc amp gamma. 0.4..2.5, exactly 1.0 at the centre, logarithmic.
float GammaFromParam( float value );

/// Hue rotation per field, in turns. +-0.02, zero centred.
///
/// Small, and it is the difference between a still picture and one that never
/// repeats. A loop with no hue rotation converges to a fixed point and stops; a
/// loop with a little cannot converge, because a fixed point would have to be a
/// colour that is its own rotation. The structure keeps re-entering itself and
/// the colour walks around the wheel for ever. It is the cheapest way to make a
/// rig that stays interesting for an hour.
float HueRotateFromParam( float value );

/// Saturation multiplier per field. 0..2, exactly 1 at the centre.
float SaturationFromParam( float value );

/// Soft-knee clip threshold. 0.3..1.0 -- where the amplifier stops being linear.
float ClipFromParam( float value );

/// Sensor grain, as a standard deviation in signal units. 0..0.08.
float NoiseFromParam( float value );

/// How much of the previous field survives independently of the taps. 0..0.98.
///
/// Phosphor persistence and camera integration time, together. Distinct from
/// Gain: gain is what goes round the geometric path and decay is what stays put,
/// so decay smears in time without moving, and gain moves without smearing.
float DecayFromParam( float value );

/// Injection strength. 0..2, exponential.
float InjectLevelFromParam( float value );

/// Injected pattern size, as a fraction of the short edge. 0.01..1.5, exponential.
float InjectSizeFromParam( float value );

/// Injected pattern centre, in frame units. -1.5..1.5.
float InjectPosFromParam( float value );

/// Iterator bank iterations. 16..2048, exponential.
int IterationsFromParam( float value );

/// Julia constant, either component. -2..2, zero centred.
float JuliaFromParam( float value );

/// Iterator bank zoom, as a magnification. 1..1e13, exponential.
///
/// The top of that range is reachable only in Extended precision, and the
/// plugin says so rather than quietly showing mush -- see `Precision`.
double EscapeZoomFromParam( float value );

/// Iterator bank centre offset, in units of the current view width.
double EscapeCentreFromParam( float value );

/// How far the rescan wraps the field onto a sphere. 0..1.
float SphereFromParam( float value );

/// Rescan tilt, in radians. +-pi/2, zero centred.
float TiltFromParam( float value );

/// Rescan spin, in turns per second. +-1, zero centred.
float SpinFromParam( float value );

/// Palette rotation, in turns. 0..1.
float PaletteShiftFromParam( float value );

} // namespace escapement
