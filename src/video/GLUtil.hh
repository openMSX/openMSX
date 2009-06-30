// $Id$

#ifndef GLUTIL_HH
#define GLUTIL_HH

// Check for availability of OpenGL.
#include "components.hh"
#ifdef COMPONENT_GL

// Include GLEW headers.
#include <GL/glew.h>
// Include OpenGL headers.
#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

#include "build-info.hh"
#include <string>
#include <cassert>

namespace openmsx {

namespace GLUtil {

// The type "GLuint" is typically "unsigned" on Windows and Linux and
// "unsigned long" on Mac OS X. We should expand some templates for both
// "GLuint" and "unsigned", but expanding a template twice is an error.
// So if GLuint is "unsigned", one of the template expansions should be
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
// ExpandFilter::ExpandType = (Type == unsigned ? NoExpansion : Type)
template <class Type> class ExpandFilter {
public:
	typedef Type ExpandType;
};
template <> class ExpandFilter<unsigned> {
public:
	typedef NoExpansion ExpandType;
};
// ExpandGL = (Type == unsigned ? NoExpansion : Type)
typedef ExpandFilter<GLuint>::ExpandType ExpandGL;


// TODO this needs glu, but atm we don't link against glu (in windows)
//void checkGLError(const std::string& prefix);

} // namespace GLUtil


/** Most basic/generic texture: only contains a texture ID.
  */
class Texture
{
public:
	explicit Texture(int type = GL_TEXTURE_2D);
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
	const int type;
	GLuint textureId;

	friend class FrameBufferObject;
};

class ColorTexture : public Texture
{
public:
	/** Create color texture with given size.
	  * Initial content is undefined.
	  */
	ColorTexture(GLsizei width, GLsizei height);

	GLsizei getWidth () const { return width;  }
	GLsizei getHeight() const { return height; }

private:
	GLsizei width;
	GLsizei height;
};

class LuminanceTexture : public Texture
{
public:
	/** Create grayscale texture with given size.
	  * Initial content is undefined.
	  */
	LuminanceTexture(GLsizei width, GLsizei height);

	/** Redefines (part of) the image for this texture.
	  */
	void updateImage(GLint x, GLint y,
	                 GLsizei width, GLsizei height,
	                 GLbyte* data);
};

/** Texture used for storing bitmap data from MSX VRAM.
  */
class BitmapTexture : public Texture
{
public:
	BitmapTexture();
	void update(int y, const GLuint* dataBGR, int lineWidth);
	void draw(int srcL, int srcT, int srcR, int srcB,
	          int dstL, int dstT, int dstR, int dstB);
private:
	static const int WIDTH = 512;
	static const int HEIGHT = 1024;
};

// TODO use GL_TEXTURE_1D for this?
class LineTexture : public Texture
{
public:
	LineTexture();
	void update(const GLuint* dataBGR, int lineWidth);
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

class FrameBufferObject
{
public:
	explicit FrameBufferObject(Texture& texture, bool doPush = false);
	~FrameBufferObject();

	void push();
	void pop();

private:
	GLuint bufferId;
};

/** Wrapper around a pixel buffer.
  * The pixel buffer will be allocated in VRAM if possible, in main RAM
  * otherwise.
  * The pixel type is templatized T.
  */
struct PixelBuffers
{
	static bool enabled;
};

template <typename T> class PixelBuffer
{
public:
	PixelBuffer();
	~PixelBuffer();

	/** Are PBOs supported by this openGL implementation?
	  * This class implements a SW fallback in case PBOs are not directly
	  * supported by this openGL implementation, but it will probably
	  * be a lot slower.
	  */
	bool openGLSupported() const;

	/** Sets the image for this buffer.
	  * TODO: Actually, only image size for now;
	  *       later, if we need it, image data too.
	  */
	void setImage(GLuint width, GLuint height);

	/** Bind this PixelBuffer. Must be called before the methods
	  * getOffset() or mapWrite() are used. (Is a wrapper around
	  * glBindBuffer).
	  */
	void bind() const;

	/** Unbind this buffer.
	  */
	void unbind() const;

	/** Gets a pointer relative to the start of this buffer.
	  * You must not dereference this pointer, but you can pass it to
	  * glTexImage etc when this buffer is bound as the source.
	  * @pre This PixelBuffer must be bound (see bind()) before calling
	  *      this method.
	  */
	T* getOffset(GLuint x, GLuint y) const;

	/** Maps the contents of this buffer into memory. The returned buffer
	  * is write-only (reading could be very slow or even result in a
	  * segfault).
	  * @return Pointer through which you can write pixels to this buffer,
	  *         or 0 if the buffer could not be mapped.
	  * @pre This PixelBuffer must be bound (see bind()) before calling
	  *      this method.
	  */
	T* mapWrite() const;

	/** Unmaps the contents of this buffer.
	  * After this call, you must no longer use the pointer returned by
	  * mapWrite.
	  */
	void unmap() const;

private:
	/** Pointer to main RAM fallback, or 0 if no main RAM buffer was
	  * allocated.
	  */
	T* allocated;

	/** Handle of the GL buffer, or 0 if no GL buffer is available.
	  */
	GLuint bufferId;

	/** Number of pixels per line.
	  */
	GLuint width;

	/** Number of lines.
	  */
	GLuint height;
};

// class PixelBuffer

template <typename T>
PixelBuffer<T>::PixelBuffer()
{
	allocated = NULL;
#ifdef GL_VERSION_1_5
	if (PixelBuffers::enabled &&
	    GLEW_ARB_pixel_buffer_object) {
		glGenBuffers(1, &bufferId);
	} else
#endif
	{
		//std::cerr << "OpenGL pixel buffers are not available" << std::endl;
		bufferId = 0;
	}
}

template <typename T>
PixelBuffer<T>::~PixelBuffer()
{
	free(allocated);
#ifdef GL_VERSION_1_5
	if (bufferId != 0) {
		glDeleteBuffers(1, &bufferId);
	}
#endif
}

template <typename T>
bool PixelBuffer<T>::openGLSupported() const
{
	return bufferId != 0;
}

template <typename T>
void PixelBuffer<T>::setImage(GLuint width, GLuint height)
{
	this->width = width;
	this->height = height;
#ifdef GL_VERSION_1_5
	if (bufferId != 0) {
		bind();
		// TODO make performance hint configurable?
		glBufferData(GL_PIXEL_UNPACK_BUFFER_ARB,
		             width * height * 4,
		             NULL, // leave data undefined
		             GL_STREAM_DRAW); // performance hint
		unbind();
	} else
#endif
	{
		allocated = reinterpret_cast<T*>(
			realloc(allocated, width * height * sizeof(T)));
	}
}

template <typename T>
void PixelBuffer<T>::bind() const
{
#ifdef GL_VERSION_1_5
	if (bufferId != 0) {
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, bufferId);
	}
#endif
}

template <typename T>
void PixelBuffer<T>::unbind() const
{
#ifdef GL_VERSION_1_5
	if (bufferId != 0) {
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, 0);
	}
#endif
}

template <typename T>
T* PixelBuffer<T>::getOffset(GLuint x, GLuint y) const
{
	assert(x < width);
	assert(y < height);
	unsigned offset = x + width * y;
#ifdef GL_VERSION_1_5
	if (bufferId != 0) {
		return static_cast<T*>(0) + offset;
	}
#endif
	return allocated + offset;
}

template <typename T>
T* PixelBuffer<T>::mapWrite() const
{
#ifdef GL_VERSION_1_5
	if (bufferId != 0) {
		return reinterpret_cast<T*>(glMapBuffer(
			GL_PIXEL_UNPACK_BUFFER_ARB, GL_WRITE_ONLY));
	}
#endif
	return allocated;
}

template <typename T>
void PixelBuffer<T>::unmap() const
{
#ifdef GL_VERSION_1_5
	if (bufferId != 0) {
		glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER_ARB);
	}
#endif
}



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
class VertexShader : public Shader
{
public:
	/** Instantiates a vertex shader.
	  * @param filename The GLSL source code for the shader.
	  */
	explicit VertexShader(const std::string& filename);
};

/** Wrapper around an OpenGL fragment shader:
  * a program executed on the GPU that computes the colours of pixels.
  */
class FragmentShader : public Shader
{
public:
	/** Instantiates a fragment shader.
	  * @param filename The GLSL source code for the shader.
	  */
	explicit FragmentShader(const std::string& filename);
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
