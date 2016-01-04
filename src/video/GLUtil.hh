#ifndef GLUTIL_HH
#define GLUTIL_HH

// Check for availability of OpenGL.
#include "components.hh"
#if COMPONENT_GL

// Include GLEW headers.
#include <GL/glew.h>
// Include OpenGL headers.
#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

#include "MemBuffer.hh"
#include "build-info.hh"
#include <string>
#include <cassert>

namespace gl {

// TODO this needs glu, but atm we don't link against glu (in windows)
//void checkGLError(const std::string& prefix);


// Dummy object, to be able to construct empty handler objects.
struct Null {};


/** Most basic/generic texture: only contains a texture ID.
  * Current implementation always assumes 2D textures.
  */
class Texture
{
public:
	Texture(const Texture&) = delete;
	Texture& operator=(const Texture&) = delete;

	/** Allocate a openGL texture name and enable/disable interpolation. */
	explicit Texture(bool interpolation = false, bool wrap = false);

	/** Create null-handle (not yet allocate an openGL handle). */
	explicit Texture(Null) : textureId(0) {}

	/** Release openGL texture name. */
	~Texture() { reset(); }

	/** Move constructor and assignment. */
	Texture(Texture&& other) noexcept
		: textureId(other.textureId)
	{
		other.textureId = 0; // 0 is not a valid openGL texture name
	}
	Texture& operator=(Texture&& other) noexcept {
		std::swap(textureId, other.textureId);
		return *this;
	}

	/** Allocate an openGL texture name. */
	void allocate();

	/** Release openGL texture name. */
	void reset();

	/** Returns the underlying openGL handler id.
	  * 0 iff no openGL texture is allocated.
	  */
	GLuint get() const { return textureId; }

	/** Makes this texture the active GL texture.
	  * The other methods of this class and its subclasses will implicitly
	  * bind the texture, so you only need this method to explicitly bind
	  * this texture for use in GL function calls outside of this class.
	  */
	void bind() {
		glBindTexture(GL_TEXTURE_2D, textureId);
	}

	/** Enable/disable bilinear interpolation for this texture. IOW selects
	 * between GL_NEAREST or GL_LINEAR filtering.
	  */
	void setInterpolation(bool interpolation);

	void setWrapMode(bool wrap);

protected:
	GLuint textureId;

	friend class FrameBufferObject;
};

class ColorTexture : public Texture
{
public:
	/** Default constructor, zero-sized texture. */
	ColorTexture() : width(0), height(0) {}

	/** Create color texture with given size.
	  * Initial content is undefined.
	  */
	ColorTexture(GLsizei width, GLsizei height);
	void resize(GLsizei width, GLsizei height);

	GLsizei getWidth () const { return width;  }
	GLsizei getHeight() const { return height; }

private:
	GLsizei width;
	GLsizei height;
};

class FrameBufferObject
{
public:
	FrameBufferObject();
	explicit FrameBufferObject(Texture& texture);
	FrameBufferObject(FrameBufferObject&& other) noexcept
		: bufferId(other.bufferId)
	{
		other.bufferId = 0;
	}
	FrameBufferObject& operator=(FrameBufferObject&& other) noexcept {
		std::swap(bufferId, other.bufferId);
		return *this;
	}
	~FrameBufferObject();

	void push();
	void pop();

private:
	GLuint bufferId;
};

struct PixelBuffers
{
	/** Global switch to disable pixel buffers using the "-nopbo" option.
	  */
	static bool enabled;
};

/** Wrapper around a pixel buffer.
  * The pixel buffer will be allocated in VRAM if possible, in main RAM
  * otherwise.
  * The pixel type is templatized T.
  */
template <typename T> class PixelBuffer
{
public:
	PixelBuffer();
	PixelBuffer(PixelBuffer&& other) noexcept;
	PixelBuffer& operator=(PixelBuffer&& other) noexcept;
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
	T* getOffset(GLuint x, GLuint y);

	/** Maps the contents of this buffer into memory. The returned buffer
	  * is write-only (reading could be very slow or even result in a
	  * segfault).
	  * @return Pointer through which you can write pixels to this buffer,
	  *         or 0 if the buffer could not be mapped.
	  * @pre This PixelBuffer must be bound (see bind()) before calling
	  *      this method.
	  */
	T* mapWrite();

	/** Unmaps the contents of this buffer.
	  * After this call, you must no longer use the pointer returned by
	  * mapWrite.
	  */
	void unmap() const;

private:
	/** Buffer for main RAM fallback (not allocated in the normal case).
	  */
	openmsx::MemBuffer<T> allocated;

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
	if (PixelBuffers::enabled && GLEW_ARB_pixel_buffer_object) {
		glGenBuffers(1, &bufferId);
	} else {
		//std::cerr << "OpenGL pixel buffers are not available" << std::endl;
		bufferId = 0;
	}
}

template <typename T>
PixelBuffer<T>::PixelBuffer(PixelBuffer<T>&& other) noexcept
	: allocated(std::move(other.allocated))
	, bufferId(other.bufferId)
	, width(other.width)
	, height(other.height)
{
	other.bufferId = 0;
}

template <typename T>
PixelBuffer<T>& PixelBuffer<T>::operator=(PixelBuffer<T>&& other) noexcept
{
	std::swap(allocated, other.allocated);
	std::swap(bufferId,  other.bufferId);
	std::swap(width,     other.width);
	std::swap(height,    other.height);
	return *this;
}

template <typename T>
PixelBuffer<T>::~PixelBuffer()
{
	glDeleteBuffers(1, &bufferId); // ok to delete '0'
}

template <typename T>
bool PixelBuffer<T>::openGLSupported() const
{
	return bufferId != 0;
}

template <typename T>
void PixelBuffer<T>::setImage(GLuint width_, GLuint height_)
{
	width = width_;
	height = height_;
	if (bufferId != 0) {
		bind();
		// TODO make performance hint configurable?
		glBufferData(GL_PIXEL_UNPACK_BUFFER_ARB,
		             width * height * 4,
		             nullptr, // leave data undefined
		             GL_STREAM_DRAW); // performance hint
		unbind();
	} else {
		allocated.resize(width * height);
	}
}

template <typename T>
void PixelBuffer<T>::bind() const
{
	if (bufferId != 0) {
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, bufferId);
	}
}

template <typename T>
void PixelBuffer<T>::unbind() const
{
	if (bufferId != 0) {
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, 0);
	}
}

template <typename T>
T* PixelBuffer<T>::getOffset(GLuint x, GLuint y)
{
	assert(x < width);
	assert(y < height);
	unsigned offset = x + width * y;
	if (bufferId != 0) {
		return static_cast<T*>(nullptr) + offset;
	} else {
		return &allocated[offset];
	}
}

template <typename T>
T* PixelBuffer<T>::mapWrite()
{
	if (bufferId != 0) {
		return reinterpret_cast<T*>(glMapBuffer(
			GL_PIXEL_UNPACK_BUFFER_ARB, GL_WRITE_ONLY));
	} else {
		return allocated.data();
	}
}

template <typename T>
void PixelBuffer<T>::unmap() const
{
	if (bufferId != 0) {
		glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER_ARB);
	}
}



/** Wrapper around an OpenGL shader: a program executed on the GPU.
  * This class is a base class for vertex and fragment shaders.
  */
class Shader
{
public:
	/** Returns true iff this shader is loaded and compiled without errors.
	  */
	bool isOK() const;

protected:
	/** Instantiates a shader.
	  * @param type The shader type: GL_VERTEX_SHADER or GL_FRAGMENT_SHADER.
	  * @param filename The GLSL source code for the shader.
	  */
	Shader(GLenum type, const std::string& filename);
	Shader(GLenum type, const std::string& header,
	                    const std::string& filename);
	~Shader();

private:
	void init(GLenum type, const std::string& header,
	                       const std::string& filename);

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
	VertexShader(const std::string& header, const std::string& filename);
};

/** Wrapper around an OpenGL fragment shader:
  * a program executed on the GPU that computes the colors of pixels.
  */
class FragmentShader : public Shader
{
public:
	/** Instantiates a fragment shader.
	  * @param filename The GLSL source code for the shader.
	  */
	explicit FragmentShader(const std::string& filename);
	FragmentShader(const std::string& header, const std::string& filename);
};

/** Wrapper around an OpenGL program:
  * a collection of vertex and fragment shaders.
  */
class ShaderProgram
{
public:
	ShaderProgram(const ShaderProgram&) = delete;
	ShaderProgram& operator=(const ShaderProgram&) = delete;

	/** Create handler and allocate underlying openGL object. */
	ShaderProgram() { allocate(); }

	/** Create null handler (don't yet allocate a openGL object). */
	explicit ShaderProgram(Null) : handle(0) {}

	/** Destroy handler object (release the underlying openGL object). */
	~ShaderProgram() { reset(); }

	/** Allocate a shader program handle. */
	void allocate();

	/** Release the shader program handle. */
	void reset();

	/** Returns the underlying openGL handler id.
	  * 0 iff no openGL program is allocated.
	  */
	GLuint get() const { return handle; }

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

	/** Bind the given name for a vertex shader attribute to the given
	  * location.
	  */
	void bindAttribLocation(unsigned index, const char* name);

	/** Gets a reference to a uniform variable declared in the shader source.
	  * Note that you have to activate this program before you can change
	  * the uniform variable's value.
	  */
	GLint getUniformLocation(const char* name) const;

	/** Makes this program the active shader program.
	  * This requires that the program is already linked.
	  */
	void activate() const;

	void validate();

private:
	GLuint handle;
};

class BufferObject
{
public:
	BufferObject();
	~BufferObject();
	BufferObject(BufferObject&& other) noexcept
		: bufferId(other.bufferId)
	{
		other.bufferId = 0;
	}
	BufferObject& operator=(BufferObject&& other) noexcept {
		std::swap(bufferId, other.bufferId);
		return *this;
	}

	GLuint get() const { return bufferId; }

private:
	GLuint bufferId;
};

} // namespace gl

#endif // COMPONENT_GL
#endif // GLUTIL_HH
