#include "GLContext.hh"

#include "GLDefaultScaler.hh"
#include "gl_transform.hh"

#include <memory>

namespace gl {

// Global variables
std::optional<Context> context;

Context::Context()
{
	VertexShader   texVertexShader  ("texture.vert");
	FragmentShader texFragmentShader("texture.frag");
	progTex.allocate();
	progTex.attach(texVertexShader);
	progTex.attach(texFragmentShader);
	progTex.bindAttribLocation(0, "a_position");
	progTex.bindAttribLocation(1, "a_texCoord");
	progTex.link();
	progTex.activate();
	glUniform1i(progTex.getUniformLocation("u_tex"), 0);
	unifTexColor = progTex.getUniformLocation("u_color");
	unifTexMvp   = progTex.getUniformLocation("u_mvpMatrix");

	VertexShader   fillVertexShader  ("fill.vert");
	FragmentShader fillFragmentShader("fill.frag");
	progFill.allocate();
	progFill.attach(fillVertexShader);
	progFill.attach(fillFragmentShader);
	progFill.bindAttribLocation(0, "a_position");
	progFill.bindAttribLocation(1, "a_color");
	progFill.link();
	progFill.activate();
	unifFillMvp = progFill.getUniformLocation("u_mvpMatrix");
}

Context::~Context() = default;

openmsx::GLScaler& Context::getFallbackScaler()
{
	if (!fallbackScaler) {
		fallbackScaler = std::make_unique<openmsx::GLDefaultScaler>();
	}
	return *fallbackScaler;
}

void Context::setupMvpMatrix(gl::vec2 logicalSize)
{
	pixelMvp = ortho(logicalSize.x, logicalSize.y);
}

} // namespace gl
