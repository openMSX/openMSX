// $Id$

#ifndef __GLUTIL_HH__
#define __GLUTIL_HH__

// Check for availability of OpenGL.
#include "components.hh"
#ifdef COMPONENT_GL

// Include OpenGL headers.
#include "probed_defs.hh"
#ifdef HAVE_GL_GL_H
#include <GL/gl.h>
#else // HAVE_GL_H
#include <gl.h>
#endif

namespace openmsx {

class Texture
{
public:
	Texture();
	virtual ~Texture();
	
	// TODO: I'd prefer to make this protected / friend.
	void bind() {
		glBindTexture(GL_TEXTURE_2D, textureId);
	}
protected:
	GLuint textureId;
};

class LineTexture: public Texture
{
public:
	LineTexture();
	void update(const GLuint* data, int lineWidth);
	void draw(int texX, int screenX, int screenY, int width, int height);
};

class StoredFrame
{
public:
	StoredFrame();
	bool isStored() { return stored; }
	void store();
	void draw(int offsetX, int offsetY);
	void drawBlend(int offsetX, int offsetY, double alpha);

private:
	
	/** Texture reserved for storing frame image data.
	  */
	Texture texture;

	/** Was the previous frame image stored?
	  */
	bool stored;
};

} // namespace openmsx

#endif // COMPONENT_GL
#endif // __GLUTIL_HH__
