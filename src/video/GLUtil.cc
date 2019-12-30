#include "GLUtil.hh"
#include "File.hh"
#include "FileContext.hh"
#include "FileException.hh"
#include "InitException.hh"
#include "vla.hh"
#include "Version.hh"
#include <iostream>
#include <vector>
#include <cstdio>

using std::string;
using namespace openmsx;

namespace gl {

/*
void checkGLError(const string& prefix)
{
	GLenum error = glGetError();
	if (error != GL_NO_ERROR) {
		string err = (char*)gluErrorString(error);
		std::cerr << "GL error: " << prefix << ": " << err << '\n';
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

FrameBufferObject::FrameBufferObject(Texture& texture)
{
	glGenFramebuffers(1, &bufferId);
	push();
	glFramebufferTexture2D(GL_FRAMEBUFFER,
	                       GL_COLOR_ATTACHMENT0,
	                       GL_TEXTURE_2D, texture.textureId, 0);
	bool success = glCheckFramebufferStatus(GL_FRAMEBUFFER) ==
	               GL_FRAMEBUFFER_COMPLETE;
	pop();
	if (!success) {
		throw InitException(
			"Your OpenGL implementation support for "
			"framebuffer objects is too limited.");
	}
}

FrameBufferObject::~FrameBufferObject()
{
	pop();
	glDeleteFramebuffers(1, &bufferId);
}

void FrameBufferObject::push()
{
	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previousId);
	glBindFramebuffer(GL_FRAMEBUFFER, bufferId);
}

void FrameBufferObject::pop()
{
	glBindFramebuffer(GL_FRAMEBUFFER, GLuint(previousId));
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
	string source = "#version 330\n" + header;
	try {
		File file(systemFileContext().resolve("shaders/" + filename));
		auto mmap = file.mmap();
		source.append(reinterpret_cast<const char*>(mmap.data()),
		              mmap.size());
	} catch (FileException& e) {
		std::cerr << "Cannot find shader: " << e.getMessage() << '\n';
		handle = 0;
		return;
	}

	// Allocate shader handle.
	handle = glCreateShader(type);
	if (handle == 0) {
		std::cerr << "Failed to allocate shader\n";
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
		std::cerr << "Failed to allocate program\n";
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
	          << ": " << infoLog << '\n';
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


// class VertexArray

VertexArray::VertexArray()
{
	glGenVertexArrays(1, &bufferId);
}

VertexArray::~VertexArray()
{
	unbind();
	glDeleteVertexArrays(1, &bufferId); // ok to delete 0-buffer
}

} // namespace gl
