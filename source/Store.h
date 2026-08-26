#pragma once

#include <FFGLSDK.h>

namespace escapement
{
/**
    The frame store: the thing that makes this a rig rather than a shader.

    A ping-pong pair of float textures and one framebuffer. The loop pass reads
    `Front()` and writes `Back()`; `Swap()` exchanges them.

    ## Not built on ffglex::FFGLFBO

    The rest of the fleet's advice is to avoid the SDK's framebuffer wrapper, and
    where a plugin can avoid a framebuffer altogether -- as orrery does -- that is
    the better answer. Escapement cannot: a feedback rig IS a frame store. So this
    is raw GL instead of a subclass, which costs about forty lines and buys not
    having to work around `FFGLFBO::Release()` testing `depthBufferID` where it
    meant `colorTextureID` and leaking the colour texture on every release.

    There is no depth buffer here at all, which the SDK's would have allocated.

    ## Why float, and why 16 and not 8

    The signal in the loop is not a colour, it is a **signal**: the proc amp puts
    it above 1 and a negative pedestal puts it below 0, and both are settings an
    operator will use. Eight-bit fixed point would clip at both ends every single
    field, which is not the soft knee the amplifier models but a hard one applied
    an extra time, in the wrong place, with no control over it.

    Half float rather than full: the store is read several times per field per
    pixel through the mip chain, so it is bandwidth that decides the frame rate at
    4K, and the loop's own noise floor is far above half float's.

    ## Mipmaps are part of the signal path

    `GenerateMips()` runs after every field, because `Focus` is a `textureLod`
    bias on the tap fetches -- the soft lens inside the loop. It is not an
    optimisation and it cannot be skipped when Focus is zero: Bloom reads the
    high levels too.
*/
class Store
{
public:
	~Store();

	/// Allocate at this size, reusing what is there if it already matches.
	/// A newly allocated store is cleared -- a texture's initial contents are
	/// whatever memory the driver handed back, and in a buffer that feeds itself
	/// that would never wash out.
	bool Ensure( GLsizei width, GLsizei height );

	void Destroy();

	bool IsValid() const
	{
		return fbo != 0 && texture[ 0 ] != 0 && texture[ 1 ] != 0;
	}

	/// The field the loop is reading.
	GLuint Front() const
	{
		return texture[ front ];
	}

	/// Bind the framebuffer with the *other* texture attached, and set the
	/// viewport to the whole store.
	///
	/// Sets its own viewport on purpose: `ffglex::ScopedFBOBinding` restores the
	/// binding and NOT the viewport, so a pass that trusts the previous one is
	/// correct until something upstream changes size.
	void BindForWrite();

	/// Exchange front and back. Called after the loop pass has written.
	void Swap();

	/// Rebuild the mip chain of the front texture. Must run after every field:
	/// Focus and Bloom both read levels above zero.
	void GenerateMips();

	/// Clear both textures to black. A resize and a fresh instance only -- NOT a
	/// parameter change. See Loop.h.
	void Clear();

	GLsizei Width() const
	{
		return width;
	}

	GLsizei Height() const
	{
		return height;
	}

private:
	GLuint fbo        = 0;
	GLuint texture[ 2 ] = { 0, 0 };
	int front         = 0;
	GLsizei width     = 0;
	GLsizei height    = 0;
};

} // namespace escapement
