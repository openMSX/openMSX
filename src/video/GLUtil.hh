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

#include "build-info.hh"
#include <string>

namespace openmsx {

namespace GLUtil {

// The type "GLuint" is typically "unsigned int" on Windows and Linux and
// "unsigned long" on Mac OS X. We should expand some templates for both
// "GLuint" and "unsigned int", but expanding a template twice is an error.
// So if GLuint is "unsigned int", one of the template expansions should be
// skipped.
//
// We solve this by expanding the class using the type argument "ExpandGL",
// which is defined like this:
// - "GLuint" if "GLuint" is not "unsigned"
// - "NoExpansion" if "GLuint" is "unsigned"
// The class being expanded should define an empty implementation for type
// argument "NoExpansion", like this:
//   template<> class SomeClass<GLUtil::NoExpansion> {};
// and the expansion itself should be done like this:
//   template class SomeClass<GLUtil::ExpandGL>;
//
class NoExpansion {};
// ExpandFilter::ExpandType = (Type == unsigned int ? NoExpansion : Type)
template <class Type> class ExpandFilter {
public:
	typedef Type ExpandType;
};
template <> class ExpandFilter<unsigned> {
public:
	typedef NoExpansion ExpandType;
};
// ExpandGL = (Type == unsigned int ? NoExpansion : Type)
typedef ExpandFilter<GLuint>::ExpandType ExpandGL;


// TODO this needs glu, but atm we don't link against glu (in windows)
//void checkGLError(const std::string& prefix);

/** Set primary drawing colour.
  */
inline void setPriColour(GLuint colour)
{
	if (OPENMSX_BIGENDIAN) {
		glColor3ub((colour >> 24) & 0xFF,
		           (colour >> 16) & 0xFF,
		           (colour >>  8) & 0xFF);
	} else {
		glColor3ub((colour >>  0) & 0xFF,
		           (colour >>  8) & 0xFF,
		           (colour >> 16) & 0xFF);
	}
}

/** Set texture drawing colour.
  */
inline void setTexColour(GLuint colour)
{
	GLuint r, g, b;
	if (OPENMSX_BIGENDIAN) {
		r = (colour >> 24) & 0xFF;
		g = (colour >> 16) & 0xFF;
		b = (colour >>  8) & 0xFF;
	} else {
		r = (colour >>  0) & 0xFF;
		g = (colour >>  8) & 0xFF;
		b = (colour >> 16) & 0xFF;
	}
	GLfloat colourVec[4] = { r / 255.0f, g / 255.0f, b / 255.0f, 1.0f };
	glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, colourVec);
}

/** Draw a filled rectangle.
  */
inline void drawRect(GLint x, GLint y, GLint width, GLint height, GLuint colour)
{
	setPriColour(colour);
	glRecti(x, y, x + width, y + height);
}

} // namespace GLUtil

class PixelBuffer;


/** Most basic/generic texture: only contains a texture ID.
  */
class Texture
{
public:
	Texture(int type = GL_TEXTURE_2D);
	virtual ~Texture();

	/** Makes this texture the active GL texture.
	  * The other methods of this class and its subclasses will implicitly
	  * bind the texture, so you only need this method to explicitly bind
	  * this texture for use in GL function calls outside of this class.
	  */
	void bind() {
		glBindTexture(type, textureId);
	}

	/** Enables bilinear interpolation for this texture.
	  */
	void enableInterpolation();

	/** Disables bilinear interpolation for this texture and uses nearest
	  * neighbour instead.
	  */
	void disableInterpolation();

	void setWrapMode(bool wrap);

	/** Draws this texture as a rectangle on the frame buffer.
	  */
	void drawRect(GLfloat tx, GLfloat ty, GLfloat twidth, GLfloat theight,
	              GLint   x,  GLint   y,  GLint   width,  GLint   height);

protected:
	int type;
	GLuint textureId;
};

class ColourTexture: public Texture
{
public:
	ColourTexture();

	/** Sets the image for this texture.
	  */
	void setImage(GLsizei width, GLsizei height, GLuint* data = NULL);

	/** Redefines (part of) the image for this texture.
	  */
	void updateImage(GLint x, GLint y,
	                 GLsizei width, GLsizei height,
	                 GLuint* data);

	/** Redefines (part of) the image for this texture.
	  */
	void updateImage(GLint x, GLint y,
	                 GLsizei width, GLsizei height,
	                 const PixelBuffer& buffer,
	                 GLuint bx, GLuint by);

	GLsizei getWidth () const { return width;  }
	GLsizei getHeight() const { return height; }

private:
	GLsizei width;
	GLsizei height;
};

class LuminanceTexture: public Texture
{
public:
	LuminanceTexture();

	/** Sets the image for this texture.
	  */
	void setImage(GLsizei width, GLsizei height, GLbyte* data = NULL);

	/** Redefines (part of) the image for this texture.
	  */
	void updateImage(GLint x, GLint y,
	                 GLsizei width, GLsizei height,
	                 GLbyte* data);
};

/** Texture used for storing bitmap data from MSX VRAM.
  */
class BitmapTexture: public Texture
{
public:
	BitmapTexture();
	void update(int y, const GLuint* data, int lineWidth);
	void draw( int srcL, int srcT, int srcR, int srcB,
	           int dstL, int dstT, int dstR, int dstB);
private:
	static const int WIDTH = 512;
	static const int HEIGHT = 1024;
};

// TODO use GL_TEXTURE_1D for this?
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
	void drawBlend(int offsetX, int offsetY,
	               int width, int height, double alpha);

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

/** Wrapper around a pixel buffer.
  * The pixel buffer will be allocated in VRAM if possible, in main RAM
  * otherwise.
  * The pixel type is GLuint, so each entry in the buffer is 32 bits wide.
  */
class PixelBuffer
{
public:
	PixelBuffer();
	~PixelBuffer();

	/** Sets the image for this buffer.
	  * TODO: Actually, only image size for now;
	  *       later, if we need it, image data too.
	  */
	void setImage(GLuint width, GLuint height);

	/** Gets a pointer relative to the start of this buffer.
	  * You must not dereference this pointer, but you can pass it to
	  * glTexImage etc when this buffer is bound as the source.
	  */
	GLuint* getOffset(GLuint x, GLuint y) const;

	/** Bind this buffer as the source for pixel transfer operations such as
	  * glTexImage.
	  */
	void bindSrc() const;

	/** Unbind this buffer as the source for pixel transfer operations.
	  */
	void unbindSrc() const;

	/** Maps the contents of this buffer into memory for writing.
	  * @return Pointer through which you can write pixels to this buffer.
	  */
	GLuint* mapWrite() const;

	/** Unmaps the contents of this buffer.
	  * After this call, you must no longer use the pointer returned by
	  * mapWrite.
	  */
	void unmap() const;

private:
	friend class ColourTexture;

	/** Handle of the GL buffer, or 0 if no GL buffer is available.
	  */
	GLuint bufferId;

	/** Pointer to main RAM fallback, or 0 if no main RAM buffer was
	  * allocated.
	  */
	GLuint* allocated;

	/** Number of pixels per line.
	  */
	GLuint width;

	/** Number of lines.
	  */
	GLuint height;
};

/** Wrapper around an OpenGL shader: a program executed on the GPU.
  * This class is a base class for vertex and fragment shaders.
  */
class Shader
{
public:
	virtual ~Shader();

	/** Returns true iff this shader is loaded and compiled without errors.
	  */
	bool isOK() const;

protected:
	/** Instantiates a shader.
	  * @param type The shader type: GL_VERTEX_SHADER or GL_FRAGMENT_SHADER.
	  * @param filename The GLSL source code for the shader.
	  */
	Shader(GLenum type, const std::string& filename);

private:
	friend class ShaderProgram;

	GLuint handle;
};

/** Wrapper around an OpenGL vertex shader:
  * a program executed on the GPU that computes per-vertex stuff.
  */
class VertexShader: public Shader
{
public:
	/** Instantiates a vertex shader.
	  * @param filename The GLSL source code for the shader.
	  */
	VertexShader(const std::string& filename);
};

/** Wrapper around an OpenGL fragment shader:
  * a program executed on the GPU that computes the colours of pixels.
  */
class FragmentShader: public Shader
{
public:
	/** Instantiates a fragment shader.
	  * @param filename The GLSL source code for the shader.
	  */
	FragmentShader(const std::string& filename);
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
	void attach(const Shader& shader);

	/** Links all attached shaders together into one program.
	  * This should be done before activating the program.
	  */
	void link();

	/** Gets a reference to a uniform variable declared in the shader source.
	  * Note that you have to activate this program before you can change
	  * the uniform variable's value.
	  */
	GLint getUniformLocation(const char* name) const;

	/** Makes this program the active shader program.
	  * This requires that the program is already linked.
	  */
	void activate() const;

	/** Deactivates all shader programs.
	  */
	static void deactivate();

	void validate();

private:
	GLuint handle;
};

} // namespace openmsx

#endif // COMPONENT_GL
#endif // GLUTIL_HH
