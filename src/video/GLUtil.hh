// $Id$

#ifndef GLUTIL_HH
#define GLUTIL_HH

// Check for availability of OpenGL.
#include "components.hh"
#ifdef COMPONENT_GL

#include "probed_defs.hh"
// Include GLEW headers.
#ifdef HAVE_GL_GLEW_H
#include <GL/glew.h>
#else // HAVE_GLEW_H
#include <glew.h>
#endif
// Include OpenGL headers.
#ifdef HAVE_GL_GL_H
#include <GL/gl.h>
#else // HAVE_GL_H
#include <gl.h>
#endif

#include <string>

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

	/** Width of the texture.
	  */
	unsigned textureWidth;

	/** Height of the texture.
	  */
	unsigned textureHeight;

	/** Was the previous frame image stored?
	  */
	bool stored;
};

/** Wrapper around an OpenGL fragment shader:
  * a program executed on the GPU that computes the colours of pixels.
  */
class FragmentShader
{
public:
	FragmentShader(const std::string& filename);
	~FragmentShader();

	/** Returns true iff this shader is loaded and compiled without errors.
	  */
	bool isOK() const;

private:
	friend class ShaderProgram;

	GLuint handle;
};

/** Wrapper around an OpenGL program:
  * a collection of vertex and fragment shaders.
  */
class ShaderProgram
{
public:
	ShaderProgram();
	~ShaderProgram();

	/** Returns true iff this program was linked without errors.
	  * Note that this will certainly return false until link() is called.
	  */
	bool isOK() const;

	/** Adds a given shader to this program.
	  */
	void attach(const FragmentShader& shader);

	/** Link all attached shaders together into one program.
	  * This should be done before activating the program.
	  */
	void link();

	/** Gets a reference to a uniform variable declared in the shader source.
	  */
	GLint getUniformLocation(const char* name) const;

	/** Make this program the active shader program.
	  * This requires that the program is already linked.
	  */
	void activate() const;

	/** Deactivate all shader programs.
	  */
	void deactivate() const;

private:
	GLuint handle;
};

} // namespace openmsx

#endif // COMPONENT_GL
#endif // GLUTIL_HH
