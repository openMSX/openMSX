// $Id$

#ifndef GLUTIL_HH
#define GLUTIL_HH

// Check for availability of OpenGL.
#include "components.hh"
#ifdef COMPONENT_GL

// Include OpenGL headers.
#include "probed_defs.hh"
#ifdef HAVE_GL_GL_H
#include <GL/gl.h>
#include <GL/glext.h>
#else // HAVE_GL_H
#include <gl.h>
#include <glext.h>
#endif

namespace openmsx {

/** Most basic/generic texture: only contains a texture ID.
  */
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

/** Texture used for storing bitmap data from MSX VRAM.
  */
class BitmapTexture: public Texture
{
public:
	BitmapTexture();
	void update(int y, const GLuint* data, int lineWidth);
	void draw(
		int srcL, int srcT, int srcR, int srcB,
		int dstL, int dstT, int dstR, int dstB
		);
private:
	static const int WIDTH = 512;
	static const int HEIGHT = 1024;
};

class LineTexture: public Texture
{
public:
	LineTexture();
	void update(const GLuint* data, int lineWidth);
	void draw(int texX, int screenX, int screenY, int width, int height);
private:
	int prevLineWidth;
};

class StoredFrame
{
public:
	StoredFrame();
	bool isStored() { return stored; }
	void store(unsigned x, unsigned y);
	void draw(int offsetX, int offsetY, int width, int height);
	void drawBlend(
		int offsetX, int offsetY, int width, int height, double alpha
		);

private:
	/** Texture reserved for storing frame image data.
	  */
	Texture texture;

	/** Width of the stored image.
	 */
	unsigned width;

	/** Height of the stored image.
	 */
	unsigned height;

	/** Was the previous frame image stored?
	  */
	bool stored;
};

} // namespace openmsx

#endif // COMPONENT_GL
#endif // GLUTIL_HH
