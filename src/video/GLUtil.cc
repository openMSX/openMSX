// $Id$

#include "GLUtil.hh"
#include "File.hh"
#include "FileContext.hh"
#include "FileException.hh"
#include <iostream>
#include <memory>
#include <cassert>
#include <cstdlib>
#include <cstring>

#ifdef GL_VERSION_2_0
#ifndef glGetShaderiv
#warning The version of GLEW you have installed is missing \
	some OpenGL 2.0 entry points.
#warning Please upgrade to GLEW 1.3.2 or higher.
#warning Until then, shaders are disabled.
#undef GL_VERSION_2_0
#endif
#endif

namespace openmsx {


// class Texture

Texture::Texture()
{
	glGenTextures(1, &textureId);
	glBindTexture(GL_TEXTURE_2D, textureId);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}

Texture::~Texture()
{
	glDeleteTextures(1, &textureId);
}


// class BitmapTexture

BitmapTexture::BitmapTexture()
	: Texture()
{
	static const unsigned SIZE = WIDTH * HEIGHT * 4;
	void* blackness = malloc(SIZE);
	memset(blackness, 0, SIZE);
	bind();
	glTexImage2D(
		GL_TEXTURE_2D,    // target
		0,                // level
		GL_RGBA,          // internal format
		512,              // width
		1024,             // height
		0,                // border
		GL_RGBA,          // format
		GL_UNSIGNED_BYTE, // type
		blackness         // data
		);
	free(blackness);
}

void BitmapTexture::update(int y, const GLuint* data, int lineWidth)
{
	assert(0 <= y && y < HEIGHT);
	assert(lineWidth <= WIDTH);
	bind();
	// Replace line in existing texture.
	glTexSubImage2D(
		GL_TEXTURE_2D,    // target
		0,                // level
		0,                // x-offset
		y,                // y-offset
		lineWidth,        // width
		1,                // height
		GL_RGBA,          // format
		GL_UNSIGNED_BYTE, // type
		data              // data
		);
}

void BitmapTexture::draw(
	int srcL, int srcT, int srcR, int srcB,
	int dstL, int dstT, int dstR, int dstB
	)
{
	static const GLfloat fx = 1.0f / static_cast<GLfloat>(WIDTH);
	static const GLfloat fy = 1.0f / static_cast<GLfloat>(HEIGHT);
	const GLfloat srcLF = srcL * fx;
	const GLfloat srcRF = srcR * fx;
	const GLfloat srcTF = srcT * fy;
	const GLfloat srcBF = srcB * fy;
	bind();
	glBegin(GL_QUADS);
	glTexCoord2f(srcLF, srcTF); glVertex2i(dstL, dstT);
	glTexCoord2f(srcRF, srcTF); glVertex2i(dstR, dstT);
	glTexCoord2f(srcRF, srcBF); glVertex2i(dstR, dstB);
	glTexCoord2f(srcLF, srcBF); glVertex2i(dstL, dstB);
	glEnd();
}


// class LineTexture

LineTexture::LineTexture()
	: Texture(), prevLineWidth(-1)
{
}

void LineTexture::update(const GLuint* data, int lineWidth)
{
	bind();
	if (prevLineWidth == lineWidth) {
		// reuse existing texture
		glTexSubImage2D(
			GL_TEXTURE_2D,    // target
			0,                // level
			0,                // x-offset
			0,                // y-offset
			lineWidth,        // width
			1,                // height
			GL_RGBA,          // format
			GL_UNSIGNED_BYTE, // type
			data              // data
			);
	} else {
		// create new texture
		prevLineWidth = lineWidth;
		glTexImage2D(
			GL_TEXTURE_2D,    // target
			0,                // level
			GL_RGBA,          // internal format
			lineWidth,        // width
			1,                // height
			0,                // border
			GL_RGBA,          // format
			GL_UNSIGNED_BYTE, // type
			data              // data
			);
	}
}

void LineTexture::draw(
	int texX, int screenX, int screenY, int width, int height)
{
	double texL = texX / 512.0f;
	double texR = (texX + width) / 512.0f;
	int screenL = screenX;
	int screenR = screenL + width;
	bind();
	glBegin(GL_QUADS);
	glTexCoord2f(texL, 1.0f); glVertex2i(screenL, screenY);
	glTexCoord2f(texR, 1.0f); glVertex2i(screenR, screenY);
	glTexCoord2f(texR, 0.0f); glVertex2i(screenR, screenY + height);
	glTexCoord2f(texL, 0.0f); glVertex2i(screenL, screenY + height);
	glEnd();
}


// class StoredFrame

static unsigned powerOfTwo(unsigned a)
{
	unsigned res = 1;
	while (a > res) res <<= 1;
	return res;
}

StoredFrame::StoredFrame()
	: stored(false)
{
}

void StoredFrame::store(unsigned width, unsigned height)
{
	texture.bind();
	if (stored && this->width == width && this->height == height) {
		glCopyTexSubImage2D(
			GL_TEXTURE_2D, // target
			0,             // level
			0,             // x-offset
			0,             // y-offset
			0,             // x
			0,             // y
			width,         // width
			height         // height
			);
	} else {
		this->width = width;
		this->height = height;
		textureWidth =
			/* nice idea, but horribly slow on my GF5200
			GLEW_VERSION_2_0 || GLEW_ARB_texture_non_power_of_two
			? width :*/ powerOfTwo(width);
		textureHeight =
			/* nice idea, but horribly slow on my GF5200
			GLEW_VERSION_2_0 || GLEW_ARB_texture_non_power_of_two
			? height :*/ powerOfTwo(height);
		glCopyTexImage2D(
			GL_TEXTURE_2D,      // target
			0,                  // level
			GL_RGB,             // internal format
			0,                  // x
			0,                  // y
			textureWidth,       // width
			textureHeight,      // height
			0                   // border
			);
	}
	stored = true;
}

void StoredFrame::draw(int offsetX, int offsetY, int width, int height)
{
	assert(stored);
	glEnable(GL_TEXTURE_2D);
	texture.bind();
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	// TODO: Create display list(s)?
	float x = static_cast<float>(width)  / textureWidth;
	float y = static_cast<float>(height) / textureHeight;
	glBegin(GL_QUADS);
	glTexCoord2f(0.0f,    y); glVertex2i(offsetX,         offsetY         );
	glTexCoord2f(   x,    y); glVertex2i(offsetX + width, offsetY         );
	glTexCoord2f(   x, 0.0f); glVertex2i(offsetX + width, offsetY + height);
	glTexCoord2f(0.0f, 0.0f); glVertex2i(offsetX,         offsetY + height);
	glEnd();
	glDisable(GL_TEXTURE_2D);
}

void StoredFrame::drawBlend(
	int offsetX, int offsetY, int width, int height, double alpha
	)
{
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	// RGB come from texture, alpha comes from fragment colour.
	glColor4f(1.0, 0.0, 0.0, alpha);
	draw(offsetX, offsetY, width, height);
	glDisable(GL_BLEND);
}


// class FragmentShader

#ifdef GL_VERSION_2_0
static std::string readTextFile(const std::string& filename)
{
	SystemFileContext context;
	File file(context.resolve(filename));
	return std::string(reinterpret_cast<char*>(file.mmap()), file.getSize());
}
#endif

#ifdef GL_VERSION_2_0
FragmentShader::FragmentShader(const std::string& filename)
{
	// Allocate shader handle.
	handle = glCreateShader(GL_FRAGMENT_SHADER);
	if (handle == 0) {
		std::cerr << "Failed to allocate shader" << std::endl;
		return;
	}

	// Load shader source.
	std::string source;
	try {
		source = readTextFile("shaders/" + filename);
	} catch (FileException& e) {
		std::cerr << "Cannot find shader: " << e.getMessage() << std::endl;
		handle = 0;
		return;
	}
	const char* sourcePtr = source.c_str();
	glShaderSource(handle, 1, &sourcePtr, NULL);

	// Compile shader and print any errors and warnings.
	glCompileShader(handle);
	const bool ok = isOK();
	GLint infoLogLength = 0;
	glGetShaderiv(handle, GL_INFO_LOG_LENGTH, &infoLogLength);
	// note: the null terminator is included, so empty string has length 1
	if (!ok || infoLogLength > 1) {
		GLchar infoLog[infoLogLength];
		glGetShaderInfoLog(handle, infoLogLength, NULL, infoLog);
		fprintf(
			stderr, "%s(s) compiling shader \"%s\":\n%s",
			ok ? "Warning" : "Error", filename.c_str(),
			infoLogLength > 1 ? infoLog : "(no details available)\n"
			);
	}
}
#else
FragmentShader::FragmentShader(const std::string& /*filename*/)
{
	handle = 0;
}
#endif

FragmentShader::~FragmentShader()
{
#ifdef GL_VERSION_2_0
	glDeleteShader(handle);
#endif
}

bool FragmentShader::isOK() const
{
#ifdef GL_VERSION_2_0
	if (handle == 0) {
		return false;
	}
	GLint compileStatus = GL_FALSE;
	glGetShaderiv(handle, GL_COMPILE_STATUS, &compileStatus);
	return compileStatus == GL_TRUE;
#else
	return false;
#endif
}


// class ShaderProgram

ShaderProgram::ShaderProgram()
{
#ifdef GL_VERSION_2_0
	// Allocate program handle.
	handle = glCreateProgram();
	if (handle == 0) {
		std::cerr << "Failed to allocate program" << std::endl;
		return;
	}
#else
	handle = 0;
#endif
}

ShaderProgram::~ShaderProgram()
{
#ifdef GL_VERSION_2_0
	glDeleteProgram(handle);
#endif
}

bool ShaderProgram::isOK() const
{
#ifdef GL_VERSION_2_0
	if (handle == 0) {
		return false;
	}
	GLint linkStatus = GL_FALSE;
	glGetProgramiv(handle, GL_LINK_STATUS, &linkStatus);
	return linkStatus == GL_TRUE;
#else
	return false;
#endif
}

#ifdef GL_VERSION_2_0
void ShaderProgram::attach(const FragmentShader& shader)
{
	// Sanity check on this program.
	if (handle == 0) {
		return;
	}
	// Sanity check on the shader.
	if (!shader.isOK()) {
		return;
	}
	// Attach it.
	glAttachShader(handle, shader.handle);
}
#else
void ShaderProgram::attach(const FragmentShader& /*shader*/)
{
}
#endif

void ShaderProgram::link()
{
#ifdef GL_VERSION_2_0
	// Sanity check on this program.
	if (handle == 0) {
		return;
	}
	// Link the program and print any errors and warnings.
	glLinkProgram(handle);
	const bool ok = isOK();
	GLint infoLogLength = 0;
	glGetProgramiv(handle, GL_INFO_LOG_LENGTH, &infoLogLength);
	// note: the null terminator is included, so empty string has length 1
	if (!ok || infoLogLength > 1) {
		GLchar infoLog[infoLogLength];
		glGetProgramInfoLog(handle, infoLogLength, NULL, infoLog);
		fprintf(
			stderr, "%s(s) linking shader program:\n%s",
			ok ? "Warning" : "Error",
			infoLogLength > 1 ? infoLog : "(no details available)\n"
			);
	}
#endif
}

#ifdef GL_VERSION_2_0
GLint ShaderProgram::getUniformLocation(const char* name) const
{
	// Sanity check on this program.
	if (!isOK()) {
		return -1;
	}
	// Get location and verify returned value.
	GLint location = glGetUniformLocation(handle, name);
	if (location == -1) {
		fprintf(
			stderr, "%s: \"%s\"\n",
			  strncmp(name, "gl_", 3) == 0
			? "Accessing built-in shader variables is not possible"
			: "Could not find shader variable",
			name
			);
	}
	return location;
}
#else
GLint ShaderProgram::getUniformLocation(const char* /*name*/) const
{
	return -1;
}
#endif

void ShaderProgram::activate() const
{
#ifdef GL_VERSION_2_0
	glUseProgram(handle);
#endif
}

void ShaderProgram::deactivate() const
{
#ifdef GL_VERSION_2_0
	glUseProgram(0);
#endif
}

} // namespace openmsx

