#include "Store.h"
#include "Diag.h"

#include <string>

namespace escapement
{
Store::~Store()
{
	Destroy();
}

bool Store::Ensure( GLsizei requestedWidth, GLsizei requestedHeight )
{
	if( requestedWidth <= 0 || requestedHeight <= 0 )
		return false;

	if( IsValid() && requestedWidth == width && requestedHeight == height )
		return true;

	Destroy();

	width  = requestedWidth;
	height = requestedHeight;

	glGenTextures( 2, texture );

	for( int i = 0; i < 2; ++i )
	{
		glBindTexture( GL_TEXTURE_2D, texture[ i ] );
		glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_HALF_FLOAT, nullptr );

		// LINEAR_MIPMAP_LINEAR, not LINEAR. Focus is a lod bias on the tap
		// fetches, and without trilinear the lens goes soft in visible steps as
		// the knob crosses each whole level.
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );

		// CLAMP_TO_EDGE, but the shader tests the coordinate before it fetches
		// and skips the tap when it is outside. The clamp is only what happens
		// at the last texel; the shader's test is what makes a camera pointed
		// off its screen see the dark room instead of an infinitely smeared
		// edge pixel.
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	}

	glGenFramebuffers( 1, &fbo );

	// Check completeness once, here, rather than trusting it. A float colour
	// attachment is not guaranteed to be renderable by the specification, and
	// on a driver where it is not, every symptom appears in the loop pass --
	// which would send the search for the cause into the shader.
	glBindFramebuffer( GL_FRAMEBUFFER, fbo );
	glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture[ 0 ], 0 );

	const GLenum status = glCheckFramebufferStatus( GL_FRAMEBUFFER );
	glBindFramebuffer( GL_FRAMEBUFFER, 0 );
	glBindTexture( GL_TEXTURE_2D, 0 );

	if( status != GL_FRAMEBUFFER_COMPLETE )
	{
		diag::error( "Store: framebuffer incomplete (status 0x" + std::to_string( status ) +
		             ") at " + std::to_string( width ) + "x" + std::to_string( height ) + " RGBA16F" );
		Destroy();
		return false;
	}

	front = 0;
	Clear();
	return true;
}

void Store::Destroy()
{
	if( fbo != 0 )
	{
		glDeleteFramebuffers( 1, &fbo );
		fbo = 0;
	}

	if( texture[ 0 ] != 0 || texture[ 1 ] != 0 )
	{
		glDeleteTextures( 2, texture );
		texture[ 0 ] = 0;
		texture[ 1 ] = 0;
	}

	width  = 0;
	height = 0;
	front  = 0;
}

void Store::BindForWrite()
{
	if( !IsValid() )
		return;

	glBindFramebuffer( GL_FRAMEBUFFER, fbo );
	glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture[ 1 - front ], 0 );
	glViewport( 0, 0, width, height );
}

void Store::Swap()
{
	front = 1 - front;
}

void Store::GenerateMips()
{
	if( !IsValid() )
		return;

	glBindTexture( GL_TEXTURE_2D, texture[ front ] );
	glGenerateMipmap( GL_TEXTURE_2D );
	glBindTexture( GL_TEXTURE_2D, 0 );
}

void Store::Clear()
{
	if( !IsValid() )
		return;

	GLint previousFBO = 0;
	GLint viewport[ 4 ] = { 0, 0, 0, 0 };
	glGetIntegerv( GL_FRAMEBUFFER_BINDING, &previousFBO );
	glGetIntegerv( GL_VIEWPORT, viewport );

	glBindFramebuffer( GL_FRAMEBUFFER, fbo );
	glViewport( 0, 0, width, height );

	for( int i = 0; i < 2; ++i )
	{
		glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture[ i ], 0 );
		glClearColor( 0.0f, 0.0f, 0.0f, 1.0f );
		glClear( GL_COLOR_BUFFER_BIT );
	}

	glBindFramebuffer( GL_FRAMEBUFFER, GLuint( previousFBO ) );
	glViewport( viewport[ 0 ], viewport[ 1 ], viewport[ 2 ], viewport[ 3 ] );

	GenerateMips();
}

} // namespace escapement
