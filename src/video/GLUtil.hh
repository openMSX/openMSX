// $Id$

#ifndef __GLUTIL_HH__
#define __GLUTIL_HH__

// Check for availability of OpenGL.
#include "components.hh"

// Remainder of this header needs GL to work.
#ifdef COMPONENT_GL

// Include OpenGL headers.
#include "probed_defs.hh"
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

#endif // COMPONENT_GL

#endif // __GLUTIL_HH__
