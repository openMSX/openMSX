// $Id$

#ifndef __GLUTIL_HH__
#define __GLUTIL_HH__

// Check for availability of OpenGL headers.
#include "config.h"
#if (defined(HAVE_GL_GL_H) || defined(HAVE_GL_H))
#define __OPENGL_AVAILABLE__

// Remainder of this header needs GL to work.
#ifdef __OPENGL_AVAILABLE__

// Include OpenGL headers.
#ifdef HAVE_GL_GL_H
#include <GL/gl.h>
#else // HAVE_GL_H
#include <gl.h>
#endif

namespace openmsx {

class LineTexture
{
public:
	LineTexture();
	~LineTexture();
	void update(const GLuint *data, int lineWidth);
	void draw(int texX, int screenX, int screenY, int width, int height);
private:
	GLuint textureId;
};

} // namespace openmsx

#endif // __OPENGL_AVAILABLE__

#endif // OpenGL header check.
#endif // __GLUTIL_HH__
