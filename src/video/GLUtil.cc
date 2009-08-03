// $Id$

#include "GLUtil.hh"
#include "File.hh"
#include "FileContext.hh"
#include "FileException.hh"
#include "InitException.hh"
#include "Math.hh"
#include "vla.hh"
#include "Version.hh"
#include <iostream>
#include <vector>
#include <memory>
#include <cstring>
#include <cstdio>

#ifdef GL_VERSION_2_0
#ifndef glGetShaderiv
#warning The version of GLEW you have installed is missing \
	some OpenGL 2.0 entry points.
#warning Please upgrade to GLEW 1.3.2 or higher.
#warning Until then, shaders are disabled.
#undef GL_VERSION_2_0
#endif
#endif

using std::string;

namespace openmsx {

/*namespace GLUtil {

void checkGLError(const string& prefix)
{
	GLenum error = glGetError();
	if (error != GL_NO_ERROR) {
		string err = (char*)gluErrorString(error);
		std::cerr << "GL error: " << prefix << ": " << err << std::endl;
	}
}

}*/


// class Texture

Texture::Texture(int type_)
	: type(type_)
{
	glGenTextures(1, &textureId);
	disableInterpolation();
}

Texture::~Texture()
{
	glDeleteTextures(1, &textureId);
}

void Texture::enableInterpolation()
{
	bind();
	glTexParameteri(type, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(type, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

void Texture::disableInterpolation()
{
	bind();
	glTexParameteri(type, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(type, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}

void Texture::setWrapMode(bool wrap)
{
	bind();
	int mode = wrap ? GL_REPEAT : GL_CLAMP;
	glTexParameteri(type, GL_TEXTURE_WRAP_S, mode);
	glTexParameteri(type, GL_TEXTURE_WRAP_T, mode);
	glTexParameteri(type, GL_TEXTURE_WRAP_R, mode);
}

void Texture::drawRect(GLfloat tx, GLfloat ty, GLfloat twidth, GLfloat theight,
                       GLint   x,  GLint   y,  GLint   width,  GLint   height)
{
	assert(type == GL_TEXTURE_2D);
	const GLint x2 = x + width;
	const GLint y2 = y + height;
	const GLfloat tx2 = tx + twidth;
	const GLfloat ty2 = ty + theight;
	bind();
	glEnable(GL_TEXTURE_2D);
	glBegin(GL_QUADS);
	glTexCoord2f(tx,  ty ); glVertex2i(x , y );
	glTexCoord2f(tx2, ty ); glVertex2i(x2, y );
	glTexCoord2f(tx2, ty2); glVertex2i(x2, y2);
	glTexCoord2f(tx,  ty2); glVertex2i(x,  y2);
	glEnd();
	glDisable(GL_TEXTURE_2D);
}


// class ColorTexture

ColorTexture::ColorTexture(GLsizei width_, GLsizei height_)
	: Texture(GL_TEXTURE_2D)
{
	width = width_;
	height = height_;
	glTexImage2D(
		GL_TEXTURE_2D,    // target
		0,                // level
		GL_RGBA8,         // internal format
		width,            // width
		height,           // height
		0,                // border
		GL_BGRA,          // format
		GL_UNSIGNED_BYTE, // type
		NULL);            // data
}


// class LuminanceTexture

LuminanceTexture::LuminanceTexture(GLsizei width, GLsizei height)
	: Texture(GL_TEXTURE_2D)
{
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage2D(
		GL_TEXTURE_2D,    // target
		0,                // level
		GL_LUMINANCE8,    // internal format
		width,            // width
		height,           // height
		0,                // border
		GL_LUMINANCE,     // format
		GL_UNSIGNED_BYTE, // type
		NULL);            // data
}

void LuminanceTexture::updateImage(
	GLint x, GLint y, GLsizei width, GLsizei height, GLbyte* data)
{
	bind();
	glTexSubImage2D(
		GL_TEXTURE_2D,    // target
		0,                // level
		x,                // offset x
		y,                // offset y
		width,            // width
		height,           // height
		GL_LUMINANCE,     // format
		GL_UNSIGNED_BYTE, // type
		data);            // data
}


// class BitmapTexture

BitmapTexture::BitmapTexture()
	: Texture(GL_TEXTURE_2D)
{
	static const unsigned SIZE = WIDTH * HEIGHT * 4;
	std::vector<byte> blackness(SIZE); // zero-initialized
	bind();
	glTexImage2D(
		GL_TEXTURE_2D,    // target
		0,                // level
		GL_RGBA8,         // internal format
		512,              // width
		1024,             // height
		0,                // border
		GL_BGRA,          // format
		GL_UNSIGNED_BYTE, // type
		&blackness[0]);   // data
}

void BitmapTexture::update(int y, const GLuint* dataBGR, int lineWidth)
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
		GL_BGRA,          // format
		GL_UNSIGNED_BYTE, // type
		dataBGR);         // data
}

void BitmapTexture::draw(
	int srcL, int srcT, int srcR, int srcB,
	int dstL, int dstT, int dstR, int dstB)
{
	static const GLfloat fx = 1.0f / GLfloat(WIDTH);
	static const GLfloat fy = 1.0f / GLfloat(HEIGHT);
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
	: Texture(GL_TEXTURE_2D) // TODO use 1D texture?
	, prevLineWidth(-1)
{
}

void LineTexture::update(const GLuint* dataBGR, int lineWidth)
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
			GL_BGRA,          // format
			GL_UNSIGNED_BYTE, // type
			dataBGR);         // data
	} else {
		// create new texture
		prevLineWidth = lineWidth;
		glTexImage2D(
			GL_TEXTURE_2D,    // target
			0,                // level
			GL_RGBA8,         // internal format
			lineWidth,        // width
			1,                // height
			0,                // border
			GL_BGRA,          // format
			GL_UNSIGNED_BYTE, // type
			dataBGR);         // data
	}
}

void LineTexture::draw(
	int texX, int screenX, int screenY, int width, int height)
{
	GLfloat texL = texX / 512.0f;
	GLfloat texR = (texX + width) / 512.0f;
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
			height);       // height
	} else {
		this->width = width;
		this->height = height;
		textureWidth =
			/* nice idea, but horribly slow on my GF5200
			GLEW_VERSION_2_0 || GLEW_ARB_texture_non_power_of_two
			? width :*/ Math::powerOfTwo(width);
		textureHeight =
			/* nice idea, but horribly slow on my GF5200
			GLEW_VERSION_2_0 || GLEW_ARB_texture_non_power_of_two
			? height :*/ Math::powerOfTwo(height);
		glCopyTexImage2D(
			GL_TEXTURE_2D,      // target
			0,                  // level
			GL_RGB,             // internal format
			0,                  // x
			0,                  // y
			textureWidth,       // width
			textureHeight,      // height
			0);                 // border
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
	float x = float(width)  / textureWidth;
	float y = float(height) / textureHeight;
	glBegin(GL_QUADS);
	glTexCoord2f(0.0f,    y); glVertex2i(offsetX,         offsetY         );
	glTexCoord2f(   x,    y); glVertex2i(offsetX + width, offsetY         );
	glTexCoord2f(   x, 0.0f); glVertex2i(offsetX + width, offsetY + height);
	glTexCoord2f(0.0f, 0.0f); glVertex2i(offsetX,         offsetY + height);
	glEnd();
	glDisable(GL_TEXTURE_2D);
}

void StoredFrame::drawBlend(
	int offsetX, int offsetY, int width, int height, double alpha)
{
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	// RGB come from texture, alpha comes from fragment color.
	glColor4f(1.0, 0.0, 0.0, GLfloat(alpha));
	draw(offsetX, offsetY, width, height);
	glDisable(GL_BLEND);
}


// class FrameBufferObject

static GLuint currentId = 0;
static std::vector<GLuint> stack;

FrameBufferObject::FrameBufferObject(Texture& texture, bool doPush)
{
	assert(texture.type == GL_TEXTURE_2D);
	glGenFramebuffersEXT(1, &bufferId);
	push();
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT,
	                          GL_COLOR_ATTACHMENT0_EXT,
	                          GL_TEXTURE_2D, texture.textureId, 0);
	bool success = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT) ==
	               GL_FRAMEBUFFER_COMPLETE_EXT;
	if (!doPush) {
		pop();
	}
	if (!success) {
		throw InitException(
			"Your OpenGL implementation support for "
			"framebuffer objects is too limited.");
	}
}

FrameBufferObject::~FrameBufferObject()
{
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
// Although this is not GL 2.0 dependent in itself, it is only used by GL 2.0
// specific routines.

#ifdef GL_VERSION_2_0
static string readTextFile(const string& filename)
{
	SystemFileContext context;
	CommandController* controller = NULL; // ok for SystemFileContext
	File file(context.resolve(*controller, filename));
	return string(reinterpret_cast<char*>(file.mmap()), file.getSize());
}
#endif


// class Shader

#ifdef GL_VERSION_2_0
Shader::Shader(GLenum type, const string& filename)
{
	// Check if GL 2.0 is present on this machine.
	if (!GLEW_VERSION_2_0) {
		handle = 0;
		return;
	}

	// Load shader source.
	string source;
	try {
		source = readTextFile("shaders/" + filename);
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
	glShaderSource(handle, 1, &sourcePtr, NULL);

	// Compile shader and print any errors and warnings.
	glCompileShader(handle);
	const bool ok = isOK();
	GLint infoLogLength = 0;
	glGetShaderiv(handle, GL_INFO_LOG_LENGTH, &infoLogLength);
	// note: the null terminator is included, so empty string has length 1
	if (!ok || (!Version::RELEASE && infoLogLength > 1)) {
		VLA(GLchar, infoLog, infoLogLength);
		glGetShaderInfoLog(handle, infoLogLength, NULL, infoLog);
		fprintf(stderr, "%s(s) compiling shader \"%s\":\n%s",
			ok ? "Warning" : "Error", filename.c_str(),
			infoLogLength > 1 ? infoLog : "(no details available)\n");
	}
}
#else
Shader::Shader(GLenum /*type*/, const string& /*filename*/)
{
	handle = 0;
}
#endif

Shader::~Shader()
{
#ifdef GL_VERSION_2_0
	if (handle != 0) {
		glDeleteShader(handle);
	}
#endif
}

bool Shader::isOK() const
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


// class VertexShader

#ifndef GL_VERSION_2_0
#ifndef GL_VERTEX_SHADER
#define GL_VERTEX_SHADER 0
#endif
#endif
VertexShader::VertexShader(const string& filename)
	: Shader(GL_VERTEX_SHADER, filename)
{
}


// class FragmentShader

#ifndef GL_VERSION_2_0
#ifndef GL_FRAGMENT_SHADER
#define GL_FRAGMENT_SHADER 0
#endif
#endif
FragmentShader::FragmentShader(const string& filename)
	: Shader(GL_FRAGMENT_SHADER, filename)
{
}


// class ShaderProgram

ShaderProgram::ShaderProgram()
{
#ifdef GL_VERSION_2_0
	// Check if GL 2.0 is present on this machine.
	if (!GLEW_VERSION_2_0) {
		//std::cerr << "Shaders not supported by installed OpenGL" << std::endl;
		handle = 0;
		return;
	}

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
	if (handle != 0) {
		glDeleteProgram(handle);
	}
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
void ShaderProgram::attach(const Shader& shader)
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
void ShaderProgram::attach(const Shader& /*shader*/)
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
	if (!ok || (!Version::RELEASE && infoLogLength > 1)) {
		VLA(GLchar, infoLog, infoLogLength);
		glGetProgramInfoLog(handle, infoLogLength, NULL, infoLog);
		fprintf(stderr, "%s(s) linking shader program:\n%s\n",
			ok ? "Warning" : "Error",
			infoLogLength > 1 ? infoLog : "(no details available)\n");
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
		fprintf(stderr, "%s: \"%s\"\n",
			  strncmp(name, "gl_", 3) == 0
			? "Accessing built-in shader variables is not possible"
			: "Could not find shader variable",
			name);
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
	if (GLEW_VERSION_2_0) {
		glUseProgram(handle);
	}
#endif
}

void ShaderProgram::deactivate()
{
#ifdef GL_VERSION_2_0
	if (GLEW_VERSION_2_0) {
		glUseProgram(0);
	}
#endif
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
	glGetProgramInfoLog(handle, infoLogLength, NULL, infoLog);
	std::cout << "Validate "
	          << ((validateStatus == GL_TRUE) ? string("OK") : string("FAIL"))
	          << ": " << infoLog << std::endl;
}

} // namespace openmsx

