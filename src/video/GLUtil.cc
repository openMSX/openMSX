#include "GLUtil.hh"
#include "File.hh"
#include "FileContext.hh"
#include "FileException.hh"
#include "InitException.hh"
#include "vla.hh"
#include "Version.hh"
#include <iostream>
#include <vector>
#include <cstring>
#include <cstdio>

#ifndef glGetShaderiv
#error The version of GLEW you have installed is missing some OpenGL 2.0 entry points. \
       Please upgrade to GLEW 1.3.2 or higher.
#endif

using std::string;
using namespace openmsx;

namespace gl {

/*
void checkGLError(const string& prefix)
{
	GLenum error = glGetError();
	if (error != GL_NO_ERROR) {
		string err = (char*)gluErrorString(error);
		std::cerr << "GL error: " << prefix << ": " << err << std::endl;
	}
}
*/


// class Texture

Texture::Texture(bool interpolation, bool wrap)
{
	allocate();
	setInterpolation(interpolation);
	setWrapMode(wrap);
}

void Texture::allocate()
{
	glGenTextures(1, &textureId);
}

void Texture::reset()
{
	glDeleteTextures(1, &textureId); // ok to delete 0-texture
	textureId = 0;
}

void Texture::setInterpolation(bool interpolation)
{
	bind();
	int mode = interpolation ? GL_LINEAR : GL_NEAREST;
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, mode);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mode);
}

void Texture::setWrapMode(bool wrap)
{
	bind();
	int mode = wrap ? GL_REPEAT : GL_CLAMP_TO_EDGE;
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, mode);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, mode);
}


// class ColorTexture

ColorTexture::ColorTexture(GLsizei width_, GLsizei height_)
{
	resize(width_, height_);
}

void ColorTexture::resize(GLsizei width_, GLsizei height_)
{
	width = width_;
	height = height_;
	bind();
	glTexImage2D(
		GL_TEXTURE_2D,    // target
		0,                // level
		GL_RGBA8,         // internal format
		width,            // width
		height,           // height
		0,                // border
		GL_BGRA,          // format
		GL_UNSIGNED_BYTE, // type
		nullptr);         // data
}


// class FrameBufferObject

static GLuint currentId = 0;
static std::vector<GLuint> stack;

FrameBufferObject::FrameBufferObject()
	: bufferId(0) // 0 is not a valid openGL name
{
}

FrameBufferObject::FrameBufferObject(Texture& texture)
{
	glGenFramebuffersEXT(1, &bufferId);
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, bufferId);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT,
	                          GL_COLOR_ATTACHMENT0_EXT,
	                          GL_TEXTURE_2D, texture.textureId, 0);
	bool success = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT) ==
	               GL_FRAMEBUFFER_COMPLETE_EXT;
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, currentId);
	if (!success) {
		throw InitException(
			"Your OpenGL implementation support for "
			"framebuffer objects is too limited.");
	}
}

FrameBufferObject::~FrameBufferObject()
{
	// It's ok to delete '0' (it's a NOP), but we anyway have to check
	// for pop().
	if (!bufferId) return;

	if (currentId == bufferId) {
		pop();
	}
	glDeleteFramebuffersEXT(1, &bufferId);
}

void FrameBufferObject::push()
{
	stack.push_back(currentId);
	currentId = bufferId;
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, currentId);
}

void FrameBufferObject::pop()
{
	assert(currentId == bufferId);
	assert(!stack.empty());
	currentId = stack.back();
	stack.pop_back();
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, currentId);
}


bool PixelBuffers::enabled = true;

// Utility function used by Shader.
static string readTextFile(const string& filename)
{
	File file(systemFileContext().resolve(filename));
	size_t size;
	const byte* data = file.mmap(size);
	return string(reinterpret_cast<const char*>(data), size);
}


// class Shader

Shader::Shader(GLenum type, const string& filename)
{
	init(type, {}, filename);
}

Shader::Shader(GLenum type, const string& header, const string& filename)
{
	init(type, header, filename);
}

void Shader::init(GLenum type, const string& header, const string& filename)
{
	// Load shader source.
	string source = "#version 110\n" + header;
	try {
		source += readTextFile("shaders/" + filename);
	} catch (FileException& e) {
		std::cerr << "Cannot find shader: " << e.getMessage() << std::endl;
		handle = 0;
		return;
	}

	// Allocate shader handle.
	handle = glCreateShader(type);
	if (handle == 0) {
		std::cerr << "Failed to allocate shader" << std::endl;
		return;
	}

	// Set shader source.
	const char* sourcePtr = source.c_str();
	glShaderSource(handle, 1, &sourcePtr, nullptr);

	// Compile shader and print any errors and warnings.
	glCompileShader(handle);
	const bool ok = isOK();
	GLint infoLogLength = 0;
	glGetShaderiv(handle, GL_INFO_LOG_LENGTH, &infoLogLength);
	// note: the null terminator is included, so empty string has length 1
	if (!ok || (!Version::RELEASE && infoLogLength > 1)) {
		VLA(GLchar, infoLog, infoLogLength);
		glGetShaderInfoLog(handle, infoLogLength, nullptr, infoLog);
		fprintf(stderr, "%s(s) compiling shader \"%s\":\n%s",
			ok ? "Warning" : "Error", filename.c_str(),
			infoLogLength > 1 ? infoLog : "(no details available)\n");
	}
}

Shader::~Shader()
{
	glDeleteShader(handle); // ok to delete '0'
}

bool Shader::isOK() const
{
	if (handle == 0) return false;
	GLint compileStatus = GL_FALSE;
	glGetShaderiv(handle, GL_COMPILE_STATUS, &compileStatus);
	return compileStatus == GL_TRUE;
}


// class VertexShader

VertexShader::VertexShader(const string& filename)
	: Shader(GL_VERTEX_SHADER, filename)
{
}

VertexShader::VertexShader(const string& header, const string& filename)
	: Shader(GL_VERTEX_SHADER, header, filename)
{
}


// class FragmentShader

FragmentShader::FragmentShader(const string& filename)
	: Shader(GL_FRAGMENT_SHADER, filename)
{
}

FragmentShader::FragmentShader(const string& header, const string& filename)
	: Shader(GL_FRAGMENT_SHADER, header, filename)
{
}


// class ShaderProgram

void ShaderProgram::allocate()
{
	handle = glCreateProgram();
	if (handle == 0) {
		std::cerr << "Failed to allocate program" << std::endl;
	}
}

void ShaderProgram::reset()
{
	glDeleteProgram(handle); // ok to delete '0'
	handle = 0;
}

bool ShaderProgram::isOK() const
{
	if (handle == 0) return false;
	GLint linkStatus = GL_FALSE;
	glGetProgramiv(handle, GL_LINK_STATUS, &linkStatus);
	return linkStatus == GL_TRUE;
}

void ShaderProgram::attach(const Shader& shader)
{
	// Sanity check on this program.
	if (handle == 0) return;

	// Sanity check on the shader.
	if (!shader.isOK()) return;

	// Attach it.
	glAttachShader(handle, shader.handle);
}

void ShaderProgram::link()
{
	// Sanity check on this program.
	if (handle == 0) return;

	// Link the program and print any errors and warnings.
	glLinkProgram(handle);
	const bool ok = isOK();
	GLint infoLogLength = 0;
	glGetProgramiv(handle, GL_INFO_LOG_LENGTH, &infoLogLength);
	// note: the null terminator is included, so empty string has length 1
	if (!ok || (!Version::RELEASE && infoLogLength > 1)) {
		VLA(GLchar, infoLog, infoLogLength);
		glGetProgramInfoLog(handle, infoLogLength, nullptr, infoLog);
		fprintf(stderr, "%s(s) linking shader program:\n%s\n",
			ok ? "Warning" : "Error",
			infoLogLength > 1 ? infoLog : "(no details available)\n");
	}
}

void ShaderProgram::bindAttribLocation(unsigned index, const char* name)
{
	glBindAttribLocation(handle, index, name);
}

GLint ShaderProgram::getUniformLocation(const char* name) const
{
	// Sanity check on this program.
	if (!isOK()) return -1;

	return glGetUniformLocation(handle, name);
}

void ShaderProgram::activate() const
{
	glUseProgram(handle);
}

// only useful for debugging
void ShaderProgram::validate()
{
	glValidateProgram(handle);
	GLint validateStatus = GL_FALSE;
	glGetProgramiv(handle, GL_VALIDATE_STATUS, &validateStatus);
	GLint infoLogLength = 0;
	glGetProgramiv(handle, GL_INFO_LOG_LENGTH, &infoLogLength);
	// note: the null terminator is included, so empty string has length 1
	VLA(GLchar, infoLog, infoLogLength);
	glGetProgramInfoLog(handle, infoLogLength, nullptr, infoLog);
	std::cout << "Validate "
	          << ((validateStatus == GL_TRUE) ? "OK" : "FAIL")
	          << ": " << infoLog << std::endl;
}


// class BufferObject

BufferObject::BufferObject()
{
	glGenBuffers(1, &bufferId);
}

BufferObject::~BufferObject()
{
	glDeleteBuffers(1, &bufferId); // ok to delete 0-buffer
}

} // namespace gl
