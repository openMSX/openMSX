#ifndef GLUTIL_HH
#define GLUTIL_HH

// Include GLEW headers.
#include <GL/glew.h>
// Include OpenGL headers.
#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

#include "MemBuffer.hh"

#include <bit>
#include <cassert>
#include <cstdint>
#include <span>
#include <string_view>

// arbitrary but distinct values, (roughly) ordered according to version number
#define OPENGL_ES_2_0 1
#define OPENGL_2_1    2
#define OPENGL_3_3    3
#define OPENGL_VERSION OPENGL_2_1

namespace gl {

void checkGLError(std::string_view prefix);


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
	[[nodiscard]] GLuint get() const { return textureId; }

	/** Return as a 'void*' (needed for 'Dear ImGui').
	  */
	[[nodiscard]] unsigned long long getImGui() const {
		assert(textureId);
		return uintptr_t(textureId);
	}

	/** Makes this texture the active GL texture.
	  * The other methods of this class and its subclasses will implicitly
	  * bind the texture, so you only need this method to explicitly bind
	  * this texture for use in GL function calls outside of this class.
	  */
	void bind() const {
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
	ColorTexture() = default;

	/** Create color texture with given size.
	  * Initial content is undefined.
	  */
	ColorTexture(GLsizei width, GLsizei height);
	void resize(GLsizei width, GLsizei height);

	[[nodiscard]] GLsizei getWidth () const { return width;  }
	[[nodiscard]] GLsizei getHeight() const { return height; }

private:
	GLsizei width = 0;
	GLsizei height = 0;
};

class FrameBufferObject
{
public:
	FrameBufferObject() = default;
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
	GLuint bufferId = 0; // 0 is not a valid openGL name
	GLint previousId = 0;
};

/** Wrapper around a pixel buffer.
  * The pixel buffer will be allocated in VRAM if possible, in main RAM
  * otherwise.
  * The pixel type is templatized T.
  *
  * Note: openGL ES 2.0 does not yet support this. So for now we always use the
  * fallback implementation, maybe we can re-enable this when we switch to
  * openGL ES 3.0.
  */
template<typename T> class PixelBuffer
{
public:
	PixelBuffer();
	PixelBuffer(const PixelBuffer& other) = delete;
	PixelBuffer(PixelBuffer&& other) noexcept;
	PixelBuffer& operator=(const PixelBuffer& other) = delete;
	PixelBuffer& operator=(PixelBuffer&& other) noexcept;
	~PixelBuffer();

	/** Allocate the maximum required size for this buffer.
	  */
	void allocate(GLuint size);

	/** Bind this PixelBuffer. Must be called before the methods
	  * getOffset() or mapWrite() are used. (Is a wrapper around
	  * glBindBuffer).
	  */
	void bind() const;

	/** Unbind this buffer.
	  */
	void unbind() const;

	/** Maps the contents of this buffer into memory. The returned buffer
	  * is write-only (reading could be very slow or even result in a
	  * segfault).
	  * @return Pointer through which you can write pixels to this buffer,
	  *         or 0 if the buffer could not be mapped.
	  * @pre This PixelBuffer must be bound (see bind()) before calling
	  *      this method.
	  */
	[[nodiscard]] std::span<T> mapWrite();

	/** Unmaps the contents of this buffer.
	  * After this call, you must no longer use the pointer returned by
	  * mapWrite.
	  */
	void unmap() const;

private:
	/** Buffer for main RAM fallback (not allocated in the normal case).
	  */
	openmsx::MemBuffer<T> allocated;

	/** Available buffer size
	 */
	GLuint size = 0;

	/** Handle of the GL buffer, or 0 if no GL buffer is available.
	  */
	//GLuint bufferId;
};

// class PixelBuffer

template<typename T>
PixelBuffer<T>::PixelBuffer()
{
	//glGenBuffers(1, &bufferId);
}

template<typename T>
PixelBuffer<T>::~PixelBuffer()
{
	//glDeleteBuffers(1, &bufferId); // ok to delete '0'
}

template<typename T>
PixelBuffer<T>::PixelBuffer(PixelBuffer<T>&& other) noexcept
	: allocated(std::move(other.allocated))
	, size(other.size)
	//, bufferId(other.bufferId)
{
	other.size = 0;
	//other.bufferId = 0;
}

template<typename T>
PixelBuffer<T>& PixelBuffer<T>::operator=(PixelBuffer<T>&& other) noexcept
{
	std::swap(allocated, other.allocated);
	std::swap(size,      other.size);
	//std::swap(bufferId,  other.bufferId);
	return *this;
}

template<typename T>
void PixelBuffer<T>::allocate(GLuint size_)
{
	size = size_;
	//if (bufferId != 0) {
	//	bind();
	//	// TODO make performance hint configurable?
	//	glBufferData(GL_PIXEL_UNPACK_BUFFER,
	//	             size * 4,
	//	             nullptr, // leave data undefined
	//	             GL_STREAM_DRAW); // performance hint
	//	unbind();
	//} else {
		allocated.resize(size);
	//}
}

template<typename T>
void PixelBuffer<T>::bind() const
{
	//if (bufferId != 0) {
	//	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, bufferId);
	//}
}

template<typename T>
void PixelBuffer<T>::unbind() const
{
	//if (bufferId != 0) {
	//	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
	//}
}

template<typename T>
std::span<T> PixelBuffer<T>::mapWrite()
{
	//if (bufferId != 0) {
	//	return std::bit_cast<T*>(glMapBuffer(
	//		GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY));
	//} else {
		return {allocated.data(), size};
	//}
}

template<typename T>
void PixelBuffer<T>::unmap() const
{
	//if (bufferId != 0) {
	//	glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
	//}
}



/** Wrapper around an OpenGL shader: a program executed on the GPU.
  * This class is a base class for vertex and fragment shaders.
  */
class Shader
{
public:
	Shader(const Shader&) = delete;
	Shader(Shader&&) = delete;
	Shader& operator=(const Shader&) = delete;
	Shader& operator=(Shader&&) = delete;

	/** Returns true iff this shader is loaded and compiled without errors.
	  */
	[[nodiscard]] bool isOK() const;

protected:
	/** Instantiates a shader.
	  * @param type The shader type: GL_VERTEX_SHADER or GL_FRAGMENT_SHADER.
	  * @param filename The GLSL source code for the shader.
	  */
	Shader(GLenum type, std::string_view filename) {
		init(type, {}, filename);
	}
	Shader(GLenum type, std::string_view header, std::string_view filename) {
		init(type, header, filename);
	}
	~Shader();

private:
	void init(GLenum type, std::string_view header,
	                       std::string_view filename);

	friend class ShaderProgram;

private:
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
	explicit VertexShader(std::string_view filename)
		: Shader(GL_VERTEX_SHADER, filename) {}
	VertexShader(std::string_view header, std::string_view filename)
		: Shader(GL_VERTEX_SHADER, header, filename) {}
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
	explicit FragmentShader(std::string_view filename)
		: Shader(GL_FRAGMENT_SHADER, filename) {}
	FragmentShader(std::string_view header, std::string_view filename)
		: Shader(GL_FRAGMENT_SHADER, header, filename) {}
};

/** Wrapper around an OpenGL program:
  * a collection of vertex and fragment shaders.
  */
class ShaderProgram
{
public:
	ShaderProgram(const ShaderProgram&) = delete;
	ShaderProgram(ShaderProgram&&) = delete;
	ShaderProgram& operator=(const ShaderProgram&) = delete;
	ShaderProgram& operator=(ShaderProgram&&) = delete;

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
	[[nodiscard]] GLuint get() const { return handle; }

	/** Returns true iff this program was linked without errors.
	  * Note that this will certainly return false until link() is called.
	  */
	[[nodiscard]] bool isOK() const;

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
	[[nodiscard]] GLint getUniformLocation(const char* name) const;

	/** Makes this program the active shader program.
	  * This requires that the program is already linked.
	  */
	void activate() const;

	void validate() const;

private:
	GLuint handle;
};

class BufferObject
{
public:
	BufferObject();
	BufferObject(const BufferObject&) = delete;
	BufferObject& operator=(const BufferObject&) = delete;
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

	[[nodiscard]] GLuint get() const { return bufferId; }

private:
	GLuint bufferId;
};

} // namespace gl

#endif // GLUTIL_HH
